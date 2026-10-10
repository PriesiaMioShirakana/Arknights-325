#ifndef STRONGHOLD_DOMAIN_CONTENT_POOLS_HPP
#define STRONGHOLD_DOMAIN_CONTENT_POOLS_HPP
#include <stronghold/domain/preparation.hpp>

namespace Stronghold
{
	struct ContentPoolWeight { std::string_view MyId{}; double MyWeight{1}; };
	struct ContentPoolRecord
	{
		std::string_view MyId{};
		PieceKind MyKind{};
		std::span<const ContentPoolWeight> MyWeighted{};
		std::span<const std::string_view> MyItems{};
		std::span<const int> MyTiers{};
		bool MyShopLevel{};
		int MyMaxTier{6};
		int MyMinTier{1};
		std::optional<int> MyExactTier{};
		std::string_view MyBond{};
		bool MyGolden{};
	};

	struct ContentPoolRoster
	{
		std::string_view MyId{};
		std::span<const std::string_view> MyBonds{};
		std::span<const std::string_view> MyGarrisons{};
	};

	struct ContentItemDraw
	{
		std::string_view MyPool{};
		std::optional<int> MyTier{};
		int MyMaxTier{6};
		int MyShopLevel{6};
	};

	struct ContentPoolResult { PieceKind MyKind{}; std::string MyId{}; bool MyGolden{}; };

	// 规则、字符串及成员表仅在同步调用期间借用；返回 ID 自持存储，不借用临时输入。
	// 原版装备加权池在空/零权重时仍消费一次随机数；干员池则可能不消费，不能统一处理。
	[[nodiscard]] std::optional<std::string> RollContentItem(
		const Catalog& _catalog,
		std::span<const ContentPoolRecord> _pools,
		Random& _random,
		ContentItemDraw _options = {}
	);

	// 成员表必须包含本玩家解析后的 DIY 成员关系；这里不使用共享目录的空槽模板推断盟约。
	// 抽卡不扣库存，内容发放步骤负责消费副本。非商店内容抽取没有 DIY 商店等级门槛。
	[[nodiscard]] std::optional<ContentPoolResult> RollContentPool(
		const Catalog& _catalog,
		const EconomySession& _economy,
		std::string_view _player,
		std::span<const ContentPoolRecord> _pools,
		std::span<const ContentPoolRoster> _roster,
		Random& _random,
		std::string_view _pool,
		int _shopLevel = 6
	);
}
#endif
