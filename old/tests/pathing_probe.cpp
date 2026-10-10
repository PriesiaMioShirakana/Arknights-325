#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

int main(int _argc, char** _argv)
{
	using namespace Stronghold;
	try
	{
		unsigned cases{};
		std::cin >> cases;
		std::cout << std::setprecision(17) << '[';
		for (unsigned scene = 0; scene < cases; ++scene)
		{
			FieldDefinition field;
			for (auto& tile : field.MyTiles)
			{
				unsigned flags{};
				std::cin >> flags;
				tile = FieldTile{.MyWalkable = (flags & 1) != 0, .MyFlyable = (flags & 2) != 0, .MyLow = (flags & 4) != 0, .MyBuild = static_cast<FieldBuild>((flags >> 3) & 3), .MyGoal = (flags & 32) != 0};
			}
			int startRow{}, startColumn{}, endRow{}, endColumn{};
			std::cin >> startRow >> startColumn >> endRow >> endColumn;
			BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one"}}, .MyTimeLimit = 60, .MyAutoFinish = false, .MyField = field};
			for (unsigned i = 0; i < 3; ++i)
			{
				CombatDefinition enemy{.MyId = "enemy", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyMoveSpeed = 4, .MyMass = 1}, .MyAttack = AttackProfile{.MyDisabled = true}, .MyFlying = i == 2};
				CombatRoute route{.MyStart = WorldPoint{.MyX = static_cast<double>(startColumn), .MyY = static_cast<double>(startRow)}, .MyEnd = WorldPoint{.MyX = static_cast<double>(endColumn), .MyY = static_cast<double>(endRow)}};
				if (i == 1) route.MySteps.emplace_back(RouteStepKind::WAIT, WorldPoint{}, 0.7);
				input.MySpawns.emplace_back(i * 0.2, "one", std::move(enemy), std::move(route));
			}
			Battle battle(std::move(input));
			if (scene) std::cout << ',';
			std::cout << '[';
			for (unsigned tick = 0; tick < 900; ++tick)
			{
				if (tick == 60 || tick == 120) battle.SetObstacle(startRow, startColumn + 1, tick == 60);
				if (tick == 80 || tick == 150) battle.SetObstacle(endRow, endColumn - 1, tick == 80, ObstacleKind::CRATE);
				if (_argc > 1)
				{
					if (tick == 20) (void)battle.ApplyStatus(1, CombatStatus::ATTRACT, StatusApplication{.MyDuration = 5, .MySource = 2, .MyPoint = WorldPoint{.MyX = 10, .MyY = 9}});
					if (tick == 30) (void)battle.ApplyStatus(1, CombatStatus::FEAR, 3, 2);
					if (tick == 70) (void)battle.ApplyStatus(1, CombatStatus::FEAR, 2, 1);
					if (tick == 180) (void)battle.ApplyStatus(2, CombatStatus::FEAR, 3, 1);
					if (tick == 200) (void)battle.ApplyStatus(3, CombatStatus::FEAR, 3, 1);
				}
				if (_argc > 1 && std::string_view(_argv[1]) == "displaced")
				{
					if (tick == 25 || tick == 35) (void)battle.Push(1, 5, PushOptions{.MyFrom = battle.Unit(2).MyPosition, .MyDirection = WorldPoint{.MyX = 1, .MyY = 1}});
					if (tick == 50) (void)battle.Pull(2, 1, PullOptions{.MyTo = WorldPoint{.MyX = 6, .MyY = 7}});
					if (tick == 90) (void)battle.Displace(3, WorldPoint{.MyX = -1, .MyY = 2}, 5);
					if (tick == 100) (void)battle.Pull(3, 2, PullOptions{.MyTo = WorldPoint{.MyX = 10, .MyY = 8}, .MyStopRadius = 1});
				}
				battle.Step();
				if (tick) std::cout << ',';
				std::cout << '[';
				bool first = true;
				for (const auto& unit : battle.Units())
				{
					if (!first) std::cout << ',';
					first = false;
					std::cout << '[' << unit.MyPosition.MyX << ',' << unit.MyPosition.MyY << ',' << unit.MyAlive << ',' << battle.RemainingDistance(unit.MyId) << ']';
				}
				std::cout << ']';
			}
			std::cout << ']';
		}
		std::cout << ']';
		if (!std::cin) throw std::runtime_error("invalid pathing input");
		return 0;
	}
	catch (const std::exception& _error) { std::cerr << _error.what() << '\n'; return 1; }
}
