#ifndef STRONGHOLD_ADAPTERS_REFERENCE_CHOICES_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_CHOICES_HPP
#include <stronghold/domain/choice_generation.hpp>
#include <stronghold/domain/content_pools.hpp>
#include <stronghold/domain/choice_rewards.hpp>
#include <stronghold/domain/preparation_items.hpp>
#include <stronghold/domain/preparation_bands.hpp>
#include <stronghold/domain/preparation_garrisons.hpp>
#include <stronghold/domain/preparation_bonds.hpp>
#include <stronghold/simulation/choice_effects.hpp>
#include <stronghold/simulation/band_effects.hpp>
#include <stronghold/simulation/garrison_effects.hpp>
#include <limits>
namespace Stronghold
{
	struct BandBattleRule
	{
		std::string_view MyId{};
		std::string_view MyBond{};
		BandBattleParameters MyParameters{};
	};

	[[nodiscard]] std::span<const BandBattleRule> ReferenceBandBattleRules() noexcept;
	[[nodiscard]] std::vector<BattleBandEffect> MakeBandBattleEffects(std::string_view _strategy);

	struct MapCharacterBandRule
	{
		unsigned MyMinimumElites{};
		unsigned MyMaximumElites{};
		std::span<const std::string_view> MyCharacters{};
	};

	[[nodiscard]] std::span<const MapCharacterBandRule> ReferenceMapCharacterBands() noexcept;

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
	[[nodiscard]] std::span<const PreparationBondEffect> ReferencePreparationBonds() noexcept;
	[[nodiscard]] const PreparationGarrisonRules& ReferencePreparationGarrisons() noexcept;
	[[nodiscard]] const BattleGarrisonRules& ReferenceBattleGarrisons() noexcept;
	[[nodiscard]] std::span<const PreparationBandRule> ReferencePreparationBands() noexcept;
	[[nodiscard]] std::span<const PreparationItemRule> ReferencePreparationItems() noexcept;
	[[nodiscard]] std::span<const ChoiceBattleRule> ReferenceChoiceBattleRules() noexcept;
	// 把借用静态表的准备效果转成 Battle 自持的输入，一次性复制，热路径不再查 JSON/黑板。
	[[nodiscard]] std::vector<BattleChoiceEffect> MakeChoiceBattleEffects(const ChoiceRewardView& _view);
	[[nodiscard]] const ChoiceGenerationRules& ReferenceChoices() noexcept;
	[[nodiscard]] std::span<const ContentPoolRecord> ReferenceContentPools() noexcept;
	[[nodiscard]] std::span<const ChoiceRewardRule> ReferenceChoiceRewards() noexcept;
}
#endif
