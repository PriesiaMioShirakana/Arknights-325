#ifndef STRONGHOLD_RUNTIME_LOCAL_MATCH_HPP
#define STRONGHOLD_RUNTIME_LOCAL_MATCH_HPP
#include <memory>
#include <stronghold/adapters/reference_catalog.hpp>
#include <stronghold/adapters/reference_match.hpp>
#include <stronghold/adapters/reference_preparation.hpp>
#include <stronghold/adapters/reference_roster.hpp>
#include <stronghold/domain/preparation_content.hpp>
#include <stronghold/simulation/boss_battle_group.hpp>

namespace Stronghold
{
	enum class MatchPhase { LOBBY, INFO_CHECK, BAND_DRAFT, BATTLE_CHECK, ROUND_START, SP_DRAFT, PREP, COMBAT, UNITE, SETTLE, FINAL_ASSAULT, HIDDEN_CORE, RESULT };
	enum class MatchFlowError { WRONG_PHASE, UNKNOWN_PLAYER, ALREADY, ELIMINATED };
	using MatchError = std::variant<MatchFlowError, DraftError, ChoiceDraftError, CommandError, RosterError, RevivalError>;

	struct MatchSeat
	{
		Seat MySeat{};
		bool MyBot{};
		PlayerRoster MyRoster{};
	};

	struct LocalMatchOptions
	{
		std::string MyMode{"mode_single_normal"};
		std::uint32_t MySeed{1};
		std::vector<MatchSeat> MyPlayers{};
		bool MyCapacityExperiment{};
		bool MyIndependentPools{};
		bool MyRevivalEnabled{};
		// 仅显式 CUSTOM 走注册表；注册表须已 Seal，地址及生命周期覆盖整个宿主。
		std::optional<std::reference_wrapper<const ContentRegistry>> MyCustomRegistry{};
		std::vector<ContentBinding> MyCustomBindings{};
		RulePositionMode MyRulePositionMode{RulePositionMode::CURRENT}; // 开局设置，所有阶段的战场共用，局中不修改。
		bool MyGarrisonEffectsAfterExit{};
		bool MyRetainGrantedGarrisonsAfterExit{true};
	};

	struct LocalMatchResult
	{
		bool MyVictory{};
		bool MyHiddenReached{};
		bool MyHiddenCleared{};
		unsigned MyRound{};
		std::string MyReason{};
		std::vector<MatchPlayerProgress> MyPlayers{};
	};

	struct MatchPhaseChange
	{
		MatchPhase MyPhase{};
		unsigned MyRound{};
		double MyTime{};
	};

	// 单一所有者的离线宿主。Advance 仅推进逻辑秒/阶段期限，StepBattles 推进确定性战斗帧。
	// 数据取自构建期适配器；未移植的专属内容不会凭黑板自动推断。无线程、网络或系统计时器。
	// 内部对象相互借用，宿主不可复制/移动；配置在战略选秀结束时冻结。
	class LocalMatch final
	{
	public:
		explicit LocalMatch(LocalMatchOptions _options);
		LocalMatch(const LocalMatch&) = delete;
		LocalMatch& operator=(const LocalMatch&) = delete;
		LocalMatch(LocalMatch&&) = delete;
		LocalMatch& operator=(LocalMatch&&) = delete;
		void Start(double _now = 0);
		void Advance(double _now);
		void StepBattles(std::uint64_t _ticks = 1);
		[[nodiscard]] std::expected<void, MatchError> InfoReady(std::string_view _player);
		[[nodiscard]] std::expected<void, MatchError> FocusStrategy(std::string_view _player, std::string_view _strategy);
		[[nodiscard]] std::expected<void, MatchError> PickStrategy(std::string_view _player, std::string_view _strategy);
		[[nodiscard]] std::expected<void, MatchError> SkipStrategy(std::string_view _player);
		[[nodiscard]] std::expected<void, MatchError> PickCard(std::string_view _player, std::size_t _index);
		[[nodiscard]] std::expected<ChangeSet, MatchError> Execute(const CommandEnvelope& _command);
		[[nodiscard]] std::expected<void, MatchError> Revive(std::string_view _donor, std::string_view _target, unsigned _round);
		[[nodiscard]] std::expected<void, MatchError> SetPaused(std::string_view _player, bool _paused);
		[[nodiscard]] std::expected<void, MatchError> SetLoadout(std::string_view _player, std::span<const LoadoutChoice> _choices);

		[[nodiscard]] MatchPhase Phase() const noexcept { return _MyPhase; }
		[[nodiscard]] unsigned Round() const noexcept { return _MyRound; }
		[[nodiscard]] double Now() const noexcept { return _MyNow; }
		[[nodiscard]] std::optional<double> Deadline() const noexcept { return _MyDeadline; }
		[[nodiscard]] bool Untimed() const noexcept { return _MyUntimed; }
		[[nodiscard]] bool Paused() const noexcept { return _MyPaused; }
		[[nodiscard]] RulePositionMode PositionMode() const noexcept { return _MyOptions.MyRulePositionMode; }
		[[nodiscard]] bool GarrisonEffectsAfterExit() const noexcept { return _MyOptions.MyGarrisonEffectsAfterExit; }

