#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_match.hpp>

int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17) << '[';
	bool first = true;
	for (const auto& m : ReferenceMatchModes())
	{
		if (!first) std::cout << ',';
		first = false;
		const auto strategy = m.StrategyRules();
		std::cout << '[' << std::quoted(m.MyId) << ',' << m.MySolo << ',' << std::quoted(m.MyDifficulty) << ',' << m.MyLastRound << ',' << m.MyBossRound << ',' << m.MyHiddenRound << ",[";
		for (std::size_t i = 0; i < m.MySpecialRounds.size(); ++i) { if (i) std::cout << ','; std::cout << m.MySpecialRounds[i]; }
		std::cout << "],[";
		for (unsigned r = 1; r <= 16; ++r)
		{
			if (r > 1) std::cout << ',';
			const auto round = m.Round(r); std::cout << '[';
			if (round.MyPreparationSeconds) std::cout << *round.MyPreparationSeconds; else std::cout << "null";
			std::cout << ',' << round.MyCombatGameSeconds << ',';
			if (round.MyBossCountdownSeconds) std::cout << *round.MyBossCountdownSeconds; else std::cout << "null";
			std::cout << ']';
		}
		std::cout << "],[";
		const auto options = m.StrategyOptions();
		for (std::size_t i = 0; i < options.size(); ++i)
		{
			if (i) std::cout << ',';
			std::cout << '[' << std::quoted(options[i].MyId) << ',' << options[i].MyStartingLife << ',' << std::quoted(m.MyStrategies[i].MyEffectId) << ",[";
			for (std::size_t j = 0; j < m.MyStrategies[i].MyBonds.size(); ++j) { if (j) std::cout << ','; std::cout << std::quoted(m.MyStrategies[i].MyBonds[j]); }
			std::cout << "]]";
		}
		std::cout << "],[" << m.MyInformationSeconds << ',' << m.MyBattleCheckSeconds << ',' << strategy.MyTurnSeconds << ',' << strategy.MySkips << ',' << std::quoted(strategy.MyDefaultId)
			<< ',' << m.MySpecialDraft.MyFirstTurnSeconds << ',' << m.MySpecialDraft.MyTurnSeconds << ',' << m.MyLifeCapPerRound
			<< ',' << m.MyDisabledCoreBonds << ',' << m.MyDisabledAddonBonds << ',' << m.MyMaxUniteHelpers
			<< ',' << m.MyInitialDp << ',' << m.MyDpPerSecond << ',' << m.MyMaxDp << "],[";
		for (std::size_t i = 0; i <= m.MyBosses.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto id = i < m.MyBosses.size() ? m.MyBosses[i].MyId : "unknown";
			std::cout << '[' << std::quoted(id) << ',' << m.BossHealth(id) << ",[";
			for (std::size_t n = 0; n <= 21; ++n) { if (n) std::cout << ','; std::cout << '[' << m.BossHealth(id, n) << ',' << m.BossHealth(id, n, true) << ']'; }
			std::cout << "]]";
		}
		std::cout << "],[";
		for (unsigned i = 0; i < 8; ++i)
		{
			if (i) std::cout << ',';
			auto final = m.MyFinalAssault; final.MyCapacityExperiment = i % 2 != 0;
			std::cout << HiddenCoreEligible(final, i < 4 ? 1200 : 2401, i % 3 ? 2 : 1, i % 2 ? 8 : 4);
		}
		std::cout << "]]";
	}
	std::cout << ']';
}
