#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_wave_generation.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;
	void String(std::string_view _value) { std::cout << '"' << _value << '"'; }
	void Pick(const std::optional<WavePick>& _pick)
	{
		if (!_pick) { std::cout << "null"; return; }
		const auto& p = *_pick;
		std::cout << '[' << p.MyRound << ',' << static_cast<unsigned>(p.MyType) << ',';
		String(p.MySpecial); std::cout << ','; String(p.MyNormal); std::cout << ','; String(p.MyElite);
		std::cout << ',' << p.MyFlying << ',' << p.MyFirstHalf << ']';
	}

	void Plan(const WaveGenerator& _generator, const WavePlan& _plan)
	{
		std::cout << '['; String(_plan.MyTemplateId); std::cout << ',';
		if (std::isfinite(_plan.MyTimeLimit)) std::cout << _plan.MyTimeLimit; else std::cout << "null";
		std::cout << ','; Pick(_plan.MyPick); std::cout << ",[";
		for (std::size_t i = 0; i < _plan.MySpawns.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& s = _plan.MySpawns[i]; const auto& p = s.MyPreview;
			std::cout << '[' << s.MyTime << ','; String(s.MyEnemyId);
			std::cout << ',' << s.MyRoute << ',' << s.MyCount << ',' << s.MyInterval << ',' << s.MyScale.MyHealth << ','
				<< s.MyScale.MyAttack << ',' << s.MyScale.MySpeed << ',';
			if (s.MyScale.MySupplyHealth) std::cout << *s.MyScale.MySupplyHealth; else std::cout << "null";
			std::cout << ',' << static_cast<unsigned>(s.MySlot) << ',' << static_cast<unsigned>(s.MyTag) << ','
				<< (s.MyActionIndex == std::numeric_limits<std::size_t>::max() ? -1 : static_cast<std::int64_t>(s.MyActionIndex)) << ',' << s.MyCounted << ',' << p.MyUpper << ',' << p.MyFlying << ',' << p.MyElite << ',' << p.MyBoss << ',';
			if (p.MyStart) std::cout << '[' << p.MyStart->MyY << ',' << p.MyStart->MyX << ']'; else std::cout << "null";
			std::cout << ',' << s.MyBounty << ','; String(s.MyBountyId); std::cout << ','; String(s.MyOwnerPlayer);
			std::cout << ','; String(s.MySourcePlayer); std::cout << ',' << s.MyCoins << ','; String(s.MyCoins > 0 ? std::string_view(s.MyRewardOwner) : std::string_view{});
			std::cout << ',' << s.MyScale.MyDefense << ',' << s.MyScale.MyResistance << ']';
		}
		std::cout << "],[";
		for (std::size_t i = 0; i < _plan.MyActions.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& a = _plan.MyActions[i];
			std::cout << '[' << a.MyIndex << ','; String(a.MyEnemyId); std::cout << ','; String(a.MyTemplateEnemy);
			std::cout << ',' << static_cast<unsigned>(a.MySlot) << ',' << a.MyTime << ',' << a.MyCount << ',' << a.MyTemplateCount << ','
				<< a.MyWindow << ',' << a.MyRoute << ',' << a.MyValid << ',' << a.MyServer << ']';
		}
		// 整个展开队列经过实际战斗构造，确保静态计划可消费；不只是比较 DTO。
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one"}}, .MyAutoFinish = false};
		input.MySpawns = _generator.MakeSpawns(_plan, input.MyPlayers, FieldRect{});
		input.MyGroundRoutes = _generator.MakeGroundRoutes(_plan, FieldRect{});
		Battle validation(std::move(input));
		std::cout << "]]";
	}
}

