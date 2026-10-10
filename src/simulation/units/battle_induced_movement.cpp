#include <numbers>
#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		bool AirMotion(const CombatUnit& _unit) noexcept
		{ return _unit.MyDefinition.MyFlying && !_unit.MyDefinition.MyWalksWhileFlying; }

		FieldPoint Here(const FieldGrid& _grid, WorldPoint _point)
		{
			const auto& rect = _grid.Rect();
			return FieldPoint{.MyRow = std::clamp(static_cast<int>(std::floor(_point.MyY + 0.5)), rect.MyFirstRow, rect.MyLastRow),
				.MyColumn = std::clamp(static_cast<int>(std::floor(_point.MyX + 0.5)), rect.MyFirstColumn, rect.MyLastColumn)};
		}

		bool Moved(WorldPoint _a, WorldPoint _b) noexcept
		{ return std::islessgreater(_a.MyX, _b.MyX) || std::islessgreater(_a.MyY, _b.MyY); }

		bool Open(const FieldGrid& _grid, bool _fly, int _key)
		{
			const auto row = _key / FieldColumns, column = _key % FieldColumns;
			return _fly ? _grid.FlyPassable(row, column) : _grid.GroundPassable(row, column);
		}
	}

	void BattleCore::MoveAttracted(CombatUnit& _unit)
	{
		const auto goal = _unit.MyStatuses.MyAttractPoint;
		if (!goal) { _unit.MyMoving = false; return; }
		if (!_MyGrid) _MyGrid.emplace();
		if (_unit.MyInducedMode != CombatStatus::ATTRACT || _unit.MyInducedVersion != _MyGrid->Version() ||
			Moved(_unit.MyInducedLast, _unit.MyPosition) || Moved(_unit.MyInducedGoal, *goal))
		{
			_unit.MyInducedPath.clear();
			_unit.MyInducedPoint = 0;
			_unit.MyInducedGoal = *goal;
			_unit.MyInducedMode = CombatStatus::ATTRACT;
			_unit.MyInducedVersion = _MyGrid->Version();
			const auto here = Here(*_MyGrid, _unit.MyPosition);
			const auto end = FieldPoint{.MyRow = static_cast<int>(goal->MyY), .MyColumn = static_cast<int>(goal->MyX)};
			if (!AirMotion(_unit) && (_MyGrid->Waypoints(here, end, _MyFieldWaypoints) || _MyGrid->Waypoints(here, end, _MyFieldWaypoints, true, true)))
			{
				_unit.MyInducedPath.reserve(_MyFieldWaypoints.size());
				for (std::size_t i = 0; i < _MyFieldWaypoints.size(); ++i)
				{
					const auto point = WorldPoint{.MyX = static_cast<double>(_MyFieldWaypoints[i].MyColumn), .MyY = static_cast<double>(_MyFieldWaypoints[i].MyRow)};
					if (i == 0 && std::isless(std::abs(point.MyX - _unit.MyPosition.MyX), 1e-6) && std::isless(std::abs(point.MyY - _unit.MyPosition.MyY), 1e-6)) continue;
					_unit.MyInducedPath.emplace_back(point);
				}
			}
			else _unit.MyInducedPath.emplace_back(*goal);
		}
		auto travel = _unit.MyStats.MyMoveSpeed * 0.5 * BattleClock::StepSeconds;
		bool moved = false;
		while (std::isgreater(travel, 1e-9) && _unit.MyInducedPoint < _unit.MyInducedPath.size())
		{
			const auto point = _unit.MyInducedPath[_unit.MyInducedPoint];
			const auto distance = Distance(_unit.MyPosition, point);
			if (std::islessequal(distance, travel)) { _unit.MyPosition = point; travel -= distance; ++_unit.MyInducedPoint; }
			else
			{
				_unit.MyPosition.MyX += (point.MyX - _unit.MyPosition.MyX) / distance * travel;
				_unit.MyPosition.MyY += (point.MyY - _unit.MyPosition.MyY) / distance * travel;
				travel = 0;
			}
			moved = true;
		}
		_unit.MyInducedLast = _unit.MyPosition;
		_unit.MyMoving = moved;
		if (moved) _unit.MyPlannedRoute = std::numeric_limits<std::size_t>::max();
	}

	void BattleCore::BuildFearTiles(CombatUnit& _unit)
	{
		_unit.MyFearTiles.clear();
		const auto stamp = _unit.MyStatuses.MyFear;
		if (!stamp || stamp->MySelf) return;
		const auto dx = stamp->MyHit.MyX - stamp->MySource.MyX, dy = stamp->MyHit.MyY - stamp->MySource.MyY;
		const auto distance = std::hypot(dx, dy);
		if (!std::isgreater(distance, 1e-9)) return;
		const auto goal = SpawnDefinition(_unit.MySpawnIndex).MyRoute.MyEnd;
		const bool fly = AirMotion(_unit);
		// 两个场的可达标记按值合并，不跨缓存插入保留引用（第二次插入可能淘汰第一个场）。
		std::array<bool, FieldTiles> reachable{};
		if (!fly)
		{
			const auto& normal = _MyGrid->Flow(static_cast<int>(goal.MyY), static_cast<int>(goal.MyX));
			for (int key = 0; key < FieldTiles; ++key) reachable[static_cast<std::size_t>(key)] = normal.MyDistance[static_cast<std::size_t>(key)] >= 0;
			const auto& fallback = _MyGrid->Flow(static_cast<int>(goal.MyY), static_cast<int>(goal.MyX), true, true);
			for (int key = 0; key < FieldTiles; ++key) reachable[static_cast<std::size_t>(key)] = reachable[static_cast<std::size_t>(key)] || fallback.MyDistance[static_cast<std::size_t>(key)] >= 0;
		}
		const auto& rect = _MyGrid->Rect();
		_unit.MyFearTiles.reserve(FieldTiles);
		for (int row = rect.MyFirstRow; row <= rect.MyLastRow; ++row)
			for (int column = rect.MyFirstColumn; column <= rect.MyLastColumn; ++column)
			{
				const auto vx = column - stamp->MyHit.MyX, vy = row - stamp->MyHit.MyY, length = std::hypot(vx, vy);
				if (std::isgreater(length, 10 + 1e-9) || (std::isgreater(length, 1e-9) &&
					std::isless(vx * dx / distance + vy * dy / distance, std::numbers::sqrt2 / 2 * length - 1e-9))) continue;
				const auto key = FieldGrid::Key(row, column);
				if ((fly ? !_MyGrid->FlyPassable(row, column) : !_MyGrid->Walkable(row, column, true)) ||
					_MyGrid->Tile(row, column).MyGoal || (!fly && !reachable[static_cast<std::size_t>(key)])) continue;
				_unit.MyFearTiles.emplace_back(key);
			}
	}

	bool BattleCore::FearReachable(const CombatUnit& _unit, int _from, int _to) const
	{
		if (_from == _to) return true;
		const bool fly = AirMotion(_unit);
		if (!Open(*_MyGrid, fly, _to)) return false;
		constexpr std::array directions{FieldPoint{.MyRow = 1}, FieldPoint{.MyColumn = 1}, FieldPoint{.MyRow = -1}, FieldPoint{.MyColumn = -1}};
		std::array<int, FieldTiles> queue{};
		std::array<bool, FieldTiles> seen{};
		queue[0] = _from;
		seen[static_cast<std::size_t>(_from)] = true;
		std::size_t begin = 0, end = 1;
		for (int steps = 1; steps <= 5 && begin < end; ++steps)
		{
			const auto stop = end;
			while (begin < stop)
			{
				const auto current = queue[begin++];
				for (const auto direction : directions)
				{
					const auto row = current / FieldColumns + direction.MyRow, column = current % FieldColumns + direction.MyColumn;
					if (!_MyGrid->InRect(row, column)) continue;
					const auto key = FieldGrid::Key(row, column);
					if (seen[static_cast<std::size_t>(key)]) continue;
					if (key == _to) return true;
					seen[static_cast<std::size_t>(key)] = true;
					if (Open(*_MyGrid, fly, key)) queue[end++] = key;
				}
			}
		}
		return false;
	}

	bool BattleCore::PlanFear(CombatUnit& _unit)
	{
		const auto here = Here(*_MyGrid, _unit.MyPosition);
		const auto key = _unit.MyInducedTile;
		auto& path = _unit.MyInducedPath;
		path.clear();
		if (!AirMotion(_unit) && key != FieldGrid::Key(here.MyRow, here.MyColumn))
		{
			const auto end = FieldPoint{.MyRow = key / FieldColumns, .MyColumn = key % FieldColumns};
			if (!Open(*_MyGrid, false, key) || !_MyGrid->Waypoints(here, end, _MyFieldWaypoints)) return false;
			path.reserve(_MyFieldWaypoints.size());
			for (std::size_t i = 1; i + 1 < _MyFieldWaypoints.size(); ++i)
				path.emplace_back(static_cast<double>(_MyFieldWaypoints[i].MyColumn), static_cast<double>(_MyFieldWaypoints[i].MyRow));
			const auto first = path.empty() ? end : FieldPoint{.MyRow = static_cast<int>(path.front().MyY), .MyColumn = static_cast<int>(path.front().MyX)};
			if (!_MyGrid->StraightClear(_unit.MyPosition.MyX, _unit.MyPosition.MyY, first))
				path.emplace(path.begin(), static_cast<double>(here.MyColumn), static_cast<double>(here.MyRow));
		}
		path.emplace_back(_unit.MyInducedGoal);
		_unit.MyInducedPoint = 0;
		_unit.MyInducedVersion = _MyGrid->Version();
		return true;
	}

	void BattleCore::PickFearPoint(CombatUnit& _unit)
	{
		const auto here = Here(*_MyGrid, _unit.MyPosition);
		const auto own = FieldGrid::Key(here.MyRow, here.MyColumn);
		auto key = own;
		if (!_unit.MyFearTiles.empty())
		{
			const auto index = static_cast<std::size_t>(std::floor(_MyRandom.Next() * static_cast<double>(_unit.MyFearTiles.size())));
			const auto candidate = _unit.MyFearTiles[index];
			if (FearReachable(_unit, own, candidate)) key = candidate;
			else _unit.MyFearTiles.erase(_unit.MyFearTiles.begin() + static_cast<std::ptrdiff_t>(index));
		}
		// 两次抽样顺序不可交换；候选不可达时回到自身格，仍复用这次偏移。
		const auto ox = (_MyRandom.Next() - 0.5) * 0.5, oy = (_MyRandom.Next() - 0.5) * 0.5;
		const auto aim = [&](int _key)
		{
			_unit.MyInducedTile = _key;
			_unit.MyInducedGoal = WorldPoint{.MyX = _key % FieldColumns + ox, .MyY = _key / FieldColumns + oy};
			return PlanFear(_unit);
		};
		if (!aim(key)) (void)aim(own);
	}

	void BattleCore::MoveFeared(CombatUnit& _unit)
	{
		if (!_MyGrid) _MyGrid.emplace();
		const auto sequence = _unit.MyStatuses.MyFear ? _unit.MyStatuses.MyFear->MySequence : 0;
		if (_unit.MyInducedMode != CombatStatus::FEAR || _unit.MyFearSequence != sequence)
		{
			_unit.MyInducedMode = CombatStatus::FEAR;
			_unit.MyFearSequence = sequence;
			_unit.MyInducedPath.clear();
			BuildFearTiles(_unit);
		}
		if (!_unit.MyInducedPath.empty() && (_unit.MyInducedVersion != _MyGrid->Version() || Moved(_unit.MyInducedLast, _unit.MyPosition))) (void)PlanFear(_unit);
		const auto& steps = SpawnDefinition(_unit.MySpawnIndex).MyRoute.MySteps;
		if (_unit.MyRouteIndex < steps.size() && steps[_unit.MyRouteIndex].MyKind == RouteStepKind::WAIT)
		{
			if (!_unit.MyWaitLeft) _unit.MyWaitLeft = steps[_unit.MyRouteIndex].MyWaitSeconds;
			*_unit.MyWaitLeft -= BattleClock::StepSeconds;
			if (std::islessequal(*_unit.MyWaitLeft, 1e-9)) { _unit.MyWaitLeft.reset(); ++_unit.MyRouteIndex; }
		}
		_unit.MyPlannedRoute = std::numeric_limits<std::size_t>::max();
		auto travel = _unit.MyStats.MyMoveSpeed * 0.5 * BattleClock::StepSeconds;
		bool moved = false;
		for (int guard = 8; guard > 0 && std::isgreater(travel, 1e-9); --guard)
		{
			if (_unit.MyInducedPath.empty() || _unit.MyInducedPoint >= _unit.MyInducedPath.size()) PickFearPoint(_unit);
			const auto point = _unit.MyInducedPath[_unit.MyInducedPoint];
			const auto distance = Distance(point, _unit.MyPosition);
			if (std::islessequal(distance, travel)) { _unit.MyPosition = point; travel -= distance; ++_unit.MyInducedPoint; }
			else
			{
				_unit.MyPosition.MyX += (point.MyX - _unit.MyPosition.MyX) / distance * travel;
				_unit.MyPosition.MyY += (point.MyY - _unit.MyPosition.MyY) / distance * travel;
				travel = 0;
			}
			moved = moved || std::isgreater(distance, 1e-9);
		}
		_unit.MyInducedLast = _unit.MyPosition;
		_unit.MyMoving = moved;
	}
}
