#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		struct ArrowScratchGuard
		{
			std::size_t& MyDepth;

			~ArrowScratchGuard() { --MyDepth; }
		};
	}

	void Battle::KjeraDeploy(UnitId _unit, const KjeraKit& _kit)
	{
		const auto& unit = Unit(_unit);
		unsigned low = 0;
		for (int key = 0; key < FieldTiles; ++key)
			if ((_kit.MyLockDrones ? std::ranges::contains(RuleRangeKeys(unit), key) : unit.MyBaseTriggerMask.test(static_cast<std::size_t>(key))) && (!_MyGrid || (_MyGrid->InRect(key / FieldColumns, key % FieldColumns) && _MyGrid->Tile(key / FieldColumns, key % FieldColumns).MyLow))) ++low;
		const auto attack = std::isgreaterequal(static_cast<double>(low), _kit.MyGroundTiles) ? _kit.MyGroundAttack : _kit.MyAttack;
		(void)AddBuff(_unit, {.MyKey = _kit.MyLockDrones ? "kjera:t1" : "talent:kjera_brow", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = attack}}});
	}

	void Battle::KjeraBeforeAttack(CombatUnit& _unit, const KjeraKit& _kit, std::vector<UnitId>& _targets)
	{
		if (!_kit.MyLockDrones)
		{
			if (_targets.empty()) return;
			const auto count = _targets.size();
			_targets.reserve(_kit.MyDrones);
			for (std::size_t i = 0; _targets.size() < _kit.MyDrones; ++i) _targets.push_back(_targets[i % count]);
			return;
		}
		auto& scratch = AcquireAttackScratch(); const ArrowScratchGuard guard{.MyDepth = _MyAttackDepth};
		const AttackProfile filter{.MyCanHitFlying = true};
		for (const auto id : _targets) if (TargetableEnemy(Unit(id), filter)) scratch.MyTargets.push_back(id);
		_unit.MyLockedDrones.resize(_kit.MyDrones);
		for (auto& drone : _unit.MyLockedDrones)
		{
			if (drone.MyTarget && TargetableEnemy(Unit(drone.MyTarget), filter)) scratch.MySeen.push_back(drone.MyTarget);
			else drone = {};
		}
		for (auto& drone : _unit.MyLockedDrones)
		{
			if (drone.MyTarget) continue;
			const auto candidate = std::ranges::find_if(scratch.MyTargets, [&](UnitId _id) { return !std::ranges::contains(scratch.MySeen, _id); });
			if (candidate != scratch.MyTargets.end()) drone.MyTarget = *candidate;
			else if (!scratch.MyTargets.empty()) drone.MyTarget = scratch.MyTargets.front();
			else
			{
				const auto held = std::ranges::find_if(_unit.MyLockedDrones, [](const LockedDrone& _drone) { return _drone.MyTarget != 0; });
				if (held != _unit.MyLockedDrones.end()) drone.MyTarget = held->MyTarget;
			}
			if (drone.MyTarget) scratch.MySeen.push_back(drone.MyTarget);
		}
		scratch.MyTargets.clear();
		const auto& funnel = _unit.MyDefinition.MyProfession;
		for (auto& drone : _unit.MyLockedDrones)
		{
			if (!drone.MyTarget) continue;
			drone.MyScale = drone.MyScale ? std::min(funnel.MyFunnelMax, *drone.MyScale + funnel.MyFunnelDelta) : funnel.MyFunnelInitial;
			scratch.MyTargets.push_back(drone.MyTarget);
			auto queue = std::ranges::find(_unit.MyDroneQueues, drone.MyTarget, &DroneDamageQueue::MyTarget);
			if (queue == _unit.MyDroneQueues.end())
			{
				_unit.MyDroneQueues.emplace_back(DroneDamageQueue{.MyTarget = drone.MyTarget});
				queue = std::prev(_unit.MyDroneQueues.end());
				queue->MyScales.reserve(_kit.MyDrones);
			}
			if (queue->MyHead)
			{
				queue->MyScales.erase(queue->MyScales.begin(), queue->MyScales.begin() + static_cast<std::ptrdiff_t>(queue->MyHead));
				queue->MyHead = 0;
			}
			queue->MyScales.push_back(*drone.MyScale);
		}
		if (!scratch.MyTargets.empty()) _targets.assign(scratch.MyTargets.begin(), scratch.MyTargets.end());
	}

	double Battle::LockedFunnelMultiplier(CombatUnit& _unit, UnitId _target)
	{
		const auto queue = std::ranges::find(_unit.MyDroneQueues, _target, &DroneDamageQueue::MyTarget);
		if (queue == _unit.MyDroneQueues.end() || queue->MyHead == queue->MyScales.size()) return _unit.MyDefinition.MyProfession.MyFunnelInitial;
		const auto value = queue->MyScales[queue->MyHead++];
		if (queue->MyHead == queue->MyScales.size()) { queue->MyScales.clear(); queue->MyHead = 0; }
		return value;
	}

	UnitId Battle::NearestProjectileTarget(UnitId _source, WorldPoint _point, double _radius, std::span<const UnitId> _skip) const
	{
		(void)_source;
		UnitId best = 0; double nearest = std::numeric_limits<double>::infinity();
		const AttackProfile filter{.MyCanHitFlying = true};
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (!TargetableEnemy(enemy, filter) || std::ranges::contains(_skip, id)) continue;
			const auto distance = BodyDistance(enemy, _point);
			if (std::isgreater(distance * distance, _radius * _radius + 1e-9)) continue;
			if (!best || std::isless(distance, nearest) || (!std::islessgreater(distance, nearest) && enemy.MySpawnSequence < Unit(best).MySpawnSequence))
			{ best = id; nearest = distance; }
		}
		return best;
	}

	void Battle::ArchetScatter(UnitId _unit, UnitId _target, WorldPoint _point, const ArchetKit& _kit)
	{
		if (!_kit.MyScatterTargets) return;
		auto& source = _MyUnits[Index(_unit)];
		const auto point = _target ? Unit(_target).MyPosition : _point;
		auto& scratch = AcquireAttackScratch(); const ArrowScratchGuard guard{.MyDepth = _MyAttackDepth};
		if (_target) scratch.MySeen.push_back(_target);
		for (unsigned i = 0; i < _kit.MyScatterTargets; ++i)
		{
			const auto next = NearestProjectileTarget(_unit, point, 1.5, scratch.MySeen);
			if (!next) break;
			scratch.MySeen.push_back(next); scratch.MyTargets.push_back(next);
		}
		for (const auto id : scratch.MyTargets)
			(void)DealDamage(_unit, id, {.MyAmount = source.MyStats.MyAttack * _kit.MyScale * MainAttackMultiplier(source, Unit(id), source.MyDefinition.MyAttack, false),
				.MyType = DamageType::PHYSICAL, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
	}

	void Battle::ArchetPursuit(UnitId _unit, const ArchetKit& _kit)
	{
		const auto& unit = Unit(_unit);
		auto& scratch = AcquireAttackScratch(); const ArrowScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyEnemyIds)
			if (TargetableEnemy(Unit(id), unit.MyDefinition.MyAttack) && InRuleRange(_unit, id)) scratch.MyTargets.push_back(id);
		SortOperatorTargets(_unit, scratch.MyTargets, 1, &unit.MyDefinition.MyAttack);
		if (!scratch.MyTargets.empty()) LaunchSkillProjectile(_unit, scratch.MyTargets.front(), 16,
			{.MyScale = _kit.MyScale, .MyHits = _kit.MyHits, .MyBounceRadius = 1.5, .MyProfileScale = true});
	}

	void Battle::ArchetTactics(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyOperatorHooksReleased || std::ranges::any_of(_MyArchets, [&](UnitId _other)
			{ return _other < _unit && Unit(_other).MyAlive && Unit(_other).MyOwner == unit.MyOwner; })) return;
		const auto& kit = std::get<ArchetKit>(*unit.MyDefinition.MyOperatorKit);
		for (std::size_t i = 0, count = _MyAllyIds.size(); i < count; ++i)
		{
			const auto& ally = Unit(_MyAllyIds[i]);
			if (ally.MyAlive && ally.MyOwner == unit.MyOwner && ally.MyKind == UnitKind::OPERATOR &&
				ally.MyDefinition.MyOperatorProfession == OperatorProfession::SNIPER && ally.MyDefinition.MySkill.MySpType == SpType::ATTACK)
				(void)GainSp(ally.MyId, kit.MyTacticsSp);
		}
	}
}
