#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_wave.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;
	void String(std::string_view _value) { std::cout << std::quoted(std::string(_value)); }

	void Board(Blackboard _board)
	{
		std::cout << '{';
		bool first = true;
		for (const auto& entry : _board.MyEntries)
		{
			if (!first) std::cout << ',';
			first = false;
			String(entry.MyKey); std::cout << ':';
			if (const auto* value = std::get_if<double>(&entry.MyValue)) std::cout << *value;
			else String(std::get<std::string_view>(entry.MyValue));
		}
		std::cout << '}';
	}

	void Groups(std::span<const WaveSpawnGroup> _groups)
	{
		std::cout << '[';
		bool first = true;
		for (const auto& group : _groups)
		{
			if (!first) std::cout << ',';
			first = false;
			std::cout << '[' << group.MyTime << ','; String(group.MyEnemyId);
			std::cout << ',' << group.MyCount << ',' << group.MyInterval << ',' << group.MyRoute << ',';
			String(group.MySlot); std::cout << ',' << static_cast<unsigned>(group.MyTag) << ',';
			String(group.MyGroup); std::cout << ','; String(group.MyPack);
			std::cout << ',' << group.MyWeight << ',' << group.MyUnharmful << ',' << static_cast<unsigned>(group.MyAction) << ']';
		}
		std::cout << ']';
	}

	void Routes(std::span<const WaveRoute> _routes)
	{
		std::cout << '[';
		bool first = true;
		for (const auto& route : _routes)
		{
			if (!first) std::cout << ',';
			first = false;
			std::cout << '[' << route.MyStart.MyY << ',' << route.MyStart.MyX << ',' << route.MyEnd.MyY << ',' << route.MyEnd.MyX << ',' << route.MyFlying << ",[";
			for (std::size_t i = 0; i < route.MySteps.size(); ++i)
			{
				if (i) std::cout << ',';
				const auto& step = route.MySteps[i];
				std::cout << '[' << static_cast<unsigned>(step.MyKind) << ',' << step.MyPosition.MyY << ',' << step.MyPosition.MyX << ',' << step.MyWaitSeconds << ']';
			}
			std::cout << "],[";
			for (std::size_t i = 0; i < route.MyPatrolSteps.size(); ++i) { if (i) std::cout << ','; std::cout << route.MyPatrolSteps[i]; }
			std::cout << "]," << route.MySpawnRandom.MyY << ',' << route.MySpawnRandom.MyX << ']';
		}
		std::cout << ']';
	}
}

