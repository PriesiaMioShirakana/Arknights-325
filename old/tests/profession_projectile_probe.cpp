#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

// 弹道绑定发射者的部署身份；移动／重部署后旧回旋弹不能归还给新的部署。
int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 3; ++scene)
	{
		AttackProfile profile{.MyCanHitFlying = scene == 0, .MyRanged = true, .MyMaxTargets = scene == 0 ? 2U : 1U,
			.MyProjectileSpeed = scene == 0 ? 15.0 : 8.0, .MyGroundOnly = scene != 0, .MySplashRadius = scene == 0 ? 0.0 : scene == 1 ? 0.9 : 1.0,
			.MyBoomerang = scene == 0, .MyFortress = scene == 2};
		CombatDefinition source{.MyId = "source", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyAttack = 100,
			.MyBaseAttackTime = 0.2, .MyBlockCount = 2, .MyRedeploySeconds = 0.5, .MyDeploymentCost = 0}, .MyAttack = profile, .MyRange = {},
			.MyProfession = ProfessionDefinition{.MyKind = scene == 0 ? ProfessionTrait::LOOPSHOOTER : scene == 1 ? ProfessionTrait::BOMBARDER : ProfessionTrait::NONE,
				.MyShockScale = 0.4, .MyShockCount = 3}};
		for (int r = -1; r <= 1; ++r)
			for (int c = 0; c <= 6; ++c) source.MyRange.emplace_back(RangeOffset{.MyRow = r, .MyColumn = c});
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {
			AllyDeployment{.MyPieceUid = 1, .MyDefinition = std::move(source), .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}}}}}, .MyAutoFinish = false};
		for (unsigned i = 0; i < 3; ++i)
			input.MySpawns.emplace_back(EnemySpawn{.MyOwnerId = "one",
				.MyDefinition = CombatDefinition{.MyId = "enemy", .MyStats = CombatStats{.MyMaxHealth = 1000000, .MyDefense = 10, .MyMoveSpeed = 0},
					.MyAttack = AttackProfile{.MyDisabled = true}, .MyFlying = i == 2},
				.MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = (scene == 2 ? 5.4 : 9) + i * 0.7, .MyY = 9}, .MyEnd = WorldPoint{.MyY = 9}}});
		Battle battle(std::move(input)); battle.Step();
		if (scene) std::cout << ',';
		std::cout << '[';
		for (unsigned tick = 0; tick < 300; ++tick)
		{
			if (tick == 2) (void)battle.LoseHealth(0, 2, 10000000);
			if (tick == 8 || tick == 20) (void)battle.MoveRedeploy(1, WorldPoint{.MyX = 5, .MyY = tick == 8 ? 10.0 : 9.0});
			if (tick == 40 || tick == 80) (void)battle.AddBuff(1, BuffDefinition{.MyKey = "block",
				.MyModifiers = std::vector{AttributeChange{.MyAttribute = Attribute::BLOCK_COUNT, .MyValue = tick == 40 ? -2.0 : 0.0}}});
			if (tick == 60) battle.Retreat(1);
			if (tick == 100) { (void)battle.ForceAttack(1); (void)battle.ForceAttack(1); }
			if (tick == 140) (void)battle.AddBuff(1, BuffDefinition{.MyKey = "atk", .MyModifiers = std::vector{
				AttributeChange{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = 1}}});
			if (tick == 145) (void)battle.LoseHealth(0, 1, 10000);
			battle.Step();
			const auto& unit = battle.Unit(1);
			if (tick) std::cout << ',';
			std::cout << '[' << unit.MyTotals.MyDamage << ',' << unit.MyTotals.MyAttacks << ',' << unit.MyProfession.MyBoomerangsOut << ','
				<< unit.MyAlive << ',' << battle.Unit(3).MyHealth << ',' << battle.Unit(4).MyHealth << ']';
		}
		std::cout << ']';
	}
	std::cout << ']';
}
