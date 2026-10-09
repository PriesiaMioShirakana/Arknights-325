#include <cmath>
#include <iomanip>
#include <iostream>
#include <stronghold/simulation/field.hpp>

namespace
{
	using namespace Stronghold;

	template <class _Range>
	void Array(const _Range& _values)
	{
		std::cout << '[';
		bool first = true;
		for (const auto value : _values) { if (!first) std::cout << ','; first = false; std::cout << value; }
		std::cout << ']';
	}
}

int main()
{
	try
	{
		using namespace Stronghold;
		unsigned cases{};
		std::cin >> cases;
		std::cout << std::setprecision(17) << '[';
		for (unsigned caseIndex = 0; caseIndex < cases; ++caseIndex)
		{
			FieldDefinition definition;
			auto& rect = definition.MyRect;
			std::cin >> rect.MyFirstRow >> rect.MyLastRow >> rect.MyFirstColumn >> rect.MyLastColumn;
			for (auto& tile : definition.MyTiles)
			{
				unsigned flags{};
				std::cin >> flags;
				tile = FieldTile{.MyWalkable = (flags & 1) != 0, .MyFlyable = (flags & 2) != 0, .MyLow = (flags & 4) != 0, .MyBuild = static_cast<FieldBuild>(flags >> 3)};
			}
			FieldGrid grid(std::move(definition));
			unsigned queries{};
			std::cin >> queries;
			if (caseIndex) std::cout << ',';
			std::cout << '[';
			for (unsigned query = 0; query < queries; ++query)
			{
				int row{}, column{}, diagonal{}, ignore{}, blockRow{}, blockColumn{}, kind{}, enabled{};
				std::cin >> row >> column >> diagonal >> ignore >> blockRow >> blockColumn >> kind >> enabled;
				grid.SetObstacle(blockRow, blockColumn, enabled != 0, kind == 1 ? ObstacleKind::BLOCK : ObstacleKind::CRATE);
				const auto& field = grid.Flow(row, column, diagonal != 0, ignore != 0);
				if (query) std::cout << ',';
				std::cout << '[' << field.MyDestination << ',';
				Array(field.MyDistance); std::cout << ','; Array(field.MyParent); std::cout << ','; Array(field.MyPenalty);
				std::cout << ','; Array(field.MyNext); std::cout << ','; Array(field.MyOfficial); std::cout << ','; Array(field.MyCost);
				std::cout << ",[";
				for (int key = 0; key < FieldTiles; ++key)
				{
					if (key) std::cout << ',';
					const auto length = FieldGrid::Length(field, key);
					if (std::isfinite(length)) std::cout << length; else std::cout << "null";
				}
				std::cout << "]]";
			}
			std::cout << ']';
		}
		std::cout << ']';
		if (!std::cin) throw std::runtime_error("invalid field probe input");
		return 0;
	}
	catch (const std::exception& _error) { std::cerr << _error.what() << '\n'; return 1; }
}
