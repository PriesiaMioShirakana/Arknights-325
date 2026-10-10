#include <iomanip>
#include <iostream>
#include <stronghold/simulation/body.hpp>

// 输出几何原语，JS 对照独立覆盖全部 399 格和边界点。
int main()
{
	using namespace Stronghold;
	unsigned cases{};
	std::cin >> cases;
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < cases; ++scene)
	{
		CombatUnit unit;
		HitArea area;
		WorldPoint point;
		int row{}, column{};
		std::cin >> unit.MyPosition.MyX >> unit.MyPosition.MyY >> area.MyWidth >> area.MyHeight >> area.MyOffsetX >> area.MyOffsetY
			>> point.MyX >> point.MyY >> row >> column;
		if (std::isgreater(area.MyWidth, 0)) unit.MyDefinition.MyHitArea = area;
		if (scene) std::cout << ',';
		std::cout << '[' << BodyDistance(unit, point) << ',';
		const auto reach = BodyTileReach(unit, row, column);
		if (std::isfinite(reach)) std::cout << reach; else std::cout << "null";
		std::cout << ',' << BodyOnTile(unit, row, column) << ",[";
		const auto tiles = BodyTiles(unit);
		bool first = true;
		std::bitset<FieldTiles> mask;
		for (int r = tiles.MyFirstRow; r <= tiles.MyLastRow; ++r)
			for (int c = tiles.MyFirstColumn; c <= tiles.MyLastColumn; ++c)
			{
				if (!first) std::cout << ',';
				first = false;
				const auto key = FieldGrid::Key(r, c);
				std::cout << key;
				mask.set(static_cast<std::size_t>(key));
			}
		std::cout << "]," << BodyInRange(unit, mask) << ']';
	}
	std::cout << ']';
	return std::cin ? 0 : 1;
}
