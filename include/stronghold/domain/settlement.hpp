#ifndef STRONGHOLD_DOMAIN_SETTLEMENT_HPP
#define STRONGHOLD_DOMAIN_SETTLEMENT_HPP
#include <expected>
#include <stronghold/domain/unite.hpp>
#include <stronghold/domain/final_assault.hpp>

namespace Stronghold
{
	class EconomySession;
	struct ActiveBounty { WaveBounty MyCard{}; unsigned MyRoundsLeft{1}; };

	struct MatchStatistics
	{
		std::uint64_t MyLifeLost{};
		std::uint64_t MyLeaks{};
		std::uint64_t MyKills{};
		std::uint64_t MyPerfectRounds{};
		std::int64_t MyFundsGained{};
		double MyDamage{};
		double MyHealing{};
		double MyBossDamage{};
	};

	struct MatchPlayerProgress
	{
		std::string MyPlayerId{};
		std::int64_t MyLife{30};
		bool MyAlive{true};
		bool MyBot{};
		bool MyLeft{};
		bool MyRevived{};
		bool MyPendingDeath{};
		std::optional<unsigned> MyEliminatedRound{};
		std::int64_t MyPendingFunds{};
		std::vector<ActiveBounty> MyBounties{};
		MatchStatistics MyStatistics{};
	};

	struct SettlementRules
	{
		std::size_t MyLifeCapPerRound{10};
		bool MyRevivalEnabled{};
	};

	struct UniteSettlementStage
	{
		std::reference_wrapper<const UnitePlan> MyPlan;
		std::reference_wrapper<const BattleResult> MyResult;
		bool MySynthetic{};
	};

	struct RoundSettlementInput
	{
		unsigned MyRound{};
		double MyNow{}; // 外部单调逻辑秒；不读取 OS 时钟。
		bool MyTeamLifePhase{};
		std::span<const BattlePlayerState> MyNormalResults{};
		std::span<const std::string> MySyntheticPlayers{};
		std::span<const UniteSettlementStage> MyUniteStages{};
	};

	struct RoundSettlementSummary
	{
		std::vector<UniteLoss> MyLosses{};
		std::optional<std::size_t> MyThrough{};
		std::vector<std::string> MyEliminated{}; // 调用方据此归还卡池、清理棋盘；待救援者不清理。
	};

	enum class RevivalError { DISABLED, WINDOW_CLOSED, STALE_ROUND, UNKNOWN_PLAYER, ELIMINATED, NOT_HELPER, LOW_LIFE, INELIGIBLE_TARGET };

	// 持有整局进度，不持有战斗或网络对象。一次回合结算原子提交；同回合结果不能重复入账。
	class RoundLedger final
	{
	public:
		RoundLedger(SettlementRules _rules, std::vector<MatchPlayerProgress> _players);
		// 悬赏由准备内容加入，后续结算统一消耗期限；ID 必须在该玩家内唯一。
		[[nodiscard]] bool AddBounty(std::string_view _player, ActiveBounty _bounty);
		[[nodiscard]] RoundSettlementSummary Settle(const RoundSettlementInput& _input);
		// 首领阶段使用团队 LP 分摊结果，不按个人漏怪淘汰，也不支付普通回合的完美悬赏。
		// 传入本关伤害而非跨关累计；与普通结算共用回合号，避免重复发放悬赏资金。
		void SettleBoss(unsigned _round, double _now, std::span<const BossPlayerProgress> _players, std::span<const BattlePlayerState> _results);
		[[nodiscard]] std::expected<void, RevivalError> Revive(std::string_view _donor, std::string_view _target, unsigned _round, double _now);
		// 到期或强制离开结算阶段时，最终淘汰待救援者。返回本次新淘汰的 ID，重复调用为空。
		[[nodiscard]] std::vector<std::string> Advance(double _now);
		[[nodiscard]] std::vector<std::string> CloseRevival();
		[[nodiscard]] bool RevivalOpen(double _now) const noexcept;
		[[nodiscard]] double RevivalDeadline() const noexcept { return _MyDeadline; }
		[[nodiscard]] std::span<const MatchPlayerProgress> Players() const noexcept { return _MyPlayers; }
		// 救援关闭后，把资金和最终淘汰一次性提交到同回合备战会话；失败不清空账本。
		void CommitToPreparation(EconomySession& _economy);
		[[nodiscard]] std::int64_t TakePendingFunds(std::string_view _player);

	private:
		[[nodiscard]] RoundSettlementSummary Apply(const RoundSettlementInput& _input);
		[[nodiscard]] bool DonorEligible(const MatchPlayerProgress& _player) const;
		void Eliminate(MatchPlayerProgress& _player);
		SettlementRules _MyRules;
		std::vector<MatchPlayerProgress> _MyPlayers;
		std::vector<std::string> _MyEligible;
		unsigned _MyRound{};
		double _MyNow{};
		double _MyDeadline{};
		bool _MyWindowOpen{};
	};
}
#endif
