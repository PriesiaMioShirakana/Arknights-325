#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct BombardmentScratchGuard
		{
			std::size_t& MyDepth;

			~BombardmentScratchGuard() { --MyDepth; }
		};

		bool Wanted(const CombatUnit& _unit)
		{
			return std::ranges::any_of(_unit.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "lemuen:wanted"; });
		}
	}

	void BattleCore::LemuenWanted()
	{
		double interval = std::numeric_limits<double>::infinity(); bool active = false;
		for (const auto id : _MyLemuens)
			if (Unit(id).MyAlive && !Unit(id).MyHidden && !Unit(id).MyOperatorHooksReleased)
			{ interval = std::min(interval, std::get<LemuenKit>(*Unit(id).MyDefinition.MyOperatorKit).MyWantedInterval); active = true; }
		if (!active) return;
		for (std::size_t i = 0, count = _MyEnemyIds.size(); i < count; ++i)
		{
			auto& enemy = _MyUnits[Index(_MyEnemyIds[i])];
			if (!enemy.MyAlive || enemy.MyHidden || (!enemy.MyDefinition.MyElite && !enemy.MyDefinition.MyLeader && enemy.MySpawnTag != EnemySpawnTag::BOSS) || Wanted(enemy)) continue;
			const bool inside = std::ranges::any_of(_MyAllyIds, [&](UnitId _id)
			{
				const auto& ally = Unit(_id);
				return ally.MyAlive && !ally.MyHidden && ally.MyKind == UnitKind::OPERATOR && std::ranges::contains(ally.MyDefinition.MyIdentity.MyBonds, "lateranoShip") && InRuleRange(_id, enemy.MyId);
			});
			if (!inside) { enemy.MyWantedTime = 0; continue; }
			enemy.MyWantedTime += 0.25;
			if (std::isgreaterequal(enemy.MyWantedTime, interval - 1e-9)) (void)AddBuff(enemy.MyId, {.MyKey = "lemuen:wanted"});
		}
	}

	void BattleCore::LemuenRange(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)]; if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) return;
		auto& keys = unit.MyNextExtraRangeKeys; keys.clear();
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id); if (!enemy.MyAlive || enemy.MyHidden || !Wanted(enemy)) continue;
			const auto bounds = BodyTiles(enemy);
			for (int r = bounds.MyFirstRow; r <= bounds.MyLastRow; ++r)
				for (int c = bounds.MyFirstColumn; c <= bounds.MyLastColumn; ++c)
					if (FieldGrid::InBounds(r, c)) keys.push_back(FieldGrid::Key(r, c));
		}
		SetExtraRange(_unit, keys);
	}

	void BattleCore::LemuenObserve(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MySource && _event.MyTarget && (_event.MyDamage.MyIsAttack || _event.MyDamage.MyIsSkill))
		{
			const auto& source = Unit(_event.MySource); const auto& target = Unit(_event.MyTarget);
			if (source.MySide != UnitSide::ALLY || target.MySide != UnitSide::ENEMY || !std::ranges::contains(source.MyDefinition.MyIdentity.MyBonds, "lateranoShip") || !Wanted(target)) return;
			double scale = -std::numeric_limits<double>::infinity();
			for (const auto id : _MyLemuens) if (Unit(id).MyAlive && !Unit(id).MyHidden && !Unit(id).MyOperatorHooksReleased) scale = std::max(scale, std::get<LemuenKit>(*Unit(id).MyDefinition.MyOperatorKit).MyWantedScale);
			if (std::isfinite(scale)) _event.MyDamage.MyMultiplier *= scale;
		}
		if (_event.MyKind != ContentEventKind::SKILL_START || !_event.MyUnit || Unit(_event.MyUnit).MyDefinition.MySkill.MyKind != SkillKind::AMMO) return;
		const auto& target = Unit(_event.MyUnit);
		for (const auto id : _MyLemuens)
		{
			const auto& unit = Unit(id); const auto& kit = std::get<LemuenKit>(*unit.MyDefinition.MyOperatorKit);
			if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased || std::isless(Time() - unit.MyDeployedAt, kit.MyExtraditionDelay - 1e-6)) continue;
			if (id == target.MyId) { if (std::isgreater(kit.MyOwnAmmo, 0)) AddSkillAmmo(id, kit.MyOwnAmmo); }
			else if (std::isgreater(kit.MyAllyAmmo, 0) && target.MySide == UnitSide::ALLY && target.MyKind == UnitKind::OPERATOR && std::ranges::contains(target.MyDefinition.MyIdentity.MyBonds, "lateranoShip")) AddSkillAmmo(target.MyId, kit.MyAllyAmmo);
		}
	}

	void BattleCore::UpdateBombardmentLock(BombardmentLock& _lock)
	{
		const auto& enemy = Unit(_lock.MyTarget);
		if (!_lock.MyGone && (!enemy.MyAlive || !enemy.MyHidden)) { _lock.MyPoint = enemy.MyPosition; if (!enemy.MyAlive) _lock.MyGone = true; }
	}

	void BattleCore::LemuenBlast(UnitId _unit, WorldPoint _point, double _attack)
	{
		const auto& kit = std::get<LemuenKit>(*Unit(_unit).MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch(); const BombardmentScratchGuard guard{.MyDepth = _MyAttackDepth};
		FoesInRadius(_point, kit.MyOuterRadius, scratch.MyTargets);
		for (const auto id : scratch.MyTargets)
		{
			const auto scale = std::islessequal(BodyDistance(Unit(id), _point), kit.MyInnerRadius + 1e-9) ? kit.MyInnerScale : kit.MyOuterScale;
			(void)DealDamage(_unit, id, {.MyAmount = _attack * scale, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSplash = true, .MyIsSkill = true});
		}
	}

	void BattleCore::LemuenFire(std::size_t _handle, unsigned _shot)
	{
		auto& volley = _MyLemuenBombardments[_handle]; const auto& unit = Unit(volley.MySource);
		if (!unit.MyAlive || unit.MyOperatorHooksReleased || unit.MyDeploySequence != volley.MyDeployment) { volley.MyLocks.clear(); return; }
		if (_shot >= volley.MyLocks.size() || _shot >= 33) return;
		const auto& kit = std::get<LemuenKit>(*unit.MyDefinition.MyOperatorKit);
		auto& mark = volley.MyLocks[_shot]; UpdateBombardmentLock(mark);
		const auto lateral = (2 * _MyRandom.Next() - 1) * kit.MySpread, longitudinal = (2 * _MyRandom.Next() - 1) * kit.MySpread;
		const auto forward = RotateOffset({.MyColumn = 1}, unit.MyFacing);
		const WorldPoint point{.MyX = mark.MyPoint.MyX + longitudinal * forward.MyColumn - lateral * forward.MyRow,
			.MyY = mark.MyPoint.MyY + longitudinal * forward.MyRow + lateral * forward.MyColumn};
		Schedule({.MyAt = Time() + 0.3, .MyKind = ScheduledKind::LEMUEN_BLAST, .MySource = unit.MyId, .MyPoint = point, .MyAmount = volley.MyAttack});
		if (_shot + 1 < volley.MyLocks.size() && _shot + 1 < 33)
			Schedule({.MyAt = Time() + 0.3, .MyKind = ScheduledKind::LEMUEN_FIRE, .MyHandle = _handle, .MyRemaining = _shot + 1});
		else volley.MyLocks.clear();
	}

	void BattleCore::LemuenSkill(UnitId _unit, const LemuenKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		const auto endAim = [&] { unit.MyLemuenAim = 0; (void)RemoveBuff(_unit, "lemuen:aim"); };
		if (_event.MyKind == ContentEventKind::DEPLOY)
			Schedule({.MyAt = Time() + _kit.MyExtraditionDelay, .MyKind = ScheduledKind::LEMUEN_EXTRADITION, .MySource = _unit, .MyVersion = unit.MyDeploySequence});
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			if (_kit.MySkill == LemuenSkillKind::INVITATION) endAim();
			if (_kit.MySkill == LemuenSkillKind::SALUTE) { unit.MyLemuenLocks.clear(); unit.MyLemuenLockAccumulator = _kit.MyLockInterval; }
		}
		if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			if (_kit.MySkill == LemuenSkillKind::INVITATION) endAim();
			if (_kit.MySkill == LemuenSkillKind::SALUTE)
			{
				if (_event.MySkillReason != SkillReason::DEATH && unit.MyAlive && !unit.MyLemuenLocks.empty())
				{
					const auto handle = _MyLemuenBombardments.size();
					_MyLemuenBombardments.push_back({.MySource = _unit, .MyDeployment = unit.MyDeploySequence, .MyAttack = unit.MyStats.MyAttack, .MyLocks = std::move(unit.MyLemuenLocks)});
					LemuenFire(handle, 0); unit.MyLemuenLocks.reserve(16);
				}
				unit.MyLemuenLocks.clear();
			}
		}
		if (_event.MyKind != ContentEventKind::SKILL_TICK || _kit.MySkill == LemuenSkillKind::GREETING) return;
		if (_kit.MySkill == LemuenSkillKind::INVITATION && unit.MyLemuenAim)
		{
			const auto& enemy = Unit(unit.MyLemuenAim);
			if (!enemy.MyAlive || enemy.MyHidden || !Wanted(enemy)) endAim();
		}
		if (_kit.MySkill == LemuenSkillKind::SALUTE) for (auto& mark : unit.MyLemuenLocks) UpdateBombardmentLock(mark);
		if (!unit.MyAlive || unit.MyHidden || unit.MyStatuses.Has(CombatStatus::STUN)) return;
		const auto spend = [&]
		{
			--unit.MySkill.MyAmmoLeft;
			ContentEvent ammo{.MyKind = ContentEventKind::AMMO_USED, .MyUnit = _unit, .MyAmount = unit.MySkill.MyAmmoLeft}; NotifyContent(ammo);
		};
		auto& scratch = AcquireAttackScratch(); const BombardmentScratchGuard guard{.MyDepth = _MyAttackDepth};
		const AttackProfile lowDefense{.MyCanHitFlying = true, .MyPriority = TargetPriority::LOW_DEFENSE};
		if (_kit.MySkill == LemuenSkillKind::INVITATION)
		{
			if (!unit.MyLemuenAim)
			{
				for (const auto id : _MyEnemyIds) if (Wanted(Unit(id)) && TargetableEnemy(Unit(id), lowDefense)) scratch.MyTargets.push_back(id);
				if (scratch.MyTargets.empty() || !std::isgreater(unit.MySkill.MyAmmoLeft, 0)) return;
				SortOperatorTargets(_unit, scratch.MyTargets, 1, &lowDefense);
				unit.MyLemuenAim = scratch.MyTargets.front(); unit.MyLemuenAimTime = 0;
				StatusFlags flags; flags.set(static_cast<std::size_t>(CombatStatus::DISARM));
				(void)AddBuff(_unit, {.MyKey = "lemuen:aim", .MyFlags = flags}); spend();
			}
			unit.MyLemuenAimTime += _event.MyDelta;
			const auto scale = std::min(_kit.MyAimFinal, _kit.MyAimBase + _kit.MyAimIncrement * std::min(_kit.MyAimSteps, std::floor((unit.MyLemuenAimTime + 1e-9) / _kit.MyAimInterval)));
			const auto id = unit.MyLemuenAim; if (!id) return;
			if (std::isless(unit.MyLemuenAimTime + 1e-9, _kit.MyAimDuration) && !std::isgreater(unit.MyStats.MyAttack * scale, Unit(id).MyHealth + Unit(id).MyStats.MyDefense)) return;
			endAim();
			(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * scale, .MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
		}
		else
		{
			unit.MyLemuenLockAccumulator += _event.MyDelta;
			if (std::isless(unit.MyLemuenLockAccumulator + 1e-9, _kit.MyLockInterval)) return;
			for (const auto id : _MyEnemyIds) if (TargetableEnemy(Unit(id), unit.MyDefinition.MyAttack) && InRuleRange(_unit, id)) scratch.MyTargets.push_back(id);
			if (scratch.MyTargets.empty()) { unit.MyLemuenLockAccumulator = _kit.MyLockInterval; return; }
			SortOperatorTargets(_unit, scratch.MyTargets, static_cast<unsigned>(scratch.MyTargets.size()), &lowDefense);
			const auto id = *std::ranges::min_element(scratch.MyTargets, [&](UnitId _a, UnitId _b)
			{ return std::ranges::count(unit.MyLemuenLocks, _a, &BombardmentLock::MyTarget) < std::ranges::count(unit.MyLemuenLocks, _b, &BombardmentLock::MyTarget); });
			unit.MyLemuenLockAccumulator -= _kit.MyLockInterval;
			unit.MyLemuenLocks.push_back({.MyTarget = id, .MyPoint = Unit(id).MyPosition}); spend();
		}
		if (unit.MySkill.MyActive && std::islessequal(unit.MySkill.MyAmmoLeft, 0)) EndSkill(_unit, SkillReason::AMMO);
	}
}
