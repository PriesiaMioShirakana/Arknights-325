#include <iomanip>
#include <iostream>
#include <stronghold/domain/settlement.hpp>

namespace
{
	using namespace Stronghold;
	void Snapshot(FinalAssault& _fight)
	{
		std::cout << '[' << static_cast<int>(_fight.Outcome()) << ',' << _fight.Hidden() << ',' << _fight.TeamLife() << ',' << _fight.Pool().Health() << ','
			<< _fight.OvertimeApplied() << ',' << _fight.CanEnterHidden() << ",[";
		for (std::size_t i = 0; i < _fight.Players().size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& p = _fight.Players()[i];
			std::cout << '[' << std::quoted(p.MyPlayerId) << ',' << p.MyLife << ',' << p.MyRoundDamage << ',' << p.MyTotalDamage << ',' << p.MyHitStep << ']';
		}
		std::cout << "],[";
		for (std::size_t i = 0; i < _fight.Hits().size(); ++i)
		{
			if (i) std::cout << ',';
			std::cout << '[' << std::quoted(_fight.Hits()[i].MyPlayerId) << ',' << _fight.Hits()[i].MyThreshold << ']';
		}
		std::cout << "]]"; _fight.ClearHits();
	}

	void Ledger(const RoundLedger& _ledger)
	{
		std::cout << '[';
		for (std::size_t i = 0; i < _ledger.Players().size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& p = _ledger.Players()[i]; const auto& s = p.MyStatistics;
			std::cout << '[' << p.MyLife << ',' << p.MyAlive << ',' << p.MyPendingFunds << ',' << s.MyFundsGained << ',' << s.MyKills << ','
				<< s.MyDamage << ',' << s.MyBossDamage << ',' << s.MyHealing << ',' << s.MyPerfectRounds << ",[";
			for (std::size_t j = 0; j < p.MyBounties.size(); ++j) { if (j) std::cout << ','; std::cout << p.MyBounties[j].MyRoundsLeft; }
			std::cout << "]]";
		}
		std::cout << ']';
	}
}

