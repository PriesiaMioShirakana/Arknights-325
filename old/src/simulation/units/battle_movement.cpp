#include <algorithm>
#include <limits>
#include "battle_core.hpp"
#include <tuple>

namespace Stronghold
{
	bool BattleCore::CheckBlock(CombatUnit& _enemy)
	{
		if (_enemy.MyBlockedBy || _enemy.MyHidden || !_enemy.MyAlive)
			return _enemy.MyBlockedBy != 0;
		if (_enemy.MyStatuses.Has(CombatStatus::UNBLOCKABLE))
			return false;
		const auto row = static_cast<int>(std::floor(_enemy.MyPosition.MyY + 0.5));
		const auto column = static_cast<int>(std::floor(_enemy.MyPosition.MyX + 0.5));
		UnitId best{};
		double bestDistance = std::numeric_limits<double>::infinity();
		for (std::size_t i = 0; i < _MyAllyIds.size(); ++i)
		{
			const auto& ally = _MyUnits[Index(_MyAllyIds[i])];
			if (!ally.MyAlive || ally.MyHidden || (ally.MyStatuses.Has(CombatStatus::STUN) || ally.MyStatuses.Has(CombatStatus::NO_BLOCK)))
				continue;
			const bool flying = _enemy.Flying();
			if (flying ? !(ally.MyDefinition.MyAttack.MyBlockFlying || ally.MyStatuses.Has(CombatStatus::BLOCK_FLYING)) : !ally.MyGround || !ally.MyGroundPassable)
				continue;
			const auto ar = static_cast<int>(ally.MyPosition.MyY);
			const auto ac = static_cast<int>(ally.MyPosition.MyX);
			if (std::abs(ar - row) > 1 || std::abs(ac - column) > 1 || (!flying && ar != row && ac != column))
				continue;
			int used = 0;
			for (const auto id : ally.MyBlocking)
				used += Unit(id).MyDefinition.MyBlockWeight;
			if (used + _enemy.MyDefinition.MyBlockWeight > ally.MyStats.MyBlockCount)
				continue;
			const auto dx = _enemy.MyPosition.MyX - ally.MyPosition.MyX;
			const auto dy = _enemy.MyPosition.MyY - ally.MyPosition.MyY;
			const auto distance = dx * dx + dy * dy;
			const auto scale = 1 + ally.MyStats.MyBlockRadiusScale;
			const auto radiusSquared = ally.MyKind == UnitKind::DEVICE ? 0.19998784 : flying ? 0.79995137 * scale * scale : 0.49999037;
			if (distance >= radiusSquared)
				continue;
			const auto earlier = best && std::tie(ally.MyPosition.MyY, ally.MyPosition.MyX) < std::tie(Unit(best).MyPosition.MyY, Unit(best).MyPosition.MyX);
			if (distance < bestDistance || (distance == bestDistance && earlier))
			{
				best = ally.MyId;
				bestDistance = distance;
			}
		}
		if (!best)
			return false;
		_enemy.MyBlockedBy = best;
		_enemy.MyMoving = false;
		_MyUnits[Index(best)].MyBlocking.emplace_back(_enemy.MyId);
		Emit(BattleEventKind::BLOCKED, best, _enemy.MyId);
		WolfBlocked(best, _enemy.MyId);
		// 原 devices.js：无攻击能力的敌人也会在一个攻击间隔后破坏阻隔工事。
		if (const auto& blocker = Unit(best); blocker.MyKind == UnitKind::DEVICE && blocker.MyDefinition.MyId == "trap_1105_accrate")
		{
			Schedule(ScheduledAction{.MyAt = Time() + std::clamp(_enemy.MyStats.AttackInterval(), 0.1, 10.0),
				.MyKind = ScheduledKind::CRATE_BREAK, .MySource = _enemy.MyId, .MyTarget = best});
		}
		return true;
	}

	void BattleCore::ReleaseBlock(CombatUnit& _enemy)
	{
		if (!_enemy.MyBlockedBy)
			return;
		std::erase(_MyUnits[Index(_enemy.MyBlockedBy)].MyBlocking, _enemy.MyId);
		_enemy.MyBlockedBy = 0;
		SwitchStealth(_enemy);
	}

