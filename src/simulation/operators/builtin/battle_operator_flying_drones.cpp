#include "battle_core.hpp"
#include <numbers>

namespace Stronghold
{
	namespace
	{
		struct FlyingScratchGuard
		{
			std::size_t& MyDepth;

			~FlyingScratchGuard() { --MyDepth; }
		};

		bool StillEnemy(const CombatUnit& _enemy)
		{
			return _enemy.MyBlockedBy || !_enemy.MyMoving || !std::isgreater(_enemy.MyStats.MyMoveSpeed, 0) || _enemy.MyStatuses.Has(CombatStatus::STUN) ||
				_enemy.MyStatuses.Has(CombatStatus::FREEZE) || _enemy.MyStatuses.Has(CombatStatus::BIND) || _enemy.MyStatuses.Has(CombatStatus::SLEEP) || _enemy.MyStatuses.Has(CombatStatus::NO_MOVE);
		}

		double AccelerateDrone(FlyingDrone& _drone, double _acceleration, double _maximum, double _delta)
		{
			const auto next = std::min(_maximum, _drone.MySpeed + _acceleration * _delta);
			const auto distance = (_drone.MySpeed + next) * 0.5 * _delta;
			_drone.MySpeed = next;
			return distance;
		}
	}

	void BattleCore::InstallWhitw2(CombatUnit& _unit, const Whitw2Kit& _kit)
	{
		_unit.MyWolfBaseHits = _unit.MyDefinition.MyAttack.MyHits;
		_unit.MyWolfBaseCap = _unit.MyDefinition.MyProfession.MyFunnelMax;
		_unit.MyHuntLocks.reserve(_kit.MyDrones + 1);
		_unit.MyFlyingDrones.reserve(_kit.MyDrones + 1);
		_unit.MyFunnelRamps.reserve(std::max<std::size_t>(_kit.MyDrones + 1, _MyEnemyIds.size()));
		Schedule({.MyAt = Time() + 1, .MyKind = ScheduledKind::WHITW2_STAGE, .MySource = _unit.MyId, .MyInterval = 1});
		const auto found = std::ranges::find(_MySiracusaHonor, _unit.MyOwner, &SiracusaHonor::MyOwner);
		if (found == _MySiracusaHonor.end()) _MySiracusaHonor.push_back({.MyOwner = _unit.MyOwner, .MySp = _kit.MyTeamSp, .MyAttackSpeed = _kit.MyTeamSpeed});
		else
		{
			found->MySp = std::max(found->MySp, _kit.MyTeamSp);
			found->MyAttackSpeed = std::max(found->MyAttackSpeed, _kit.MyTeamSpeed);
		}
	}

