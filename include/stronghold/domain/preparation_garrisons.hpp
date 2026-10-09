#ifndef STRONGHOLD_DOMAIN_PREPARATION_GARRISONS_HPP
#define STRONGHOLD_DOMAIN_PREPARATION_GARRISONS_HPP
#include <array>
#include <stronghold/domain/content_pools.hpp>

namespace Stronghold
{
	enum class GarrisonEvent { GAIN, ROUND_START, PREP_END, SOLD, PRICE, REFRESH };

	enum class GarrisonKind
	{
		ADD_BOND, ADD_BOND_CHESS_ALL, ADD_BOND_METHOD, ADD_MULTIPLE_BOND,
		ADD_BOND_ACTIVATED_MOST_LAYER, ADD_ACT_BOND_DIFF_LV_MOST_LAYER,
		ADD_BOND_IN_HAND, ADD_BOND_POSITION, ADD_BOND_ROUND_COIN_COST,
		ADD_REFRESH_CNT_MULTIPLIER_BOND_LAYER, GAIN_BOND_LAYER_BY_REFRESH_CNT,
		CHESS_PRICE, GAIN_EQUIP, GAIN_FREE_REFRESH_COUNT, GAIN_RANDOM_EQUIP_CHESS_IN_POOL,
		POOL_EQUIP, POOL_CHAR, MOST_BOND, ONCE_GOLD, ONCE_GOLD_WITH_BOND_CONDITION,
		SELL_CHESS_GAIN_SPECIAL_GOODS, TRIGGER_ANOTHER, TRIGGER_FRONT_COUNT,
		FRONT_SAME_EFFECT_PREP_FIN, FRONT_SAME_EFFECT_PREP_START
	};

	enum class GarrisonMethod { NONE, SHOP_LEVEL, ROUND_GAINED, HAND_COUNT, SAME_ROW, BOND_TIERS };

	// 构建期消解事件、效果、条件和黑板；运行时只访问类型化参数。
	struct PreparationGarrisonRule
	{
		std::string_view MyId{};
		GarrisonEvent MyEvent{};
		GarrisonKind MyKind{};
		GarrisonMethod MyMethod{};
		double MyCount{1};
		double MyMultiplier{1};
		double MyLayer{};
		double MyMaximum{};
		double MyCheckCount{};
		double MyPrice{};
		double MyRefreshCount{1};
		std::span<const std::string_view> MyBonds{};
		std::span<const double> MyBondCounts{};
		std::string_view MyChess{};
		std::string_view MyPool{};
		std::array<std::string_view, 6> MyLevelPools{};
		std::span<const int> MyRounds{};
		std::span<const ContentPoolWeight> MyGoldenWeights{};
		GarrisonEvent MyTrigger{GarrisonEvent::GAIN};
		bool MyRequireActive{};
		bool MyBoardOnly{};
		bool MySameRow{};
		bool MyBehind{};
		bool MyFarthest{};
	};

	struct PreparationGarrisonRules
	{
		std::span<const PreparationGarrisonRule> MyEffects{};
		unsigned MyInvestRepeat{2};
		double MyInvestLayer{100};
	};
}
#endif
