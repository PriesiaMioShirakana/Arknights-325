#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

// 炮台必须在普通弹道阶段之后行动；射程、选址和盟约变化一起做逐帧对照。
int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 3; ++scene)
	{
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {AllyDeployment{.MyPieceUid = 1,
			.MyDefinition = CombatDefinition{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 10000}, .MyAttack = AttackProfile{.MyDisabled = true}},
			.MyPosition = WorldPoint{.MyX = 5, .MyY = 11}}}, .MyBonds = {BondLayer{.MyId = "alpha", .MyLayers = 2}, BondLayer{.MyId = "beta", .MyLayers = 1}}}},
			.MyAutoFinish = false, .MyField = FieldDefinition{}};
		if (scene == 1)
		{
			input.MyField->MyRect = FieldRect{.MyFirstRow = 9, .MyLastRow = 12};
			input.MyField->MyTiles[FieldGrid::Key(9, 7)] = FieldTile{.MyWalkable = false, .MyLow = false, .MyBuild = FieldBuild::NONE};
			input.MyField->MyTiles[FieldGrid::Key(9, 8)].MyBuild = FieldBuild::NONE;
		}
		TurretSpawn turret{.MyAlias = "gun", .MyDevice = DeviceSpawn{.MyId = "trap_1104_aclasert",
			.MyPosition = WorldPoint{.MyX = 8, .MyY = scene == 1 ? 8.0 : 10.0}, .MyHealth = 3000, .MyDefense = 200,
			.MyAttack = 900, .MyAttackTime = 4}, .MyPlayerId = "one", .MyAttackSpeedPerLayer = 1,
			.MyMaxAttackSpeedBonus = 300, .MyFragilityPerLayer = 0.001, .MyMaxDamageScale = 1.3, .MyFragilityDuration = 2};
		for (int col = 0; col < FieldColumns; ++col) turret.MyRange.set(static_cast<std::size_t>(FieldGrid::Key(9, col)));
		input.MyTurrets.emplace_back(turret);
		input.MySpawns.emplace_back(0, "one", CombatDefinition{.MyId = "enemy", .MyStats = CombatStats{.MyMaxHealth = 100000, .MyResistance = 10},
			.MyAttack = AttackProfile{.MyDisabled = true}, .MyFlying = scene == 2},
			CombatRoute{.MyStart = WorldPoint{.MyX = 10, .MyY = 9}, .MyEnd = WorldPoint{.MyY = 9}});
		Battle battle(std::move(input));
		if (scene) std::cout << ',';
		std::cout << '[';
		for (unsigned tick = 0; tick < 360; ++tick)
		{
			if (tick == 20 && battle.SpawnTurret(turret) != 2) return 1;
			if (tick == 50) battle.SetBondLayers("one", "alpha", 400);
			if (tick == 100) (void)battle.ApplyStatus(2, CombatStatus::STUN, 2);
			if (tick == 160) battle.SetBondLayers("one", "alpha", 0);
			if (tick == 230) battle.SetBondLayers("one", "beta", 30);
			battle.Step();
			const auto& enemy = battle.Unit(3);
			const auto& gun = battle.Unit(2);
			if (tick) std::cout << ',';
			std::cout << '[' << enemy.MyHealth << ',' << enemy.MyStats.MyDamageTakenMultiplier - 1 << ',' << gun.MyStats.MyAttackSpeed << ','
				<< gun.MyTotals.MyAttacks << ',' << gun.MyPosition.MyX << ',' << gun.MyPosition.MyY << ']';
		}
		std::cout << ']';
	}
	std::cout << ']';
}