		[[nodiscard]] bool RetainGrantedGarrisonsAfterExit() const noexcept { return _MyOptions.MyRetainGrantedGarrisonsAfterExit; }

		[[nodiscard]] const WaveSetup& Setup() const noexcept { return _MySetup; }
		[[nodiscard]] const MatchBans& Bans() const noexcept { return _MyBans; }
		[[nodiscard]] const EconomySession& Economy() const noexcept { return *_MyEconomy; }
		[[nodiscard]] const PreparationContent* Content() const noexcept { return _MyContent.get(); }
		[[nodiscard]] const RoundLedger* Ledger() const noexcept { return _MyLedger ? &*_MyLedger : nullptr; }
		[[nodiscard]] const StrategyDraft* Strategies() const noexcept { return _MyStrategy ? &*_MyStrategy : nullptr; }
		[[nodiscard]] const SpecialDraft* Choices() const noexcept { return _MySpecial ? &*_MySpecial : nullptr; }
		[[nodiscard]] const LocalMatchResult* Result() const noexcept { return _MyResult ? &*_MyResult : nullptr; }
		[[nodiscard]] std::span<const Battle> Fields() const noexcept;
		[[nodiscard]] std::span<const MatchPhaseChange> PhaseChanges() const noexcept { return _MyHistory; }
		[[nodiscard]] std::span<const BattlePlayerState> NormalResults() const noexcept { return _MyNormalResults; }

	private:
		struct Player
		{
			Seat MySeat{};
			bool MyBot{};
			bool MyInfoReady{};
			PlayerRoster MyRoster{};
			SummonCatalog MySummons{{}, {}};
			std::vector<ContentPoolRoster> MyContentRoster{};
			std::vector<BondRosterRecord> MyBondRoster{};
		};

		struct UniteRound
		{
			UnitePlan MyPlan{};
			BattleResult MyResult{};
			std::vector<EnemySpawn> MySpawns{};
		};

		[[nodiscard]] Player* FindPlayer(std::string_view _id);
		[[nodiscard]] const Player& PlayerAt(std::string_view _id) const;
		[[nodiscard]] std::vector<Seat> AliveSeats() const;
		[[nodiscard]] std::vector<BondState> Bonds(const Player& _player);
		[[nodiscard]] std::vector<WaveBounty> Bounties(std::string_view _player) const;
		[[nodiscard]] BattlePlayerInput BattlePlayer(const Player& _player, bool _boss, bool _right);
		[[nodiscard]] BattleInput BattleField(WavePlan _wave, std::vector<BattlePlayerInput> _players, bool _boss,
			bool _unite, std::string_view _salt);
		void ConfigurePlayers();
		void SetPhase(MatchPhase _phase, std::optional<double> _seconds = {});
		void EnterStrategies();
		void FinishStrategies();
		void StartRound(unsigned _round);
		void EnterChoices();
		void Award(const ChoiceAward& _award);
		void EnterPreparation();
		void UpdateLayouts();
		void EndPreparation(bool _deadline);
		void StartCombat();
		void FinishCombat();
		void StartUnite(UnitePlan _plan);
		void FinishUnite();
		[[nodiscard]] std::optional<UnitePlan> PlanNextUnite(bool _relay);
		void Settle();
		void AfterSettlement();
		void StartBoss();
		void FinishBoss();
		void Finish(bool _victory, bool _hiddenCleared, std::string_view _reason);
		void SyncLayers(std::span<const BattlePlayerState> _players);

		LocalMatchOptions _MyOptions;
		Catalog _MyCatalog;
		const MatchRules& _MyRules;
		WaveGenerator _MyWaves;
		Random _MyDraftRandom;
		Random _MyMetaRandom;
		WaveSetup _MySetup;
		MatchBans _MyBans;
		std::vector<Player> _MyPlayers;
		std::vector<Seat> _MySeats;
		std::vector<std::string_view> _MyLiveBonds;
		std::optional<EconomySession> _MyEconomy;
		std::optional<RoundLedger> _MyLedger;
		std::unique_ptr<PreparationContent> _MyContent;
		PreparationBondCalculator _MyBondCalculator;
		std::vector<BondNumber> _MyLayerScratch;

		std::optional<StrategyDraft> _MyStrategy;
		std::optional<SpecialDraft> _MySpecial;
		std::optional<WavePlan> _MyNormalWave;
		std::vector<BossFieldGroup> _MyBossPairs;
		std::vector<WavePlan> _MyBossWaves;
		std::vector<Battle> _MyFields;
		std::vector<BattlePlayerState> _MyNormalResults;
		std::vector<UniteRound> _MyUniteRounds;
		std::unique_ptr<FinalAssault> _MyAssault;
		std::unique_ptr<BossBattleGroup> _MyBossFields;
		std::optional<LocalMatchResult> _MyResult;
		std::vector<MatchPhaseChange> _MyHistory;

		MatchPhase _MyPhase{MatchPhase::LOBBY};
		unsigned _MyRound{};
		double _MyNow{};
		std::optional<double> _MyDeadline{};
		bool _MyUntimed{};
		bool _MyCombatDone{};
		bool _MyBossDone{};
		bool _MyPaused{};
		double _MyPausedAt{};
	};
}
#endif
