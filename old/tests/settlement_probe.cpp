#include <iomanip>
#include <iostream>
#include <stronghold/domain/settlement.hpp>

namespace
{
	using namespace Stronghold;
	void Snapshot(const RoundLedger& _ledger, double _now)
	{
		std::cout << '[' << _ledger.RevivalOpen(_now) << ",[";
		for (std::size_t i = 0; i < _ledger.Players().size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& p = _ledger.Players()[i]; const auto& s = p.MyStatistics;
			std::cout << '[' << p.MyLife << ',' << p.MyAlive << ',' << p.MyPendingDeath << ',' << p.MyRevived << ',';
			if (p.MyEliminatedRound) std::cout << *p.MyEliminatedRound; else std::cout << "null";
			std::cout << ',' << p.MyPendingFunds << ',' << s.MyLifeLost << ',' << s.MyLeaks << ',' << s.MyKills << ','
				<< s.MyPerfectRounds << ',' << s.MyFundsGained << ',' << s.MyDamage << ',' << s.MyHealing << ",[";
			for (std::size_t j = 0; j < p.MyBounties.size(); ++j) { if (j) std::cout << ','; std::cout << p.MyBounties[j].MyRoundsLeft; }
			std::cout << "]]";
		}
		std::cout << "]]";
	}
}

int main()
{
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 36; ++scene)
	{
		std::vector<MatchPlayerProgress> players; players.reserve(4);
		std::vector<BattlePlayerState> normal; normal.reserve(4);
		for (unsigned i = 0; i < 4; ++i)
		{
			players.emplace_back(MatchPlayerProgress{.MyPlayerId = std::to_string(i), .MyLife = i < 2 ? 2 : i == 2 && scene % 5 == 0 ? 10 : 30,
				.MyBot = i == 2 && scene % 8 == 0, .MyLeft = i == 3 && scene % 11 == 0, .MyRevived = i == 0 && scene % 9 == 0,
				.MyBounties = {ActiveBounty{.MyCard = WaveBounty{.MyCoins = 5, .MyPerfect = true}}, ActiveBounty{.MyCard = WaveBounty{.MyCoins = 17}, .MyRoundsLeft = 2}}});
			normal.emplace_back(BattlePlayerState{.MyPlayerId = std::to_string(i), .MyKilled = 3U + i, .MyDamage = 100.0 + i, .MyHealing = 10.0 + i,
				.MyPerfect = i >= 2 && !(scene % 9 == 0 && i == 2), .MyCoins = 1.9 + i});
			for (unsigned j = 0; j < (i == 0 ? 12U : i == 1 ? 5U : 0U); ++j) normal.back().MyLeaks.emplace_back(LeakedEnemy{.MyCounted = true});
		}
		UnitePlan plan{.MyHelpers = {"2", "3"}, .MyLeakers = {"0", "1"}, .MyNotReentered = {UniteLoss{.MyPlayerId = "0", .MyCount = 1}}, .MyRelayRound = scene % 4 == 0 ? 1U : 0U};
		UnitePlan second = plan; second.MyHelpers = {"3"}; second.MyRelayRound = 2;
		const bool relay = plan.MyRelayRound != 0;
		if (relay) plan.MyHelpers = {"2"};
		BattleResult first{.MyReason = BattleEndReason::TIMEOUT};
		BattleResult last{.MyReason = BattleEndReason::CLEARED};
		for (unsigned i = 2; i < 4; ++i)
		{
			auto entry = BattlePlayerState{.MyPlayerId = std::to_string(i), .MyKilled = 2, .MyDamage = 30, .MyHealing = 4, .MyCoins = 5.8 + i};
			if (i == 3 && relay) last.MyPlayers.emplace_back(std::move(entry)); else first.MyPlayers.emplace_back(std::move(entry));
		}
		for (unsigned i = 0; i < 4; ++i) first.MyPlayers[0].MyLeaks.emplace_back(LeakedEnemy{.MySourcePlayer = i < 3 ? "0" : "1", .MyCounted = true});
		if (relay) for (unsigned i = 0; i < 4; ++i) last.MyPlayers[0].MyLeaks.emplace_back(LeakedEnemy{.MySourcePlayer = i == 0 ? "0" : "1", .MyCounted = true});
		std::vector<UniteSettlementStage> stages;
		if (scene % 6 != 0)
		{
			stages.emplace_back(UniteSettlementStage{.MyPlan = std::cref(plan), .MyResult = std::cref(first), .MySynthetic = scene % 7 == 0});
			if (relay) stages.emplace_back(UniteSettlementStage{.MyPlan = std::cref(second), .MyResult = std::cref(last), .MySynthetic = scene % 7 == 0});
		}
		const std::vector<std::string> synthetic = scene % 10 == 0 ? std::vector<std::string>{"2"} : std::vector<std::string>{};
		RoundLedger ledger(SettlementRules{.MyRevivalEnabled = scene % 12 != 0}, std::move(players));
		const RoundSettlementInput input{.MyRound = 1, .MyNow = 100, .MyTeamLifePhase = scene % 13 == 0, .MyNormalResults = normal,
			.MySyntheticPlayers = synthetic, .MyUniteStages = stages};
		const auto summary = ledger.Settle(input);
		if (scene) std::cout << ',';
		std::cout << "[[";
		for (std::size_t i = 0; i < summary.MyLosses.size(); ++i) { if (i) std::cout << ','; std::cout << summary.MyLosses[i].MyCount; }
		std::cout << "],"; if (summary.MyThrough) std::cout << *summary.MyThrough; else std::cout << "null";
		std::cout << ','; Snapshot(ledger, 100); std::cout << ",[";
		std::cout << ledger.Revive("2", "0", 2, 100.1).has_value() << ',';
		const auto now = scene % 3 == 0 ? 115.0 : 100.2;
		std::cout << ledger.Revive("2", "0", 1, now).has_value() << ',' << ledger.Revive("2", "0", 1, now).has_value() << ','
			<< ledger.Revive("3", "1", 1, now).has_value() << "],";
		Snapshot(ledger, now); (void)ledger.Advance(115); std::cout << ','; Snapshot(ledger, 115);
		// 重复结果必须拒绝且保持所有资金／期限；先输出状态，再断言拒绝。
		bool rejected = false; try { (void)ledger.Settle(input); } catch (const std::invalid_argument&) { rejected = true; }
		if (!rejected) return 1;
		std::cout << ",[";
		for (unsigned i = 0; i < 4; ++i) { if (i) std::cout << ','; std::cout << ledger.TakePendingFunds(std::to_string(i)); }
		std::cout << "]]";
	}
	std::cout << ']';
}
