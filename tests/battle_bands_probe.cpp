#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_choices.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	void Snapshot(const Battle& _battle)
	{
		std::cout << "[[";
		for (std::size_t i = 0; i < _battle.Units().size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& unit = _battle.Units()[i];
			std::cout << '[' << unit.MyHealth << ',' << unit.MyAlive << ',' << unit.MyStats.MyMaxHealth << ',' << unit.MyStats.MyAttack << ','
				<< unit.MyStats.MyRedeployMultiplier << ',' << _battle.SpTotal(unit.MyId) << ',' << unit.MySkill.MyActive << ',';
			if (std::isfinite(unit.MyRespawnAt)) std::cout << unit.MyRespawnAt; else std::cout << "null";
			std::cout << ",["; bool first = true;
			for (const auto& buff : unit.MyBuffs) if (buff.MyDefinition.MyKey.starts_with("band:"))
			{
				if (!first) std::cout << ',';
				first = false;
				std::cout << '[' << std::quoted(buff.MyDefinition.MyKey) << ',' << buff.MyDefinition.MyStacks << ',' << buff.MyDefinition.MyShield.MyHits << ']';
			}
			std::cout << "]]";
		}
		std::cout << "]," << _battle.BondLayers("p", "egirShip") << ',' << _battle.Players()[0].MyDeaths << ',' << _battle.RandomState() << ']';
	}
}

int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17);
	for (std::string band; std::cin >> band;)
	{
		unsigned scene = 0, seed = 0; std::cin >> scene >> seed;
		CombatDefinition ally{.MyId = "a", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyAttack = 100, .MyDefense = 80,
			.MyResistance = 30, .MyBlockCount = 0, .MyRedeploySeconds = 1}, .MyAttack = AttackProfile{.MyDisabled = true},
			.MySkill = SkillDefinition{.MyKind = SkillKind::DURATION, .MySpType = SpType::TIME, .MyTrigger = SkillTrigger::NEVER, .MySpCost = 10, .MyDuration = 0.1}};
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "p", .MyMirrorDeployment = scene % 2 != 0}, BattlePlayerInput{.MyPlayerId = "q"}},
			.MyAutoFinish = false, .MySeed = seed, .MyLayerGainsEnabled = scene != 7};
		constexpr std::array<WorldPoint, 5> positions{{{5, 5}, {5, 6}, {4, 5}, {6, 5}, {5, 4}}};
		for (unsigned i = 0; i < positions.size(); ++i)
		{
			auto definition = ally;
			definition.MyId = i < 2 ? (i ? "a_b" : "a") : "b" + std::to_string(i);
			definition.MyIdentity = UnitContentIdentity{.MyBaseChess = i < 2 ? "a" : "b", .MyTier = i + 1,
				.MyGolden = i == 1 || i == 3, .MyMeleePosition = scene % 3 != 0, .MyBonds = {"egirShip"}};
			input.MyPlayers[0].MyUnits.push_back({.MyPieceUid = i + 1, .MyDefinition = std::move(definition), .MyPosition = positions[i],
				.MyFacing = static_cast<Facing>(scene % 4), .MyKind = i == 4 ? UnitKind::TOKEN : UnitKind::OPERATOR,
				.MyDeferred = scene == 6 && i == 3});
		}
		input.MyPlayers[1].MyUnits.push_back({.MyPieceUid = 6, .MyDefinition = ally, .MyPosition = {15, 5}});
		input.MyPlayers[0].MyBandEffects = MakeBandBattleEffects(band);
		if (input.MyPlayers[0].MyBandEffects.empty()) return 2;
		input.MyPlayers[0].MyBonds.push_back({.MyId = "egirShip", .MyActive = true});
		for (unsigned i = 1; i <= scene; ++i) input.MyPlayers[0].MyBonds.push_back({.MyId = "bond" + std::to_string(i), .MyActive = true});
		auto enemy = ally; enemy.MyId = "enemy"; enemy.MySkill = {};
		enemy.MyStats.MyMaxHealth = 1000000; enemy.MyStats.MyDefense = scene % 2 ? 0 : 200; enemy.MyStats.MyResistance = scene % 2 ? 90 : 0;
		input.MySpawns.push_back({.MyOwnerId = "p", .MyDefinition = std::move(enemy), .MyRoute = CombatRoute{.MyStart = {10, 9}, .MyEnd = {0, 9}}});
		Battle battle(std::move(input)); battle.Start(); battle.Step();
		std::cout << '['; Snapshot(battle);
		for (unsigned tick = 0; tick < 80; ++tick)
		{
			if (tick % 4 == 0)
			{
				const std::array<UnitId, 1> targets{7};
				for (UnitId i = 1; i <= 5; ++i) (void)battle.ForceAttack(i, targets);
			}
			if (tick % 9 == 1) (void)battle.LoseHealth(0, 1, 100000);
			if (tick % 9 == 2) (void)battle.Redeploy(1);
			if (tick % 11 == 3) (void)battle.LoseHealth(0, 2, 100000);
			if (tick % 11 == 4) (void)battle.Redeploy(2);
			if (tick % 5 == 2) for (UnitId i = 1; i <= 4; ++i) if (battle.Unit(i).MyAlive) { (void)battle.GainSp(i, 10); (void)battle.ActivateSkill(i); }
			if (tick % 7 == 3) for (UnitId i = 1; i <= 4; ++i) battle.EndSkill(i, SkillReason::STOPPED);
			if (tick % 4 == 1) for (UnitId i = 1; i <= 6; ++i) (void)battle.DealDamage(7, i, 55, DamageType::TRUE_DAMAGE);
			if (tick % 3 == 0) (void)battle.DealDamage(1, 7, 150, tick % 2 ? DamageType::PHYSICAL : DamageType::ARTS);
			std::cout << ','; Snapshot(battle);
			battle.Step(); std::cout << ','; Snapshot(battle);
		}
		std::cout << "]\n";
		if (!battle.ContentErrors().empty()) return 1;
	}
}