int main()
{
	std::cout << std::setprecision(17) << "[[";
	for (unsigned scene = 0; scene < 96; ++scene)
	{
		if (scene) std::cout << ',';
		const BossHealthScale global{.MySolo = 0.25, .MyCoop = 1.5, .MyFullTeam = 4, .MyPerPlayer = scene % 3 != 0, .MyAliveScaling = scene % 4 == 0};
		const BossHealthScale mode{.MySolo = scene % 5 ? std::nullopt : std::optional(0.75), .MyCoop = scene % 7 ? std::nullopt : std::optional(-1.0),
			.MyFullTeam = scene % 8 ? std::nullopt : std::optional(7.8), .MyPerPlayer = scene % 9 ? std::nullopt : std::optional(false)};
		const bool solo = scene % 6 == 0, experiment = scene % 2 == 0;
		const std::size_t count = scene % 24;
		const auto share = BossPoolShare(mode, global, solo, scene % 11 ? std::optional(static_cast<double>(count)) : std::nullopt);
		const FinalAssaultRules rules{.MySolo = solo, .MyCapacityExperiment = experiment, .MyHasHiddenRound = scene % 5 != 0, .MyHiddenDifficultyAllowed = scene % 7 != 0,
			.MyGameSecondsPerRealSecond = scene % 3 ? 2.0 : 1.5, .MyOvertimeDrainPerRealSecond = scene % 4 ? 1.0 : 0.5};
		std::cout << '[' << share << ',' << BossPoolHealth(1000.5, share, 1.1, solo, count, experiment) << ','
			<< HiddenCoreEligible(rules, scene % 2 ? 1200.0 : 2401.0, scene % 3 ? 2 : 1, count) << ",[";
		constexpr std::array times{0.0, 240.0, 300.0, 301.999, 302.0, 304.0, 999.5};
		for (std::size_t i = 0; i < times.size(); ++i) { if (i) std::cout << ','; std::cout << BossOvertimeDue(rules, times[i]); }
		std::cout << "]]";
	}
	std::cout << "],[";
	for (unsigned scene = 0; scene < 48; ++scene)
	{
		if (scene) std::cout << ',';
		constexpr std::array counts{1U, 2U, 3U, 4U, 8U}; const unsigned count = counts[scene % counts.size()];
		std::vector<BossParticipant> players; std::vector<MatchPlayerProgress> progress; std::vector<BattlePlayerState> results;
		players.reserve(count); progress.reserve(count); results.reserve(count);
		for (unsigned i = 0; i < count; ++i)
		{
			players.emplace_back(BossParticipant{.MyPlayerId = "p" + std::to_string(i), .MySeat = static_cast<int>(10 - i), .MyLife = 11 + 3 * i, .MyActivatedLayers = scene % 4 ? 1300.0 : 300.0});
			progress.emplace_back(MatchPlayerProgress{.MyPlayerId = players.back().MyPlayerId, .MyLife = players.back().MyLife,
				.MyBounties = {ActiveBounty{.MyCard = WaveBounty{.MyCoins = 999, .MyPerfect = true}}, ActiveBounty{.MyCard = WaveBounty{.MyCoins = 7}, .MyRoundsLeft = 2}}});
			results.emplace_back(BattlePlayerState{.MyPlayerId = players.back().MyPlayerId, .MyKilled = 2 + i, .MyDamage = 100.0 + i, .MyHealing = 50, .MyCoins = 3.9 + i});
		}
		const FinalAssaultRules rules{.MySolo = scene % 7 == 0, .MyCapacityExperiment = true, .MyHasHiddenRound = scene % 3 != 0, .MyHiddenDifficultyAllowed = scene % 11 != 0};
		FinalAssault fight(rules, players, 1000); RoundLedger ledger(SettlementRules{}, std::move(progress));
		std::cout << "[[";
		for (std::size_t i = 0; i < fight.Fields().size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& f = fight.Fields()[i];
			std::cout << '[' << std::quoted(f.MyPlayers[0]) << ',' << std::quoted(f.MyPlayers[1]) << ',' << f.MyCount << ',' << f.MySoloTemplate << ']';
		}
		std::cout << "],["; Snapshot(fight);
		for (unsigned step = 0; step < 10; ++step)
		{
			switch (step)
			{
			case 0: (void)fight.Damage("p0", 199.25); break;
			case 1: (void)fight.Damage("p0", 0.75); break;
			case 2: (void)fight.Damage("p0", 301); break;
			case 3: fight.LoseLife(scene % 6 ? 1.25 : 10000); break;
			case 4: (void)fight.Pool().Damage("p0", 300); fight.Observe(); break;
			case 5: fight.Advance(301.999); break;
			case 6: fight.Advance(302); break;
			case 7: fight.Advance(304); break;
			case 8: fight.Advance(304); break;
			case 9: (void)fight.Damage("p0", 199); fight.LoseLife(10000); break;
			}
			std::cout << ','; Snapshot(fight);
		}
		std::cout << "],";
		ledger.SettleBoss(14, 500, fight.Players(), results); Ledger(ledger);
		bool rejected = false; try { ledger.SettleBoss(14, 501, fight.Players(), results); } catch (const std::invalid_argument&) { rejected = true; }
		if (!rejected) return 2;
		std::cout << ",[";
		if (fight.CanEnterHidden())
		{
			fight.BeginHidden(100); Snapshot(fight); (void)fight.Damage("p0", 30); std::cout << ','; Snapshot(fight);
			if (scene % 2) fight.LoseLife(10000); else (void)fight.Damage("p0", 70);
			std::cout << ','; Snapshot(fight);
			ledger.SettleBoss(15, 800, fight.Players(), results);
		}
		std::cout << "],"; Ledger(ledger);
		const auto result = fight.Result();
		std::cout << ",[" << result.MyVictory << ',' << result.MyHiddenReached << ',' << result.MyHiddenCleared << "]]";
	}
	std::cout << "]]";
}
