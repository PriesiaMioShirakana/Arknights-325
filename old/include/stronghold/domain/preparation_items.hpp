#ifndef STRONGHOLD_DOMAIN_PREPARATION_ITEMS_HPP
#define STRONGHOLD_DOMAIN_PREPARATION_ITEMS_HPP
#include <span>
#include <cstdint>
#include <string_view>
#include <string>
#include <vector>
#include <stronghold/domain/bonds.hpp>
#include <stronghold/domain/catalog.hpp>
#include <stronghold/domain/choice_generation.hpp>
#include <stronghold/domain/range.hpp>

namespace Stronghold
{
	// 数据适配器按实际道具的普通/进阶配置生成效果，不在热路径解析 buff key 或 JSON。
	// 同一道具可同时拥有主动使用、销毁和持有期间触发的效果，数组顺序保留原版注册顺序。
	enum class PreparationItemKind
	{
		COINS,
		LAYERS,
		SHOP_CHESS,
		ROUND_COINS,
		SAME_BOND_CHESS,
		NEXT_ROUND_COINS,
		DEPLOY_CAP,
		PROMOTE,
		PROMOTE_NEXT_ROUND,
		MIMIC,
		OFFER_SAME_BOND,
		BEACON,
		SELL_BONUS,
		TRANSFORM,
		COPY_ART,
		TRAINING_BOUNTY,
		BAND_BOUNTY,
		DESTROY_PASS,
		CAULDRON
	};

	struct PreparationItemEffect
	{
		PreparationItemKind MyKind{};
		std::int64_t MyCount{};
		std::int64_t MyMinimum{};
		std::int64_t MyMaximum{};
		std::string_view MyBond{};
		std::span<const std::string_view> MyOtherItems{};
	};

	struct PreparationItemRule
	{
		std::string_view MyId{};
		ItemUse MyUse{};
		std::span<const PreparationItemEffect> MyEffects{};
		std::span<const RangeOffset> MyRange{};
		BondItem MyBond{};
		bool MyFallbackPerfect{};
	};

	// 悬赏保留原数据顺序，调用方只提供敌人存在的记录；禁用列表由当前模式提供。
	struct PreparationArtRules
	{
		std::span<const ChoiceCardRecord> MyBounties{};
		std::span<const std::string_view> MyInactiveEnemies{};
	};

	// 已消耗道具留下的回合效果。信标挂在赠送者身上，赠送者淘汰后仍会尝试交付。
	struct PreparationItemState
	{
		PreparationItemKind MyKind{};
		std::uint64_t MySource{};
		std::int64_t MyCount{};
		std::string MyRecipient{};
		std::string MyChess{};
		std::vector<std::string_view> MyBonds{};
	};
}
#endif