// 对照全部波次元数据与 3 种区域下的展开队列；使用真实 Battle 创建验证属性与半格起点。
int main()
{
	using namespace Stronghold;
	constexpr std::array Rects{FieldRect{.MyFirstRow = 9, .MyLastRow = 12, .MyFirstColumn = 2, .MyLastColumn = 10},
		FieldRect{.MyFirstRow = 9, .MyLastRow = 12, .MyFirstColumn = 2, .MyLastColumn = 18},
		FieldRect{.MyFirstRow = 1, .MyLastRow = 5, .MyFirstColumn = 2, .MyLastColumn = 18}};
	std::cout << std::setprecision(17) << '[';
	bool first = true;
	for (const auto& wave : ReferenceWaves())
	{
		if (&ReferenceWave(wave.MyId) != &wave) throw std::runtime_error("wave index failed");
		if (!first) std::cout << ',';
		first = false;
		std::cout << '['; String(wave.MyId);
		std::cout << ",[" << static_cast<unsigned>(wave.MyKind) << ',' << wave.MySolo << ','; String(wave.MyBossId);
		std::cout << ',' << wave.MyMaxPlayTime << ',' << wave.MyInitialDp << ',' << wave.MyDpPerSecond << ',' << wave.MyMaxDp << ','
			<< wave.MyCharacterLimit << ',' << wave.MyMoveMultiplier << ','; String(wave.MyMusic); std::cout << ',' << wave.MyTotalCount << "],";
		Board(wave.MySlotCounts); std::cout << ','; Routes(wave.MyRoutes); std::cout << ','; Routes(wave.MyExtraRoutes);
		std::cout << ','; Groups(wave.MySpawns); std::cout << ",{";
		for (std::size_t i = 0; i < wave.MyBranches.size(); ++i)
		{
			const auto& branch = wave.MyBranches[i];
			if (&wave.Branch(branch.MyId) != &branch) throw std::runtime_error("branch index failed");
			if (i) std::cout << ',';
			String(branch.MyId); std::cout << ":[";
			for (std::size_t phase = 0; phase < branch.MyPhases.size(); ++phase) { if (phase) std::cout << ','; Groups(branch.MyPhases[phase].MyActions); }
			std::cout << ']';
		}
		std::cout << "},[";
		for (std::size_t i = 0; i < wave.MyEnemyOverrides.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& enemy = wave.MyEnemyOverrides[i];
			if (&wave.Enemy(enemy.MyId) != &enemy) throw std::runtime_error("override index failed");
			std::cout << '['; String(enemy.MyId); std::cout << ','; Board(enemy.MyTalent); std::cout << ','; Board(enemy.MyTalentStrings); std::cout << ",[";
			for (std::size_t sk = 0; sk < enemy.MySkills.size(); ++sk)
			{
				if (sk) std::cout << ',';
				const auto& skill = enemy.MySkills[sk];
				std::cout << '['; String(skill.MyId); std::cout << ',' << skill.MyPriority << ',' << skill.MyCooldown << ',' << skill.MyInitialCooldown << ',' << skill.MySpCost << ',';
				Board(skill.MyBlackboard); std::cout << ','; Board(skill.MyStrings); std::cout << ']';
			}
			std::cout << "]]";
		}
		std::cout << "],[";
		for (std::size_t i = 0; i < wave.MyDevices.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& device = wave.MyDevices[i];
			std::cout << '['; String(device.MyId); std::cout << ','; String(device.MyAlias);
			std::cout << ',' << device.MyPosition.MyY << ',' << device.MyPosition.MyX << ',' << static_cast<unsigned>(device.MyFacing) << ',' << device.MyHidden << ']';
		}
		std::cout << "],[";
		for (std::size_t i = 0; i < wave.MyUsedBy.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& use = wave.MyUsedBy[i];
			std::cout << '['; String(use.MyModeId); std::cout << ',' << use.MyRound << ','; String(use.MyBossId); std::cout << ']';
		}
		std::cout << "],[";
		for (unsigned scene = 0; scene < Rects.size(); ++scene)
		{
			if (scene) std::cout << ',';
			BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one"}, BattlePlayerInput{.MyPlayerId = "two", .MyRightHalf = true}},
				.MyAutoFinish = false, .MyField = FieldDefinition{.MyRect = Rects[scene]}};
			const auto spawns = wave.MakeSpawns(input.MyPlayers, Rects[scene], scene == 1 ? EnemyMultipliers{.MyHealth = 1.5, .MyAttack = 0.7, .MyDefense = 1.2, .MyResistance = 0.5, .MySpeed = 0.8} : EnemyMultipliers{});
			input.MySpawns = spawns;
			Battle validation(input); // 同时检查整个队列能通过构造时校验，不只验证动态创建。
			input.MySpawns.clear();
			Battle battle(std::move(input)); battle.Start();
			std::cout << '[';
			for (std::size_t i = 0; i < spawns.size(); ++i)
			{
				if (i) std::cout << ',';
				const auto& spawn = spawns[i];
				const auto& unit = battle.Unit(battle.SpawnEnemy(spawn));
				const auto& s = unit.MyStats;
				std::cout << '[' << spawn.MyTime << ','; String(spawn.MyDefinition.MyId); std::cout << ','; String(spawn.MyOwnerId);
				std::cout << ',' << spawn.MyCounted << ',' << spawn.MyLifeCost << ',' << static_cast<unsigned>(spawn.MyTag) << ",["
					<< unit.MyPosition.MyY << ',' << unit.MyPosition.MyX << ',' << s.MyMaxHealth << ',' << s.MyAttack << ',' << s.MyDefense << ',' << s.MyResistance << ',' << s.MyAttackSpeed << ','
					<< s.MyBaseAttackTime << ',' << s.MyMoveSpeed << ',' << s.MyMass << ',' << unit.MyDefinition.MyBlockWeight << "],[";
				for (std::size_t st = 0; st < spawn.MyRoute.MySteps.size(); ++st)
				{
					if (st) std::cout << ',';
					const auto& step = spawn.MyRoute.MySteps[st];
					std::cout << '[' << static_cast<unsigned>(step.MyKind) << ',' << step.MyPosition.MyY << ',' << step.MyPosition.MyX << ',' << step.MyWaitSeconds << ']';
				}
				std::cout << "]," << spawn.MyRoute.MyEnd.MyY << ',' << spawn.MyRoute.MyEnd.MyX << ']';
			}
			std::cout << ']';
		}
		std::cout << "],[";
		// 每个道路索引都经过适配器；最后一例使用无效索引验证全地面道路回退。
		for (std::size_t routeIndex = 0; routeIndex <= wave.MyRoutes.size(); ++routeIndex)
		{
			if (routeIndex) std::cout << ',';
			const std::array used{routeIndex};
			BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one"}}, .MyAutoFinish = false,
				.MyField = FieldDefinition{.MyRect = Rects[1]}, .MyGroundRoutes = wave.MakeGroundRoutes(used, Rects[1])};
			Battle battle(std::move(input));
			String(battle.GroundPathTiles().to_string());
		}
		std::cout << "]]";
	}
	std::cout << ']';
}
