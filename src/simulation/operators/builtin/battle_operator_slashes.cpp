#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct SlashScratchGuard
		{
			std::size_t& MyDepth;

			~SlashScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::BlkkgtStatus(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_STATUS || !_event.MyTarget || (_event.MyStatus != CombatStatus::STUN && _event.MyStatus != CombatStatus::FREEZE)) return;
		const auto& unit = Unit(_event.MyTarget);
		if (unit.MyBlkkgtSlashing && !unit.MyOperatorHooksReleased) _event.MyCancel = true;
	}

	void BattleCore::BlkkgtTick(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto& kit = std::get<BlkkgtKit>(*unit.MyDefinition.MyOperatorKit);
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased || !std::isgreater(kit.MyFirstTremble, 0)) return;
		auto& scratch = AcquireAttackScratch(); const SlashScratchGuard guard{.MyDepth = _MyAttackDepth};
		const AttackProfile filter{.MyCanHitFlying = true};
		for (const auto id : _MyEnemyIds)
			if (TargetableEnemy(Unit(id), filter) && !Unit(id).Flying() && InRuleRange(_unit, id) && !std::ranges::contains(unit.MyBlkkgtSeen, id)) scratch.MyTargets.push_back(id);
		for (const auto id : scratch.MyTargets) { unit.MyBlkkgtSeen.push_back(id); (void)ApplyStatus(id, CombatStatus::TREMBLE, kit.MyFirstTremble, _unit); }
	}

	void BattleCore::BlkkgtSlash(UnitId _unit, const BlkkgtKit& _kit, double _scale)
	{
		const auto& unit = Unit(_unit);
		auto& scratch = AcquireAttackScratch(); const SlashScratchGuard guard{.MyDepth = _MyAttackDepth};
		auto filter = unit.MyDefinition.MyAttack; filter.MyCanHitFlying = true; filter.MyGroundOnly = false;
		for (const auto id : _MyEnemyIds) if (TargetableEnemy(Unit(id), filter) && InRuleRange(_unit, id)) scratch.MyTargets.push_back(id);
		SortOperatorTargets(_unit, scratch.MyTargets, _kit.MyTargets);
		for (const auto id : scratch.MyTargets) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _scale, .MyType = DamageType::PHYSICAL,
			.MyTags = DamageTag::SKILL | DamageTag::SLASH, .MyIsSkill = true});
	}

	void BattleCore::BlkkgtPull(UnitId _unit, const BlkkgtKit& _kit, bool _final)
	{
		const auto& unit = Unit(_unit); const auto point = RulePosition(unit); const auto forward = RotateOffset({.MyColumn = 1}, unit.MyFacing);
		auto& scratch = AcquireAttackScratch(); const SlashScratchGuard guard{.MyDepth = _MyAttackDepth};
		const AttackProfile filter{.MyCanHitFlying = true};
		for (const auto id : _MyEnemyIds) if (TargetableEnemy(Unit(id), filter) && InRuleRange(_unit, id)) scratch.MyTargets.push_back(id);
		const WorldPoint to{.MyX = point.MyX + (_final ? 0 : 0.5 * forward.MyColumn), .MyY = point.MyY + (_final ? 0 : 0.5 * forward.MyRow)};
		for (const auto id : scratch.MyTargets) (void)Pull(id, _final ? _kit.MyFinalForce : _kit.MyPullForce,
			{.MyTo = to, .MyCenter = point, .MyCenterUnit = UsesInitialPosition(unit) ? 0 : _unit});
	}

	void BattleCore::BlkkgtSkill(UnitId _unit, const BlkkgtKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
		{
			if (std::isgreater(_kit.MyPenetration, 0) && Unit(_event.MyTarget).MyStatuses.Has(CombatStatus::TREMBLE)) _event.MyDamage.MyDefenseIgnorePercent += _kit.MyPenetration;
			const auto chance = unit.MySkill.MyActive || unit.MyBlkkgtFinale ? _kit.MySkillChance : _kit.MyChance;
			if (std::isgreater(chance, 0) && (std::isgreaterequal(chance, 1) || std::isless(_MyRandom.Next(), chance)))
			{
				_event.MyDamage.MyAmount *= _kit.MyCriticalScale;
				if (std::isgreater(_kit.MyTremble, 0) && Unit(_event.MyTarget).MyAlive) (void)ApplyStatus(_event.MyTarget, CombatStatus::TREMBLE, _kit.MyTremble, _unit);
			}
			if (_event.MyDamage.MyIsSkill && std::isgreater(_kit.MySkillScale, 1)) _event.MyDamage.MyMultiplier *= _kit.MySkillScale;
		}
		if (_kit.MySkill == BlkkgtSkillKind::LAUGH && _event.MyKind == ContentEventKind::SKILL_START)
		{
			auto& scratch = AcquireAttackScratch(); const SlashScratchGuard guard{.MyDepth = _MyAttackDepth};
			OperatorEnemiesInGrid(_unit, _kit.MyRange, scratch.MyTargets, false);
			std::erase_if(scratch.MyTargets, [&](UnitId _id) { return Unit(_id).Flying() || !TargetableEnemy(Unit(_id), unit.MyDefinition.MyAttack); });
			SortOperatorTargets(_unit, scratch.MyTargets, _kit.MyTargets);
			for (const auto id : scratch.MyTargets)
				for (unsigned i = 0, count = Unit(id).MyBlockedBy ? _kit.MyBlockedHits : _kit.MyFreeHits; i < count && Unit(id).MyAlive; ++i)
					(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyScale, .MyType = DamageType::PHYSICAL, .MyTags = DamageTag::SKILL | DamageTag::SLASH, .MyIsSkill = true});
		}
		if (_kit.MySkill != BlkkgtSkillKind::SILENCE) return;
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			unit.MyBlkkgtSlashing = true; unit.MyBlkkgtSlashes = 1; unit.MyBlkkgtAccumulator = unit.MyBlkkgtPullAccumulator = 0;
			StatusFlags flags; flags.set(static_cast<std::size_t>(CombatStatus::INVULNERABLE));
			(void)AddBuff(_unit, {.MyKey = "blkkgt:slashes", .MySource = _unit, .MyDuration = _kit.MySlashes * _kit.MyInterval + 1, .MyFlags = flags});
			BlkkgtSlash(_unit, _kit, _kit.MyScale);
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK && unit.MyBlkkgtSlashing)
		{
			unit.MyBlkkgtAccumulator += _event.MyDelta; unit.MyBlkkgtPullAccumulator += _event.MyDelta;
			while (std::isgreaterequal(unit.MyBlkkgtAccumulator + 1e-9, _kit.MyInterval) && unit.MyBlkkgtSlashes < _kit.MySlashes)
			{ unit.MyBlkkgtAccumulator -= _kit.MyInterval; ++unit.MyBlkkgtSlashes; BlkkgtSlash(_unit, _kit, _kit.MyScale); }
			if (std::isgreaterequal(unit.MyBlkkgtPullAccumulator + 1e-9, _kit.MyPullInterval)) { unit.MyBlkkgtPullAccumulator -= _kit.MyPullInterval; BlkkgtPull(_unit, _kit, false); }
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			unit.MyBlkkgtSlashing = false; (void)RemoveBuff(_unit, "blkkgt:slashes");
			if (_event.MySkillReason == SkillReason::DEATH || !unit.MyAlive) return;
			{
				unit.MyBlkkgtFinale = true;
				struct FinaleGuard { bool& MyActive; ~FinaleGuard() { MyActive = false; } } guard{.MyActive = unit.MyBlkkgtFinale};
				BlkkgtSlash(_unit, _kit, _kit.MyFinalScale);
			}
			BlkkgtPull(_unit, _kit, true);
		}
	}
}
