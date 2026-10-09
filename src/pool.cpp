#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <stronghold/domain/pool.hpp>

namespace Stronghold
{
	SharedPool::SharedPool(const Catalog& _catalog, const std::set<std::string, std::less<>>& _banned, double _scale)
	{
		if (!std::isfinite(_scale) || _scale < 1 || _scale > 1.5)
			throw std::invalid_argument("pool scale must be 1..1.5");
		for (const auto& id : _catalog.VisibleChess())
		{
			if (_banned.contains(id))
				continue;
			const auto& d = _catalog.At(id);
			const auto cap = static_cast<int>(std::ceil(static_cast<double>(d.MyPoolCopies) * _scale));
			if (cap > 0)
				_MyEntries.emplace(id, PoolEntry{cap, cap, d.MyTier});
		}
	}

	std::optional<std::string> SharedPool::Roll(Random& _random, const RollOptions& _options) const
	{
		const auto eligible = [&_options](const auto& _id, const PoolEntry& _entry)
		{
			return _entry.MyRemaining > 0 && _entry.MyTier >= _options.MyMinTier &&
				(!_options.MyIncluded || std::ranges::find(*_options.MyIncluded, _id) != _options.MyIncluded->end()) && (_options.MyExactTier ? _entry.MyTier == *_options.MyExactTier : _entry.MyTier <= _options.MyMaxTier) && !_options.
				MyExcluded.contains(_id);
		};

		std::int64_t total = 0;
		for (const auto& [id, entry] : _MyEntries)
			if (eligible(id, entry))
				total += entry.MyRemaining;
		const auto extraEligible = [&](const PrivatePoolEntry& _entry)
		{
			return (!_options.MyShopLevel || _entry.MyShopLevel <= *_options.MyShopLevel) && eligible(_entry.MyId, _entry.MyStock);
		};
		for (const auto& entry : _options.MyExtra)
			if (extraEligible(entry)) total += entry.MyStock.MyRemaining;
		if (total == 0)
			return std::nullopt; // Empty pool consumes no RNG draw.
		auto r = _random.Next() * static_cast<double>(total);
		for (const auto& [id, entry] : _MyEntries)
			if (eligible(id, entry))
			{
				r -= entry.MyRemaining;
				if (std::isless(r, 0.0))
					return id;
			}
		for (const auto& entry : _options.MyExtra)
			if (extraEligible(entry))
			{
				r -= entry.MyStock.MyRemaining;
				if (std::isless(r, 0.0)) return entry.MyId;
			}
		throw std::logic_error("weighted pool roll exhausted");
	}

	std::optional<std::string> SharedPool::RollItem(Random& _random, int _maxTier, const Catalog& _catalog) const
	{
		if (_maxTier < 1 || _maxTier > 6)
			throw std::invalid_argument("invalid shop level");
		const auto shares = TierShares(_maxTier);
		auto r = _random.Next();
		int tier = 0;
		for (int t = 1; t <= 6; ++t)
		{
			if (shares[static_cast<std::size_t>(t - 1)] <= 0)
				continue;
			tier = t;
			r -= shares[static_cast<std::size_t>(t - 1)];
			if (std::isless(r, 0.0))
				break;
		}
		if (tier == 0)
			tier = 1 + static_cast<int>(r * _maxTier);
		const auto pick = [&](int _tier) -> std::optional<std::string>
		{
			const auto& items = _catalog.ShopItems(_tier);
			if (items.empty())
				return std::nullopt;
			return items[_random.Index(static_cast<std::uint32_t>(items.size()))];
		};
		for (int t = tier; t >= 1; --t)
			if (auto item = pick(t))
				return item;
		for (int t = tier + 1; t <= 6; ++t)
			if (auto item = pick(t))
				return item;
		return std::nullopt;
	}

	std::vector<PoolGroup> PoolGroups(std::span<const Seat> _players, bool _experimental, bool _independent)
	{
		if (_players.empty() || _players.size() > 20 || (!_experimental && _players.size() > 4))
			throw std::invalid_argument("invalid player count");
		std::vector<Seat> sorted(_players.begin(), _players.end());
		std::ranges::sort(sorted, {}, &Seat::MySeat);
		std::set<std::string> ids;
		int lastSeat = -1;
		for (const auto& player : sorted)
		{
			if (player.MySeat < 0 || player.MySeat >= 20 || player.MySeat == lastSeat || player.MyPlayerId.empty() || !ids.insert(player.MyPlayerId).second)
				throw std::invalid_argument("invalid or duplicate seat");
			lastSeat = player.MySeat;
		}
		const auto count = sorted.size();
		const auto groups = _independent ? count : _experimental && count >= 7 ? (count + 3) / 4 : 1;
		std::vector<PoolGroup> result(groups);
		std::size_t offset = 0;
		for (std::size_t i = 0; i < groups; ++i)
		{
			const auto n = count / groups + (i < count % groups ? 1 : 0);
			result[i].MyScale = !_independent && _experimental && count > 4 && count < 7 ? static_cast<double>(count) / 4 : 1;
			for (std::size_t j = 0; j < n; ++j)
				result[i].MyPlayers.push_back(sorted[offset++].MyPlayerId);
		}
		return result;
	}
} // namespace Stronghold
