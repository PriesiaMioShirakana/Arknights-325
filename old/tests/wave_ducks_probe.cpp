#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_wave_generation.hpp>
#include <stronghold/domain/unite.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	void Print(const WavePlan& _plan)
	{
		std::cout << '[';
		for (std::size_t i = 0; i < _plan.MySpawns.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& s = _plan.MySpawns[i];
			std::cout << '[' << std::quoted(s.MyEnemyId) << ',' << s.MyTime << ',' << s.MyRoute << ',' << s.MyCount << ',' << s.MyInterval << ','
				<< static_cast<unsigned>(s.MyTag) << ',' << static_cast<unsigned>(s.MySlot) << ',' << s.MyCounted << ','
				<< s.MyScale.MyHealth << ',' << s.MyScale.MyAttack << ',' << s.MyScale.MyDefense << ',' << s.MyScale.MyResistance << ',' << s.MyScale.MySpeed << ',';
			if (s.MyScale.MySupplyHealth) std::cout << *s.MyScale.MySupplyHealth; else std::cout << "null";
			std::cout << ',' << s.MyCoins << ',' << std::quoted(s.MyRewardOwner) << ',' << std::quoted(s.MyBountyId) << ',' << std::quoted(s.MyOwnerPlayer) << ']';
		}
		std::cout << ']';
	}

	void ValidateRewardCarry(const WaveGenerator& _generator, EnemySpawn _duck)
	{
		_duck.MyTime = 0; _duck.MyOwnerId = "p";
		_duck.MyDefinition.MyStats.MyMoveSpeed = 0;
		_duck.MyDefinition.MyAttack.MyDisabled = true;
		_duck.MyRoute = CombatRoute{.MyStart = {5, 9}, .MyEnd = {1, 9}};
		Battle normal(BattleInput{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "p"}}, .MySpawns = {_duck}, .MyTimeLimit = 0.1, .MyAutoFinish = false});
		normal.Advance(10);
		const auto result = normal.Result();
		const auto& leaker = result.MyPlayers.front();
		if (leaker.MyLeaks.size() != 1 || leaker.MyLifeLost != 1 || leaker.MyCoins != 0)
			throw std::logic_error("duck leak must cost one life without paying a bounty");
		const BattlePlayerState helper{.MyPlayerId = "q", .MyPerfect = true};
		const std::array players{UniteParticipant{.MyPlayerId = "p", .MyResult = std::cref(leaker)},
			UniteParticipant{.MyPlayerId = "q", .MySeat = 1, .MyResult = std::cref(helper)}};
		const auto plan = PlanUnite({}, players, ReferenceWaveGeneration().MyEnemies);
		if (!plan || plan->MyLeaks.size() != 1 || plan->MyLeaks.front().MyCoins != 1)
			throw std::logic_error("duck bounty was lost while building unite");
		const auto wave = _generator.BuildUnite(plan->MyLeaks, 1);
		const CombatDefinition ally{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 100}, .MyAttack = AttackProfile{.MyDisabled = true}};
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "q", .MyUnits = {AllyDeployment{.MyPieceUid = 1,
			.MyDefinition = ally, .MyPosition = {5, 3}}}}}, .MyTimeLimit = 10, .MyAutoFinish = false};
		input.MySpawns = _generator.MakeSpawns(wave, input.MyPlayers, {});
		input.MySpawns.front().MyTime = 0;
		input.MySpawns.front().MyDefinition.MyAttack.MyDisabled = true;
		Battle unite(std::move(input)); unite.Start(); unite.Step();
		const auto enemy = std::ranges::find(unite.Units(), UnitSide::ENEMY, &CombatUnit::MySide);
		if (enemy == unite.Units().end()) throw std::logic_error("duck did not enter unite");
		const auto uid = enemy->MyId;
		(void)unite.LoseHealth(1, uid, enemy->MyHealth + 1);
		(void)unite.LoseHealth(1, uid, 100000);
		if (unite.Players().front().MyCoins != 1) throw std::logic_error("duck must pay its unite killer exactly once");
	}
}

int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17) << '[';
	bool first = true;
	bool carryChecked = false;
	for (const auto& mode : ReferenceWaveModes())
		for (const auto seed : {0U, 1U, 42U, 20261009U, 0xffffffffU})
		{
			WaveGenerator generator(ReferenceWaveGeneration(), mode, ReferenceWaves());
			Random random(seed); const auto setup = generator.Setup(random);
			for (unsigned scenario = 0; scenario < 20; ++scenario)
			{
				const auto round = scenario < 13 ? scenario + 1 : scenario < 16 ? mode.MyBossRound : mode.MyHiddenRound;
				auto wave = scenario < 13 ? generator.BuildNormal(setup, round) : generator.BuildBoss(setup, round,
					scenario < 16 ? setup.MyBossId : setup.MyHiddenBossId, scenario % 3 == 0);
				const auto side = scenario < 13 ? WaveSide::ANY : static_cast<WaveSide>(scenario % 3);
				const std::array bounty{WaveBounty{.MyId = "card", .MyEnemyId = "enemy_1422_lrsldr", .MyCount = 3, .MyCoins = 7}};
				wave = generator.WithBounties(wave, round, bounty, "p", side, scenario < 13);
				if (scenario == 19) wave.MySpawns.clear();
				const auto count = generator.ReplaceDucks(wave, random, ReferenceDuckWave(), "p", side);
				if (!first) std::cout << ',';
				first = false;
				std::cout << '[' << std::quoted(mode.MyId) << ',' << seed << ',' << scenario << ',' << count << ',' << random.State() << ',';
				Print(wave);
				// Shared boss fields apply the second player's selection to the already edited list.
				const auto second = scenario >= 13 ? generator.ReplaceDucks(wave, random, ReferenceDuckWave(), "q", WaveSide::RIGHT) : 0;
				std::cout << ',' << second << ',' << random.State() << ','; Print(wave); std::cout << ']';
				BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "p"}, BattlePlayerInput{.MyPlayerId = "q", .MyRightHalf = true}}, .MyAutoFinish = false};
				input.MySpawns = generator.MakeSpawns(wave, input.MyPlayers, {});
				for (const auto& spawn : input.MySpawns)
					if (spawn.MyTag == EnemySpawnTag::DUCK)
					{
						if (!spawn.MyBounty || spawn.MyBounty->MyCoins != 1 || spawn.MyLifeCost != 1 ||
							!spawn.MyModifiers || spawn.MyModifiers->MyBountyCoins != 1 || !spawn.MyModifiers->MySlot.empty()) return 1;
						if (!carryChecked) { ValidateRewardCarry(generator, spawn); carryChecked = true; }
					}
				Battle validated(std::move(input));
			}
		}
	std::cout << "]\n";
	return carryChecked ? 0 : 1;
}
