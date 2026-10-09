#ifndef STRONGHOLD_DOMAIN_PREPARATION_CONTENT_HPP
#define STRONGHOLD_DOMAIN_PREPARATION_CONTENT_HPP
#include <stronghold/domain/choice_rewards.hpp>

namespace Stronghold
{
	// 准备内容状态所有者；当前执行机变奖励，并为后续道具/战略效果保留同一个层数账本。
	// 借用经济会话、结算账本和只读规则；三者必须覆盖其生命周期。
	// 只有宿主能调用 Apply：传入 SpecialDraft 成功产生的奖励，重复调用表示再次获得同一卡片。
	// 一次团队奖励按选卡者→其余存活玩家顺序执行，在经济/层数/悬赏/随机流全部成功后提交。
	class PreparationContent final
	{
	public:
		PreparationContent(EconomySession& _economy, RoundLedger& _ledger, std::span<const ChoiceRewardRule> _rules,
			std::span<const ContentPoolRecord> _pools, std::span<const BondRule> _bonds,
			std::span<const ChoiceRewardPlayerConfig> _players);
		[[nodiscard]] std::expected<ChoiceRewardResult, ChoiceRewardError> Apply(std::string_view _picker, const ChoiceCard& _card, Random& _random);
		// 在准备期限清理之后、EndPreparation 之前调用，记录火力/锐利条件供战斗使用。
		void OnPreparationEnd();
		[[nodiscard]] std::optional<ChoiceRewardView> View(std::string_view _player) const;
		[[nodiscard]] std::optional<BondLayerChange> AddLayers(std::string_view _player, std::string_view _bond, double _count);

	private:
		struct Player
		{
			std::string MyId;
			std::vector<ContentPoolRoster> MyRoster;
			BondLayerLedger MyLayers;
			std::vector<ChoiceEffectState> MyEffects{};
			std::vector<ChoiceDeviceSetting> MyDevices{};
			std::uint64_t MySequence{};
		};
		[[nodiscard]] const ChoiceRewardRule* Rule(std::string_view _id) const;
		EconomySession& _MyEconomy;
		RoundLedger& _MyLedger;
		std::span<const ChoiceRewardRule> _MyRules;
		std::span<const ContentPoolRecord> _MyPools;
		std::vector<Player> _MyPlayers;
	};
}
#endif
