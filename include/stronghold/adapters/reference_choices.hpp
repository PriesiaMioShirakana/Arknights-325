#ifndef STRONGHOLD_ADAPTERS_REFERENCE_CHOICES_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_CHOICES_HPP
#include <stronghold/domain/choice_generation.hpp>
#include <stronghold/domain/content_pools.hpp>
#include <stronghold/domain/choice_rewards.hpp>
#include <stronghold/domain/preparation_items.hpp>
#include <stronghold/simulation/choice_effects.hpp>
namespace Stronghold
{
	struct ChoiceEnemyRule { ChoiceEnemyRank MyRank{}; std::span<const AttributeChange> MyModifiers{}; };
	struct ChoiceBattleRule
	{
		std::string_view MyId{};
		ChoiceGate MyGate{};
		double MySelfHeal{};
		std::span<const AttributeChange> MyOperatorModifiers{};
		std::span<const AttributeChange> MyFullHealthModifiers{};
		std::span<const ChoiceEnemyRule> MyEnemies{};
	};
	[[nodiscard]] std::span<const PreparationItemRule> ReferencePreparationItems() noexcept;
	[[nodiscard]] std::span<const ChoiceBattleRule> ReferenceChoiceBattleRules() noexcept;
	// 把借用静态表的准备效果转成 Battle 自持的输入，一次性复制，热路径不再查 JSON/黑板。
	[[nodiscard]] std::vector<BattleChoiceEffect> MakeChoiceBattleEffects(const ChoiceRewardView& _view);
	[[nodiscard]] const ChoiceGenerationRules& ReferenceChoices() noexcept;
	[[nodiscard]] std::span<const ContentPoolRecord> ReferenceContentPools() noexcept;
	[[nodiscard]] std::span<const ChoiceRewardRule> ReferenceChoiceRewards() noexcept;
}
#endif
