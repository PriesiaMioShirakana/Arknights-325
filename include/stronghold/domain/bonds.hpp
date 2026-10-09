#ifndef STRONGHOLD_DOMAIN_BONDS_HPP
#define STRONGHOLD_DOMAIN_BONDS_HPP
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace Stronghold
{
	inline constexpr double BondLayerCap = 999;

	[[nodiscard]] inline double LayerGainRoom(double _before, double _gain) noexcept
	{
		if (!std::isgreater(_gain, 0)) return 0;
		const auto before = std::isfinite(_before) && std::isgreater(_before, 0) ? _before : 0;
		return std::max(0.0, std::min(_gain, BondLayerCap - before));
	}

	enum class BondCountMode { BOARD, BOARD_AND_HAND, BOARD_ALL_ELITES };

	struct BondRule
	{
		std::string_view MyId{};
		BondCountMode MyCountMode{};
		std::span<const double> MyThresholds{}; // 已排序且有限；缺省阈值由数据适配器解析。
		bool MyCore{};
		bool MyDownward{};
		std::optional<double> MyMaximum{};
	};

	struct BondItem
	{
		bool MyCanGiveBond{};
		std::string_view MyGrantedBond{};
	};

	struct BondMember
	{
		std::string_view MyBaseId{}; // 同一干员的普通/精锐和重复副本共用基础 ID。
		bool MyGolden{};
		std::span<const std::string_view> MyBonds{};
		std::span<const BondItem> MyItems{};
	};

	struct BondNumber { std::string_view MyId{}; double MyValue{}; };
	struct BondCountBonus { std::string_view MyId{}; std::int64_t MyValue{}; };

	struct BondState
	{
		std::string_view MyId{};
		std::int64_t MyCount{};
		bool MyActive{};
		unsigned MyTier{};
		double MyLayers{};
		bool MyHarmony{};
		bool MyOff{};
	};

	struct BondComputation
	{
		std::span<const BondState> MyEnabled{}; // 包含本模式启用的零成员盟约，适合作战输入。
		std::span<const BondState> MyDisabled{}; // 仅包含有成员的禁用盟约，供 UI 展示。
	};

	[[nodiscard]] unsigned BondTier(const BondRule& _rule, std::int64_t _count) noexcept;
	[[nodiscard]] double ActivatedLayers(std::span<const BondState> _states) noexcept;
	// 在调用方持有的视图副本上增加结果层数；不会更改计算器缓存或持久层数。
	void AddBondGains(std::span<BondState> _states, std::span<const BondNumber> _gains);

	// 无效果回调的成员/档位计算。规则及其字符串由调用方持有，必须比计算器活得更久。
	// 输入仅在 Compute 期间借用；输出视图有效至下次 Compute，跨调用持有时应复制。
	class BondCalculator final
	{
	public:
		explicit BondCalculator(std::span<const BondRule> _rules);
		[[nodiscard]] BondComputation Compute(
			std::span<const BondMember> _board,
			std::span<const BondMember> _hand,
			std::span<const BondNumber> _layers = {},
			std::span<const BondCountBonus> _bonus = {},
			std::span<const std::string_view> _inactive = {}
		);
		// 激活优先、层数降序、数据顺序兜底，末尾追加禁用盟约；省略无成员/层数的启用项。
		[[nodiscard]] std::vector<BondState> View(std::span<const BondNumber> _gains = {}) const;

	private:
		struct Variant
		{
			std::string_view MyBase{};
			bool MyGolden{};
			bool operator==(const Variant&) const = default;
		};

		struct Members
		{
			std::vector<std::string_view> MyBoard{};
			std::vector<std::string_view> MyAll{};
			std::vector<Variant> MyVariants{};
			std::int64_t MyRaw{};
			std::int64_t MyBonus{};
			bool MyOff{};
		};

		std::span<const BondRule> _MyRules;
		std::vector<Members> _MyMembers;
		std::vector<BondState> _MyEnabled;
		std::vector<BondState> _MyDisabled;
	};
}
#endif
