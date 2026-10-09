#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

// 同时观察地形叠层、离开后的清理、移动再部署和气流的方向关系。
int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 15; ++scene)
	{
		const bool wind = scene >= 12, flying = scene < 12 && scene % 3 == 2;
		FieldDefinition field;
		field.MyTerrainRules.MyInfectionDuration = 3;
		for (int row = 0; row < FieldRows; ++row)
			for (int column = 0; column < FieldColumns; ++column)
			{
				auto& tile = field.MyTiles[static_cast<std::size_t>(FieldGrid::Key(row, column))];
				if (!wind && column >= 13)
				{
					tile.MyTerrain = static_cast<FieldTerrain>(scene / 3 + 1);
					if (tile.MyTerrain == FieldTerrain::DEEPSEA) tile.MyBuild = FieldBuild::NONE;
				}
				if (wind && column >= 4 && column <= 16)
					field.MyAirflow[static_cast<std::size_t>(FieldGrid::Key(row, column))] = Airflow{.MyX = 1, .MyY = 0,
						.MyAllyEqualAttack = 0.3, .MyAllyOppositeAttack = -0.2, .MyAllyVerticalAttack = 0.1,
						.MyEnemyEqualSpeed = 0.4, .MyEnemyOppositeSpeed = -0.3};
			}
		CombatDefinition ally{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 10000, .MyAttack = 100, .MyBlockCount = 0},
			.MyAttack = AttackProfile{.MyDisabled = true}, .MySkill = SkillDefinition{.MyKind = SkillKind::INSTANT,
				.MySpType = SpType::HURT, .MyTrigger = SkillTrigger::NEVER, .MySpCost = 100}};
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {AllyDeployment{
			.MyPieceUid = 1, .MyDefinition = std::move(ally), .MyPosition = WorldPoint{.MyX = 15, .MyY = 5},
			.MyFacing = scene == 13 ? Facing::LEFT : scene == 14 ? Facing::UP : Facing::RIGHT}}}}, .MyAutoFinish = false};
		input.MyField = std::move(field);
		input.MySpawns.emplace_back(0, "one", CombatDefinition{.MyId = "enemy",
			.MyStats = CombatStats{.MyMaxHealth = 10000, .MyAttack = 100, .MyMoveSpeed = 0.8, .MyMass = scene % 3 == 1 ? 3.0 : 1.0},
			.MyAttack = AttackProfile{.MyDisabled = true}, .MyFlying = flying}, CombatRoute{
			.MyStart = WorldPoint{.MyX = scene == 13 ? 4.0 : 16.0, .MyY = 9}, .MyEnd = WorldPoint{.MyX = scene == 13 ? 20.0 : 0.0, .MyY = 9}});
		Battle battle(std::move(input));
		if (scene) std::cout << ',';
		std::cout << '[';
		for (unsigned tick = 0; tick < 600; ++tick)
		{
			if (tick == 120) (void)battle.Relocate(1, WorldPoint{.MyX = 2, .MyY = 5});
			if (tick == 240) (void)battle.MoveRedeploy(1, WorldPoint{.MyX = 15, .MyY = 6});
			if (tick == 180) (void)battle.ApplyStatus(2, CombatStatus::STUN, 2);
			battle.Step();
			if (tick) std::cout << ',';
			std::cout << '[';
			for (UnitId id : {1U, 2U})
			{
				if (id == 2) std::cout << ',';
				const auto& unit = battle.Unit(id);
				std::cout << unit.MyHealth << ',' << unit.MyStats.MyAttack << ',' << unit.MyStats.MyAttackSpeed << ','
					<< unit.MyStats.MyMoveSpeed << ',' << unit.MyPosition.MyX << ',' << unit.MyPosition.MyY << ','
					<< unit.MyStatuses.Has(CombatStatus::STEALTH) << ',' << unit.MySkill.MySp;
			}
			std::cout << ']';
		}
		std::cout << ']';
	}
	std::cout << ']';
}
