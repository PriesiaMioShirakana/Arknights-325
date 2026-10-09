#include <stronghold/domain/settlement.hpp>
#include <stronghold/domain/preparation.hpp>

namespace Stronghold
{
	namespace
	{
		constexpr std::int64_t RevivalCost = 10;
		constexpr std::int64_t MinimumDonorLife = 11;
		constexpr double RevivalWindowSeconds = 15;

		std::int64_t Coins(double _amount)
		{
			if (!std::isfinite(_amount) || !std::isless(_amount, static_cast<double>(std::numeric_limits<std::int64_t>::max())))
				throw std::invalid_argument("invalid settlement currency");
			return std::isgreater(_amount, 0) ? static_cast<std::int64_t>(std::trunc(_amount)) : 0;
		}

		template <class _Integer>
		void Add(_Integer& _total, _Integer _amount)
		{
			if constexpr (std::is_signed_v<_Integer>) if (_amount < 0) throw std::overflow_error("negative settlement increment");
			if (_total > std::numeric_limits<_Integer>::max() - _amount) throw std::overflow_error("settlement counter overflow");
			_total += _amount;
		}

		void AddStat(double& _total, double _amount)
		{
			if (!std::isfinite(_amount) || !std::isfinite(_total + _amount)) throw std::invalid_argument("invalid settlement statistic");
			_total += _amount;
		}

		std::size_t LeakCount(const BattlePlayerState& _result)
		{ return static_cast<std::size_t>(std::ranges::count_if(_result.MyLeaks, &LeakedEnemy::MyCounted)); }
	}