	void BattleCore::Whitw2Honor(ContentEvent& _event)
	{
		if (!_event.MyUnit || (_event.MyKind != ContentEventKind::DEPLOY && _event.MyKind != ContentEventKind::SKILL_START)) return;
		auto& unit = _MyUnits[Index(_event.MyUnit)];
		if (unit.MyKind != UnitKind::OPERATOR || !std::ranges::contains(unit.MyDefinition.MyIdentity.MyBonds, "siracusaShip")) return;
		const auto found = std::ranges::find(_MySiracusaHonor, unit.MyOwner, &SiracusaHonor::MyOwner);
		if (found == _MySiracusaHonor.end()) return;
		// 队伍配置只安装一次，并保留较强数值；来源离场不会释放这项队伍效果。
		if (_event.MyKind == ContentEventKind::DEPLOY)
		{
			if (std::isgreater(found->MySp, 0) && !(_event.MyInitial && unit.MyCarry && unit.MyCarry->MySp && std::isfinite(*unit.MyCarry->MySp)))
				(void)GainSp(unit.MyId, found->MySp, SpReason::TALENT);
		}
		else if (std::islessgreater(found->MyAttackSpeed, 0) && unit.MySiracusaHonorDeployment != unit.MyDeploySequence)
		{
			unit.MySiracusaHonorDeployment = unit.MyDeploySequence;
			(void)AddBuff(unit.MyId, {.MyKey = "whitw2:honor", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = found->MyAttackSpeed}}});
		}
	}

	void BattleCore::Whitw2Stage(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<Whitw2Kit>(*unit.MyDefinition.MyOperatorKit);
		const auto stage = static_cast<unsigned>(std::clamp(std::floor((Time() - unit.MyDeployedAt + 1e-6) / kit.MyStageInterval), 0.0, 3.0));
		if (stage <= unit.MyWolfStage) return;
		unit.MyWolfStage = stage;
		if (stage >= 1) unit.MyDefinition.MyProfession.MyFunnelMax = unit.MyWolfBaseCap * kit.MyCapScale;
		if (stage >= 3) unit.MyDefinition.MyAttack.MyHits = unit.MyWolfBaseHits + 1;
	}

	void BattleCore::Whitw2Targets(CombatUnit& _unit, const Whitw2Kit& _kit, std::vector<UnitId>& _targets)
	{
		if (!_unit.MySkill.MyActive) return;
		const AttackProfile filter{.MyCanHitFlying = true};
		auto& scratch = AcquireAttackScratch(); const FlyingScratchGuard guard{.MyDepth = _MyAttackDepth};
		if (_kit.MySkill == Whitw2SkillKind::LAZY)
		{
			if (!_unit.MyLazyLock || !TargetableEnemy(Unit(_unit.MyLazyLock), filter) || !StillEnemy(Unit(_unit.MyLazyLock)))
			{
				for (const auto id : _MyEnemyIds) if (TargetableEnemy(Unit(id), filter) && StillEnemy(Unit(id))) scratch.MyTargets.push_back(id);
				_unit.MyLazyLock = scratch.MyTargets.empty() ? 0 : scratch.MyTargets[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MyTargets.size()))];
			}
			if (_unit.MyLazyLock) { _targets.assign(1, _unit.MyLazyLock); return; }
			_targets.clear();
			const auto& profile = EffectiveAttack(_unit);
			for (const auto id : _MyEnemyIds) if (TargetableEnemy(Unit(id), profile) && BodyInRange(Unit(id), _unit.MyBaseTriggerMask)) _targets.push_back(id);
			SortOperatorTargets(_unit.MyId, _targets, 1, &profile);
			return;
		}
		if (_kit.MySkill != Whitw2SkillKind::HUNT) return;
		const auto& profile = EffectiveAttack(_unit);
		for (const auto key : RuleRangeKeys(_unit))
			for (const auto id : _MyEnemyIds)
				if (TargetableEnemy(Unit(id), profile) && BodyOnTile(Unit(id), key / FieldColumns, key % FieldColumns) && !std::ranges::contains(scratch.MyTargets, id)) scratch.MyTargets.push_back(id);
		for (const auto id : _unit.MyBlocking) if (TargetableEnemy(Unit(id), profile) && !std::ranges::contains(scratch.MyTargets, id)) scratch.MyTargets.push_back(id);
		if (scratch.MyTargets.empty()) { _targets.clear(); return; }
		const auto count = _kit.MyDrones + (_unit.MyWolfStage >= 3 ? 1 : 0);
		std::erase_if(_unit.MyHuntLocks, [&](UnitId _id) { return !std::ranges::contains(scratch.MyTargets, _id); });
		if (_unit.MyHuntLocks.size() > count) _unit.MyHuntLocks.resize(count);
		while (_unit.MyHuntLocks.size() < count)
		{
			scratch.MySeen.clear();
			for (const auto id : scratch.MyTargets) if (!std::ranges::contains(_unit.MyHuntLocks, id)) scratch.MySeen.push_back(id);
			const auto& candidates = scratch.MySeen.empty() ? scratch.MyTargets : scratch.MySeen;
			_unit.MyHuntLocks.push_back(candidates[_MyRandom.Index(static_cast<std::uint32_t>(candidates.size()))]);
		}
		_targets.assign(_unit.MyHuntLocks.begin(), _unit.MyHuntLocks.end());
	}

	void BattleCore::ReleaseFlyingDrones(UnitId _unit, unsigned _count)
	{
		auto& unit = _MyUnits[Index(_unit)];
		const auto point = RulePosition(unit); const auto forward = RotateOffset({.MyColumn = 1}, unit.MyFacing);
		const auto facing = std::atan2(static_cast<double>(forward.MyRow), static_cast<double>(forward.MyColumn));
		for (unsigned i = 0; i < _count; ++i)
		{
			const auto angle = facing + 2 * std::numbers::pi * i / _count;
			unit.MyFlyingDrones.push_back({.MyPosition = point, .MyHeading = {.MyX = std::cos(angle), .MyY = std::sin(angle)}});
		}
	}

	void BattleCore::FlyDrone(UnitId _unit, std::size_t _index, double _delta)
	{
		auto& unit = _MyUnits[Index(_unit)];
		const auto& kit = std::get<Whitw2Kit>(*unit.MyDefinition.MyOperatorKit);
		const auto activation = unit.MySkill.MyActivations;
		// 状态回调可能结束技能并清空容器。小状态快照在回调前发布，之后只校验稳定 ID 与施放序号。
		auto drone = unit.MyFlyingDrones[_index];
		const auto publish = [&]()
		{
			if (!unit.MyAlive || !unit.MySkill.MyActive || unit.MySkill.MyActivations != activation || _index >= unit.MyFlyingDrones.size()) return false;
			unit.MyFlyingDrones[_index] = drone;
			return true;
		};
		const AttackProfile filter{.MyCanHitFlying = true};
		const auto valid = [&](UnitId _target) { return _target && TargetableEnemy(Unit(_target), filter); };
		drone.MyCooldown = std::max(0.0, drone.MyCooldown - _delta);
		if (drone.MyPhase == DroneFlightPhase::SPREAD)
		{
			const auto distance = AccelerateDrone(drone, 1.9, 2, _delta);
			drone.MyPosition.MyX += drone.MyHeading.MyX * distance; drone.MyPosition.MyY += drone.MyHeading.MyY * distance;
			drone.MyAge += _delta;
			if (std::isless(drone.MyAge + 1e-9, kit.MySpreadTime)) { (void)publish(); return; }
			drone.MyPhase = DroneFlightPhase::SEEK;
		}
		if (drone.MyPhase == DroneFlightPhase::LOCK && !valid(drone.MyTarget))
		{
			const auto row = _MyRandom.Next() * 1.5 - 0.75, column = _MyRandom.Next() * 1.5 - 0.75;
			const auto forward = RotateOffset({.MyColumn = 1}, unit.MyFacing), left = RotateOffset({.MyRow = 1}, unit.MyFacing);
			const auto point = Unit(drone.MyTarget).MyPosition;
			drone.MyPosition = {.MyX = point.MyX + column * forward.MyColumn + row * left.MyColumn, .MyY = point.MyY + column * forward.MyRow + row * left.MyRow};
			drone.MyTarget = 0; drone.MyPhase = DroneFlightPhase::SEEK;
		}
		else if (drone.MyPhase == DroneFlightPhase::CHASE && !valid(drone.MyTarget)) { drone.MyTarget = 0; drone.MyPhase = DroneFlightPhase::SEEK; }
		if (drone.MyPhase == DroneFlightPhase::SEEK)
		{
			double nearest = std::numeric_limits<double>::infinity(), ownNearest = nearest;
			for (const auto id : _MyEnemyIds)
			{
				if (!valid(id)) continue;
				const auto distance = Distance(drone.MyPosition, Unit(id).MyPosition), ownDistance = Distance(RulePosition(unit), Unit(id).MyPosition);
				if (std::isless(distance, nearest - 1e-9) || (std::islessequal(distance, nearest + 1e-9) && std::isless(ownDistance, ownNearest - 1e-9)))
				{ drone.MyTarget = id; nearest = distance; ownNearest = ownDistance; }
			}
			if (!drone.MyTarget)
			{
				if (!drone.MyOrbit) drone.MyOrbit = WorldPoint{.MyX = drone.MyPosition.MyX - drone.MyHeading.MyY * 0.9, .MyY = drone.MyPosition.MyY + drone.MyHeading.MyX * 0.9};
				const auto angle = std::atan2(drone.MyPosition.MyY - drone.MyOrbit->MyY, drone.MyPosition.MyX - drone.MyOrbit->MyX) + _delta / 0.9;
				drone.MyPosition = {.MyX = drone.MyOrbit->MyX + 0.9 * std::cos(angle), .MyY = drone.MyOrbit->MyY + 0.9 * std::sin(angle)};
				drone.MyHeading = {.MyX = -std::sin(angle), .MyY = std::cos(angle)};
				(void)publish(); return;
			}
			drone.MyPhase = DroneFlightPhase::CHASE; drone.MySpeed = 2; drone.MyOrbit.reset();
		}
		const auto target = drone.MyTarget;
		if (drone.MyPhase == DroneFlightPhase::CHASE)
		{
			const auto distance = AccelerateDrone(drone, 1, 4, _delta);
			if (std::isgreater(BodyDistance(Unit(target), drone.MyPosition), distance + 1e-9))
			{
				const auto dx = Unit(target).MyPosition.MyX - drone.MyPosition.MyX, dy = Unit(target).MyPosition.MyY - drone.MyPosition.MyY;
				const auto length = std::hypot(dx, dy);
				if (std::isgreater(length, 1e-9)) drone.MyHeading = {.MyX = dx / length, .MyY = dy / length};
				drone.MyPosition.MyX += drone.MyHeading.MyX * distance; drone.MyPosition.MyY += drone.MyHeading.MyY * distance;
				(void)publish(); return;
			}
			drone.MyPhase = DroneFlightPhase::LOCK;
			if (!publish()) return;
			if (std::isgreater(kit.MyFear, 0)) (void)ApplyStatus(target, CombatStatus::FEAR, kit.MyFear, _unit);
		}
		drone.MyPosition = Unit(target).MyPosition;
		if (!publish() || std::isgreater(drone.MyCooldown, 1e-9) || !valid(target)) return;
		drone.MyCooldown = unit.MyStats.AttackInterval();
		const auto& funnel = unit.MyDefinition.MyProfession;
		drone.MyRamp = drone.MyRampTarget == target ? std::min(funnel.MyFunnelMax, drone.MyRamp + funnel.MyFunnelDelta) : funnel.MyFunnelInitial;
		drone.MyRampTarget = target;
		if (publish()) (void)DealDamage(_unit, target, {.MyAmount = unit.MyStats.MyAttack * unit.MyStats.MyAttackScaleMultiplier * drone.MyRamp,
			.MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::DRONE_ATTACK)});
	}

	void BattleCore::FlyingDronesTick(UnitId _unit, double _delta)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto& kit = std::get<Whitw2Kit>(*unit.MyDefinition.MyOperatorKit);
		const auto activation = unit.MySkill.MyActivations;
		const auto gone = [&]() { return !unit.MyAlive || !unit.MySkill.MyActive || unit.MySkill.MyActivations != activation; };
		const auto desired = kit.MyDrones + (unit.MyWolfStage >= 3 ? 1 : 0);
		if (unit.MyFlyingDrones.size() < desired) ReleaseFlyingDrones(_unit, static_cast<unsigned>(desired - unit.MyFlyingDrones.size()));
		for (std::size_t i = 0; i < unit.MyFlyingDrones.size(); ++i)
		{
			if (gone()) return;
			FlyDrone(_unit, i, _delta);
		}
		if (gone()) return;
		const AttackProfile filter{.MyCanHitFlying = true};
		auto& scratch = AcquireAttackScratch(); const FlyingScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto& drone : unit.MyFlyingDrones)
		{
			FoesInRadius(drone.MyPosition, kit.MyRadius, scratch.MyTargets);
			for (const auto id : scratch.MyTargets) if (TargetableEnemy(Unit(id), filter) && !std::ranges::contains(scratch.MySeen, id)) scratch.MySeen.push_back(id);
		}
		if (std::islessgreater(kit.MySlow, 0)) for (const auto id : scratch.MySeen) (void)AddBuff(id, {.MyKey = "whitw2:slow", .MySource = _unit, .MyDuration = 0.2,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::MOVE_MULTIPLIER, .MyValue = std::max(0.0, 1 + kit.MySlow)}}});
		unit.MyFlyingDroneAccumulator += _delta;
		if (std::isless(unit.MyFlyingDroneAccumulator + 1e-9, 1)) return;
		unit.MyFlyingDroneAccumulator -= 1;
		for (const auto id : scratch.MySeen)
		{
			if (gone()) return;
			if (Unit(id).MyAlive) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * kit.MyDotScale, .MyType = DamageType::ARTS,
				.MyTags = DamageTag::SKILL | DamageTag::DRONE | DamageTag::DOT, .MyIsSkill = true});
		}
	}

	void BattleCore::Whitw2Skill(UnitId _unit, const Whitw2Kit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY)
		{
			unit.MyWolfStage = 0;
			unit.MyDefinition.MyAttack.MyHits = unit.MyWolfBaseHits;
			unit.MyDefinition.MyProfession.MyFunnelMax = unit.MyWolfBaseCap;
		}
		if (_event.MyKind == ContentEventKind::DAMAGED && !_event.MyElement && unit.MyWolfStage >= 2 && std::isgreater(_kit.MySilence, 0) &&
			_event.MyTarget && Unit(_event.MyTarget).MyAlive && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
			(void)ApplyStatus(_event.MyTarget, CombatStatus::SILENCE, _kit.MySilence, _unit);
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			unit.MyLazyLock = 0; unit.MyHuntLocks.clear(); unit.MyFunnelRamps.clear();
			if (_kit.MySkill == Whitw2SkillKind::HAVOC)
			{
				unit.MyFlyingDrones.clear(); unit.MyFlyingDroneAccumulator = 0;
				ReleaseFlyingDrones(_unit, _kit.MyDrones + (unit.MyWolfStage >= 3 ? 1 : 0));
			}
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			unit.MyHuntLocks.clear(); unit.MyFunnelRamps.clear(); unit.MyFlyingDrones.clear();
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK && _kit.MySkill == Whitw2SkillKind::HAVOC) FlyingDronesTick(_unit, _event.MyDelta);
	}
}
