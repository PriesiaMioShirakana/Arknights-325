#ifndef STRONGHOLD_DOMAIN_CHOICE_GENERATION_HPP
#define STRONGHOLD_DOMAIN_CHOICE_GENERATION_HPP
#include <stronghold/domain/special_draft.hpp>

namespace Stronghold
{
	enum class BountyDraftKind { NONE, INITIAL, BOSS, HUNTER };
	struct ChoiceCardRecord
	{
		ChoiceCardKind MyKind{};
		std::string_view MyId{};
		std::string_view MyName{};
		std::string_view MyDescription{};
		std::string_view MyRichDescription{};
		std::optional<int> MyTier{};
		std::string_view MyEnemyId{};
		unsigned MyCount{1};
		std::int64_t MyCoins{};
		unsigned MyBattles{1};
		bool MyPerfect{};
		bool MyTeam{};
		std::string_view MyTacticKind{};
	};

	struct ChoiceWeight { std::size_t MyValue{}; double MyWeight{1}; };
	struct BountyDraftEntry { std::size_t MyCard{}; BountyDraftKind MyPool{}; int MySeries{}; };
	struct TacticDraftEntry
	{
		std::size_t MyCard{};
		std::string_view MyStage{};
		std::span<const std::string_view> MyTargetBonds{}; // 空表示并非只作用于这些盟约，无需过滤。
		double MyWeight{1};
	};

	struct BountyDraftGroup
	{
		std::span<const ChoiceWeight> MyCards{};
		unsigned MySeen{1};
		unsigned MyOpen{};
	};

	struct BountyDraftRule
	{
		std::span<const int> MySeries{};
		std::span<const int> MyTiers{};
		unsigned MyPerSeries{1};
		std::span<const std::size_t> MyPreferred{};
		unsigned MySize{7};
		std::span<const int> MyOnePerSeries{};
		unsigned MyMaxSeries16{2};
		std::span<const std::size_t> MyGiants{};
		std::span<const std::size_t> MyCards{};
	};

	struct BountyDraftSpec
	{
		BountyDraftKind MyKind{};
		unsigned MyCount{6};
		unsigned MySlots{};
		bool MyPickSeen{};
		std::span<const BountyDraftGroup> MyGroups{};
		BountyDraftRule MyRule{};
	};

	struct ChoiceFamilyRecord { std::string_view MyName{}; std::string_view MyDescription{}; };
	struct ChoiceShopSlot { std::span<const ChoiceWeight> MyKinds{}; }; // 0=盟约之币，1..6=阶级。
	struct ChoiceSchedule
	{
		std::string_view MyMode{};
		unsigned MyRound{};
		std::span<const ChoiceWeight> MyFamilies{}; // MyValue 对应 ChoiceFamily。
		unsigned MyCount{}; // 0 使用该模式的 format；正数按原版最多取六张。
		std::array<int, 2> MySupplyTiers{1, 6};
		BountyDraftKind MyBountyKind{};
		std::array<std::span<const std::string_view>, 4> MyEvents{};
	};

	struct ChoiceGenerationRules
	{
		std::span<const ChoiceCardRecord> MyCards{};
		std::span<const BountyDraftEntry> MyBounties{}; // 仅可选、敌人存在的条目，保留源数据顺序。
		std::span<const TacticDraftEntry> MyTactics{};
		std::array<std::span<const ChoiceWeight>, 6> MyItemsByTier{}; // 按 ID 排序，权重用于机密商店。
		std::optional<std::size_t> MyCoinCard{};
		std::span<const BountyDraftSpec> MyBountySpecs{};
		std::span<const unsigned> MyShopRounds{}; // 空表示所有回合。
		std::span<const ChoiceShopSlot> MyShopSlots{};
		std::span<const unsigned> MyTacticRounds{};
		std::span<const std::string_view> MyTacticKinds{};
		bool MyHasTacticSpec{};
		std::span<const ChoiceSchedule> MySchedules{};
		std::array<ChoiceFamilyRecord, 4> MyFamilies{};
		unsigned MySoloCount{3};
		unsigned MyCoopCount{6};
	};

	struct ChoiceGenerationInput
	{
		std::string_view MyMode{};
		unsigned MyRound{1};
		bool MySolo{};
		std::string_view MyStage{};
		std::optional<std::span<const std::string_view>> MyLiveBonds{}; // 无值不筛选；空 span 表示没有可用盟约。
		std::size_t MyPlayerCount{4};
		bool MyCapacityExperiment{};
	};

	struct ChoiceDraft
	{
		ChoiceFamily MyFamily{};
		std::string_view MyName{};
		std::string_view MyDescription{};
		std::string_view MyEventId{};
		std::vector<ChoiceCard> MyCards{};
	};

	// 低频备战生成；输入表只读共享，返回卡片拥有其可变文本和奖励数据。
	// 结果内的元数据 string_view 与悬赏敌人 ID 仍借用输入静态表。
	[[nodiscard]] std::optional<ChoiceDraft> GenerateChoices(const ChoiceGenerationRules& _rules, const ChoiceGenerationInput& _input, Random& _random);
}
#endif
