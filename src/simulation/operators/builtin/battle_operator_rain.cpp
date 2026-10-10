#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct RainScratchGuard
		{
			std::size_t& MyDepth;

			~RainScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::HealingTargets(UnitId _unit, std::vector<UnitId>& _targets, bool _abnormal) const
	{
		_targets.clear();
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (!ally.MyAlive || ally.MyHidden || ally.MyKind == UnitKind::DEVICE || !InRuleRange(_unit, id)) continue;
			if (_abnormal ? (HasAbnormal(id) && (id == _unit || !ally.MyStatuses.Has(CombatStatus::ISOLATED))) :
				(std::isless(ally.MyHealth, ally.MyStats.MyMaxHealth - 1e-6) && (id == _unit || (!ally.MyStatuses.Has(CombatStatus::NO_HEAL) && !ally.MyDefinition.MyAttack.MyNoHeal)))) _targets.push_back(id);
		}
		std::ranges::sort(_targets, [&](UnitId _left, UnitId _right)
		{
			const auto& left = Unit(_left); const auto& right = Unit(_right);
			return std::tuple{left.MyHealth / left.MyStats.MyMaxHealth, left.MyDeploySequence} < std::tuple{right.MyHealth / right.MyStats.MyMaxHealth, right.MyDeploySequence};
		});
	}

	void BattleCore::LumenBeforeAttack(CombatUnit& _unit, const LumenKit& _kit, std::vector<UnitId>& _targets)
	{
		if (!_kit.MyDefault || _kit.MySkill != LumenSkillKind::LIGHT || !_unit.MySkill.MyActive) return;
		auto& scratch = AcquireAttackScratch(); const RainScratchGuard guard{.MyDepth = _MyAttackDepth};
		HealingTargets(_unit.MyId, scratch.MyTargets, true);
		_unit.MyLumenAbnormal = !scratch.MyTargets.empty();
		if (_unit.MyLumenAbnormal) _targets.assign(1, scratch.MyTargets.front());
		if (_unit.MyDefinition.MySkill.MyAttack) _unit.MyDefinition.MySkill.MyAttack->MyHealScale = _unit.MyLumenAbnormal ? _kit.MyHealScale : 1;
	}

	void BattleCore::LumenTick(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto& kit = std::get<LumenKit>(*unit.MyDefinition.MyOperatorKit);
		if (!kit.MyDefault || kit.MySkill != LumenSkillKind::LIGHT || !unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased ||
			!unit.MySkill.MyActive || unit.MyStatuses.Has(CombatStatus::STUN) || unit.MyStatuses.Has(CombatStatus::DISARM) || std::isgreater(unit.MyAttackCooldown, 0)) return;
		auto& scratch = AcquireAttackScratch(); const RainScratchGuard guard{.MyDepth = _MyAttackDepth};
		HealingTargets(_unit, scratch.MyTargets, false);
		if (!scratch.MyTargets.empty()) return;
		HealingTargets(_unit, scratch.MyTargets, true);
		if (scratch.MyTargets.empty()) return;
		(void)ForceAttack(_unit, std::span<const UnitId>{scratch.MyTargets.data(), 1});
		unit.MyAttackCooldown = unit.MyStats.AttackInterval();
	}

	void BattleCore::LumenHealHit(UnitId _unit, UnitId _target, const LumenKit& _kit)
	{
		const auto& unit = Unit(_unit);
		if (_kit.MySkill == LumenSkillKind::LIGHT) { if (unit.MyLumenAbnormal) (void)CleanseAbnormal(_target); return; }
		if (_kit.MySkill != LumenSkillKind::RAIN || !std::isgreater(_kit.MyRainScale, 0) || !std::isgreater(_kit.MyRainDuration, 0)) return;
		const auto& target = Unit(_target); const auto point = RulePosition(target);
		auto& scratch = AcquireAttackScratch(); const RainScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id); const auto position = RulePosition(ally);
			if (ally.MyOwner == target.MyOwner && ally.MyAlive && !ally.MyHidden && ally.MyKind != UnitKind::DEVICE &&
				!ally.MyStatuses.Has(CombatStatus::NO_HEAL) && !ally.MyDefinition.MyAttack.MyNoHeal &&
				std::islessequal(std::hypot(position.MyX - point.MyX, position.MyY - point.MyY), 1.5 + 1e-9)) scratch.MyTargets.push_back(id);
		}
		for (const auto id : scratch.MyTargets) (void)AddBuff(id, {.MyKey = unit.MyLumenRainKey, .MySource = _unit,
			.MyDuration = _kit.MyRainDuration, .MyInterval = _kit.MyRainInterval, .MyNotifyTick = true});
	}

	void BattleCore::LumenSkill(UnitId _unit, const LumenKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_kit.MySkill == LumenSkillKind::LIGHT && _event.MyKind == ContentEventKind::ATTACK && unit.MySkill.MyActive)
		{ if (!unit.MyLumenAbnormal) _event.MyNoAmmo = true; unit.MyLumenAbnormal = false; }
		if (_kit.MySkill != LumenSkillKind::SHOWER || _event.MyKind != ContentEventKind::SKILL_START) return;
		const bool full = unit.MyDefinition.MySkill.MyMaxCharges > 1 && unit.MySkill.MyCharges >= unit.MyDefinition.MySkill.MyMaxCharges - 1;
		auto& scratch = AcquireAttackScratch(); const RainScratchGuard guard{.MyDepth = _MyAttackDepth};
		HealingTargets(_unit, scratch.MyTargets, false);
		if (full)
		{
			HealingTargets(_unit, scratch.MySeen, true);
			for (const auto id : scratch.MySeen) if (!std::ranges::contains(scratch.MyTargets, id)) scratch.MyTargets.push_back(id);
		}
		if (scratch.MyTargets.size() > _kit.MyTargets) scratch.MyTargets.resize(_kit.MyTargets);
		for (const auto id : scratch.MyTargets) { (void)Heal(_unit, id, unit.MyStats.MyAttack * _kit.MyHealScale, {.MySkillHeal = true}); if (full) (void)CleanseAbnormal(id); }
	}

	void BattleCore::LumenObserve(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::BUFF_TICK && _event.MyUnit && _event.MyBuff)
		{
			const auto& target = Unit(_event.MyUnit);
			const auto found = std::ranges::find(target.MyBuffs, _event.MyBuff, &CombatBuff::MyId);
			if (found == target.MyBuffs.end() || !found->MyDefinition.MyKey.starts_with("lumen:rain:") || !found->MyDefinition.MySource) return;
			const auto& source = Unit(found->MyDefinition.MySource);
			if (source.MyOperatorHooksReleased || !std::isless(target.MyHealth, target.MyStats.MyMaxHealth)) return;
			(void)Heal(source.MyId, target.MyId, source.MyStats.MyAttack * std::get<LumenKit>(*source.MyDefinition.MyOperatorKit).MyRainScale, {.MyHot = true});
		}
		if (_event.MyKind != ContentEventKind::STATUS_APPLIED || !_event.MyTarget || !AbnormalStatus(_event.MyStatus) || Unit(_event.MyTarget).MySide != UnitSide::ALLY) return;
		for (const auto id : _MyLumens)
		{
			auto& unit = _MyUnits[Index(id)]; const auto& kit = std::get<LumenKit>(*unit.MyDefinition.MyOperatorKit);
			if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased || std::isless(Time(), unit.MyLumenReady) || !std::isgreater(kit.MyEmergencyScale, 0) || !InRuleRange(id, _event.MyTarget)) continue;
			unit.MyLumenReady = Time() + kit.MyEmergencyCooldown;
			(void)Heal(id, _event.MyTarget, unit.MyStats.MyAttack * kit.MyEmergencyScale);
		}
	}

	void BattleCore::LumenHealing(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_HEAL || !_event.MySource || !_event.MyTarget || _event.MyHealOptions.MyRegen) return;
		const auto& unit = Unit(_event.MySource); const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<LumenKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || unit.MyOperatorHooksReleased || !std::isgreater(kit->MyResist, 0) || !std::isgreater(kit->MyResistDuration, 0)) return;
		const auto& target = Unit(_event.MyTarget);
		const auto after = std::min(target.MyStats.MyMaxHealth, target.MyHealth + _event.MyAmount);
		(void)ApplyStatus(target.MyId, CombatStatus::RESIST, {.MyDuration = std::isgreater(after / target.MyStats.MyMaxHealth, kit->MyHealthyThreshold) ? kit->MyHealthyDuration : kit->MyResistDuration,
			.MySource = unit.MyId, .MyValue = kit->MyResist});
	}
}
