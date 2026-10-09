#ifndef STRONGHOLD_ADAPTERS_REFERENCE_COMBAT_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_COMBAT_HPP
#include <algorithm>
#include <span>
#include <string_view>
#include <variant>
#include <stronghold/simulation/combat_types.hpp>

namespace Stronghold
{
	// 构建器保留黑板的键名和数值/字符串类别。所有视图引用可执行文件中的静态数据。
	struct BlackboardEntry
	{
		std::string_view MyKey{};
		std::variant<double, std::string_view> MyValue{};
	};

	struct Blackboard
	{
		std::span<const BlackboardEntry> MyEntries{};
		[[nodiscard]] const BlackboardEntry* Find(std::string_view _key) const noexcept
		{
			const auto found = std::ranges::lower_bound(MyEntries, _key, {}, &BlackboardEntry::MyKey);
			return found != MyEntries.end() && found->MyKey == _key ? &*found : nullptr;
		}
		[[nodiscard]] double Number(std::string_view _key, double _fallback = 0) const noexcept
		{
			const auto* entry = Find(_key);
			const auto* value = entry ? std::get_if<double>(&entry->MyValue) : nullptr;
			return value ? *value : _fallback;
		}
	};

	struct EnemySkillRecord
	{
		std::string_view MyId{};
		double MyPriority{};
		double MyCooldown{};
		double MyInitialCooldown{};
		double MySpCost{};
		Blackboard MyBlackboard{};
		Blackboard MyStrings{};
	};

	struct EnemyAbilityRecord
	{
		std::string_view MyText{};
		std::string_view MyFormat{};
	};

	struct EnemySpRecord
	{
		SpType MyType{SpType::NONE};
		double MyMaximum{};
		double MyInitial{};
		double MyIncrement{};
	};

	enum class EnemyRank { NORMAL, ELITE, BOSS };

	struct EnemyRecord
	{
		std::string_view MyId{};
		std::string_view MyName{};
		CombatStats MyStats{};
		AttackProfile MyAttack{};
		EnemyRank MyRank{};
		bool MyFlying{};
		int MyBlockWeight{1};
		int MyLifeCost{1};
		bool MyCounted{true};
		bool MyStaticBody{};
		std::optional<HitArea> MyHitArea{};
		std::uint64_t MyImmunities{};
		Blackboard MyTalent{};
		Blackboard MyTalentStrings{};
		std::span<const EnemySkillRecord> MySkills{};
		std::span<const EnemyAbilityRecord> MyAbilities{};
		std::span<const std::string_view> MyTags{};
		EnemySpRecord MySp{};

		// 此处只解析基础定义；具体天赋、技能及首领脚本由内置内容模块安装。
		// 返回值独立拥有 ID，静态数据不被一局战斗的属性修改污染。
		[[nodiscard]] CombatDefinition MakeDefinition() const
		{
			return CombatDefinition{.MyId = std::string(MyId), .MyStats = MyStats, .MyAttack = MyAttack,
				.MyRange = {}, .MyFlying = MyFlying, .MyBlockWeight = MyBlockWeight,
				.MyImmunities = StatusFlags(MyImmunities), .MyLeader = MyRank == EnemyRank::BOSS,
				.MyStaticBody = MyStaticBody, .MyElite = MyRank == EnemyRank::ELITE, .MyHitArea = MyHitArea};
		}
	};

	[[nodiscard]] std::span<const EnemyRecord> ReferenceEnemies() noexcept;
	[[nodiscard]] const EnemyRecord& ReferenceEnemy(std::string_view _id);
	[[nodiscard]] std::string_view ReferenceCombatFingerprint() noexcept;
}
#endif
