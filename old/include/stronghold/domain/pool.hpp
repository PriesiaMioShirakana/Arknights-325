#ifndef STRONGHOLD_DOMAIN_POOL_HPP
#define STRONGHOLD_DOMAIN_POOL_HPP
#include <algorithm>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <stronghold/core/random.hpp>
#include <stronghold/domain/catalog.hpp>

namespace Stronghold
{
	struct PoolEntry
	{
		int MyCapacity{};
		int MyRemaining{};
		int MyTier{};
	};

	struct PrivatePoolEntry
	{
		std::string MyId;
		PoolEntry MyStock{};
		int MyShopLevel{1};
	};

	struct RollOptions
	{
		int MyMaxTier{6};
		std::optional<int> MyExactTier;
		std::set<std::string, std::less<>> MyExcluded;
		std::span<const PrivatePoolEntry> MyExtra{};
		std::optional<int> MyShopLevel{}; // 仅限制额外库存；内容奖励不传此项。
		int MyMinTier{1};
		std::optional<std::span<const std::string_view>> MyIncluded{}; // 无值不筛选；空列表明确表示没有候选。
	};

	class SharedPool final
	{
	public:
		SharedPool(
			const Catalog& _catalog, const std::set<std::string, std::less<>>& _banned = {}, double _scale = 1.0);

		[[nodiscard]] const PoolEntry* Find(std::string_view _id) const noexcept
		{
			const auto it = _MyEntries.find(_id);
			return it == _MyEntries.end() ? nullptr : &it->second;
		}

		[[nodiscard]] int Take(std::string_view _id, int _count = 1)
		{
			if (_count < 0)
				throw std::invalid_argument("negative pool withdrawal");
			const auto it = _MyEntries.find(_id);
			if (it == _MyEntries.end())
				return 0;
			const auto taken = std::min(_count, it->second.MyRemaining);
			it->second.MyRemaining -= taken;
			return taken;
		}

		[[nodiscard]] int Give(std::string_view _id, int _count = 1)
		{
			if (_count < 0)
				throw std::invalid_argument("negative pool return");
			const auto it = _MyEntries.find(_id);
			if (it == _MyEntries.end())
				return 0;
			auto& entry = it->second;
			const auto returned = std::min(_count, entry.MyCapacity - entry.MyRemaining);
			entry.MyRemaining += returned;
			return returned;
		}

		[[nodiscard]] std::optional<std::string> Roll(Random& _random, const RollOptions& _options = {}) const;

		[[nodiscard]] std::optional<std::string> RollItem(Random& _random, int _maxTier, const Catalog& _catalog) const;

		[[nodiscard]] std::array<double, 6> TierShares(int _maxTier) const
		{
			std::array<double, 6> shares{};
			double total = 0;
			for (const auto& [id, entry] : _MyEntries)
			{
				(void)id;
				if (entry.MyTier > _maxTier)
					continue;
				shares[static_cast<std::size_t>(entry.MyTier - 1)] += entry.MyRemaining;
				total += entry.MyRemaining;
			}
			if (total > 0)
				for (auto& n : shares)
					n /= total;
			return shares;
		}

		[[nodiscard]] const auto& Entries() const noexcept { return _MyEntries; }

	private:
		std::map<std::string, PoolEntry, std::less<>> _MyEntries;
	};

	struct Seat
	{
		int MySeat{};
		std::string MyPlayerId;
	};

	struct PoolGroup
	{
		std::vector<std::string> MyPlayers;
		double MyScale{1};
	};

	[[nodiscard]] std::vector<PoolGroup>
	PoolGroups(std::span<const Seat> _players, bool _experimental, bool _independent);
} // namespace Stronghold
#endif
