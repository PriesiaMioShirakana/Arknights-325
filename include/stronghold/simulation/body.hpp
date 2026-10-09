#ifndef STRONGHOLD_SIMULATION_BODY_HPP
#define STRONGHOLD_SIMULATION_BODY_HPP
#include <stronghold/simulation/combat_types.hpp>

namespace Stronghold
{
	struct BodyRectangle
	{
		double MyLeft{};
		double MyRight{};
		double MyBottom{};
		double MyTop{};
	};

	// 纯几何：受击区域不改变阻挡、移动和弹道落点；溅射中心判定仍使用单位位置。
	[[nodiscard]] inline BodyRectangle HitRectangle(const CombatUnit& _unit) noexcept
	{
		const auto area = _unit.MyDefinition.MyHitArea.value_or(HitArea{});
		const auto x = _unit.MyPosition.MyX + area.MyOffsetX, y = _unit.MyPosition.MyY + area.MyOffsetY;
		return BodyRectangle{.MyLeft = x - area.MyWidth / 2, .MyRight = x + area.MyWidth / 2,
			.MyBottom = y - area.MyHeight / 2, .MyTop = y + area.MyHeight / 2};
	}

	[[nodiscard]] inline FieldRect BodyTiles(const CombatUnit& _unit) noexcept
	{
		if (!_unit.MyDefinition.MyHitArea)
		{
			const auto y = std::floor(_unit.MyPosition.MyY + 0.5), x = std::floor(_unit.MyPosition.MyX + 0.5);
			if (!std::isgreaterequal(y, 0) || !std::isless(y, FieldRows) || !std::isgreaterequal(x, 0) || !std::isless(x, FieldColumns)) return FieldRect{.MyFirstRow = 1, .MyLastRow = 0};
			const auto row = static_cast<int>(y), column = static_cast<int>(x);
			return FieldRect{.MyFirstRow = row, .MyLastRow = row, .MyFirstColumn = column, .MyLastColumn = column};
		}
		const auto box = HitRectangle(_unit);
		// 开区间相交：只触到格边不算占格。先钳制再转整数，极大合法尺寸也不溢出。
		return FieldRect{
			.MyFirstRow = static_cast<int>(std::clamp(std::floor(box.MyBottom + 0.5 + 1e-9), 0.0, static_cast<double>(FieldRows))),
			.MyLastRow = static_cast<int>(std::clamp(std::ceil(box.MyTop - 0.5 - 1e-9), -1.0, static_cast<double>(FieldRows - 1))),
			.MyFirstColumn = static_cast<int>(std::clamp(std::floor(box.MyLeft + 0.5 + 1e-9), 0.0, static_cast<double>(FieldColumns))),
			.MyLastColumn = static_cast<int>(std::clamp(std::ceil(box.MyRight - 0.5 - 1e-9), -1.0, static_cast<double>(FieldColumns - 1)))};
	}

	[[nodiscard]] inline bool BodyInRange(const CombatUnit& _unit, const std::bitset<FieldTiles>& _mask) noexcept
	{
		const auto tiles = BodyTiles(_unit);
		for (int row = tiles.MyFirstRow; row <= tiles.MyLastRow; ++row)
			for (int column = tiles.MyFirstColumn; column <= tiles.MyLastColumn; ++column)
				if (_mask[static_cast<std::size_t>(FieldGrid::Key(row, column))]) return true;
		return false;
	}

	[[nodiscard]] inline bool BodyOnTile(const CombatUnit& _unit, int _row, int _column) noexcept
	{
		if (!_unit.MyDefinition.MyHitArea) return !std::islessgreater(std::floor(_unit.MyPosition.MyY + 0.5), _row) &&
			!std::islessgreater(std::floor(_unit.MyPosition.MyX + 0.5), _column);
		const auto box = HitRectangle(_unit);
		return std::isgreater(_column + 0.5, box.MyLeft + 1e-9) && std::isless(_column - 0.5, box.MyRight - 1e-9) &&
			std::isgreater(_row + 0.5, box.MyBottom + 1e-9) && std::isless(_row - 0.5, box.MyTop - 1e-9);
	}

	[[nodiscard]] inline double BodyDistance(const CombatUnit& _unit, WorldPoint _point) noexcept
	{
		if (!_unit.MyDefinition.MyHitArea) return Distance(_unit.MyPosition, _point);
		const auto box = HitRectangle(_unit);
		const auto dx = std::isless(_point.MyX, box.MyLeft) ? box.MyLeft - _point.MyX : std::isgreater(_point.MyX, box.MyRight) ? _point.MyX - box.MyRight : 0;
		const auto dy = std::isless(_point.MyY, box.MyBottom) ? box.MyBottom - _point.MyY : std::isgreater(_point.MyY, box.MyTop) ? _point.MyY - box.MyTop : 0;
		return std::hypot(dx, dy);
	}

	[[nodiscard]] inline double BodyTileReach(const CombatUnit& _unit, int _row, int _column) noexcept
	{
		if (!_unit.MyDefinition.MyHitArea) return std::max(std::abs(std::floor(_unit.MyPosition.MyY + 0.5) - _row), std::abs(std::floor(_unit.MyPosition.MyX + 0.5) - _column));
		const auto tiles = BodyTiles(_unit);
		if (tiles.MyFirstRow > tiles.MyLastRow || tiles.MyFirstColumn > tiles.MyLastColumn) return std::numeric_limits<double>::infinity();
		// 矩形占格连续，最近格的切比雪夫距离无需枚举整个区域。
		return std::max({0.0, static_cast<double>(tiles.MyFirstRow) - _row, static_cast<double>(_row) - tiles.MyLastRow,
			static_cast<double>(tiles.MyFirstColumn) - _column, static_cast<double>(_column) - tiles.MyLastColumn});
	}
}
#endif
