#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 12; ++scene)
	{
		SharedBossPool pool(1000);
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {AllyDeployment{
			.MyPieceUid = 1, .MyDefinition = CombatDefinition{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 1000},
				.MyAttack = AttackProfile{.MyDisabled = true}}, .MyPosition = WorldPoint{.MyX = 1, .MyY = 1}}}}},
			.MyAutoFinish = false, .MyBossBattle = scene == 10, .MySharedBoss = scene == 10 ? std::optional(std::ref(pool)) : std::nullopt};
		for (unsigned i = 0; i < 4; ++i)
		{
			const bool healer = i == 0;
			input.MySpawns.emplace_back(EnemySpawn{.MyOwnerId = "one",
				.MyDefinition = CombatDefinition{.MyId = std::to_string(i), .MyStats = CombatStats{.MyMaxHealth = i == 1 ? 2000.0 : 1000,
					.MyAttack = healer ? 100.0 : 0, .MyBaseAttackTime = 0.5, .MyMoveSpeed = healer && scene == 3 ? 2.0 : 0},
					.MyAttack = AttackProfile{.MyDisabled = !healer, .MyHealing = healer, .MyRanged = true,
						.MyEnemyRange = scene == 1 || scene == 2 ? 0.0 : 3, .MyAnimationDuration = 1.2, .MyAnimationHit = 0.9},
					.MyLeader = i == 3, .MyHitArea = scene == 2 && i == 2 ? std::optional(HitArea{.MyWidth = 3, .MyHeight = 1}) : std::nullopt},
				.MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = 10.0 + (i == 1 ? 0.9 : i == 2 ? 2.0 : 0), .MyY = i == 3 ? 11.0 : 10},
					.MyEnd = WorldPoint{.MyX = 0, .MyY = i == 3 ? 11.0 : 10}},
				.MyTag = scene == 10 && i == 3 ? EnemySpawnTag::BOSS : EnemySpawnTag::NONE});
		}
		Battle battle(std::move(input)); battle.Step();
		(void)battle.LoseHealth(0, 2, 900);
		if (scene != 11)
		{
			(void)battle.LoseHealth(0, 3, 1200); (void)battle.LoseHealth(0, 4, 600);
			(void)battle.LoseHealth(1, 5, 700);
		}
		if (scene == 4) (void)battle.ApplyStatus(5, CombatStatus::NO_HEAL, 3);
		if (scene == 5) { (void)battle.ApplyStatus(5, CombatStatus::SLEEP, 3); (void)battle.ApplyStatus(5, CombatStatus::STEALTH, 3); }
		if (scene == 6) (void)battle.ApplyStatus(2, CombatStatus::DISARM, 2);
		if (scene == 7) (void)battle.ApplyStatus(2, CombatStatus::FEAR, 2);
		if (scene == 8) (void)battle.ApplyStatus(2, CombatStatus::PALSY, 3);
		if (scene == 9) (void)battle.AddBuff(2, BuffDefinition{.MyKey = "healing", .MyModifiers = std::vector{
			AttributeChange{.MyAttribute = Attribute::HEALING_DEALT_MULTIPLIER, .MyValue = 1.5},
			AttributeChange{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = 100}}});
		if (scene) std::cout << ',';
		std::cout << '[';
		for (unsigned tick = 0; tick < 180; ++tick)
		{
			if (tick == 30) (void)battle.ApplyStatus(2, CombatStatus::STUN, 0.3);
			if (tick == 40) (void)battle.ApplyStatus(3, CombatStatus::HEAL_FREE, 1);
			if (tick == 80 && scene != 11) (void)battle.LoseHealth(0, 4, 200);
			battle.Step();
			if (tick) std::cout << ',';
			const auto& healer = battle.Unit(2);
			std::cout << '[' << healer.MyPosition.MyX << ',' << healer.MyAttackCooldown << ',' << healer.MyTotals.MyAttacks << ',' << healer.MyTotals.MyHealing;
			for (UnitId id = 2; id <= 5; ++id) std::cout << ',' << battle.Unit(id).MyHealth;
			std::cout << ']';
		}
		if (!battle.ContentErrors().empty()) return 1;
		std::cout << ']';
	}
	std::cout << ']';
}
