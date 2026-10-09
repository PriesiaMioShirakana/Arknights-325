#ifndef STRONGHOLD_DOMAIN_CHOICE_REWARDS_HPP
#define STRONGHOLD_DOMAIN_CHOICE_REWARDS_HPP
#include <stronghold/domain/bond_ledger.hpp>
#include <stronghold/domain/choice_conditions.hpp>
#include <stronghold/domain/content_pools.hpp>
#include <stronghold/domain/settlement.hpp>
#include <stronghold/domain/special_draft.hpp>

namespace Stronghold
{
	enum class ChoiceRewardKind { ITEM_POOL, LAYERS, FUNDS, FREE_REFRESH, UPGRADE_ITEM, UPGRADE_CHESS, BOND_CHESS };
	struct ChoiceDeviceSetting { std::string_view MyAlias{}; bool MyActive{}; };
	struct ChoiceRewardAction
	{
		ChoiceRewardKind MyKind{};
		std::int64_t MyCount{1};
		std::string_view MyPool{};
		std::span<const std::string_view> MyBonds{};
	};
	struct ChoiceRewardRule
	{
		std::string_view MyId{};
		ChoiceCardKind MyKind{};
		std::span<const ChoiceRewardAction> MyActions{};
		std::span<const ChoiceDeviceSetting> MyDevices{};
		ChoiceGate MyGate{};
		bool MyBattleEffect{};
		std::string_view MyEnemyId{};
		unsigned MyEnemyCount{1};
		std::int64_t MyCoins{};
		unsigned MyBattles{1};
		bool MyPerfect{};
	};
	struct ChoiceEffectState
	{
		std::string MyId{};
		std::string_view MyRule{};
		std::uint64_t MySequence{};
		unsigned MyRound{};
		std::optional<bool> MyPreparationPassed{};
		std::optional<std::size_t> MyBenchCount{};
	};
	struct ChoiceRewardPlayerConfig { std::string_view MyPlayerId{}; std::span<const ContentPoolRoster> MyRoster{}; };
	struct ChoiceRewardView
	{
		std::string MyPlayerId{};
		std::vector<BondLayerProgress> MyLayers{};
		std::vector<ChoiceEffectState> MyBattleEffects{};
		std::vector<ChoiceDeviceSetting> MyDevices{};
		std::uint64_t MySequence{};
	};
	enum class ChoiceRewardError { UNKNOWN_PLAYER, ELIMINATED, BAD_CARD };
	struct ChoiceRecipient { std::string MyPlayerId{}; std::vector<EconomyEvent> MyEvents{}; std::vector<BondLayerChange> MyLayers{}; };
	struct ChoiceRewardResult { std::vector<ChoiceRecipient> MyRecipients{}; };

}
#endif
