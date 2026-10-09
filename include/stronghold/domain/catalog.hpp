#ifndef STRONGHOLD_DOMAIN_CATALOG_HPP
#define STRONGHOLD_DOMAIN_CATALOG_HPP
#include <algorithm>
#include <array>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <stronghold/domain/board.hpp>
#include <vector>

namespace Stronghold
{
	enum class PieceKind
	{
		CHESS,
		ITEM
	};

	enum class ItemUse { EQUIPMENT, ART, CONSUME_ON_EQUIP };

	struct Definition
	{
		std::string MyId;
		std::string MyBaseId;
		std::string MyGoldenId;
		PieceKind MyKind{PieceKind::CHESS};
		int MyTier{1};
		int MyPrice{};
		int MySellPrice{};
		int MyPoolCopies{};
		int MyMergeCount{};
		bool MyGolden{};
		bool MyShopEligible{};
		PlacementClass MyPlacement{PlacementClass::ANY};
		ItemUse MyItemUse{};
		bool MyRequiresSelection{}; // 自选模板须先配置玩家专属身体。
	};

	struct ShopLayout
	{
		int MyChess{};
		int MyItems{};
	};

	struct EconomyRules
	{
		std::vector<int> MyIncome{0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, 12, 12, 12, 12, 12};
		int MyIncomeCap{12};
		int MyRefreshPrice{1};
		int MyGoldenCopies{3};
		int MyMaxLevel{6};
		std::array<int, 5> MyUpgrades{5, 8, 11, 12, 13};
		std::array<ShopLayout, 6> MyLayouts{{{3, 1}, {4, 1}, {4, 1}, {5, 1}, {5, 1}, {5, 1}}};
		std::size_t MyHandSize{10};
		std::size_t MyDeployCap{8};
		std::size_t MyTemporarySize{5};
		std::size_t MyEquipmentPerChess{2};
		int MyRewardCount{3};
		int MyRewardTierOffset{1};
		int MyRewardMaxTier{6};
		int MyRewardPrice{};

		[[nodiscard]] int IncomeAt(int _round) const
		{
			if (_round < 1 || _round > 10000)
				throw std::invalid_argument("round out of range");
			const auto index = static_cast<std::size_t>(_round);
			return index < MyIncome.size() ? MyIncome[index] : std::min(MyIncomeCap, 3 + _round);
		}

		[[nodiscard]] int UpgradeAt(int _level) const
		{
			if (_level < 1 || _level > MyMaxLevel)
				throw std::invalid_argument("shop level out of range");
			return _level == MyMaxLevel ? 0 : MyUpgrades.at(static_cast<std::size_t>(_level - 1));
		}

		[[nodiscard]] ShopLayout LayoutAt(int _level) const
		{
			if (_level < 1 || _level > MyMaxLevel)
				throw std::invalid_argument("shop level out of range");
			return MyLayouts.at(static_cast<std::size_t>(_level - 1));
		}

		void Validate() const;
	};

	// Immutable values; file/database adapters construct a Catalog before starting sessions.
	class Catalog final
	{
	public:
		Catalog(std::vector<Definition> _definitions, std::map<std::string, EconomyRules, std::less<>> _modes);

		[[nodiscard]] const Definition* Find(std::string_view _id) const noexcept
		{
			const auto it = _MyDefinitions.find(_id);
			return it == _MyDefinitions.end() ? nullptr : &it->second;
		}

		[[nodiscard]] const Definition& At(std::string_view _id) const
		{
			const auto* definition = Find(_id);
			if (!definition)
				throw std::out_of_range("unknown definition");
			return *definition;
		}

		[[nodiscard]] const EconomyRules& Rules(std::string_view _mode) const
		{
			const auto it = _MyModes.find(_mode);
			if (it == _MyModes.end())
				throw std::invalid_argument("unknown mode");
			return it->second;
		}

		[[nodiscard]] const std::vector<std::string>& VisibleChess() const noexcept { return _MyChess; }

		[[nodiscard]] const std::vector<std::string>& ShopItems(int _tier) const
		{
			if (_tier < 1 || _tier > 6)
				throw std::invalid_argument("invalid item tier");
			return _MyItems[static_cast<std::size_t>(_tier - 1)];
		}

		[[nodiscard]] std::size_t Size() const noexcept { return _MyDefinitions.size(); }

		[[nodiscard]] const auto& Modes() const noexcept { return _MyModes; }

	private:
		std::map<std::string, Definition, std::less<>> _MyDefinitions;
		std::map<std::string, EconomyRules, std::less<>> _MyModes;
		std::vector<std::string> _MyChess;
		std::array<std::vector<std::string>, 6> _MyItems;
	};
} // namespace Stronghold
#endif
