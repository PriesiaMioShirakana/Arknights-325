#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct WaitingScratchGuard
		{
			std::size_t& MyDepth;

			~WaitingScratchGuard() { --MyDepth; }
		};

		bool PreferredWaiting(const CombatUnit& _unit)
		{
			const auto profession = _unit.MyDefinition.MyOperatorProfession;
			return profession == OperatorProfession::WARRIOR || profession == OperatorProfession::CASTER || profession == OperatorProfession::SNIPER;
		}
	}

	void BattleCore::SkillEnemiesIgnoringStealth(UnitId _unit, std::span<const RangeOffset> _range, std::vector<UnitId>& _targets)
	{
		_targets.clear(); _targets.reserve(_MyEnemyIds.size());
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (enemy.MyAlive && !enemy.MyHidden && !enemy.MyStatuses.Has(CombatStatus::UNTARGETABLE) && OperatorInGrid(_unit, id, _range)) _targets.push_back(id);
		}
		SortOperatorTargets(_unit, _targets, static_cast<unsigned>(_targets.size()));
	}

	void BattleCore::TargetsOnLine(CombatUnit& _unit, std::vector<UnitId>& _targets)
	{
		if (_targets.empty()) return;
		const auto main = _targets.front(); const auto point = Unit(main).MyPosition;
		const auto forward = RotateOffset({.MyColumn = 1}, _unit.MyFacing);
		const auto line = static_cast<int>(std::floor((forward.MyColumn ? point.MyY : point.MyX) + 0.5));
		auto& scratch = AcquireAttackScratch(); const WaitingScratchGuard guard{.MyDepth = _MyAttackDepth};
		scratch.MyTargets.push_back(main);
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (id == main || !TargetableEnemy(enemy, _unit.MyDefinition.MyAttack) || !InRuleRange(_unit.MyId, id)) continue;
			for (int n = 0, end = forward.MyColumn ? FieldColumns : FieldRows; n < end; ++n)
				if (BodyOnTile(enemy, forward.MyColumn ? line : n, forward.MyColumn ? n : line)) { scratch.MyTargets.push_back(id); break; }
		}
		if (scratch.MyTargets.size() > 1) _targets.assign(scratch.MyTargets.begin(), scratch.MyTargets.end());
	}

	void BattleCore::Svash2Slash(UnitId _unit, const Svash2Kit& _kit)
	{
		const auto attack = Unit(_unit).MyStats.MyAttack;
		auto& scratch = AcquireAttackScratch(); const WaitingScratchGuard guard{.MyDepth = _MyAttackDepth};
		SkillEnemiesIgnoringStealth(_unit, _kit.MyRange, scratch.MyTargets);
		if (scratch.MyTargets.size() > _kit.MyTargets) scratch.MyTargets.resize(_kit.MyTargets);
		for (const auto id : scratch.MyTargets)
		{
			(void)DealDamage(_unit, id, {.MyAmount = attack * _kit.MyScale, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
			if (!Unit(id).MyAlive) continue;
			(void)ApplyStatus(id, CombatStatus::COLD, _kit.MyCold, _unit);
			(void)ApplyStatus(id, CombatStatus::REVEAL, _kit.MyCold, _unit);
		}
	}

	void BattleCore::Svash2Snow(UnitId _unit)
	{
		const auto& unit = Unit(_unit); if (!unit.MyAlive || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<Svash2Kit>(*unit.MyDefinition.MyOperatorKit);
		const auto scale = std::isgreaterequal(Time() - unit.MyDeployedAt, kit.MyTalentDelay - 1e-9) ? 2 : 1;
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id); const auto& identity = ally.MyDefinition.MyIdentity;
			if (!ally.MyAlive || ally.MyKind != UnitKind::OPERATOR || (identity.MyNationId != "kjerag" && !std::ranges::contains(identity.MyBonds, "kjeragShip"))) continue;
			(void)AddBuff(id, {.MyKey = "svash2:snow", .MyDuration = 0.5, .MyModifiers = std::vector<AttributeChange>{
				{.MyAttribute = Attribute::DEFENSE_FLAT, .MyValue = kit.MyDefense * scale}, {.MyAttribute = Attribute::HEALTH_REGEN_RATIO, .MyValue = kit.MyRegen * scale}}});
		}
	}

	void BattleCore::Svash2Observe(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BATTLE_START && _event.MyKind != ContentEventKind::DEPLOY && _event.MyKind != ContentEventKind::DEATH && _event.MyKind != ContentEventKind::BEFORE_STATUS) return;
		for (std::size_t i = 0, count = _MySvash2s.size(); i < count; ++i)
		{
			const auto& unit = Unit(_MySvash2s[i]); if (unit.MyOperatorHooksReleased) continue;
			const auto& kit = std::get<Svash2Kit>(*unit.MyDefinition.MyOperatorKit);
			const auto left = [&](const CombatUnit& _ally)
			{ return _ally.MyKind == UnitKind::OPERATOR && _ally.MyOwner == unit.MyOwner && (_ally.MyId == unit.MyId || std::isless(_ally.MyDefinition.MyStats.MyDeploymentCost, unit.MyEyeCost)); };
			if (_event.MyKind == ContentEventKind::BEFORE_STATUS)
			{
				if (_event.MyStatus == CombatStatus::FREEZE && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ALLY &&
					std::ranges::any_of(Unit(_event.MyTarget).MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "svash2:snow"; })) _event.MyCancel = true;
				continue;
			}
			if (_event.MyKind == ContentEventKind::BATTLE_START)
			{
				if (unit.MyAlive && std::isgreater(kit.MyDeploySp, 0))
					for (std::size_t j = 0, n = _MyAllyIds.size(); j < n; ++j)
						if (Unit(_MyAllyIds[j]).MyAlive && left(Unit(_MyAllyIds[j]))) (void)GainSp(_MyAllyIds[j], kit.MyDeploySp, SpReason::TALENT);
				continue;
			}
			if (!_event.MyUnit) continue;
			auto& ally = _MyUnits[Index(_event.MyUnit)];
			if (_event.MyKind == ContentEventKind::DEATH)
			{
				if (left(ally) && !ally.MyRemoved && (ally.MyId == unit.MyId || unit.MyAlive) && std::isfinite(ally.MyRespawnAt) &&
					std::isgreater(kit.MyRedeployMultiplier, 0) && std::isless(kit.MyRedeployMultiplier, 1))
					ally.MyRespawnAt = ally.MyRemovedAt + (ally.MyRespawnAt - ally.MyRemovedAt) * kit.MyRedeployMultiplier;
				continue;
			}
			if (!_event.MyInitial && left(ally) && (ally.MyId == unit.MyId || unit.MyAlive) && std::isgreater(kit.MyDeploySp, 0)) (void)GainSp(ally.MyId, kit.MyDeploySp, SpReason::TALENT);
			if (ally.MyKind != UnitKind::OPERATOR || ally.MyOwner != unit.MyOwner) continue;
			if (ally.MySvashCostBase) { ally.MyDefinition.MyStats.MyDeploymentCost = *ally.MySvashCostBase; ally.MySvashCostBase.reset(); }
			if (_event.MyInitial) continue;
			if (std::isgreater(ally.MySvashShield, 0))
			{ (void)AddBuff(ally.MyId, {.MyKey = "svash2:barrier", .MyShield = {.MyHealth = ally.MySvashShield}}); ally.MySvashShield = 0; }
			const auto casts = ally.MySvashCasts; ally.MySvashCasts = 0;
			for (unsigned n = 0; n < casts; ++n) Svash2Slash(ally.MyId, kit);
		}
	}

	void BattleCore::Svash2Skill(UnitId _unit, const Svash2Kit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::SKILL_TICK && _kit.MySkill == Svash2SkillKind::CHANGE)
		{
			unit.MySvashDpAccumulator += _event.MyDelta;
			while (std::isgreaterequal(unit.MySvashDpAccumulator, _kit.MyDpInterval - 1e-9))
			{ unit.MySvashDpAccumulator -= _kit.MyDpInterval; (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyPeriodicDp); }
			unit.MySvashRevealAccumulator += _event.MyDelta;
			if (std::isless(unit.MySvashRevealAccumulator, 0.25 - 1e-9)) return;
			unit.MySvashRevealAccumulator = 0;
			auto& scratch = AcquireAttackScratch(); const WaitingScratchGuard guard{.MyDepth = _MyAttackDepth};
			SkillEnemiesIgnoringStealth(_unit, _kit.MyRange, scratch.MyTargets);
			for (const auto id : scratch.MyTargets) (void)ApplyStatus(id, CombatStatus::REVEAL, 0.5, _unit);
		}
		if (_event.MyKind != ContentEventKind::SKILL_START) return;
		if (_kit.MySkill == Svash2SkillKind::EDGE) Svash2Slash(_unit, _kit);
		else (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyDp);
		if (_kit.MySkill == Svash2SkillKind::CHANGE)
		{ unit.MySvashDpAccumulator = 0; unit.MySvashRevealAccumulator = 0.25; if (unit.MySvashSwapped) return; unit.MySvashSwapped = true; }
		auto& scratch = AcquireAttackScratch(); const WaitingScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (id != _unit && ally.MyKind == UnitKind::OPERATOR && ally.MyOwner == unit.MyOwner && !ally.MyAlive && !ally.MyRemoved) scratch.MyTargets.push_back(id);
		}
		if (_kit.MySkill == Svash2SkillKind::CHANGE)
		{
			const auto cost = [&](UnitId _id) { const auto& ally = Unit(_id); return ally.MySvashCostBase.value_or(ally.MyDefinition.MyStats.MyDeploymentCost); };
			std::ranges::sort(scratch.MyTargets, [&](UnitId _a, UnitId _b) { return std::islessgreater(cost(_a), cost(_b)) ? std::isgreater(cost(_a), cost(_b)) : _a < _b; });
			if (scratch.MyTargets.size() < 2) return;
			const auto preferred = std::ranges::find_if(scratch.MyTargets, [&](UnitId _id) { return PreferredWaiting(Unit(_id)); });
			const auto hi = preferred == scratch.MyTargets.end() ? scratch.MyTargets.front() : *preferred;
			const auto lo = scratch.MyTargets.back() == hi ? scratch.MyTargets[scratch.MyTargets.size() - 2] : scratch.MyTargets.back();
			const auto ch = cost(hi), cl = cost(lo); if (!std::islessgreater(ch, cl)) return;
			const auto assign = [&](UnitId _id, double _cost)
			{
				auto& ally = _MyUnits[Index(_id)]; auto& current = ally.MyDefinition.MyStats.MyDeploymentCost;
				if (ally.MySvashCostBase) { current = std::max(0.0, _cost - (*ally.MySvashCostBase - current)); ally.MySvashCostBase = _cost; }
				else current = _cost;
			};
			assign(hi, cl); assign(lo, ch); return;
		}
		UnitId pick = 0; int bestRank = 3; double bestCost = 0;
		for (const auto id : scratch.MyTargets)
		{
			auto& ally = _MyUnits[Index(id)]; const auto cost = ally.MyDefinition.MyStats.MyDeploymentCost;
			const bool right = std::isgreaterequal(cost, unit.MyEyeCost); const int rank = right ? (PreferredWaiting(ally) ? 0 : 1) : 2;
			if (rank < bestRank || (rank == bestRank && (right ? std::isless(cost, bestCost) : std::isgreater(cost, bestCost)))) { pick = id; bestRank = rank; bestCost = cost; }
			if (!right && _kit.MySkill == Svash2SkillKind::EDGE) ally.MySvashCasts = std::min(_kit.MyMaxCasts, ally.MySvashCasts + 1);
		}
		if (!pick) return;
		auto& ally = _MyUnits[Index(pick)]; auto& cost = ally.MyDefinition.MyStats.MyDeploymentCost;
		if (std::isgreater(_kit.MyCostCut, 0)) { if (!ally.MySvashCostBase) ally.MySvashCostBase = cost; cost = std::max(0.0, cost - _kit.MyCostCut); }
		if (_kit.MySkill == Svash2SkillKind::PLAN) ally.MySvashShield = unit.MyStats.MyMaxHealth * _kit.MyShieldRatio;
	}
}
