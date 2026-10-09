#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		struct HealingGuardScratch
		{
			std::size_t& MyDepth;

			~HealingGuardScratch() { --MyDepth; }
		};
	}

	UnitId Battle::InjuredAllyInGrid(UnitId _unit, std::span<const RangeOffset> _range, bool _othersOnly) const
	{
		UnitId best = 0;
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if ((_othersOnly && id == _unit) || !ally.MyAlive || ally.MyHidden || ally.MyKind == UnitKind::DEVICE ||
				!std::isless(ally.MyHealth, ally.MyStats.MyMaxHealth - 1e-6) || !OperatorInGrid(_unit, id, _range) ||
				(id != _unit && (ally.MyStatuses.Has(CombatStatus::NO_HEAL) || ally.MyDefinition.MyAttack.MyNoHeal))) continue;
			const auto ratio = ally.MyHealth / ally.MyStats.MyMaxHealth;
			const auto bestRatio = best ? Unit(best).MyHealth / Unit(best).MyStats.MyMaxHealth : 1;
			if (!best || std::isless(ratio, bestRatio) || (!std::islessgreater(ratio, bestRatio) && ally.MyDeploySequence < Unit(best).MyDeploySequence)) best = id;
		}
		return best;
	}

	void Battle::BlemshHeal(UnitId _unit, const BlemshKit& _kit, bool _othersOnly)
	{
		if (!std::isgreater(_kit.MyHealScale, 0)) return;
		const auto target = InjuredAllyInGrid(_unit, _kit.MyHealRange, _othersOnly);
		if (target) (void)Heal(_unit, target, Unit(_unit).MyStats.MyAttack * _kit.MyHealScale);
	}

	void Battle::BlemshSkill(UnitId _unit, const BlemshKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY && Unit(_event.MyTarget).MyStatuses.Has(CombatStatus::SLEEP))
			_event.MyDamage.MyMultiplier *= _kit.MySleepScale;
		if (_event.MyKind == ContentEventKind::BEFORE_HEAL && _event.MyTarget && std::isless(Unit(_event.MyTarget).MyHealth / Unit(_event.MyTarget).MyStats.MyMaxHealth, _kit.MyLowHealthThreshold))
			_event.MyAmount *= _kit.MyLowHealthHealScale;
		if (_kit.MySkill == BlemshSkillKind::INCARNATE && _event.MyKind == ContentEventKind::ATTACK && unit.MySkill.MyActive) BlemshHeal(_unit, _kit, true);
		if (_kit.MySkill != BlemshSkillKind::SLEEP) return;
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			const auto point = RulePosition(unit);
			const int row = static_cast<int>(std::floor(point.MyY + 0.5)), column = static_cast<int>(std::floor(point.MyX + 0.5));
			const auto duration = std::isgreater(unit.MySkill.MyTimeLeft, 0) ? unit.MySkill.MyTimeLeft : std::max(0.1, unit.MyDefinition.MySkill.MyDuration);
			auto& scratch = AcquireAttackScratch(); const HealingGuardScratch guard{.MyDepth = _MyAttackDepth};
			for (const auto id : _MyEnemyIds)
			{
				const auto& enemy = Unit(id);
				if (enemy.MyAlive && !enemy.MyHidden && (enemy.MyBlockedBy == _unit || (!enemy.Flying() && BodyOnTile(enemy, row, column)))) scratch.MyTargets.push_back(id);
			}
			for (const auto id : scratch.MyTargets) (void)ApplyStatus(id, CombatStatus::SLEEP, duration, _unit);
			unit.MyBlemshRegenAccumulator = 0;
			if (std::isgreater(_kit.MyRegenRatio, 0)) RefreshOperatorAura(unit.MySkillAura);
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK)
		{
			unit.MyBlemshRegenAccumulator += _event.MyDelta;
			if (std::isless(unit.MyBlemshRegenAccumulator, 0.25 - 1e-9)) return;
			unit.MyBlemshRegenAccumulator = 0;
			if (std::isgreater(_kit.MyRegenRatio, 0)) RefreshOperatorAura(unit.MySkillAura);
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			const auto& key = _MyOperatorAuras[unit.MySkillAura].MyBuffKey;
			for (std::size_t i = 0, count = _MyAllyIds.size(); i < count; ++i) (void)RemoveBuff(_MyAllyIds[i], key);
		}
	}

	void Battle::BlemshAttackSp(UnitId _unit)
	{
		const auto& attacker = Unit(_unit);
		if (attacker.MyKind != UnitKind::OPERATOR || attacker.MyDefinition.MySkill.MySpType != SpType::HURT) return;
		UnitId first = 0;
		for (const auto id : _MyBlemshs)
		{
			const auto& unit = Unit(id);
			if (unit.MyAlive && unit.MyOwner == attacker.MyOwner && (!first || id < first)) first = id;
		}
		if (first && !Unit(first).MyOperatorHooksReleased)
			(void)GainSp(_unit, std::get<BlemshKit>(*Unit(first).MyDefinition.MyOperatorKit).MyAttackSp);
	}

	double Battle::PerTargetFunnel(CombatUnit& _unit, UnitId _target)
	{
		const auto& definition = _unit.MyDefinition.MyProfession;
		const auto found = std::ranges::find(_unit.MyFunnelRamps, _target, &FunnelRampEntry::MyTarget);
		if (found == _unit.MyFunnelRamps.end())
		{
			_unit.MyFunnelRamps.push_back({.MyTarget = _target, .MyTick = Tick(), .MyScale = definition.MyFunnelInitial});
			return definition.MyFunnelInitial;
		}
		if (found->MyTick != Tick())
		{
			found->MyScale = std::min(definition.MyFunnelMax, found->MyScale + definition.MyFunnelDelta);
			found->MyTick = Tick();
		}
		return found->MyScale;
	}
}
