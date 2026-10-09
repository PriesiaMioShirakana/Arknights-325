#ifndef STRONGHOLD_DOMAIN_PREPARATION_BANDS_HPP
#define STRONGHOLD_DOMAIN_PREPARATION_BANDS_HPP
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

namespace Stronghold
{
	enum class PreparationBandKind
	{
		TIER_LAYERS, REFRESH_BOND, COPY_SHOP, ROUND_GIFT, PERIODIC_GIFT, INCOME,
		ROUND_POOL, SPEND_CHESS, SPECIAL_REFRESH, PERIODIC_BOND, FIRST_DISCOUNT,
		SPEND_POOL, ACTIVE_LAYERS, LEVEL_OFFER, INTEREST, SELL_EXCHANGE,
		SHOP_GIFT, BUY_PENDING, REFRESH_GIFT, PERIODIC_OFFER, TRIGGER_GAIN
	};

	struct PreparationBandEffect
	{
		PreparationBandKind MyKind{};
		std::int64_t MyCount{1};
		std::int64_t MyMaximum{std::numeric_limits<std::int64_t>::max()};
		std::int64_t MyThreshold{};
		std::int64_t MyAlternate{};
		int MyRound{};
		int MyPeriod{1};
		std::string_view MyChess{};
		std::string_view MyBond{};
		std::string_view MyPool{};
		std::span<const int> MyLevels{};
	};

	struct PreparationBandRule
	{
		std::string_view MyId{};
		std::span<const PreparationBandEffect> MyEffects{};
		bool MyKeepFunds{};
	};
}
#endif
