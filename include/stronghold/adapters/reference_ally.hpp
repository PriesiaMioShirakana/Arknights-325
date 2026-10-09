#ifndef STRONGHOLD_ADAPTERS_REFERENCE_ALLY_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_ALLY_HPP
#include <stronghold/adapters/reference_combat.hpp>

namespace Stronghold
{
	// 数据参数和行为实现分开：黑板不自动解释成通用增益，具体技能内容决定如何消费。
	struct AllySkillRecord
	{
		std::string_view MyId{};
		std::string_view MyName{};
		int MyIndex{-1};
		std::string_view MyType{};
		std::string_view MyDurationType{};
		double MyDuration{};
		SpType MySpType{SpType::NONE};
		double MySpCost{};
		double MyInitialSp{};
		unsigned MyMaxCharges{1};
		std::span<const RangeOffset> MyRange{};
		bool MyHasRange{};
		std::string_view MyTrigger{}; // 保留 MLYSS_WTRMAN 等内容专有策略名字。
		std::span<const RangeOffset> MyTriggerRange{};
		bool MyHasTriggerRange{};
		bool MyTriggerAllies{};
		Blackboard MyBlackboard{};
		Blackboard MyStrings{};
		std::string_view MyDescription{};
	};

	struct AllyTalentRecord
	{
		std::string_view MyName{};
		std::string_view MyDescription{};
		Blackboard MyBlackboard{};
		Blackboard MyStrings{};
		std::span<const RangeOffset> MyRange{};
		bool MyHasRange{};
		std::string_view MyTokenKey{};
	};

	// 已组合的一种身体数据。字符串和 span 均指向编译期表，查询不分配、不解析 JSON。
	struct AllyRecord
	{
		std::string_view MyId{};
		std::string_view MyName{};
		std::string_view MyCharacterId{};
		std::string_view MyProfession{};
		std::string_view MySubProfession{};
		std::string_view MyPosition{};
		CombatStats MyStats{};
		std::span<const RangeOffset> MyRange{};
		std::string_view MyDamageType{};
		std::string_view MyAttackKind{};
		std::string_view MyProjectile{};
		std::optional<bool> MyCanHitFlying{};
		std::string_view MyTargetPriority{};
		std::string_view MyTraitText{};
		Blackboard MyTraitBlackboard{};
		std::uint64_t MyImmunities{};
		std::optional<AllySkillRecord> MySkill{};
		std::span<const AllyTalentRecord> MyTalents{};
		std::span<const std::string_view> MyTokens{};
		std::uint64_t MyStartingFlags{}; // 召唤物不可选中／异常特性，由内容安装为持久 Buff。
		std::optional<double> MyWithdrawDuration{}; // skcom_withdraw 没有可施放技能，但保留寿命参数。
		AttackProfile MyBaseAttack{}; // 职业的基础数值配置；尚未迁移的职业／技能脚本不能由此推断。
		std::span<const RangeOffset> MyTraitFrontRange{};
		bool MyHasTraitFrontRange{};
		ProfessionDefinition MyProfessionTraits{};
		PlacementClass MyPreparationPlacement{PlacementClass::MELEE};
		std::span<const RangeOffset> MyPreparationRange{}; // 部署时的技能/模组常驻范围，不是战斗中技能开启后的范围。

		// 职业和内容层必须显式提供解析后的攻击／技能；这里不把缺少脚本的黑板伪装成完整技能。
		[[nodiscard]] CombatDefinition MakeDefinition(AttackProfile _attack, SkillDefinition _skill = {}) const
		{
			auto stats = MyStats;
			stats.MyMoveSpeed = 0; // 棋盘友军身体不沿敌人路线行走。
			return CombatDefinition{.MyId = std::string(MyId), .MyStats = stats, .MyAttack = std::move(_attack),
				.MyRange = std::vector<RangeOffset>(MyRange.begin(), MyRange.end()),
				.MyImmunities = StatusFlags(MyImmunities), .MySkill = std::move(_skill),
				.MyTraitFrontRange = MyHasTraitFrontRange ? std::optional(std::vector<RangeOffset>(MyTraitFrontRange.begin(), MyTraitFrontRange.end())) : std::nullopt, .MyProfession = MyProfessionTraits};
		}
	};

