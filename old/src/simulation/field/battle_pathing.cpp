#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::SetObstacle(int _row, int _column, bool _enabled, ObstacleKind _kind)
	{
		if (!_MyGrid) throw std::logic_error("obstacle requires a field definition");
		if (Finished()) return;
		_MyGrid->SetObstacle(_row, _column, _enabled, _kind);
		for (std::size_t i = 0; i < _MyAllyIds.size(); ++i)
		{
			auto& unit = _MyUnits[Index(_MyAllyIds[i])];
			unit.MyGroundPassable = _MyGrid->GroundPassable(static_cast<int>(unit.MyPosition.MyY), static_cast<int>(unit.MyPosition.MyX), true);
			if (!unit.MyGroundPassable) ReleaseBlocked(unit);
		}
	}

	void BattleCore::PlanRoute(CombatUnit& _unit, WorldPoint _destination)
	{
		auto& path = _unit.MyPath;
		path.clear();
		_unit.MyPathPoint = 0;
		_unit.MyPlannedRoute = _unit.MyRouteIndex;
		_unit.MyPathVersion = _MyGrid ? _MyGrid->Version() : 0;
		if (_MyGrid && (!_unit.MyDefinition.MyFlying || _unit.MyDefinition.MyWalksWhileFlying))
		{
			const auto row = static_cast<int>(std::floor(_unit.MyPosition.MyY + 0.5));
			const auto column = static_cast<int>(std::floor(_unit.MyPosition.MyX + 0.5));
			if (FieldGrid::InBounds(row, column))
			{
				const auto key = FieldGrid::Key(row, column);
				const auto endRow = static_cast<int>(_destination.MyY), endColumn = static_cast<int>(_destination.MyX);
				const auto& normal = _MyGrid->Flow(endRow, endColumn);
				const auto& field = normal.MyDistance[static_cast<std::size_t>(key)] >= 0 ? normal : _MyGrid->Flow(endRow, endColumn, true, true);
				if (field.MyDistance[static_cast<std::size_t>(key)] >= 0)
				{
					path.reserve(FieldTiles);
					auto cursor = key;
					while (cursor != field.MyDestination && path.size() < FieldTiles)
					{
						cursor = field.MyNext[static_cast<std::size_t>(cursor)];
						if (cursor < 0) break;
						path.emplace_back(static_cast<double>(cursor % FieldColumns), static_cast<double>(cursor / FieldColumns));
					}
					if (path.empty()) path.emplace_back(_destination);
					const auto first = FieldPoint{.MyRow = static_cast<int>(path.front().MyY), .MyColumn = static_cast<int>(path.front().MyX)};
					if (!_MyGrid->StraightClear(_unit.MyPosition.MyX, _unit.MyPosition.MyY, first))
						path.emplace(path.begin(), static_cast<double>(column), static_cast<double>(row));
				}
			}
		}
		if (path.empty() || std::islessgreater(path.back().MyX, _destination.MyX) || std::islessgreater(path.back().MyY, _destination.MyY)) path.emplace_back(_destination);
		// 路点后缀缓存让索敌的剩余路程查询只需一次 hypot，而不逐帧重走路径。
		_unit.MyPathSuffix.assign(path.size(), 0);
		for (std::size_t i = path.size() - 1; i > 0; --i)
			_unit.MyPathSuffix[i - 1] = _unit.MyPathSuffix[i] + Distance(path[i - 1], path[i]);
	}

	void BattleCore::RebuildRouteTail(std::size_t _spawn) const
	{
		if (!_MyGrid || _MyRouteTailVersions[_spawn] == _MyGrid->Version()) return;
		const auto& spawn = SpawnDefinition(_spawn);
		const auto& route = spawn.MyRoute;
		auto& tail = _MyRouteTails[_spawn];
		std::ranges::fill(tail, 0);
		auto position = route.MyStart;
		for (std::size_t i = 0; i <= route.MySteps.size(); ++i)
		{
			const auto step = i == route.MySteps.size() ? RouteStep{.MyPosition = route.MyEnd} : route.MySteps[i];
			if (step.MyKind == RouteStepKind::MOVE)
			{
				tail[i] = Distance(position, step.MyPosition);
				const auto row = static_cast<int>(std::floor(position.MyY + 0.5)), column = static_cast<int>(std::floor(position.MyX + 0.5));
				if ((!spawn.MyDefinition.MyFlying || spawn.MyDefinition.MyWalksWhileFlying) && FieldGrid::InBounds(row, column))
				{
					const auto key = FieldGrid::Key(row, column);
					const auto r = static_cast<int>(step.MyPosition.MyY), c = static_cast<int>(step.MyPosition.MyX);
					const auto& normal = _MyGrid->Flow(r, c);
					const auto& field = normal.MyDistance[static_cast<std::size_t>(key)] >= 0 ? normal : _MyGrid->Flow(r, c, true, true);
					const auto length = FieldGrid::Length(field, key);
					if (std::isfinite(length)) tail[i] = length;
				}
			}
			if (step.MyKind == RouteStepKind::MOVE || step.MyKind == RouteStepKind::APPEAR) position = step.MyPosition;
		}
		for (std::size_t i = tail.size() - 1; i > 0; --i) tail[i - 1] += tail[i];
		_MyRouteTailVersions[_spawn] = _MyGrid->Version();
	}
}
