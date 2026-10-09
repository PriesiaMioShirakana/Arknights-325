#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		bool SameTile(WorldPoint _left, WorldPoint _right) noexcept
		{ return !std::islessgreater(_left.MyX, _right.MyX) && !std::islessgreater(_left.MyY, _right.MyY); }
	}

	bool Battle::CanDeploy(WorldPoint _position, UnitId _excludedUnit, bool _includeDown) const
	{
		if (!std::isfinite(_position.MyX) || !std::isfinite(_position.MyY) ||
			std::isless(_position.MyX, 0) || std::isgreater(_position.MyX, FieldColumns - 1) ||
			std::isless(_position.MyY, 0) || std::isgreater(_position.MyY, FieldRows - 1) ||
			std::islessgreater(std::floor(_position.MyX), _position.MyX) || std::islessgreater(std::floor(_position.MyY), _position.MyY)) return false;
		if (_MyGrid && !_MyGrid->InRect(static_cast<int>(_position.MyY), static_cast<int>(_position.MyX))) return false;
		for (const auto id : _MyAllyIds)
		{
			const auto& unit = Unit(id);
			if (id == _excludedUnit || unit.MyRemoved) continue;
			if (unit.MyAlive && SameTile(unit.MyPosition, _position)) return false;
			if (_includeDown && IsDown(id) && SameTile(RestPosition(id), _position)) return false;
		}
		return true;
	}

	UnitId Battle::SpawnToken(TokenSpawn _spawn)
	{
		ValidateDefinition(_spawn.MyDefinition, false);
		ValidateContent(_spawn.MyDefinition.MyContent, ContentTag::CUSTOM_OPERATOR);
		if (!std::isfinite(_spawn.MyDuration) || std::isless(_spawn.MyDuration, 0) ||
			(_spawn.MyHealth && !std::isfinite(*_spawn.MyHealth)) ||
			(_spawn.MyFacing && static_cast<unsigned>(*_spawn.MyFacing) > static_cast<unsigned>(Facing::LEFT)))
			throw std::invalid_argument("invalid token options");
		const auto owner = _spawn.MyOwnerUnit ? Unit(_spawn.MyOwnerUnit).MyOwner : Owner(_spawn.MyPlayerId);
		if (owner == NoPlayer || (_spawn.MyOwnerUnit && Unit(_spawn.MyOwnerUnit).MySide != UnitSide::ALLY))
			throw std::invalid_argument("token requires an ally or player owner");
		if (!_MyStarted || Finished() || !CanDeploy(_spawn.MyPosition, 0, false)) return 0;
		if (_MyUnits.size() == std::numeric_limits<UnitId>::max()) throw std::length_error("battle unit IDs exhausted");
		auto& unit = _MyUnits.emplace_back();
		unit.MyId = static_cast<UnitId>(_MyUnits.size());
		unit.MyKind = UnitKind::TOKEN;
		unit.MyOwner = owner;
		unit.MyOwnerUnit = _spawn.MyOwnerUnit;
		unit.MyDefinition = std::move(_spawn.MyDefinition);
		unit.MyStats = ResolveStats(unit.MyDefinition.MyStats, {});
		unit.MyHealth = unit.MyStats.MyMaxHealth;
		unit.MyPosition = unit.MyHome = _spawn.MyPosition;
		unit.MyFacing = _spawn.MyFacing.value_or(_spawn.MyOwnerUnit ? Unit(_spawn.MyOwnerUnit).MyFacing : Facing::RIGHT);
		unit.MyRespawnAt = std::numeric_limits<double>::infinity();
		_MyAllyIds.emplace_back(unit.MyId);
		AttachContent(unit.MyDefinition.MyContent, unit.MyId, 0, owner);
		InstallProfession(unit);
		InstallOperatorKit(unit);
		InstallUnitEffects(unit);
		if (_spawn.MyUntargetable)
		{
			StatusFlags flags;
			flags.set(static_cast<std::size_t>(CombatStatus::UNTARGETABLE));
			unit.MyBuffs.emplace_back(CombatBuff{.MyDefinition = BuffDefinition{.MyKey = "trait:untargetable", .MyFlags = flags, .MyPersistent = true},
				.MyId = ++_MyBuffSequence, .MyRemaining = std::numeric_limits<double>::infinity()});
		}
		if (!Deploy(unit))
		{
			unit.MyRemoved = true;
			RetireContent(unit.MyId, 0);
			return 0;
		}
		if (_spawn.MyHealth && unit.MyAlive) unit.MyHealth = std::clamp(*_spawn.MyHealth, 1.0, unit.MyStats.MyMaxHealth);
		if (std::isgreater(_spawn.MyDuration, 0))
		{
			unit.MyExpiresAt = Time() + _spawn.MyDuration;
			Schedule(ScheduledAction{.MyAt = unit.MyExpiresAt, .MyKind = ScheduledKind::TOKEN_EXPIRE, .MyTarget = unit.MyId});
		}
		return unit.MyId;
	}

	UnitId Battle::SpawnDevice(DeviceSpawn _spawn)
	{
		if (!std::isfinite(_spawn.MyAttack) || std::isless(_spawn.MyAttack, 0) ||
			!std::isfinite(_spawn.MyAttackTime) || !std::isgreater(_spawn.MyAttackTime, 0) || !std::isfinite(_spawn.MyAttackSpeed) ||
			_spawn.MyId.empty() || !std::isfinite(_spawn.MyHealth) || !std::isgreater(_spawn.MyHealth, 0) ||
			!std::isfinite(_spawn.MyDefense) || std::isless(_spawn.MyDefense, 0) || !std::isfinite(_spawn.MyResistance) ||
			std::isless(_spawn.MyResistance, 0) || std::isgreater(_spawn.MyResistance, 100) || _spawn.MyBlockCount < 0 ||
			(_spawn.MyObstacleKind != ObstacleKind::BLOCK && _spawn.MyObstacleKind != ObstacleKind::CRATE))
			throw std::invalid_argument("invalid device definition");
		if (!_MyStarted || Finished() || !CanDeploy(_spawn.MyPosition)) return 0;
		if (_MyUnits.size() == std::numeric_limits<UnitId>::max()) throw std::length_error("battle unit IDs exhausted");
		if (!_MyGrid) _MyGrid.emplace();
		auto& unit = _MyUnits.emplace_back();
		unit.MyId = static_cast<UnitId>(_MyUnits.size());
		unit.MyKind = UnitKind::DEVICE;
		unit.MyOwner = NoPlayer;
		unit.MyDefinition = CombatDefinition{.MyId = std::move(_spawn.MyId), .MyStats = CombatStats{
			.MyMaxHealth = _spawn.MyHealth, .MyAttack = _spawn.MyAttack, .MyDefense = _spawn.MyDefense, .MyResistance = _spawn.MyResistance,
			.MyAttackSpeed = _spawn.MyAttackSpeed, .MyBaseAttackTime = _spawn.MyAttackTime,
			.MyBlockCount = _spawn.MyBlockCount, .MyTaunt = -1, .MySpRecovery = 0}, .MyAttack = AttackProfile{.MyDisabled = true, .MyNoHeal = true}};
		unit.MyStats = ResolveStats(unit.MyDefinition.MyStats, {});
		unit.MyPosition = unit.MyHome = _spawn.MyPosition;
		unit.MyHealth = unit.MyStats.MyMaxHealth;
		unit.MyAlive = true;
		unit.MyGround = true;
		unit.MyGroundPassable = _MyGrid->Tile(static_cast<int>(_spawn.MyPosition.MyY), static_cast<int>(_spawn.MyPosition.MyX)).MyWalkable;
		unit.MyObstacle = _spawn.MyObstacle;
		unit.MyObstacleKind = _spawn.MyObstacleKind;
		unit.MyDeploySequence = ++_MyDeploySequence;
		unit.MyAggroSequence = unit.MyDeploySequence;
		unit.MyRespawnAt = std::numeric_limits<double>::infinity();
		_MyAllyIds.emplace_back(unit.MyId);
		if (unit.MyObstacle) SetObstacle(static_cast<int>(unit.MyPosition.MyY), static_cast<int>(unit.MyPosition.MyX), true, unit.MyObstacleKind);
		Emit(BattleEventKind::SPAWNED, unit.MyId);
		if (_spawn.MyRemoveOnDeploy) RemoveUnit(unit, RemovalReason::REMOVED, 0, true);
		ContentEvent event{.MyKind = ContentEventKind::DEPLOY, .MyUnit = unit.MyId};
		NotifyContent(event);
		return unit.MyId;
	}

	const EnemySpawn& Battle::SpawnDefinition(std::size_t _index) const
	{
		return _index < _MyInput.MySpawns.size() ? _MyInput.MySpawns[_index] : _MyExtraSpawns.at(_index - _MyInput.MySpawns.size());
	}

	UnitId Battle::CreateEnemy(const EnemySpawn& _spawn, std::size_t _index)
	{
		if (_MyUnits.size() == std::numeric_limits<UnitId>::max()) throw std::length_error("battle unit IDs exhausted");
		auto& unit = _MyUnits.emplace_back();
		unit.MyId = static_cast<UnitId>(_MyUnits.size());
		unit.MySide = UnitSide::ENEMY;
		unit.MyKind = UnitKind::ENEMY;
		unit.MySpawnTag = _spawn.MyTag;
		unit.MyDefinition = _spawn.MyDefinition;
		unit.MyStats = ResolveStats(unit.MyDefinition.MyStats, {});
		unit.MyOwner = Owner(_spawn.MyOwnerId);
		unit.MyPosition = unit.MyHome = _spawn.MyRoute.MyStart;
		unit.MyAlive = true;
		unit.MyHealth = unit.MyStats.MyMaxHealth;
		unit.MyDeploySequence = ++_MyDeploySequence;
		unit.MyAggroSequence = unit.MyDeploySequence;
		unit.MySpawnSequence = ++_MySpawnSequence;
		if (_MyInput.MySharedBoss && _spawn.MyTag == EnemySpawnTag::BOSS) unit.MyDefinition.MySharedBoss = true;
		if (_MyInput.MySharedBoss && unit.MyDefinition.MySharedBoss) SyncBossHealth(unit);
		unit.MySpawnIndex = _index;
		_MyEnemyIds.emplace_back(unit.MyId);
		Emit(BattleEventKind::SPAWNED, unit.MyId);
		AttachContent(unit.MyDefinition.MyContent, unit.MyId, 0, unit.MyOwner);
		RefreshTerrain(unit, true);
		ContentEvent event{.MyKind = ContentEventKind::DEPLOY, .MyUnit = unit.MyId};
		NotifyContent(event);
		return unit.MyId;
	}

	UnitId Battle::SpawnEnemy(EnemySpawn _spawn)
	{
		ValidateSpawnMetadata(_spawn);
		ValidateDefinition(_spawn.MyDefinition, true);
		ValidateContent(_spawn.MyDefinition.MyContent, ContentTag::CUSTOM_ENEMY);
		ValidatePoint(_spawn.MyRoute.MyStart, true);
		ValidatePoint(_spawn.MyRoute.MyEnd);
		const auto owner = Owner(_spawn.MyOwnerId);
		if (_spawn.MyLifeCost < 0) throw std::invalid_argument("negative enemy life cost");
		for (const auto& step : _spawn.MyRoute.MySteps)
		{
			ValidatePoint(step.MyPosition);
			if (static_cast<unsigned>(step.MyKind) > static_cast<unsigned>(RouteStepKind::APPEAR) ||
				!std::isfinite(step.MyWaitSeconds) || std::isless(step.MyWaitSeconds, 0)) throw std::invalid_argument("invalid enemy route step");
		}
		if (!_MyStarted || Finished()) return 0;
		const auto index = _MyInput.MySpawns.size() + _MyExtraSpawns.size();
		const auto& stored = _MyExtraSpawns.emplace_back(std::move(_spawn));
		// 原有出生队列不重排，动态敌人拥有独立稳定记录；它们的路线可在回调中被引用。
		auto& tail = _MyRouteTails.emplace_back(stored.MyRoute.MySteps.size() + 2, 0.0);
		auto position = stored.MyRoute.MyStart;
		for (std::size_t i = 0; i <= stored.MyRoute.MySteps.size(); ++i)
		{
			const auto step = i == stored.MyRoute.MySteps.size() ? RouteStep{.MyPosition = stored.MyRoute.MyEnd} : stored.MyRoute.MySteps[i];
			if (step.MyKind == RouteStepKind::MOVE) tail[i] = Distance(position, step.MyPosition);
			if (step.MyKind == RouteStepKind::MOVE || step.MyKind == RouteStepKind::APPEAR) position = step.MyPosition;
		}
		for (std::size_t i = tail.size() - 1; i > 0; --i) tail[i - 1] += tail[i];
		_MyRouteTailVersions.emplace_back(std::numeric_limits<std::uint64_t>::max());
		if (stored.MyCounted) { ++_MyTotal; ++_MyPlayers[owner].MyTotal; }
		return CreateEnemy(stored, index);
	}

	void Battle::Retreat(UnitId _unit, bool _permanent, RemovalReason _reason, bool _dying)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_reason != RemovalReason::RETREAT && _reason != RemovalReason::EXPIRED && _reason != RemovalReason::MERCHANT && _reason != RemovalReason::RAID) throw std::invalid_argument("invalid retreat reason");
		if (!_MyStarted || Finished() || !unit.MyAlive || unit.MySide != UnitSide::ALLY) return;
		RemoveUnit(unit, _reason, 0, _permanent, _dying);
	}

	bool Battle::Redeploy(UnitId _unit, bool _free, std::optional<WorldPoint> _tile, bool _keepSp)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (!_MyStarted || Finished() || unit.MyAlive || unit.MyRemoved || unit.MyRemoving || unit.MySide != UnitSide::ALLY) return false;
		const auto position = _tile.value_or(RestPosition(_unit));
		if (!CanDeploy(position, unit.MyId)) return false;
		auto& player = _MyPlayers[unit.MyOwner];
		const auto cost = unit.MyDefinition.MyStats.MyDeploymentCost;
		if (!_free && std::isless(player.MyDp + 1e-9, cost)) return false;
		const auto paid = _free ? 0 : std::min(player.MyDp, cost);
		player.MyDp -= paid;
		const auto total = SpTotal(_unit);
		if (!Deploy(unit, false, position, _keepSp ? std::optional(total) : std::nullopt)) { player.MyDp = std::min(_MyInput.MyMaxDp, player.MyDp + paid); return false; }
		return true;
	}

	void Battle::RemoveUnit(CombatUnit& _unit, RemovalReason _reason, UnitId _source, bool _permanent, bool _dying)
	{
		_unit.MyAlive = false;
		_unit.MyRemovalReason = _reason;
		_unit.MyRemovedAt = Time();
		_unit.MyRemoving = true;
		EndSkill(_unit.MyId, SkillReason::DEATH);
		_unit.MyRemoving = false;
		if (_unit.MySide == UnitSide::ALLY)
		{
			ReleaseBlocked(_unit);
			for (std::size_t i = 0; i < _unit.MyStatuses.MyRemaining.size(); ++i)
				if (std::isgreater(_unit.MyStatuses.MyRemaining[i], 0)) Emit(BattleEventKind::STATUS_REMOVED, 0, _unit.MyId, 0, static_cast<CombatStatus>(i));
			_unit.MyStatuses = {};
			for (const auto& buff : _unit.MyBuffs)
				if (!buff.MyDefinition.MyPersistent) RetireContent(_unit.MyId, buff.MyId);
			std::erase_if(_unit.MyBuffs, [](const CombatBuff& _buff) { return !_buff.MyDefinition.MyPersistent; });
			Recalculate(_unit);
			_unit.MyShields.clear();
			if (_unit.MyKind == UnitKind::OPERATOR && !_permanent)
			{
				_unit.MyRespawnAt = Time() + std::max(0.0, _unit.MyStats.MyRedeploySeconds * _unit.MyStats.MyRedeployMultiplier);
				if (IsDown(_unit.MyId)) LayBody(_unit);
				if (_reason == RemovalReason::KILLED) ++_MyPlayers[_unit.MyOwner].MyDeaths;
			}
			else _unit.MyRemoved = true;
			if (_unit.MyKind == UnitKind::DEVICE && _unit.MyObstacle)
				SetObstacle(static_cast<int>(_unit.MyPosition.MyY), static_cast<int>(_unit.MyPosition.MyX), false, _unit.MyObstacleKind);
		}
		else
		{
			_unit.MyRemoved = true;
			ReleaseBlock(_unit);
			if (_reason == RemovalReason::KILLED)
			{
				if (SpawnDefinition(_unit.MySpawnIndex).MyCounted) { ++_MyKilled; ++_MyPlayers[_unit.MyOwner].MyKilled; }
				if (_source) ++_MyUnits[Index(_source)].MyTotals.MyKills;
				PayBounty(_unit, _source);
			}
		}
		if (_reason != RemovalReason::LEAK) Emit(BattleEventKind::DIED, _source, _unit.MyId);
		ContentEvent event{.MyKind = ContentEventKind::DEATH, .MyUnit = _unit.MyId, .MySource = _source, .MyTarget = _unit.MyId, .MyRemovalReason = _reason, .MyDying = _dying};
		NotifyContent(event);
		// 本次退场事件先结算；关闭保留时，获授特质及其属性不会带入下一次部署。
		if (RevokeGrantedGarrisons(_unit.MyId) && _unit.MyKind == UnitKind::OPERATOR && !_unit.MyAlive && !_unit.MyRemoved)
			_unit.MyRespawnAt = _unit.MyRemovedAt + std::max(0.0, _unit.MyStats.MyRedeploySeconds * _unit.MyStats.MyRedeployMultiplier);
		if (_unit.MyRemoved && !_unit.MyAlive && _reason != RemovalReason::LEAK)
		{
			RetireContent(_unit.MyId, 0);
			for (const auto& buff : _unit.MyBuffs) RetireContent(_unit.MyId, buff.MyId);
		}
	}
}
