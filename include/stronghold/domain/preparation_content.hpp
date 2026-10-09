#ifndef STRONGHOLD_DOMAIN_PREPARATION_CONTENT_HPP
#define STRONGHOLD_DOMAIN_PREPARATION_CONTENT_HPP
#include <stronghold/domain/choice_rewards.hpp>
#include <stronghold/domain/preparation_items.hpp>
#include <stronghold/domain/preparation_bands.hpp>
#include <stronghold/domain/preparation_garrisons.hpp>
#include <stronghold/domain/preparation_bonds.hpp>

namespace Stronghold
{
	// 准备内容状态所有者；机变奖励和道具效果共用经济事务、层数账本和宿主随机流。
	// 借用经济会话、结算账本和只读规则；三者必须覆盖其生命周期。
	// 只有宿主能调用 Apply：传入 SpecialDraft 成功产生的奖励，重复调用表示再次获得同一卡片。
	// 一次团队奖励按选卡者→其余存活玩家顺序执行，在经济/层数/悬赏/随机流全部成功后提交。
	class PreparationContent final
	{
	public:
		PreparationContent(EconomySession& _economy, RoundLedger& _ledger, std::span<const ChoiceRewardRule> _rules,
			std::span<const ContentPoolRecord> _pools, std::span<const BondRule> _bonds,
			std::span<const ChoiceRewardPlayerConfig> _players, std::span<const PreparationItemRule> _items = {},
			PreparationArtRules _arts = {}, std::span<const PreparationBandRule> _bands = {}, PreparationGarrisonRules _garrisons = {},
			std::span<const PreparationBondEffect> _bondEffects = {});
		[[nodiscard]] std::expected<ChoiceRewardResult, ChoiceRewardError> Apply(std::string_view _picker, const ChoiceCard& _card, Random& _random);

		// 客户端准备命令入口；内容效果与经济变更在同一个事务中执行。
		[[nodiscard]] std::expected<ChangeSet, CommandError> Execute(const CommandEnvelope& _command, Random& _random);
		// 带战略收入修正的回合入口；随后调用 OnRoundStart，再开放准备阶段。
		void BeginRound(int _round, Random& _random);
		void BeginPreparation(Random& _random);
		[[nodiscard]] std::optional<std::int64_t> PriceOf(std::string_view _player, const ShopSlot& _slot) const;
		// BeginRound(round, false) 之后、BeginPreparation 之前调用，每回合恰好一次。
		// 先按玩家顺序处理存活者的装备/持续效果，再交付淘汰者留下的信标。
		[[nodiscard]] ChoiceRewardResult OnRoundStart(Random& _random);
		// 内容赠送与战后效果必须经过本入口，才能触发获得干员时的装备效果。
		[[nodiscard]] std::expected<GrantResult, CommandError> GrantPiece(std::string_view _player,
			std::string_view _definition, Random& _random, GrantOptions _options = {});
		[[nodiscard]] std::expected<GrantResult, CommandError> ApplyInventoryEffect(std::string_view _player,
			const InventoryEffect& _effect, Random& _random);
		[[nodiscard]] std::expected<ChangeSet, CommandError> OnBattleResult(std::string_view _player, int _round, Random& _random);
		[[nodiscard]] std::optional<std::vector<PreparationItemState>> ItemEffects(std::string_view _player) const;

		// 在准备期限清理之后、EndPreparation 之前调用，记录火力/锐利条件供战斗使用。
		void OnPreparationEnd();
		void OnPreparationEnd(Random& _random);
		[[nodiscard]] std::optional<ChoiceRewardView> View(std::string_view _player) const;
		// 无随机流重载仅用于未安装盟约奖励的独立层数账本；安装奖励时必须传入宿主随机流。
		[[nodiscard]] std::optional<BondLayerChange> AddLayers(std::string_view _player, std::string_view _bond, double _count);
		[[nodiscard]] std::optional<BondLayerChange> AddLayers(std::string_view _player, std::string_view _bond, double _count, Random& _random);
		// 宿主仅提交本轮普通战斗的累计收益，重复/乱序的累计值不会重复入账。
		[[nodiscard]] std::expected<bool, CommandError> SynchronizeLayers(std::string_view _player, int _round,
			std::span<const BondNumber> _gains);
		[[nodiscard]] std::expected<bool, CommandError> SynchronizeLayers(std::string_view _player, int _round,
			std::span<const BondNumber> _gains, Random& _random);

