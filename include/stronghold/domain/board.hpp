#ifndef STRONGHOLD_DOMAIN_BOARD_HPP
#define STRONGHOLD_DOMAIN_BOARD_HPP
#include <array>
#include <cstddef>
#include <stdexcept>

namespace Stronghold
{
	enum class Facing
	{
		UP,
		RIGHT,
		DOWN,
		LEFT
	};

	enum class Terrain
	{
		BLOCKED,
		GROUND,
		HIGH
	};

	enum class PlacementClass
	{
		ANY,
		MELEE,
		HIGH_ONLY
	};

	enum class DeployField
	{
		NORMAL,
		BOSS_LEFT,
		BOSS_RIGHT
	};

	struct BoardPosition
	{
		int MyRow{};
		int MyColumn{};

		[[nodiscard]] constexpr bool InField() const noexcept
		{ return MyRow >= 9 && MyRow <= 12 && MyColumn >= 2 && MyColumn <= 10; }

		// Reading order: descending row, ascending column (distinct from merge deployment order).
		[[nodiscard]] constexpr std::size_t Index() const
		{
			if (!InField())
				throw std::out_of_range("board position out of range");
			return static_cast<std::size_t>((12 - MyRow) * 9 + MyColumn - 2);
		}

		[[nodiscard]] static constexpr BoardPosition FromIndex(std::size_t _index)
		{
			if (_index >= 36)
				throw std::out_of_range("board index out of range");
			return {12 - static_cast<int>(_index / 9), 2 + static_cast<int>(_index % 9)};
		}

		bool operator==(const BoardPosition&) const = default;
	};

	struct BoardLayout
	{
		std::array<Terrain, 36> MyTiles{};

		[[nodiscard]] constexpr bool CanPlace(PlacementClass _placement, BoardPosition _position) const noexcept
		{
			if (!_position.InField())
				return false;
			const auto terrain = MyTiles[_position.Index()];
			if (terrain != Terrain::GROUND && terrain != Terrain::HIGH)
				return false;
			switch (_placement)
			{
			case PlacementClass::MELEE:
				return terrain == Terrain::GROUND;
			case PlacementClass::HIGH_ONLY:
				return terrain == Terrain::HIGH;
			case PlacementClass::ANY:
				return true;
			}
			return false;
		}

		// Same explicitly degraded/test layout as upstream buildDeployMap(null).
		[[nodiscard]] static constexpr BoardLayout Fallback() noexcept
		{
			BoardLayout layout;
			for (std::size_t i = 0; i < layout.MyTiles.size(); ++i)
				layout.MyTiles[i] = BoardPosition::FromIndex(i).MyColumn == 9 ? Terrain::BLOCKED : Terrain::GROUND;
			return layout;
		}
	};

	struct StageBoards
	{
		BoardLayout MyNormal;
		BoardLayout MyBossLeft;
		BoardLayout MyBossRight;

		[[nodiscard]] const BoardLayout& At(DeployField _field) const
		{
			switch (_field)
			{
			case DeployField::NORMAL:
				return MyNormal;
			case DeployField::BOSS_LEFT:
				return MyBossLeft;
			case DeployField::BOSS_RIGHT:
				return MyBossRight;
			}
			throw std::invalid_argument("invalid deploy field");
		}
	};
} // namespace Stronghold
#endif