	void BattleCore::UpdateEnemy(CombatUnit& _unit)
	{
		const auto previousCooldown = _unit.MyAttackCooldown;
		const bool stunned = _unit.MyStatuses.Has(CombatStatus::STUN);
		if (!_unit.MyHidden && !stunned && _unit.MyAttackCooldown > 0)
			_unit.MyAttackCooldown = std::max(0.0, _unit.MyAttackCooldown - BattleClock::StepSeconds);
		if (_unit.MyHidden || stunned)
			_unit.MySwing = false;
		const bool winding = !_unit.MyHidden && !stunned && EnemyAttack(_unit, previousCooldown);
		// Stunned enemies can still be caught by an adjacent blocker; hidden ones continue route waits.
		if (stunned && !_unit.MyHidden)
		{
			_unit.MyAttackStandUntil = 0;
			(void)CheckBlock(_unit);
			return;
		}
		if (!_unit.MyAlive || _unit.MyBlockedBy)
			return;
		if (!_unit.MyHidden && CheckBlock(_unit))
			return;
		if (_unit.MyStatuses.Has(CombatStatus::NO_MOVE))
		{
			_unit.MyMoving = false;
			return;
		}
		if (_unit.MyStatuses.Has(CombatStatus::FEAR) && !_unit.MyHidden) { MoveFeared(_unit); return; }
		if (_unit.MyInducedMode == CombatStatus::FEAR)
		{
			_unit.MyInducedMode = CombatStatus::COUNT;
			_unit.MyInducedPath.clear();
			_unit.MyPlannedRoute = std::numeric_limits<std::size_t>::max();
		}
		const bool standing = winding || std::isless(Time(), _unit.MyAttackStandUntil);
		if (_unit.MyStatuses.Has(CombatStatus::ATTRACT))
		{
			if (standing) _unit.MyMoving = false;
			else MoveAttracted(_unit);
			return;
		}
		_unit.MyInducedMode = CombatStatus::COUNT;
		MoveEnemy(_unit, standing);
	}

	void BattleCore::MoveEnemy(CombatUnit& _unit, bool _standing)
	{
		const auto& route = SpawnDefinition(_unit.MySpawnIndex).MyRoute;
		double budget = BattleClock::StepSeconds;
		int guard = 16;
		while (budget > 1e-9 && guard-- > 0 && _unit.MyAlive)
		{
			const bool final = _unit.MyRouteIndex == route.MySteps.size();
			if (_unit.MyRouteIndex > route.MySteps.size())
			{
				Leak(_unit, false);
				return;
			}
			const auto step = final ? RouteStep{.MyKind = RouteStepKind::MOVE, .MyPosition = route.MyEnd} : route.MySteps[_unit.MyRouteIndex];
			if (step.MyKind == RouteStepKind::WAIT)
			{
				if (!_unit.MyWaitLeft)
					_unit.MyWaitLeft = step.MyWaitSeconds;
				const auto used = std::min(budget, *_unit.MyWaitLeft);
				*_unit.MyWaitLeft -= used;
				budget -= used;
				_unit.MyMoving = false;
				if (*_unit.MyWaitLeft <= 1e-9)
				{
					_unit.MyWaitLeft.reset();
					++_unit.MyRouteIndex;
				}
				continue;
			}
			if (step.MyKind == RouteStepKind::DISAPPEAR)
			{
				_unit.MyHidden = true;
				_unit.MyAttackStandUntil = 0;
				_standing = false;
				++_unit.MyRouteIndex;
				continue;
			}
			if (step.MyKind == RouteStepKind::APPEAR)
			{
				_unit.MyPosition = step.MyPosition;
				_unit.MyHidden = false;
				++_unit.MyRouteIndex;
				continue;
			}
			const auto speed = _unit.MyStats.MyMoveSpeed * 0.5;
			if (_standing || speed <= 0)
			{
				_unit.MyMoving = false;
				return;
			}
			if (_unit.MyPlannedRoute != _unit.MyRouteIndex || (_MyGrid && _unit.MyPathVersion != _MyGrid->Version())) PlanRoute(_unit, step.MyPosition);
			_unit.MyMoving = true;
			auto travel = speed * budget;
			int points = 64;
			while (std::isgreater(travel, 1e-9) && points-- > 0 && _unit.MyPathPoint < _unit.MyPath.size())
			{
				const auto destination = _unit.MyPath[_unit.MyPathPoint];
				const auto distance = Distance(destination, _unit.MyPosition);
				if (std::islessequal(distance, travel))
				{
					_unit.MyPosition = destination;
					travel -= distance;
					++_unit.MyPathPoint;
				}
				else
				{
					_unit.MyPosition.MyX += (destination.MyX - _unit.MyPosition.MyX) / distance * travel;
					_unit.MyPosition.MyY += (destination.MyY - _unit.MyPosition.MyY) / distance * travel;
					travel = 0;
				}
				if (CheckBlock(_unit)) return;
			}
			budget = travel / speed;
			if (_unit.MyPathPoint >= _unit.MyPath.size())
			{
				++_unit.MyRouteIndex;
				if (final) { Leak(_unit, false); return; }
			}
		}
	}