	private:
		friend struct PreparationHooks;

		enum class BandEvent { ROUND_START, PREP_END, BUY, REFRESH, LEVEL_UP, SPEND, SOLD };
		enum class BondEvent { ROUND_START, PREP_START, PREP_END, BUY, REFRESH, GAIN, SOLD, MERGE, LAYERS };

		struct BandState
		{
			std::int64_t MySpent{};
			std::uint64_t MyRefreshes{};
			unsigned MyPendingRefreshes{};
			int MyRound{};
			std::int64_t MyRoundCount{};
			int MyDiscountRound{};
			bool MyComplete{};
		};

		struct GarrisonCounter
		{
			PieceUid MyUid{};
			int MyRound{};
			std::uint64_t MyRefreshes{};
		};

		struct ItemCounter
		{
			PieceUid MyUid{};
			std::uint64_t MySells{};
			int MyGainRound{};
			std::uint64_t MyGains{};
		};

		struct Player
		{
			std::string MyId;
			std::vector<ContentPoolRoster> MyRoster;
			BondLayerLedger MyLayers;
			std::vector<ChoiceEffectState> MyEffects{};
			std::vector<ChoiceDeviceSetting> MyDevices{};
			std::uint64_t MySequence{};

			std::vector<std::string_view> MyInactiveBonds{};
			std::vector<PreparationItemState> MyItemEffects{};
			std::vector<ItemCounter> MyItemCounters{};
			int MyBattleResultRound{};
			const PreparationBandRule* MyBand{};
			BandState MyBandState{};
			std::vector<GarrisonCounter> MyGarrisonCounters{};
			std::vector<std::int64_t> MyBondCounters{};
			int MyPrepEndedRound{};
		};

