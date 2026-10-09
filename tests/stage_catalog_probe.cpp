#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_stage.hpp>
#include <stronghold/simulation/battle.hpp>

// 每张实图覆盖常规/联防/首领区域及四组地图卡开关，验证装置身份、出生次序和平台高度。
int main()
{
	using namespace Stronghold;
	constexpr std::array Rects{FieldRect{.MyFirstRow = 9, .MyLastRow = 12, .MyFirstColumn = 2, .MyLastColumn = 10},
		FieldRect{.MyFirstRow = 9, .MyLastRow = 12, .MyFirstColumn = 2, .MyLastColumn = 18},
		FieldRect{.MyFirstRow = 1, .MyLastRow = 5, .MyFirstColumn = 2, .MyLastColumn = 18}};
	std::cout << std::setprecision(17) << '[';
	bool first = true;
	for (const auto& stage : ReferenceBattleStages())
	{
		if (!first) std::cout << ',';
		first = false;
		std::cout << '[' << std::quoted(std::string(stage.MyId)) << ",[";
		for (unsigned scene = 0; scene < 12; ++scene)
		{
			const auto rect = Rects[scene / 4];
			const unsigned mode = scene % 4;
			std::vector<DeviceOverride> overrides;
			overrides.reserve(stage.MyDevices.size() * 2);
			if (mode)
				for (const auto& device : stage.MyDevices)
				{
					if (device.MyAlias.empty()) continue;
					overrides.emplace_back(device.MyAlias, mode != 2);
					if (mode == 3) overrides.emplace_back(device.MyAlias, false, "one");
				}
			BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one"}, BattlePlayerInput{.MyPlayerId = "two", .MyMirrorDeployment = scene >= 8, .MyRightHalf = true}}, .MyAutoFinish = false};
			for (const auto& device : stage.MyDevices)
				if ((device.MyRole == "platform" || device.MyRole == "mound") && device.MyPosition.MyRow >= rect.MyFirstRow && device.MyPosition.MyRow <= rect.MyLastRow &&
					device.MyPosition.MyColumn >= rect.MyFirstColumn && device.MyPosition.MyColumn <= rect.MyLastColumn)
				{
					input.MyPlayers[0].MyUnits.emplace_back(AllyDeployment{.MyPieceUid = 1, .MyDefinition = CombatDefinition{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 1000},
						.MyAttack = AttackProfile{.MyDisabled = true}}, .MyPosition = WorldPoint{.MyX = static_cast<double>(device.MyPosition.MyColumn), .MyY = static_cast<double>(device.MyPosition.MyRow)}});
					break;
				}
			stage.Apply(input, rect, overrides);
			FieldGrid grid(*input.MyField);
			Battle battle(std::move(input));
			battle.Start();
			for (const auto& unit : battle.Units())
				if (unit.MyAlive && unit.MyObstacle) grid.SetObstacle(static_cast<int>(unit.MyPosition.MyY), static_cast<int>(unit.MyPosition.MyX), true, unit.MyObstacleKind);
			if (scene) std::cout << ',';
			std::cout << "[[";
			for (int key = 0; key < FieldTiles; ++key)
			{
				if (key) std::cout << ',';
				const auto tile = stage.MyTiles[static_cast<std::size_t>(key)];
				const auto flags = (tile.MyWalkable ? 1U : 0U) | (tile.MyFlyable ? 2U : 0U) | (tile.MyLow ? 4U : 0U) |
					(static_cast<unsigned>(tile.MyBuild) << 3) | (tile.MyGoal ? 32U : 0U) | (static_cast<unsigned>(tile.MyTerrain) << 6) |
					(grid.Obstacle(key / FieldColumns, key % FieldColumns, ObstacleKind::BLOCK) ? 512U : 0U) |
					(grid.Obstacle(key / FieldColumns, key % FieldColumns, ObstacleKind::CRATE) ? 1024U : 0U);
				std::cout << flags;
			}
			std::cout << "],[";
			bool firstUnit = true;
			for (const auto& unit : battle.Units())
			{
				if (!firstUnit) std::cout << ',';
				firstUnit = false;
				std::cout << '[' << std::quoted(unit.MyDefinition.MyId) << ',' << unit.MyHealth << ',' << unit.MyAlive << ',' << unit.MyRemoved << ','
					<< unit.MyPosition.MyX << ',' << unit.MyPosition.MyY << ',' << unit.MyGround << ',' << unit.MyDeploySequence << ']';
			}
			std::cout << "],[";
			if (!battle.Units().empty() && battle.Unit(1).MyKind == UnitKind::OPERATOR)
			{
				const auto position = battle.Unit(1).MyPosition;
				if (!battle.Relocate(1, position)) return 1;
				std::cout << battle.Unit(1).MyGround << ',';
				if (!battle.MoveRedeploy(1, position)) return 1;
				std::cout << battle.Unit(1).MyGround;
			}
			std::cout << "]]";
		}
		std::cout << "]]";
	}
	std::cout << ']';
}