	RoundLedger::RoundLedger(SettlementRules _rules, std::vector<MatchPlayerProgress> _players)
		: _MyRules(_rules), _MyPlayers(std::move(_players))
	{
		if (!_rules.MyLifeCapPerRound || _rules.MyLifeCapPerRound > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))
			throw std::invalid_argument("invalid round LP cap");
		for (std::size_t i = 0; i < _MyPlayers.size(); ++i)
		{
			const auto& player = _MyPlayers[i];
			if (player.MyPlayerId.empty() || player.MyLife < 0 || player.MyPendingFunds < 0 || player.MyPendingDeath)
				throw std::invalid_argument("invalid initial progress");
			for (std::size_t j = 0; j < i; ++j)
				if (_MyPlayers[j].MyPlayerId == player.MyPlayerId) throw std::invalid_argument("duplicate progress player");
		}
	}

	RoundSettlementSummary RoundLedger::Settle(const RoundSettlementInput& _input)
	{
		if (_input.MyRound <= _MyRound || _MyWindowOpen || !std::isfinite(_input.MyNow) || std::isless(_input.MyNow, _MyNow) ||
			!std::isfinite(_input.MyNow + RevivalWindowSeconds)) throw std::invalid_argument("invalid settlement transition");
		// 低频回合边界以值事务保护资金、悬赏期限和死亡状态；无效战报不能造成部分入账。
		auto next = *this;
		auto summary = next.Apply(_input);
		*this = std::move(next);
		return summary;
	}

	void RoundLedger::SettleBoss(unsigned _round, double _now, std::span<const BossPlayerProgress> _players, std::span<const BattlePlayerState> _results)
	{
		if (_round <= _MyRound || _MyWindowOpen || !std::isfinite(_now) || std::isless(_now, _MyNow)) throw std::invalid_argument("invalid boss settlement transition");
		auto next = *this;
		for (std::size_t i = 0; i < _players.size(); ++i)
		{
			const auto& boss = _players[i];
			const auto player = std::ranges::find(next._MyPlayers, boss.MyPlayerId, &MatchPlayerProgress::MyPlayerId);
			if (player == next._MyPlayers.end() || !player->MyAlive || player->MyPendingDeath || boss.MyLife < 0 ||
				!std::isfinite(boss.MyRoundDamage) || std::isless(boss.MyRoundDamage, 0)) throw std::invalid_argument("invalid boss settlement player");
			for (std::size_t j = 0; j < i; ++j)
				if (_players[j].MyPlayerId == boss.MyPlayerId) throw std::invalid_argument("duplicate boss settlement player");
			player->MyLife = boss.MyLife;
			AddStat(player->MyStatistics.MyBossDamage, boss.MyRoundDamage);
		}
		for (std::size_t i = 0; i < _results.size(); ++i)
		{
			const auto& result = _results[i];
			if (std::ranges::find(_players, result.MyPlayerId, &BossPlayerProgress::MyPlayerId) == _players.end()) throw std::invalid_argument("boss result from non-participant");
			for (std::size_t j = 0; j < i; ++j)
				if (_results[j].MyPlayerId == result.MyPlayerId) throw std::invalid_argument("duplicate boss field result");
			auto& player = *std::ranges::find(next._MyPlayers, result.MyPlayerId, &MatchPlayerProgress::MyPlayerId);
			const auto coins = Coins(result.MyCoins);
			Add(player.MyPendingFunds, coins); Add(player.MyStatistics.MyFundsGained, coins);
			Add(player.MyStatistics.MyKills, static_cast<std::uint64_t>(result.MyKilled));
			AddStat(player.MyStatistics.MyDamage, result.MyDamage);
			for (auto& bounty : player.MyBounties) if (bounty.MyRoundsLeft) --bounty.MyRoundsLeft;
			std::erase_if(player.MyBounties, [](const ActiveBounty& _bounty) { return _bounty.MyRoundsLeft == 0; });
		}
		next._MyRound = _round; next._MyNow = _now; next._MyEligible.clear(); next._MyDeadline = 0;
		*this = std::move(next);
	}

	RoundSettlementSummary RoundLedger::Apply(const RoundSettlementInput& _input)
	{
		_MyRound = _input.MyRound; _MyNow = _input.MyNow; _MyEligible.clear(); _MyDeadline = 0;
		RoundSettlementSummary summary; summary.MyLosses.reserve(_MyPlayers.size());
		const auto stages = _input.MyUniteStages;
		const bool relay = !stages.empty() && stages.front().MyPlan.get().MyRelayRound != 0;
		const bool ran = std::ranges::any_of(stages, [](const UniteSettlementStage& _stage) { return !_stage.MySynthetic; });
		const auto survivors = ran ? UniteSurvivors(stages.back().MyPlan, stages.back().MyResult) : std::vector<UniteLoss>{};
		if (ran && !stages.back().MyPlan.get().MyLeakers.empty())
		{
			summary.MyThrough = 0;
			for (const auto& id : stages.back().MyPlan.get().MyLeakers)
				if (const auto loss = std::ranges::find(survivors, id, &UniteLoss::MyPlayerId); loss != survivors.end()) Add(*summary.MyThrough, loss->MyCount);
		}
		for (const auto& stage : stages)
		{
			if (stage.MySynthetic || (stage.MyResult.get().MyReason != BattleEndReason::CLEARED && stage.MyResult.get().MyReason != BattleEndReason::TIMEOUT)) continue;
			for (const auto& id : stage.MyPlan.get().MyHelpers)
			{
				const auto normal = std::ranges::find(_input.MyNormalResults, id, &BattlePlayerState::MyPlayerId);
				if (normal == _input.MyNormalResults.end() || !normal->MyPerfect || LeakCount(*normal) ||
					std::ranges::find(_input.MySyntheticPlayers, id) != _input.MySyntheticPlayers.end() ||
					std::ranges::find(stage.MyResult.get().MyPlayers, id, &BattlePlayerState::MyPlayerId) == stage.MyResult.get().MyPlayers.end()) continue;
				if (std::ranges::find(_MyEligible, id) == _MyEligible.end()) _MyEligible.emplace_back(id);
			}
		}
		for (auto& player : _MyPlayers)
		{
			if (!player.MyAlive) continue;
			const auto found = std::ranges::find(_input.MyNormalResults, player.MyPlayerId, &BattlePlayerState::MyPlayerId);
			const BattlePlayerState empty;
			const auto& own = found == _input.MyNormalResults.end() ? empty : *found;
			const auto counted = LeakCount(own);
			auto loss = counted;
			if (ran && std::ranges::find(stages.back().MyPlan.get().MyLeakers, player.MyPlayerId) != stages.back().MyPlan.get().MyLeakers.end())
			{
				const auto final = std::ranges::find(survivors, player.MyPlayerId, &UniteLoss::MyPlayerId);
				loss = final == survivors.end() ? 0 : final->MyCount;
			}
			loss = std::min(loss, _MyRules.MyLifeCapPerRound);
			player.MyLife -= static_cast<std::int64_t>(loss);
			summary.MyLosses.emplace_back(UniteLoss{.MyPlayerId = player.MyPlayerId, .MyCount = loss});
			auto& stats = player.MyStatistics;
			Add(stats.MyLifeLost, static_cast<std::uint64_t>(loss)); Add(stats.MyLeaks, static_cast<std::uint64_t>(counted));
			Add(stats.MyKills, static_cast<std::uint64_t>(own.MyKilled)); AddStat(stats.MyDamage, own.MyDamage); AddStat(stats.MyHealing, own.MyHealing);
			const bool perfect = !counted && own.MyPerfect;
			if (perfect) Add(stats.MyPerfectRounds, std::uint64_t{1});
			auto coins = Coins(own.MyCoins);
			// 接力 helpers 不复用；若收到重复玩家行，兼容原 Object.assign 的最后一行覆盖规则。
			for (std::size_t i = stages.size(); i > 0; --i)
			{
				const auto& players = stages[i - 1].MyResult.get().MyPlayers;
				const auto extra = std::ranges::find(players, player.MyPlayerId, &BattlePlayerState::MyPlayerId);
				if (extra == players.end()) continue;
				Add(coins, Coins(extra->MyCoins)); Add(stats.MyKills, static_cast<std::uint64_t>(extra->MyKilled)); AddStat(stats.MyDamage, extra->MyDamage);
				if (relay) AddStat(stats.MyHealing, extra->MyHealing); // 原单场结算不累计联防治疗量。
				break;
			}
			for (auto& bounty : player.MyBounties)
			{
				if (perfect && bounty.MyCard.MyPerfect) Add(coins, std::max(std::int64_t{0}, bounty.MyCard.MyCoins));
				if (bounty.MyRoundsLeft) --bounty.MyRoundsLeft;
			}
			std::erase_if(player.MyBounties, [](const ActiveBounty& _bounty) { return !_bounty.MyRoundsLeft; });
			Add(player.MyPendingFunds, coins); Add(stats.MyFundsGained, coins);
		}
		const bool canRescue = _MyRules.MyRevivalEnabled && !_input.MyTeamLifePhase && std::ranges::any_of(_MyPlayers, [&](const MatchPlayerProgress& _player) { return DonorEligible(_player); });
		for (auto& player : _MyPlayers)
		{
			if (!player.MyAlive || player.MyLife > 0) continue;
			if (canRescue && !player.MyRevived && !player.MyLeft) { player.MyLife = 0; player.MyAlive = false; player.MyPendingDeath = true; }
			else { Eliminate(player); summary.MyEliminated.emplace_back(player.MyPlayerId); }
		}
		_MyWindowOpen = canRescue && std::ranges::any_of(_MyPlayers, &MatchPlayerProgress::MyPendingDeath);
		if (_MyWindowOpen) _MyDeadline = _MyNow + RevivalWindowSeconds;
		return summary;
	}

	bool RoundLedger::DonorEligible(const MatchPlayerProgress& _player) const
	{ return _player.MyAlive && !_player.MyBot && !_player.MyLeft && _player.MyLife >= MinimumDonorLife && std::ranges::find(_MyEligible, _player.MyPlayerId) != _MyEligible.end(); }

	bool RoundLedger::RevivalOpen(double _now) const noexcept
	{ return _MyRules.MyRevivalEnabled && _MyWindowOpen && std::isfinite(_now) && std::isgreaterequal(_now, _MyNow) && std::isless(_now, _MyDeadline); }

	std::expected<void, RevivalError> RoundLedger::Revive(std::string_view _donor, std::string_view _target, unsigned _round, double _now)
	{
		if (!_MyRules.MyRevivalEnabled) return std::unexpected(RevivalError::DISABLED);
		if (!RevivalOpen(_now)) return std::unexpected(RevivalError::WINDOW_CLOSED);
		if (_round != _MyRound) return std::unexpected(RevivalError::STALE_ROUND);
		const auto donor = std::ranges::find(_MyPlayers, _donor, &MatchPlayerProgress::MyPlayerId);
		if (donor == _MyPlayers.end() || donor->MyBot || donor->MyLeft) return std::unexpected(RevivalError::UNKNOWN_PLAYER);
		if (!donor->MyAlive) return std::unexpected(RevivalError::ELIMINATED);
		if (!DonorEligible(*donor)) return std::unexpected(donor->MyLife < MinimumDonorLife ? RevivalError::LOW_LIFE : RevivalError::NOT_HELPER);
		const auto target = std::ranges::find(_MyPlayers, _target, &MatchPlayerProgress::MyPlayerId);
		if (target == _MyPlayers.end() || donor == target || target->MyAlive || !target->MyPendingDeath || target->MyLeft || target->MyRevived)
			return std::unexpected(RevivalError::INELIGIBLE_TARGET);
		Add(donor->MyStatistics.MyLifeLost, static_cast<std::uint64_t>(RevivalCost));
		target->MyRevived = true; target->MyAlive = true; target->MyPendingDeath = false; target->MyLife = 1;
		donor->MyLife -= RevivalCost; _MyNow = _now;
		return {};
	}

	void RoundLedger::Eliminate(MatchPlayerProgress& _player)
	{
		_player.MyLife = 0; _player.MyAlive = false; _player.MyPendingDeath = false; _player.MyEliminatedRound = _MyRound;
		_player.MyBounties.clear(); _player.MyPendingFunds = 0;
	}

	std::vector<std::string> RoundLedger::CloseRevival()
	{
		std::vector<std::string> eliminated; eliminated.reserve(_MyPlayers.size());
		_MyWindowOpen = false;
		for (auto& player : _MyPlayers)
			if (player.MyPendingDeath) { Eliminate(player); eliminated.emplace_back(player.MyPlayerId); }
		return eliminated;
	}

	std::vector<std::string> RoundLedger::Advance(double _now)
	{
		if (!std::isfinite(_now) || std::isless(_now, _MyNow)) throw std::invalid_argument("non-monotonic settlement time");
		_MyNow = _now;
		return _MyWindowOpen && std::isgreaterequal(_now, _MyDeadline) ? CloseRevival() : std::vector<std::string>{};
	}

	void RoundLedger::CommitToPreparation(EconomySession& _economy)
	{
		if (_MyWindowOpen || std::ranges::any_of(_MyPlayers, &MatchPlayerProgress::MyPendingDeath) || _MyRound > static_cast<unsigned>(std::numeric_limits<int>::max()))
			throw std::logic_error("settlement is awaiting rescue resolution");
		std::vector<EconomySettlement> players; players.reserve(_MyPlayers.size());
		for (const auto& player : _MyPlayers)
			players.emplace_back(EconomySettlement{.MyPlayerId = player.MyPlayerId, .MyFunds = player.MyPendingFunds, .MyEliminated = !player.MyAlive});
		_economy.ApplySettlement(static_cast<int>(_MyRound), players);
		for (auto& player : _MyPlayers) player.MyPendingFunds = 0;
	}

	std::int64_t RoundLedger::TakePendingFunds(std::string_view _player)
	{
		const auto it = std::ranges::find(_MyPlayers, _player, &MatchPlayerProgress::MyPlayerId);
		if (it == _MyPlayers.end()) throw std::out_of_range("unknown progress player");
		return std::exchange(it->MyPendingFunds, 0);
	}
	bool RoundLedger::AddBounty(std::string_view _player, ActiveBounty _bounty)
	{
		const auto found = std::ranges::find(_MyPlayers, _player, &MatchPlayerProgress::MyPlayerId);
		if (found == _MyPlayers.end() || !found->MyAlive || _bounty.MyCard.MyId.empty() || _bounty.MyCard.MyEnemyId.empty() ||
			_bounty.MyCard.MyCount == 0 || _bounty.MyCard.MyCount > 20 || _bounty.MyCard.MyCoins < 0 ||
			_bounty.MyRoundsLeft == 0 || _bounty.MyRoundsLeft > 99) return false;
		if (std::ranges::any_of(found->MyBounties, [&](const ActiveBounty& _old) { return _old.MyCard.MyId == _bounty.MyCard.MyId; })) return false;
		found->MyBounties.emplace_back(std::move(_bounty));
		return true;
	}

}
