#include <cmath>
#include <stronghold/domain/content_pools.hpp>

namespace Stronghold
{
	namespace
	{
		const ContentPoolRecord* FindPool(std::span<const ContentPoolRecord> _pools, std::string_view _id)
		{
			const auto found = std::ranges::find(_pools, _id, &ContentPoolRecord::MyId);
			return found == _pools.end() ? nullptr : &*found;
		}

		double Weight(double _weight)
		{
			if (!std::isfinite(_weight)) throw std::invalid_argument("content pool weight must be finite");
			return std::max(0.0, _weight);
		}

		template <class _Eligible>
		std::optional<std::string> DrawList(const ContentPoolRecord& _pool, Random& _random, _Eligible _eligible, bool _item)
		{
			if (!_pool.MyWeighted.empty())
			{
				double total = 0;
				std::optional<std::string_view> first, last;
				for (const auto& entry : _pool.MyWeighted)
					if (_eligible(entry.MyId))
					{
						if (!first) first = entry.MyId;
						last = entry.MyId; total += Weight(entry.MyWeight);
					}
				if (!std::isfinite(total)) throw std::overflow_error("content pool total weight overflow");
				if (!_item && !std::isgreater(total, 0.0)) return first ? std::optional(std::string(*first)) : std::nullopt;
				auto value = _random.Next() * total;
				for (const auto& entry : _pool.MyWeighted)
					if (_eligible(entry.MyId))
					{
						value -= Weight(entry.MyWeight);
						if (std::isless(value, 0.0)) return std::string(entry.MyId);
					}
				return last ? std::optional(std::string(*last)) : std::nullopt;
			}
			std::size_t count = 0;
			for (const auto id : _pool.MyItems) if (_eligible(id)) ++count;
			if (!count) return {};
			auto index = static_cast<std::size_t>(_random.Next() * static_cast<double>(count));
			for (const auto id : _pool.MyItems) if (_eligible(id) && index-- == 0) return std::string(id);
			throw std::logic_error("content item draw exhausted");
		}
	}

	std::optional<std::string> RollContentItem(const Catalog& _catalog, std::span<const ContentPoolRecord> _pools, Random& _random, ContentItemDraw _options)
	{
		const auto* pool = FindPool(_pools, _options.MyPool);
		const auto isItem = [&](std::string_view _id)
		{
			const auto* definition = _catalog.Find(_id);
			return definition && definition->MyKind == PieceKind::ITEM;
		};
		if (pool && pool->MyKind == PieceKind::ITEM && (!pool->MyWeighted.empty() || !pool->MyItems.empty()))
			return DrawList(*pool, _random, isItem, true);
		std::array<int, 6> range{1, 2, 3, 4, 5, 6};
		std::span<const int> tiers;
		int exact = 0;
		if (pool && pool->MyKind == PieceKind::ITEM)
			tiers = !pool->MyTiers.empty() ? pool->MyTiers : std::span<const int>(range).first(static_cast<std::size_t>(pool->MyShopLevel ? std::clamp(_options.MyShopLevel, 1, 6) : 6));
		else if (_options.MyTier) { exact = *_options.MyTier; tiers = std::span<const int>(&exact, 1); }
		else tiers = std::span<const int>(range).first(static_cast<std::size_t>(std::clamp(_options.MyMaxTier, 1, 6)));
		std::size_t count = 0;
		for (const auto tier : tiers) if (tier >= 1 && tier <= 6) count += _catalog.ShopItems(tier).size();
		if (!count) return {};
		auto index = static_cast<std::size_t>(_random.Next() * static_cast<double>(count));
		for (const auto tier : tiers)
			if (tier >= 1 && tier <= 6)
			{
				const auto& items = _catalog.ShopItems(tier);
				if (index < items.size()) return items[index];
				index -= items.size();
			}
		throw std::logic_error("content tier draw exhausted");
	}

	std::optional<ContentPoolResult> RollContentPool(const Catalog& _catalog, const EconomySession& _economy, std::string_view _player,
		std::span<const ContentPoolRecord> _pools, std::span<const ContentPoolRoster> _roster, Random& _random, std::string_view _pool, int _shopLevel)
	{
		const auto* pool = FindPool(_pools, _pool);
		if (!pool) return {};
		if (pool->MyKind == PieceKind::ITEM)
		{
			auto id = RollContentItem(_catalog, _pools, _random, ContentItemDraw{.MyPool = _pool, .MyShopLevel = _shopLevel});
			return id ? std::optional(ContentPoolResult{.MyKind = PieceKind::ITEM, .MyId = std::move(*id)}) : std::nullopt;
		}
		if (pool->MyKind != PieceKind::CHESS) return {};
		const auto free = [&](std::string_view _id)
		{
			const auto* definition = _catalog.Find(_id);
			if (!definition || definition->MyKind != PieceKind::CHESS) return false;
			const auto stock = _economy.CopyStock(_player, definition->MyBaseId);
			return !stock || stock->MyRemaining > 0;
		};
		std::optional<std::string> id;
		if (!pool->MyWeighted.empty() || !pool->MyItems.empty()) id = DrawList(*pool, _random, free, false);
		else
		{
			RollOptions options;
			options.MyMaxTier = pool->MyShopLevel ? std::clamp(_shopLevel, 1, 6) : pool->MyMaxTier;
			options.MyMinTier = pool->MyMinTier; options.MyExactTier = pool->MyExactTier;
			std::vector<std::string_view> members;
			if (!pool->MyBond.empty())
			{
				members.reserve(_roster.size());
				for (const auto& member : _roster)
					if (std::ranges::find(member.MyBonds, pool->MyBond) != member.MyBonds.end()) members.emplace_back(member.MyId);
				options.MyIncluded = members;
			}
			id = _economy.RollChess(_player, _random, options);
		}
		if (!id) return {};
		if (pool->MyGolden)
			if (const auto& definition = _catalog.At(*id); !definition.MyGoldenId.empty()) *id = definition.MyGoldenId;
		return ContentPoolResult{.MyKind = PieceKind::CHESS, .MyId = std::move(*id), .MyGolden = pool->MyGolden};
	}
}
