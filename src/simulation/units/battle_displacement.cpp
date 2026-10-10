#include <array>
#include <numbers>
#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		// 对照 battle/displacement.js：按 JS Math.round（负半数向 +∞）求受力等级。
		// 先限制浮点范围再转数组下标，极大或非有限外部输入不会产生越界转换。
		constexpr std::array PushTiles{0.12, 0.44, 1.7, 2.14, 2.96, 3.53};
		constexpr std::array EffectTiles{0.085, 0.374, 1.562, 1.987, 2.773, 3.331};
		constexpr double StopRadius = 0.6708;

		double Finite(double _value, double _fallback = 0) noexcept
		{ return std::isfinite(_value) ? _value : _fallback; }

		double ForceLevel(const CombatUnit& _unit, double _force) noexcept
		{ return std::floor(Finite(_force) + 0.5) - _unit.MyStats.MyMass; }

		double PushLength(double _level, bool _effect) noexcept
		{
			const auto level = std::floor(Finite(_level, -99) + 0.5);
			if (std::islessequal(level, -3)) return 0;
			const auto index = static_cast<std::size_t>(std::min(3.0, level) + 2);
			return (_effect ? EffectTiles : PushTiles)[index];
		}

		WorldPoint Forward(Facing _facing) noexcept
		{
			const auto offset = RotateOffset(RangeOffset{.MyColumn = 1}, _facing);
			return WorldPoint{.MyX = static_cast<double>(offset.MyColumn), .MyY = static_cast<double>(offset.MyRow)};
		}
	}

	bool BattleCore::Displaceable(const CombatUnit& _unit) const noexcept
	{
		return _MyStarted && !Finished() && _unit.MyAlive && _unit.MySide == UnitSide::ENEMY &&
			!_unit.MyDefinition.MySharedBoss && !_unit.MyDefinition.MyStaticBody &&
			!_unit.MyStatuses.Has(CombatStatus::NO_DISPLACE);
	}

	double BattleCore::PushDistance(UnitId _enemy, double _force, bool _effect) const
	{
		const auto& enemy = Unit(_enemy);
		return Displaceable(enemy) ? PushLength(ForceLevel(enemy, _force), _effect) : 0;
	}

	double BattleCore::Push(UnitId _enemy, double _force, const PushOptions& _options)
	{
		const auto& enemy = Unit(_enemy);
		if (!Displaceable(enemy)) return 0;
		auto level = ForceLevel(enemy, _force);
		const auto origin = _options.MyFrom.value_or(enemy.MyPosition);
		const auto vx = enemy.MyPosition.MyX - Finite(origin.MyX, enemy.MyPosition.MyX);
		const auto vy = enemy.MyPosition.MyY - Finite(origin.MyY, enemy.MyPosition.MyY);
		const auto distance = std::hypot(vx, vy);
		WorldPoint direction{.MyX = Finite(_options.MyDirection.MyX), .MyY = Finite(_options.MyDirection.MyY)};
		const auto length = std::hypot(direction.MyX, direction.MyY);
		if (std::isgreater(length, 0))
		{
			direction.MyX /= length;
			direction.MyY /= length;
			if (_options.MyFrom && !_options.MyFixed && (std::isless(distance, 0.25) ||
				(!_options.MyFixedAngle && std::isless(vx * direction.MyX + vy * direction.MyY, distance * std::numbers::sqrt2 / 2))))
			{
				level -= 2;
				if (std::isgreater(distance, 1e-6)) direction = WorldPoint{.MyX = vx / distance, .MyY = vy / distance};
			}
		}
		else if (std::isgreater(distance, 1e-6)) direction = WorldPoint{.MyX = vx / distance, .MyY = vy / distance};
		else if (!_options.MyInward && _options.MyFrom && _options.MyFromFacing) direction = Forward(*_options.MyFromFacing);
		else return 0;
		auto travel = PushLength(level, _options.MyEffect);
		if (_options.MyInward && !std::isgreater(length, 0))
		{
			direction.MyX = -direction.MyX;
			direction.MyY = -direction.MyY;
			travel = std::min(travel, std::max(0.0, distance - StopRadius));
		}
		return Displace(_enemy, direction, travel);
	}

	double BattleCore::Pull(UnitId _enemy, double _force, const PullOptions& _options)
	{
		const auto& enemy = Unit(_enemy);
		if (_options.MyCenterUnit) (void)Index(_options.MyCenterUnit);
		if (!Displaceable(enemy)) return 0;
		if (_options.MyCenterUnit && Unit(_options.MyCenterUnit).MySide == UnitSide::ALLY &&
			enemy.MyBlockedBy == _options.MyCenterUnit) return 0;
		const WorldPoint destination{.MyX = Finite(_options.MyTo.MyX, enemy.MyPosition.MyX),
			.MyY = Finite(_options.MyTo.MyY, enemy.MyPosition.MyY)};
		const auto dx = destination.MyX - enemy.MyPosition.MyX, dy = destination.MyY - enemy.MyPosition.MyY;
		const auto distance = std::hypot(dx, dy);
		if (!std::isgreater(distance, 1e-6)) return 0;
		const WorldPoint direction{.MyX = dx / distance, .MyY = dy / distance};
		const auto center = _options.MyCenterUnit ? Unit(_options.MyCenterUnit).MyPosition : _options.MyCenter.value_or(destination);
		const auto radius = std::max(0.0, Finite(_options.MyStopRadius));
		const auto wx = enemy.MyPosition.MyX - Finite(center.MyX, destination.MyX);
		const auto wy = enemy.MyPosition.MyY - Finite(center.MyY, destination.MyY);
		const auto dot = wx * direction.MyX + wy * direction.MyY, squared = wx * wx + wy * wy;
		auto full = distance;
		// 取射线与急停圆的较小非负交点。偏离圆的射线仍以目标点为终点。
		if (std::islessequal(squared, radius * radius)) full = 0;
		else
		{
			const auto discriminant = dot * dot - squared + radius * radius;
			if (std::isgreaterequal(discriminant, 0))
			{
				const auto root = -dot - std::sqrt(discriminant);
				if (std::isgreaterequal(root, 0)) full = std::min(full, root);
			}
		}
		const auto level = ForceLevel(enemy, _force);
		const auto travel = std::isgreaterequal(level, 0) ? full : !std::islessgreater(level, -1) ? std::min(full, 0.35 * distance) :
			!std::islessgreater(level, -2) ? std::min(full, 0.03) : 0;
		return std::isgreater(travel, 1e-6) ? Displace(_enemy, direction, travel) : 0;
	}

	double BattleCore::PullToFront(UnitId _enemy, UnitId _source, double _force)
	{
		const auto& source = Unit(_source);
		const auto direction = Forward(source.MyFacing);
		return Pull(_enemy, _force, PullOptions{.MyTo = WorldPoint{
			.MyX = source.MyPosition.MyX + direction.MyX * 0.5, .MyY = source.MyPosition.MyY + direction.MyY * 0.5},
			.MyStopRadius = StopRadius, .MyCenterUnit = _source});
	}

	double BattleCore::Displace(UnitId _enemy, WorldPoint _direction, double _distance)
	{
		auto& enemy = _MyUnits[Index(_enemy)];
		if (!Displaceable(enemy)) return 0;
		const auto dx = Finite(_direction.MyX), dy = Finite(_direction.MyY), length = std::hypot(dx, dy);
		const auto distance = std::min(Finite(_distance), 2.0 * FieldColumns);
		if (!std::isgreater(length, 0) || !std::isgreater(distance, 0)) return 0;
		if (!_MyGrid) _MyGrid.emplace();
		const auto ux = dx / length, uy = dy / length;
		const bool air = enemy.MyDefinition.MyFlying && !enemy.MyDefinition.MyWalksWhileFlying;
		double moved = 0;
		// 0.1 格离散检测是原规则的一部分：不能直接线段求交，否则临墙终点会改变。
		while (std::isless(moved + 1e-9, distance))
		{
			const auto step = std::min(0.1, distance - moved);
			const WorldPoint next{.MyX = enemy.MyPosition.MyX + ux * step, .MyY = enemy.MyPosition.MyY + uy * step};
			const auto row = static_cast<int>(std::floor(next.MyY + 0.5)), column = static_cast<int>(std::floor(next.MyX + 0.5));
			if (!(air ? _MyGrid->InRect(row, column) : _MyGrid->GroundPassable(row, column))) break;
			enemy.MyPosition = next;
			moved += step;
		}
		if (std::isgreater(moved, 0))
		{
			ReleaseBlock(enemy);
			enemy.MyAttackStandUntil = -std::numeric_limits<double>::infinity();
			enemy.MyPlannedRoute = std::numeric_limits<std::size_t>::max();
		}
		return moved;
	}
}
