#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

// 文本夹具只承载显式参数；实际结果由 JS 原实现产生，避免测试复写推拉公式。
int main()
{
	using namespace Stronghold;
	try
	{
		unsigned cases{};
		std::cin >> cases;
		std::cout << std::setprecision(17) << '[';
		for (unsigned scene = 0; scene < cases; ++scene)
		{
			int mode{}, terrain{}, flags{}, facing{}, options{};
			double mass{}, force{}, distance{}, stop{};
			WorldPoint start{}, from{}, direction{}, to{}, center{};
			std::cin >> mode >> terrain >> flags >> facing >> options >> mass >> force >> distance >> stop
				>> start.MyX >> start.MyY >> from.MyX >> from.MyY >> direction.MyX >> direction.MyY
				>> to.MyX >> to.MyY >> center.MyX >> center.MyY;
			FieldDefinition field;
			if (terrain == 1)
				for (int row = 0; row < FieldRows; ++row) field.MyTiles[static_cast<std::size_t>(FieldGrid::Key(row, 11))].MyWalkable = false;
			if (terrain == 2) field.MyRect = FieldRect{.MyFirstRow = 5, .MyLastRow = 13, .MyFirstColumn = 4, .MyLastColumn = 16};
			CombatDefinition ally{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyBlockCount = 3}, .MyAttack = AttackProfile{.MyDisabled = true}};
			BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {AllyDeployment{
				.MyPieceUid = 1, .MyDefinition = std::move(ally), .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}, .MyFacing = static_cast<Facing>(facing)}}}},
				.MyAutoFinish = false, .MyField = field};
			CombatDefinition enemy{.MyId = "enemy", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyMoveSpeed = 0, .MyMass = mass},
				.MyAttack = AttackProfile{.MyDisabled = true}, .MyFlying = (flags & 1) != 0, .MyLeader = (flags & 32) != 0,
				.MyWalksWhileFlying = (flags & 2) != 0, .MyStaticBody = (flags & 4) != 0, .MySharedBoss = (flags & 8) != 0};
			input.MySpawns.emplace_back(0, "one", std::move(enemy), CombatRoute{.MyStart = start, .MyEnd = WorldPoint{.MyX = 15, .MyY = 9}});
			Battle battle(std::move(input));
			battle.Step();
			if (flags & 16) (void)battle.ApplyStatus(2, CombatStatus::NO_DISPLACE, 10);
			double moved{};
			if (mode == 0) moved = battle.Displace(2, direction, distance);
			if (mode == 1) moved = battle.Push(2, force, PushOptions{.MyFrom = (options & 1) ? std::optional(from) : std::nullopt,
				.MyDirection = direction, .MyFromFacing = static_cast<Facing>(facing), .MyFixed = (options & 2) != 0,
				.MyFixedAngle = (options & 4) != 0, .MyInward = (options & 8) != 0, .MyEffect = (options & 16) != 0});
			if (mode == 2) moved = battle.Pull(2, force, PullOptions{.MyTo = to, .MyCenter = (options & 1) ? std::optional(center) : std::nullopt, .MyStopRadius = stop});
			if (mode == 3) moved = battle.PullToFront(2, 1, force);
			if (mode == 4) moved = battle.PushDistance(2, force, (options & 16) != 0);
			const auto& unit = battle.Unit(2);
			if (scene) std::cout << ',';
			std::cout << '[' << moved << ',' << unit.MyPosition.MyX << ',' << unit.MyPosition.MyY << ',' << (unit.MyBlockedBy != 0) << ']';
		}
		std::cout << ']';
		if (!std::cin) throw std::runtime_error("invalid displacement input");
		return 0;
	}
	catch (const std::exception& _error) { std::cerr << _error.what() << '\n'; return 1; }
}
