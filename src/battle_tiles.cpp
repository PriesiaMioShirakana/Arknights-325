#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		bool SameTile(WorldPoint _left, WorldPoint _right) noexcept
		{ return !std::islessgreater(_left.MyX, _right.MyX) && !std::islessgreater(_left.MyY, _right.MyY); }
	}

	bool Battle::IsDown(UnitId _unit) const
	{
		const auto& unit = Unit(_unit);
		return unit.MySide == UnitSide::ALLY && unit.MyKind == UnitKind::OPERATOR && !unit.MyAlive && !unit.MyRemoved &&
			unit.MyDeploySequence && std::isfinite(unit.MyRespawnAt);
	}

	WorldPoint Battle::RestPosition(UnitId _unit) const
	{
		const auto& unit = Unit(_unit);
		return IsDown(_unit) ? unit.MyBody.value_or(unit.MyPosition) : unit.MyHome;
	}

	bool Battle::ReservedTile(WorldPoint _position) const
	{
		if (!CanDeploy(_position)) return true;
		for (const auto id : _MyAllyIds)
		{
			const auto& unit = Unit(id);
			if (!unit.MyAlive && !unit.MyRemoved && (unit.MyKind == UnitKind::OPERATOR || unit.MyKind == UnitKind::TOKEN) &&
				SameTile(RestPosition(id), _position)) return true;
		}
		return false;
	}

	const std::bitset<FieldTiles>& Battle::GroundPathTiles()
	{
		if (!_MyGrid) _MyGrid.emplace();
		if (_MyGroundPathVersion == _MyGrid->Version()) return _MyGroundPathMask;
		_MyGroundPathMask.reset();
		const auto& rect = _MyGrid->Rect();
		const auto clampPoint = [&](WorldPoint _point)
		{
			return FieldPoint{.MyRow = std::clamp(static_cast<int>(std::floor(_point.MyY + 0.5)), rect.MyFirstRow, rect.MyLastRow),
				.MyColumn = std::clamp(static_cast<int>(std::floor(_point.MyX + 0.5)), rect.MyFirstColumn, rect.MyLastColumn)};
		};
		// 复用战斗的寻路输出缓冲；这里没有内容回调，缓存重建不可能与路径更新重入。
		for (const auto& route : _MyInput.MyGroundRoutes)
		{
			auto previous = clampPoint(route.MyStart);
			const auto walk = [&](WorldPoint _point)
			{
				const auto next = clampPoint(_point);
				if (_MyGrid->Waypoints(previous, next, _MyFieldWaypoints) ||
					_MyGrid->Waypoints(previous, next, _MyFieldWaypoints, true, true))
				{
					// findPath 在原版中进一步展开平滑折线；Bresenham 的严格分支决定斜线经过哪一侧格子。
					_MyGroundPathMask.set(static_cast<std::size_t>(FieldGrid::Key(previous.MyRow, previous.MyColumn)));
					for (std::size_t i = 1; i < _MyFieldWaypoints.size(); ++i)
					{
						auto point = _MyFieldWaypoints[i - 1];
						const auto end = _MyFieldWaypoints[i];
						const int dx = std::abs(end.MyColumn - point.MyColumn), dy = std::abs(end.MyRow - point.MyRow);
						const int sx = point.MyColumn < end.MyColumn ? 1 : -1, sy = point.MyRow < end.MyRow ? 1 : -1;
						int error = dx - dy;
						while (point.MyColumn != end.MyColumn || point.MyRow != end.MyRow)
						{
							const int twice = error * 2;
							if (twice > -dy) { error -= dy; point.MyColumn += sx; }
							if (twice < dx) { error += dx; point.MyRow += sy; }
							_MyGroundPathMask.set(static_cast<std::size_t>(FieldGrid::Key(point.MyRow, point.MyColumn)));
						}
					}
				}
				previous = next;
			};
			for (const auto& step : route.MySteps)
				if (step.MyKind == RouteStepKind::MOVE || step.MyKind == RouteStepKind::APPEAR) walk(step.MyPosition);
			walk(route.MyEnd);
		}
		_MyGroundPathVersion = _MyGrid->Version();
		return _MyGroundPathMask;
	}

	std::optional<WorldPoint> Battle::FindTacticalPoint(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		const auto& paths = GroundPathTiles();
		const auto forward = RotateOffset(RangeOffset{.MyColumn = 1}, unit.MyFacing);
		std::optional<WorldPoint> best;
		int bestPriority = 2, bestRow = 0, bestColumn = 0;
		double bestDistance = std::numeric_limits<double>::infinity();
		for (int key = 0; key < FieldTiles; ++key)
		{
			if (!unit.MyBaseRangeMask.test(static_cast<std::size_t>(key))) continue;
			const int row = key / FieldColumns, column = key % FieldColumns;
			const WorldPoint point{.MyX = static_cast<double>(column), .MyY = static_cast<double>(row)};
			if (!_MyGrid->GroundPassable(row, column) || !_MyGrid->CanStand(row, column) || ReservedTile(point)) continue;
			const int dy = row - static_cast<int>(unit.MyPosition.MyY), dx = column - static_cast<int>(unit.MyPosition.MyX);
			const int localRow = dy * forward.MyColumn - dx * forward.MyRow;
			const int localColumn = dy * forward.MyRow + dx * forward.MyColumn;
			const double distance = std::max(std::abs(dy), std::abs(dx)) + 0.01 * std::abs(localRow);
			if (!std::isgreater(distance, 0)) continue;
			const int priority = paths.test(static_cast<std::size_t>(key)) ? 0 : 1;
			if (priority < bestPriority || (priority == bestPriority && (std::isless(distance, bestDistance - 1e-9) ||
				(std::islessequal(std::abs(distance - bestDistance), 1e-9) &&
					(localRow < bestRow || (localRow == bestRow && localColumn < bestColumn))))))
			{
				best = point;
				bestPriority = priority;
				bestDistance = distance;
				bestRow = localRow;
				bestColumn = localColumn;
			}
		}
		return best;
	}

	void Battle::LayBody(CombatUnit& _unit)
	{
		_unit.MyBody = _unit.MyPosition;
		if (SameTile(_unit.MyPosition, _unit.MyHome)) return;
		// 倒在其它棋子初始位置时尝试回自己的初始位置；已移除棋子的初始位置也参与判定。
		const bool anotherHome = std::ranges::any_of(_MyAllyIds, [&](UnitId _id)
		{
			const auto& ally = Unit(_id);
			return _id != _unit.MyId && ally.MyPieceUid && (ally.MyKind == UnitKind::OPERATOR || ally.MyKind == UnitKind::TOKEN) &&
				SameTile(ally.MyHome, _unit.MyPosition);
		});
		if ((_unit.MyDownAtHome || anotherHome) && !ReservedTile(_unit.MyHome)) _unit.MyBody = _unit.MyHome;
	}

	void Battle::SetDownAtHome(UnitId _unit, bool _enabled)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_MyStarted && !Finished() && unit.MyAlive && unit.MyKind == UnitKind::OPERATOR) unit.MyDownAtHome = _enabled;
	}

	bool Battle::Relocate(UnitId _unit, WorldPoint _position)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (!_MyStarted || Finished() || !unit.MyAlive || unit.MySide != UnitSide::ALLY || !CanDeploy(_position, _unit)) return false;
		ReleaseBlocked(unit);
		if (unit.MyObstacle) SetObstacle(static_cast<int>(unit.MyPosition.MyY), static_cast<int>(unit.MyPosition.MyX), false, unit.MyObstacleKind);
		unit.MyPosition = _position;
		if (_MyGrid)
		{
			const auto tile = _MyGrid->Tile(static_cast<int>(_position.MyY), static_cast<int>(_position.MyX));
			unit.MyGround = tile.MyLow && !tile.MyElevated;
		}
		else unit.MyGround = true;
		unit.MyGroundPassable = !_MyGrid || _MyGrid->GroundPassable(static_cast<int>(_position.MyY), static_cast<int>(_position.MyX), true);
		if (unit.MyObstacle) SetObstacle(static_cast<int>(_position.MyY), static_cast<int>(_position.MyX), true, unit.MyObstacleKind);
		RefreshRange(unit);
		return true;
	}

	bool Battle::MoveRedeploy(UnitId _unit, WorldPoint _position, bool _clearSp)
	{
		if (!Relocate(_unit, _position)) return false;
		auto& unit = _MyUnits[Index(_unit)];
		unit.MyDeploySequence = ++_MyDeploySequence;
		unit.MyAggroSequence = unit.MyDeploySequence;
		auto& skill = unit.MySkill;
		const auto kind = unit.MyDefinition.MySkill.MyKind;
		// 移动重部署保留 HP、Buff 与技能进度，只有非持续活动技能的 SP 可按要求清空。
		if (_clearSp && kind != SkillKind::NONE && kind != SkillKind::PASSIVE && !(skill.MyActive && IsTimedSkill(kind)))
		{
			skill.MySp = 0;
			skill.MyCharges = std::islessequal(SpCost(_unit), 0) ? unit.MyDefinition.MySkill.MyMaxCharges : 0;
		}
		Emit(BattleEventKind::DEPLOYED, _unit);
		if (_MyGrid)
		{
			const auto tile = _MyGrid->Tile(static_cast<int>(_position.MyY), static_cast<int>(_position.MyX));
			if (tile.MyDeploymentElevation) unit.MyGround = tile.MyLow && !*tile.MyDeploymentElevation;
		}
		RefreshTerrain(unit);
		ContentEvent event{.MyKind = ContentEventKind::DEPLOY, .MyUnit = _unit, .MyMove = true};
		NotifyContent(event);
		return true;
	}
}