	struct AllyLoadoutRecord
	{
		int MySkillIndex{-1};
		std::string_view MyModuleId{}; // 空 = 无选项；"none" = 显式不装备。
		bool MySkillDefault{};
		bool MyModuleDefault{};
		AllyRecord MyBody{};
		std::string_view MyEquippedModuleId{};
		unsigned MyModuleLevel{};
		bool MyModuleActive{};
	};

	// 自选的有效配置在构建时枚举；运行时验证只查表，不组合外部可变数据。
	struct DiyChoiceRecord
	{
		std::string_view MyCharacterId{};
		int MySkillIndex{};
		std::string_view MyModuleId{}; // "none" 表示不装备；普通形态仍保留精锐形态的选择。
		bool MyPrototype{};
		std::span<const std::string_view> MyBonds{};
		std::string_view MyTokenOwner{}; // charId@训练状态，供自选召唤物选取对应变体。
		AllyLoadoutRecord MyLoadout{};
	};

	struct OperatorRecord
	{
		std::string_view MyId{};
		std::string_view MyBaseId{};
		bool MyGolden{};
		bool MyDiy{};
		unsigned MyTier{};
		std::span<const std::string_view> MyBonds{};
		std::span<const std::string_view> MyGarrisons{};
		int MyDefaultSkill{-1};
		std::string_view MyDefaultModule{};
		std::span<const AllyLoadoutRecord> MyLoadouts{};
		std::optional<AllyLoadoutRecord> MyStandIn{};
		std::span<const DiyChoiceRecord> MyDiyChoices{};

		// 非法技能和模组选项分别退回默认，与 DataSource.getChess 的容错语义相同。
		[[nodiscard]] const AllyLoadoutRecord& Loadout(
			std::optional<int> _skill = {},
			std::optional<std::string_view> _module = {},
			bool _standIn = false
		) const;

		// 原型允许省略锁定选项；自有干员必须指定技能。非法选择抛出 invalid_argument。
		[[nodiscard]] const DiyChoiceRecord& Diy(
			std::string_view _character,
			std::optional<int> _skill = {},
			std::optional<std::string_view> _module = {}
		) const;
	};

	struct TokenVariantRecord
	{
		std::string_view MyOwnerId{};
		int MySkillIndex{-1};
		std::string_view MyModuleId{};
		bool MyDefault{};
		AllyRecord MyBody{};
		double MyCount{1};
		std::span<const std::string_view> MySources{};
		bool MyHasSources{};
	};

	struct TokenRecord
	{
		std::string_view MyId{};
		bool MyPlaceable{};
		bool MyOwnerRange{};
		double MyDeployLimit{};
		std::string_view MyFallbackOwner{}; // 原数据中第一个变体的拥有者，保留原插入顺序。
		std::span<const TokenVariantRecord> MyVariants{};
		std::span<const TokenVariantRecord> MyDiyVariants{};
		TokenVariantRecord MyUnowned{}; // 自选拥有者无匹配变体时用顶层默认，不借用其它拥有者。

		[[nodiscard]] const TokenVariantRecord& DiyVariant(const DiyChoiceRecord& _choice) const;

		// 缺少拥有者变体时先取普通形态，再取原记录首个变体；不在运行时组合黑板。
		[[nodiscard]] const TokenVariantRecord& Variant(
			std::string_view _owner = {},
			std::optional<int> _skill = {},
			std::optional<std::string_view> _module = {}
		) const;
	};

	[[nodiscard]] std::span<const OperatorRecord> ReferenceOperators() noexcept;
	[[nodiscard]] const OperatorRecord& ReferenceOperator(std::string_view _id);
	[[nodiscard]] std::span<const TokenRecord> ReferenceTokens() noexcept;
	[[nodiscard]] const TokenRecord& ReferenceToken(std::string_view _id);
	[[nodiscard]] std::string_view ReferenceAllyFingerprint() noexcept;
}
#endif
