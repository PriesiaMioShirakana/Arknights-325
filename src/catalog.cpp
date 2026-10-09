#include <algorithm>
#include <stdexcept>
#include <stronghold/domain/catalog.hpp>

namespace Stronghold
{
	void EconomyRules::Validate() const
	{
		const auto bounded = [](int _value) { return _value >= 0 && _value <= 1000000; };
		if (MyMaxLevel < 1 || MyMaxLevel > 6 || !bounded(MyIncomeCap) || !bounded(MyRefreshPrice) || MyGoldenCopies < 1 || MyGoldenCopies > 100 || MyHandSize <
			1 || MyHandSize > 100 || MyRewardCount < 1 || MyRewardCount > 6 || !bounded(MyRewardPrice) || MyRewardMaxTier < 1 || MyRewardMaxTier > 6 ||
			MyRewardTierOffset < -6 || MyRewardTierOffset > 6 || MyDeployCap < 1 || MyDeployCap > 36 || MyTemporarySize > 100 || MyEquipmentPerChess < 1 || MyEquipmentPerChess > 100)
			throw std::invalid_argument("invalid economy rules");
		if (MyMaxArtsPerRound == 0 || MyMaxArtsPerRound > 1000000)
			throw std::invalid_argument("invalid art limit");
		for (const auto n : MyIncome)
			if (!bounded(n))
				throw std::invalid_argument("invalid income");
		for (const auto n : MyUpgrades)
			if (!bounded(n))
				throw std::invalid_argument("invalid upgrade price");
		for (const auto s : MyLayouts)
			if (s.MyChess < 0 || s.MyChess > 8 || s.MyItems < 0 || s.MyItems > 4)
				throw std::invalid_argument("invalid shop layout");
	}

	Catalog::Catalog(std::vector<Definition> _definitions, std::map<std::string, EconomyRules, std::less<>> _modes)
		: _MyModes(std::move(_modes))
	{
		if (_MyModes.empty())
			throw std::invalid_argument("catalog requires a mode");
		for (const auto& [id, rules] : _MyModes)
		{
			if (id.empty())
				throw std::invalid_argument("empty mode");
			rules.Validate();
		}
		for (auto& d : _definitions)
		{
			if (d.MyId.empty() || d.MyId.size() > 128 || d.MyBaseId.empty() || d.MyTier < 1 || d.MyTier > 6 || d.MyPrice < 0 || d.MyPrice > 1000000 || d.
				MySellPrice < 0 || d.MySellPrice > 1000000 || d.MyPoolCopies < 0 || d.MyPoolCopies > 1000000 || d.MyMergeCount < 0 || d.MyMergeCount > 99 || (d.
					MyGolden && (d.MyMergeCount != 0 || d.MyShopEligible)) || (d.MyKind != PieceKind::CHESS && d.MyKind != PieceKind::ITEM) || (d.MyPlacement !=
					PlacementClass::ANY && d.MyPlacement != PlacementClass::MELEE && d.MyPlacement != PlacementClass::HIGH_ONLY) || (d.MyItemUse != ItemUse::EQUIPMENT && d.MyItemUse != ItemUse::ART && d.MyItemUse != ItemUse::CONSUME_ON_EQUIP))
				throw std::invalid_argument("invalid definition: " + d.MyId);
			if (d.MyRequiresSelection && (d.MyKind != PieceKind::CHESS || d.MyShopEligible)) throw std::invalid_argument("selected chess cannot enter shared shops");
			const auto id = d.MyId;
			if (!_MyDefinitions.emplace(id, std::move(d)).second)
				throw std::invalid_argument("duplicate definition: " + id);
		}
		for (const auto& [id, d] : _MyDefinitions)
		{
			const auto* base = Find(d.MyBaseId);
			if (!base || base->MyRequiresSelection != d.MyRequiresSelection || base->MyKind != d.MyKind || base->MyGolden || base->MyBaseId != base->MyId)
				throw std::invalid_argument("invalid base: " + id);
			if (d.MyMergeCount > 1)
			{
				const auto* golden = Find(d.MyGoldenId);
				if (!golden || !golden->MyGolden || golden->MyKind != d.MyKind || golden->MyBaseId != d.MyBaseId)
					throw std::invalid_argument("invalid merge target: " + id);
			}
			if (!d.MyShopEligible)
				continue;
			if (d.MyKind == PieceKind::CHESS && d.MyBaseId != d.MyId)
				throw std::invalid_argument("shop chess must use its canonical pool id: " + id);
			if (d.MyKind == PieceKind::CHESS)
				_MyChess.push_back(id);
			else
				_MyItems.at(static_cast<std::size_t>(d.MyTier - 1)).push_back(id);
		}
	}
} // namespace Stronghold