		[[nodiscard]] const ChoiceRewardRule* Rule(std::string_view _id) const;
		[[nodiscard]] const PreparationItemRule* ItemRule(std::string_view _id) const;
		[[nodiscard]] std::vector<std::string_view> ItemBonds(const Player& _player, const Piece& _piece) const;
		[[nodiscard]] std::vector<BondState> ItemBondStates(const EconomySession& _economy,
			const Player& _player, std::size_t _index) const;
		[[nodiscard]] std::vector<BondState> ComputeItemBondStates(const EconomySession& _economy,
			const Player& _player, std::size_t _index) const;
		[[nodiscard]] std::vector<std::pair<PieceUid, PieceUid>> EquippedItems(const EconomySession& _economy, std::size_t _player) const;
		void OnItemGain(EconomySession& _economy, std::vector<Player>& _players, std::string_view _player,
			PieceUid _uid, std::vector<EconomyEvent>& _events) const;
		void OnItemSold(EconomySession& _economy, std::vector<Player>& _players, std::string_view _player,
			Random& _random, std::vector<EconomyEvent>& _events) const;
		[[nodiscard]] std::expected<ChangeSet, CommandError> ExecuteEconomy(const CommandEnvelope& _command, Random& _random);
		void ApplyBand(EconomySession& _economy, std::vector<Player>& _players, std::size_t _player,
			BandEvent _event, Random& _random, std::vector<EconomyEvent>& _events,
			std::string_view _definition = {}, std::int64_t* _amount = nullptr) const;
		[[nodiscard]] bool BandMember(const Player& _player, std::string_view _definition, std::string_view _bond) const;
		[[nodiscard]] std::int64_t BandPrice(const EconomySession& _economy, const Player& _player, const ShopSlot& _slot) const;
		void BandIncome(const EconomySession& _economy, const Player& _player, std::int64_t& _income) const;
		[[nodiscard]] const PreparationGarrisonRule* GarrisonRule(std::string_view _id) const;
		[[nodiscard]] std::span<const std::string_view> Garrisons(const Player& _player, std::string_view _definition) const;
		void RunGarrisons(EconomySession& _economy, std::vector<Player>& _players, std::size_t _player,
			GarrisonEvent _event, Random& _random, std::vector<EconomyEvent>& _events,
			const Piece* _piece = nullptr, std::span<const PieceUid> _refreshed = {}) const;
		void RunPieceGarrisons(EconomySession& _economy, std::vector<Player>& _players, std::size_t _player,
			GarrisonEvent _event, const Piece& _source, const Piece& _self, bool _hand, bool _trigger,
			Random& _random, std::vector<EconomyEvent>& _events) const;
		void ApplyGarrison(EconomySession& _economy, std::vector<Player>& _players, std::size_t _player,
			const PreparationGarrisonRule& _rule, const Piece& _self, bool _hand, bool _trigger,
			Random& _random, std::vector<EconomyEvent>& _events) const;
		void ApplyBonds(EconomySession& _economy, std::vector<Player>& _players, std::size_t _player,
			BondEvent _event, Random& _random, std::vector<EconomyEvent>& _events, std::string_view _bond = {}) const;
		[[nodiscard]] std::optional<BondLayerChange> AddContentLayers(EconomySession& _economy,
			std::vector<Player>& _players, std::size_t _player, std::string_view _bond, double _count,
			Random& _random, std::vector<EconomyEvent>& _events) const;
		[[nodiscard]] std::optional<std::size_t> ItemRecipient(const EconomySession& _economy,
			std::span<const Player> _players, std::size_t _sender, std::span<const std::string_view> _bonds, Random& _random) const;
		[[nodiscard]] bool GrantItemChess(EconomySession& _economy, std::size_t _player,
			std::string_view _id, std::vector<EconomyEvent>& _events) const;
		[[nodiscard]] std::optional<CommandError> ApplyItem(EconomySession& _economy, std::vector<Player>& _players,
			std::size_t _player, const PreparationItemEffect& _effect, const EquipItem& _command,
			Random& _random, std::vector<EconomyEvent>& _events, bool& _keep) const;
		[[nodiscard]] std::expected<ChangeSet, CommandError> ExecuteArt(const CommandEnvelope& _command, Random& _random);
		[[nodiscard]] std::expected<ChangeSet, CommandError> ExecuteDestroy(const CommandEnvelope& _command, Random& _random);
		[[nodiscard]] std::optional<CommandError> ApplyArt(EconomySession& _economy, RoundLedger& _ledger,
			std::size_t _player, const UseArt& _command, const PreparationItemRule& _rule,
			Random& _random, std::vector<EconomyEvent>& _events) const;
		[[nodiscard]] std::expected<std::size_t, CommandError> ItemPlayer(const CommandEnvelope& _command) const;

		EconomySession& _MyEconomy;
		RoundLedger& _MyLedger;
		std::span<const ChoiceRewardRule> _MyRules;
		std::span<const ContentPoolRecord> _MyPools;
		std::vector<Player> _MyPlayers;

		std::span<const PreparationItemRule> _MyItems;
		std::span<const BondRule> _MyBonds;
		int _MyItemRound{};
		int _MyBandEndRound{};
		PreparationGarrisonRules _MyGarrisons;
		std::span<const PreparationBondEffect> _MyBondEffects;

		std::vector<const ChoiceCardRecord*> _MyTrainingBounties;
		std::vector<const ChoiceCardRecord*> _MyBandBounties;
		std::array<std::vector<const ChoiceCardRecord*>, 2> _MyFallbackBounties;
	};
}
#endif
