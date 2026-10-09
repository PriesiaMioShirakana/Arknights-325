#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		struct RecoveryScratchGuard
		{
			std::size_t& MyDepth;

			~RecoveryScratchGuard() { --MyDepth; }
		};
	}

	void Battle::LisaAura(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<LisaKit>(*unit.MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch(); const RecoveryScratchGuard guard{.MyDepth = _MyAttackDepth};
		GenericEnemies(_unit, scratch.MyTargets);
		if (!scratch.MyTargets.empty() && std::isgreater(kit.MyModuleSp, 0)) (void)AddBuff(_unit, {.MyKey = "lisa:module", .MyDuration = 0.5,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::SP_RECOVERY_FLAT, .MyValue = kit.MyModuleSp}}});
		if (!std::isgreater(kit.MyFragile, 0)) return;
		const auto value = kit.MyFragile * (unit.MySkill.MyActive ? kit.MyBoost : 1);
		for (const auto id : scratch.MyTargets)
			if (Unit(id).MyAlive && Unit(id).MyStatuses.Has(CombatStatus::SLUGGISH)) (void)ApplyStatus(id, CombatStatus::FRAGILE, {.MyDuration = 0.5, .MySource = _unit, .MyValue = value});
	}

	void Battle::LisaSkill(UnitId _unit, const LisaKit& _kit, ContentEvent& _event)
	{
		if (!_kit.MyFox) return;
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::SKILL_START) { unit.MyAreaAuraAccumulator = 0.25; unit.MyAreaHealAccumulator = 0; }
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			for (std::size_t i = 0, count = _MyAllyIds.size(); i < count; ++i) (void)RemoveBuff(_MyAllyIds[i], unit.MyFoxKey);
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK)
		{
			auto& scratch = AcquireAttackScratch(); const RecoveryScratchGuard guard{.MyDepth = _MyAttackDepth};
			unit.MyAreaAuraAccumulator += _event.MyDelta;
			if (std::isgreaterequal(unit.MyAreaAuraAccumulator, 0.25))
			{
				unit.MyAreaAuraAccumulator = 0; GenericEnemies(_unit, scratch.MyTargets);
				for (const auto id : scratch.MyTargets) (void)ApplyStatus(id, CombatStatus::SLUGGISH, 0.5, _unit);
			}
			unit.MyAreaHealAccumulator += _event.MyDelta;
			if (std::isless(unit.MyAreaHealAccumulator, 1)) return;
			unit.MyAreaHealAccumulator -= 1;
			if (!std::isgreater(_kit.MyHealRatio, 0)) return;
			OperatorAlliesInRange(_unit, scratch.MyTargets);
			const auto amount = unit.MyStats.MyAttack * _kit.MyHealRatio;
			for (const auto id : scratch.MyTargets) (void)AddBuff(id, {.MyKey = unit.MyFoxKey, .MySource = _unit, .MyDuration = 1.25,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALTH_REGEN, .MyValue = amount}}});
		}
	}

	void Battle::DemkniTargets(UnitId _unit, const DemkniKit& _kit, std::vector<UnitId>& _targets) const
	{
		_targets.clear(); _targets.reserve(_MyAllyIds.size());
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (!ally.MyAlive || ally.MyKind == UnitKind::DEVICE || (_kit.MySkill == DemkniSkillKind::CALCIFY && ally.MyHidden) || !OperatorInGrid(_unit, id, _kit.MyRange) ||
				(id != _unit && (ally.MyStatuses.Has(CombatStatus::NO_HEAL) || ally.MyDefinition.MyAttack.MyNoHeal))) continue;
			_targets.push_back(id);
		}
	}

	void Battle::DemkniAuto(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (unit.MyOperatorHooksReleased || !CanAutoSkill(unit) || std::isless(Time() - unit.MySkill.MyLastStart, unit.MyStats.AttackInterval() - 1e-9)) return;
		auto& scratch = AcquireAttackScratch(); const RecoveryScratchGuard guard{.MyDepth = _MyAttackDepth};
		DemkniTargets(_unit, std::get<DemkniKit>(*unit.MyDefinition.MyOperatorKit), scratch.MyTargets);
		if (std::ranges::any_of(scratch.MyTargets, [&](UnitId _id) { return std::isless(Unit(_id).MyHealth, Unit(_id).MyStats.MyMaxHealth - 1e-6); }))
			(void)ActivateSkill(_unit, false, SkillReason::TRIGGER);
	}

	void Battle::DemkniSuit(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<DemkniKit>(*unit.MyDefinition.MyOperatorKit);
		const auto stacks = static_cast<unsigned>(std::clamp(std::floor((Time() - unit.MyDeployedAt + 1e-6) / kit.MyGrowthInterval), 0.0, static_cast<double>(kit.MyMaxStacks)));
		const auto found = std::ranges::find(unit.MyBuffs, "saria:suit", [](const CombatBuff& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
		if (stacks && (found == unit.MyBuffs.end() || found->MyDefinition.MyStacks != stacks))
			(void)AddBuff(_unit, {.MyKey = "saria:suit", .MyStacks = stacks, .MyMaxStacks = kit.MyMaxStacks, .MyModifiers = std::vector<AttributeChange>(kit.MyGrowth.begin(), kit.MyGrowth.end())});
	}

	void Battle::DemkniSkill(UnitId _unit, const DemkniKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::BEFORE_HEAL)
		{
			LowHealthHealBonus(_event, _kit.MyLowHealthThreshold, std::isgreater(_kit.MyLowHealScale, 1) ? _kit.MyLowHealScale : 0, false, true, true);
			if (_event.MyTarget && !_event.MyHealOptions.MyRegen && std::isgreater(_event.MyAmount, 0) && std::isgreater(_kit.MyHealSp, 0)) (void)GainSp(_event.MyTarget, _kit.MyHealSp);
		}
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			unit.MyAreaHealAccumulator = 0; unit.MyAreaAuraAccumulator = 0.25;
			if (_kit.MySkill == DemkniSkillKind::MEDICINE)
			{
				auto& scratch = AcquireAttackScratch(); const RecoveryScratchGuard guard{.MyDepth = _MyAttackDepth};
				DemkniTargets(_unit, _kit, scratch.MyTargets);
				const auto amount = unit.MyStats.MyAttack * _kit.MyHealScale;
				for (const auto id : scratch.MyTargets) if (std::isless(Unit(id).MyHealth, Unit(id).MyStats.MyMaxHealth - 1e-6)) (void)Heal(_unit, id, amount);
			}
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK && _kit.MySkill == DemkniSkillKind::CALCIFY)
		{
			auto& scratch = AcquireAttackScratch(); const RecoveryScratchGuard guard{.MyDepth = _MyAttackDepth};
			unit.MyAreaAuraAccumulator += _event.MyDelta;
			if (std::isgreaterequal(unit.MyAreaAuraAccumulator, 0.25 - 1e-9))
			{
				unit.MyAreaAuraAccumulator = 0;
				for (const auto id : _MyEnemyIds)
					if (Unit(id).MyAlive && !Unit(id).MyHidden && OperatorInGrid(_unit, id, _kit.MyRange)) scratch.MyTargets.push_back(id);
				for (const auto id : scratch.MyTargets) (void)AddBuff(id, {.MyKey = "saria:calcify", .MyDuration = 0.5, .MyModifiers = std::vector<AttributeChange>(_kit.MyCalcify.begin(), _kit.MyCalcify.end())});
			}
			unit.MyAreaHealAccumulator += _event.MyDelta;
			if (std::isless(unit.MyAreaHealAccumulator, 1 - 1e-9)) return;
			unit.MyAreaHealAccumulator -= 1;
			const auto amount = unit.MyStats.MyAttack * _kit.MyHealScale;
			if (!std::isgreater(amount, 0)) return;
			DemkniTargets(_unit, _kit, scratch.MyTargets);
			for (const auto id : scratch.MyTargets) if (std::isless(Unit(id).MyHealth, Unit(id).MyStats.MyMaxHealth - 1e-6)) (void)Heal(_unit, id, amount);
		}
	}
}
