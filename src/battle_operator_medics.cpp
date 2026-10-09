#include <stronghold/simulation/battle.hpp>
#include <tuple>

namespace Stronghold
{
	namespace
	{
		struct MedicKitScratchGuard
		{
			std::size_t& MyDepth;

			~MedicKitScratchGuard() { --MyDepth; }
		};

		double ElementLoad(const CombatUnit& _unit)
		{
			return *std::ranges::max_element(_unit.MyElements.MyGauges);
		}
	}

	void Battle::HaroldBeforeAttack(CombatUnit& _unit, std::vector<UnitId>& _targets)
	{
		auto& scratch = AcquireAttackScratch(); const MedicKitScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (!ally.MyAlive || ally.MyHidden || ally.MyKind == UnitKind::DEVICE || !InRuleRange(_unit.MyId, id) ||
				(id != _unit.MyId && (ally.MyStatuses.Has(CombatStatus::NO_HEAL) || ally.MyDefinition.MyAttack.MyNoHeal))) continue;
			if (std::isless(ally.MyHealth, ally.MyStats.MyMaxHealth - 1e-6) || std::isgreater(ElementLoad(ally), 0)) scratch.MyTargets.push_back(id);
		}
		std::ranges::sort(scratch.MyTargets, [&](UnitId _left, UnitId _right)
		{
			const auto& left = Unit(_left); const auto& right = Unit(_right);
			return std::tuple{-ElementLoad(left), left.MyHealth / left.MyStats.MyMaxHealth, left.MyDeploySequence} <
				std::tuple{-ElementLoad(right), right.MyHealth / right.MyStats.MyMaxHealth, right.MyDeploySequence};
		});
		if (!scratch.MyTargets.empty())
		{
			const auto count = std::min(std::max<std::size_t>(1, _targets.size()), scratch.MyTargets.size());
			_targets.assign(scratch.MyTargets.begin(), scratch.MyTargets.begin() + static_cast<std::ptrdiff_t>(count));
		}
		// 记住治疗前超过半量表的目标；基础元素恢复可能已把量表降到一半以下。
		_unit.MyHaroldHalf.clear();
		for (const auto id : _targets) if (std::isgreater(ElementLoad(Unit(id)), 500)) _unit.MyHaroldHalf.push_back(id);
	}

	void Battle::OperatorHealHit(UnitId _source, UnitId _target)
	{
		const auto& source = Unit(_source);
		if (const auto* lumen = source.MyDefinition.MyOperatorKit ? std::get_if<LumenKit>(source.MyDefinition.MyOperatorKit) : nullptr; lumen && source.MySkill.MyActive) LumenHealHit(_source, _target, *lumen);
		if (const auto* reckpr = source.MyDefinition.MyOperatorKit ? std::get_if<ReckprKit>(source.MyDefinition.MyOperatorKit) : nullptr;
			reckpr && reckpr->MyGuard && (!reckpr->MySharedGuard || std::isgreater(reckpr->MyGuardHeal, 0)) && source.MySkill.MyActive && Unit(_target).MyAlive && Unit(_target).MySide == UnitSide::ALLY && Unit(_target).MyKind == UnitKind::OPERATOR)
			(void)AddBuff(_target, {.MyKey = source.MyGuardHealKey, .MySource = _source, .MyDuration = reckpr->MyGuardDuration});
		const auto* kit = source.MyDefinition.MyOperatorKit ? std::get_if<HaroldKit>(source.MyDefinition.MyOperatorKit) : nullptr;
		if (kit && kit->MyTriage && source.MySkill.MyActive && std::isgreater(kit->MyRecoveryScale, 1) && std::ranges::contains(source.MyHaroldHalf, _target))
			(void)ReduceElement(_target, source.MyStats.MyAttack * source.MyDefinition.MyAttack.MyElementHealRatio * (kit->MyRecoveryScale - 1));
	}

	void Battle::HaroldElementHit(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::ELEMENT_HIT || !_event.MyTarget) return;
		const auto& target = Unit(_event.MyTarget);
		if (target.MySide != UnitSide::ALLY || !std::isgreater(ElementLoad(target), 500)) return;
		double resistance = 0;
		for (const auto id : _MyHarolds)
		{
			const auto& source = Unit(id);
			if (!source.MyAlive || source.MyOperatorHooksReleased || !InRuleRange(id, target.MyId)) continue;
			resistance = std::max(resistance, std::get<HaroldKit>(*source.MyDefinition.MyOperatorKit).MyResistance);
		}
		_event.MyElementHit.MyMultiplier *= 1 - resistance;
	}

	UnitId Battle::PapyrsTarget(UnitId _source) const
	{
		UnitId best = 0;
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (id == _source || !ally.MyAlive || ally.MyHidden || ally.MyKind == UnitKind::DEVICE || ally.MyStatuses.Has(CombatStatus::ISOLATED) ||
				ally.MyStatuses.Has(CombatStatus::NO_HEAL) || ally.MyDefinition.MyAttack.MyNoHeal || !InRuleRange(_source, id)) continue;
			if (!best || std::isgreater(ally.MyStats.MyMaxHealth, Unit(best).MyStats.MyMaxHealth) ||
				(!std::islessgreater(ally.MyStats.MyMaxHealth, Unit(best).MyStats.MyMaxHealth) && ally.MyDeploySequence < Unit(best).MyDeploySequence)) best = id;
		}
		return best;
	}

	void Battle::PapyrsSkill(UnitId _unit, const PapyrsKit&, ContentEventKind _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event == ContentEventKind::SKILL_START || _event == ContentEventKind::SKILL_ENDING)
		{
			if (unit.MyPapyrsLock) --_MyOperatorBeforeAttackHandlers;
			unit.MyPapyrsLock = 0;
			unit.MyPapyrsAbort = false;
			if (_event == ContentEventKind::SKILL_START)
			{
				unit.MyPapyrsLock = PapyrsTarget(_unit);
				if (unit.MyPapyrsLock) ++_MyOperatorBeforeAttackHandlers;
				else unit.MyPapyrsAbort = true;
			}
		}
		else if (_event == ContentEventKind::SKILL_TICK)
		{
			if (unit.MyPapyrsAbort)
			{
				unit.MyPapyrsAbort = false;
				EndSkill(_unit, SkillReason::NO_TARGET);
				AddSkillCharge(_unit);
			}
			else if (!unit.MyPapyrsLock || !Unit(unit.MyPapyrsLock).MyAlive) EndSkill(_unit, SkillReason::TARGET);
		}
	}

	void Battle::NotifyOperatorHealing(ContentEvent& _event, bool _last)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_HEAL || !_event.MyTarget || !std::isgreater(_event.MyAmount, 0)) return;
		if (!_last && _event.MySource)
		{
			const auto& source = Unit(_event.MySource);
			const auto* kit = source.MyDefinition.MyOperatorKit ? std::get_if<PapyrsKit>(source.MyDefinition.MyOperatorKit) : nullptr;
			if (kit && !source.MyOperatorHooksReleased && std::isgreater(kit->MyShieldScale, 0) && !(_event.MyTarget == source.MyId && _event.MyHealOptions.MyRegen))
			{
				const auto scale = source.MySkill.MyActive && source.MySkill.MyPending ? kit->MySkillShieldScale : 1;
				(void)AddBuff(_event.MyTarget, {.MyKey = "papyrs:shield", .MySource = source.MyId, .MyDuration = kit->MyShieldDuration,
					.MyShield = {.MyHealth = source.MyStats.MyAttack * kit->MyShieldScale * scale}});
			}
		}
		if (!_last) return;
		const auto& target = Unit(_event.MyTarget);
		const auto* kit = target.MyDefinition.MyOperatorKit ? std::get_if<HumusKit>(target.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || target.MyOperatorHooksReleased || !std::isgreater(kit->MyOverhealCap, 0)) return;
		const auto over = _event.MyAmount - (target.MyStats.MyMaxHealth - target.MyHealth);
		if (!std::isgreater(over, 0)) return;
		const auto found = std::ranges::find(target.MyBuffs, "humus:recycle", [](const auto& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
		const auto have = found == target.MyBuffs.end() ? 0 : found->MyDefinition.MyShield.MyHealth;
		const auto value = std::min(target.MyStats.MyMaxHealth * kit->MyOverhealCap, have + over);
		if (std::isgreater(value, have + 1e-6)) (void)AddBuff(target.MyId, {.MyKey = "humus:recycle", .MySource = target.MyId, .MyShield = {.MyHealth = value}});
	}
}