	double BattleCore::RemainingDistance(UnitId _enemy) const
	{
		const auto& unit = Unit(_enemy);
		if (unit.MySide != UnitSide::ENEMY)
			throw std::invalid_argument("route distance requires an enemy");
		const auto& route = SpawnDefinition(unit.MySpawnIndex).MyRoute;
		if (unit.MyRouteIndex > route.MySteps.size())
			return 0;
		const auto index = unit.MyRouteIndex;
		const auto step = index == route.MySteps.size() ? RouteStep{.MyKind = RouteStepKind::MOVE, .MyPosition = route.MyEnd} : route.MySteps[index];
		RebuildRouteTail(unit.MySpawnIndex);
		const auto& tail = _MyRouteTails[unit.MySpawnIndex];
		if (step.MyKind == RouteStepKind::MOVE && unit.MyPlannedRoute == unit.MyRouteIndex && unit.MyPathPoint < unit.MyPath.size())
			return Distance(unit.MyPosition, unit.MyPath[unit.MyPathPoint]) + unit.MyPathSuffix[unit.MyPathPoint] + tail[index + 1];
		return step.MyKind == RouteStepKind::MOVE ? Distance(unit.MyPosition, step.MyPosition) + tail[index + 1] : tail[index];
	}

	void BattleCore::Leak(CombatUnit& _unit, bool _timeout)
	{
		if (_unit.MyNoLeak) return;
		if (SpawnDefinition(_unit.MySpawnIndex).MyCounted) ++_MyLeaked;
		auto owner = _unit.MyResponsiblePlayer;
		if (!_timeout)
		{
			// 到达另一半场目标时，由目标所在半场承担漏怪；超时仍归原出生玩家。
			const bool right = std::isgreaterequal(std::floor(_unit.MyPosition.MyX + 0.5), 11);
			owner = 0;
			for (std::size_t i = 0; i < _MyInput.MyPlayers.size(); ++i)
				if (_MyInput.MyPlayers[i].MyKind == BattlePlayerKind::PARTICIPANT && _MyInput.MyPlayers[i].MyRole == BattlePlayerRole::DEFENDER && _MyInput.MyPlayers[i].MyRightHalf.value_or(_MyInput.MyPlayers[i].MyMirrorDeployment) == right) { owner = i; break; }
		}
		auto& player = _MyPlayers[owner];
		const auto& spawn = SpawnDefinition(_unit.MySpawnIndex);
		const bool boss = _unit.MySpawnTag == EnemySpawnTag::BOSS;
		player.MyLeaks.emplace_back(LeakedEnemy{.MyEnemyId = _unit.MyDefinition.MyId, .MyModifiers = spawn.MyModifiers,
			.MyLifeCost = spawn.MyLifeCost, .MySourcePlayer = spawn.MySourcePlayer.empty() ? _MyPlayers[_unit.MyResponsiblePlayer].MyPlayerId : spawn.MySourcePlayer,
			.MyTag = _unit.MySpawnTag, .MyCounted = spawn.MyCounted || boss, .MyBoss = boss});
		if (spawn.MyCounted || boss) player.MyPerfect = false;
		if (SpawnDefinition(_unit.MySpawnIndex).MyCounted) ++player.MyLeaked;
		player.MyLifeLost += SpawnDefinition(_unit.MySpawnIndex).MyLifeCost;
		if (!_timeout)
		{
			RemoveUnit(_unit, RemovalReason::LEAK);
		}
		Emit(BattleEventKind::LEAKED, _unit.MyId);
		if (!_timeout)
		{
			// 漏怪自身的处理器仍能接收最后一次通知；超时只记录结果，不执行死亡／漏怪钩子。
			ContentEvent event{.MyKind = ContentEventKind::ENEMY_LEAK, .MyUnit = _unit.MyId, .MyTarget = _unit.MyId};
			NotifyContent(event);
			(void)LoseTeamLife(spawn.MyLifeCost);
			RetireContent(_unit.MyId, 0);
			for (const auto& buff : _unit.MyBuffs) RetireContent(_unit.MyId, buff.MyId);
		}
	}
}
