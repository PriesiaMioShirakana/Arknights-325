#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

// 相同地图旋转四个朝向；道路元数据不等同于直接指定的敌人出生路线。
int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 12; ++scene)
	{
		const auto mode = scene / 4;
		CombatDefinition source{.MyId = "source", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyAttack = 100,
			.MyDefense = 50, .MyBaseAttackTime = 0.5, .MyBlockCount = 0},
			.MyAttack = AttackProfile{.MyCanHitFlying = true, .MyRanged = true, .MyScaling = AttackScaling::REINFORCEMENT, .MyConditionalScale = 1.75},
			.MyRange = {}, .MyProfession = ProfessionDefinition{.MyKind = ProfessionTrait::TACTICIAN}};
		for (int r = -2; r <= 2; ++r)
			for (int c = -1; c <= 3; ++c) source.MyRange.emplace_back(RangeOffset{.MyRow = r, .MyColumn = c});
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {
			AllyDeployment{.MyPieceUid = 1, .MyDefinition = std::move(source), .MyPosition = WorldPoint{.MyX = 10, .MyY = 8}, .MyFacing = static_cast<Facing>(scene % 4)},
			AllyDeployment{.MyPieceUid = 2, .MyDefinition = CombatDefinition{.MyId = "waiting", .MyAttack = AttackProfile{.MyDisabled = true}},
				.MyPosition = WorldPoint{.MyX = 10, .MyY = 9}, .MyDeferred = true}}}}, .MyAutoFinish = false, .MyField = FieldDefinition{}};
		if (mode)
			input.MyGroundRoutes.emplace_back(CombatRoute{.MyStart = WorldPoint{.MyX = -0.5, .MyY = 9},
				.MySteps = {RouteStep{.MyKind = RouteStepKind::MOVE, .MyPosition = WorldPoint{.MyX = 6, .MyY = 9}},
					RouteStep{.MyKind = RouteStepKind::WAIT, .MyWaitSeconds = 2}}, .MyEnd = WorldPoint{.MyX = 20, .MyY = 9}});
		if (mode == 2)
			input.MyGroundRoutes.emplace_back(CombatRoute{.MyStart = WorldPoint{.MyY = 6},
				.MySteps = {RouteStep{.MyKind = RouteStepKind::APPEAR, .MyPosition = WorldPoint{.MyX = 10, .MyY = 7}}},
				.MyEnd = WorldPoint{.MyX = 20, .MyY = 6}});
		Battle battle(std::move(input)); battle.Start();
		if (scene) std::cout << ',';
		std::cout << '[';
		for (unsigned tick = 0; tick < 180; ++tick)
		{
			if (tick == 2)
			{
				const auto id = battle.Unit(1).MyProfession.MyReinforcement;
				const auto position = battle.Unit(id).MyPosition;
				(void)battle.SpawnEnemy(EnemySpawn{.MyOwnerId = "one", .MyDefinition = CombatDefinition{.MyId = "enemy",
					.MyStats = CombatStats{.MyMaxHealth = 100000, .MyMoveSpeed = 0}, .MyAttack = AttackProfile{.MyDisabled = true}},
					.MyRoute = CombatRoute{.MyStart = position, .MyEnd = WorldPoint{}}});
			}
			if (tick == 20) (void)battle.MoveRedeploy(1, WorldPoint{.MyX = 11, .MyY = 8});
			if (tick == 30 || tick == 70) battle.SetObstacle(9, 12, tick == 30);
			if (tick == 40) (void)battle.AddBuff(1, BuffDefinition{.MyKey = "long", .MyModifiers = std::vector{
				AttributeChange{.MyAttribute = Attribute::RANGE_EXTEND, .MyValue = 2},
				AttributeChange{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = 1}}});
			if (tick == 50 || tick == 90)
			{
				(void)battle.LoseHealth(0, battle.Unit(1).MyProfession.MyReinforcement, 100000);
				(void)battle.MoveRedeploy(1, WorldPoint{.MyX = tick == 50 ? 10.0 : 11.0, .MyY = 8});
			}
			battle.Step();
			const auto& unit = battle.Unit(1);
			const auto& token = battle.Unit(unit.MyProfession.MyReinforcement);
			const auto next = battle.FindTacticalPoint(1);
			if (tick) std::cout << ',';
			std::cout << '[' << token.MyId << ',' << token.MyPosition.MyX << ',' << token.MyPosition.MyY << ',' << token.MyHealth << ','
				<< token.MyStats.MyAttack << ',' << token.MyStats.MyDefense << ',' << unit.MyTotals.MyDamage << ','
				<< (next ? next->MyX : -1) << ',' << (next ? next->MyY : -1) << ",\"" << battle.GroundPathTiles().to_string() << "\"]";
		}
		std::cout << ']';
	}
	std::cout << ']';
}