int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17) << '[';
	bool first = true;
	for (const auto& mode : ReferenceWaveModes())
	{
		WaveGenerator generator(ReferenceWaveGeneration(), mode, ReferenceWaves());
		for (const auto seed : {0U, 1U, 42U, 20261009U, 0xffffffffU})
		{
			Random random(seed);
			const auto setup = generator.Setup(random);
			if (!first) std::cout << ',';
			first = false;
			std::cout << '['; String(mode.MyId); std::cout << ',' << seed << ',' << random.State() << ',';
			String(setup.MyStageId); std::cout << ','; String(setup.MyBossId); std::cout << ','; String(setup.MyHiddenBossId);
			std::cout << ",[";
			for (std::size_t i = 0; i < setup.MyFactions.size(); ++i) { if (i) std::cout << ','; std::cout << static_cast<unsigned>(setup.MyFactions[i]); }
			std::cout << "],[";
			for (std::size_t i = 0; i < setup.MyTypeSlots.size(); ++i) { if (i) std::cout << ','; std::cout << static_cast<unsigned>(setup.MyTypeSlots[i]); }
			std::cout << "],[";
			for (std::size_t i = 0; i < setup.MyPicks.size(); ++i) { if (i) std::cout << ','; Pick(setup.MyPicks[i]); }
			std::cout << "],[";
			for (unsigned round = 1; round <= ReferenceWaveGeneration().MyRoundCount; ++round)
			{
				if (round > 1) std::cout << ',';
				Plan(generator, generator.BuildNormal(setup, round));
			}
			std::cout << "],[";
			for (unsigned i = 0; i < 4; ++i)
			{
				if (i) std::cout << ',';
				Plan(generator, generator.BuildBoss(setup, i < 2 ? mode.MyBossRound : mode.MyHiddenRound,
					i < 2 ? setup.MyBossId : setup.MyHiddenBossId, i % 2 == 0));
			}
			std::cout << "],[";
			// 三种半场选择覆盖动作插入、无效飞行宿主、完美悬赏和首领模板的固定动作。
			const std::array bounties{
				WaveBounty{.MyId = "zero", .MyEnemyId = "enemy_1422_lrsldr", .MyCount = 0, .MyCoins = 7},
				WaveBounty{.MyId = "air", .MyEnemyId = "enemy_1005_yokai", .MyCount = 3, .MyCoins = 11},
				WaveBounty{.MyId = "many", .MyEnemyId = "enemy_1427_lrnazg", .MyCount = 25, .MyCoins = 9},
				WaveBounty{.MyId = "perfect", .MyEnemyId = "enemy_1042_frostd", .MyCount = 2, .MyCoins = 100, .MyPerfect = true},
				WaveBounty{.MyId = "missing", .MyEnemyId = "no_such_enemy", .MyCoins = 10}};
			for (unsigned i = 0; i < 18; ++i)
			{
				if (i) std::cout << ',';
				const bool boss = i < 9 || (i >= 12 && i < 15);
				const unsigned round = i < 3 ? 1 : i < 6 ? 10 : boss ? mode.MyBossRound : mode.MyHiddenRound;
				const auto base = i < 6 ? generator.BuildNormal(setup, round) : generator.BuildBoss(setup, round, boss ? setup.MyBossId : setup.MyHiddenBossId, false);
				Plan(generator, generator.WithBounties(base, round, bounties, "one", static_cast<WaveSide>(i % 3), i < 12));
			}
			std::cout << "],[";
			constexpr std::array<std::string_view, 8> Keys{"enemy_1422_lrsldr", "enemy_1005_yokai", "enemy_1427_lrnazg", "enemy_1042_frostd", "enemy_1000_gopro_2", "enemy_1041_lazerd", "enemy_1425_lrcmra", "enemy_1040_bombd"};
			std::vector<WaveLeak> leaks; leaks.reserve(49);
			for (unsigned i = 0; i < 48; ++i)
			{
				leaks.emplace_back(WaveLeak{.MyEnemyId = std::string(Keys[i % Keys.size()]), .MySourcePlayer = std::to_string(i % 3),
					.MyModifiers = i % 2 ? std::optional(EnemySpawnModifiers{.MyHealth = 2, .MyAttack = 1.2, .MyDefense = 1.1, .MyResistance = 0.8, .MySpeed = 1.3,
						.MySupplyHealth = 1.6, .MySlot = "S", .MyBountyId = i % 5 == 0 ? "leaked" : ""}) : std::nullopt,
					.MyToken = i % 7 == 0, .MyBounty = i % 5 == 0, .MyBountyId = i % 2 && i % 5 == 0 ? "leaked" : "",
					.MyCoins = i % 5 == 0 ? 17 : 0, .MyRewardOwner = i % 10 == 0 ? "away" : ""});
			}
			leaks.emplace_back(WaveLeak{.MyEnemyId = "no_such_enemy"});
			Plan(generator, generator.BuildUnite(leaks, 1)); std::cout << ','; Plan(generator, generator.BuildUnite(leaks, 2));
			std::cout << "]]";
		}
	}
	std::cout << ']';
}
