#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

// 固定帧序列覆盖动态层数、所属玩家、冻结免疫、抵抗和隐藏路线；与原 JS 比较状态剩余时间。
int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 6; ++scene)
	{
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyBonds = {BondLayer{.MyId = "kjeragShip", .MyLayers = 2}}},
			BattlePlayerInput{.MyPlayerId = "two"}}, .MyAutoFinish = false};
		for (unsigned enemy = 0; enemy < 4; ++enemy)
		{
			CombatDefinition definition{.MyId = "enemy", .MyStats = CombatStats{.MyMaxHealth = 10000, .MyResistance = 40},
				.MyAttack = AttackProfile{.MyDisabled = true}};
			if (enemy == 2) definition.MyImmunities.set(static_cast<std::size_t>(CombatStatus::FREEZE));
			CombatRoute route{.MyStart = WorldPoint{.MyX = 10, .MyY = 9}, .MyEnd = WorldPoint{.MyX = 0, .MyY = 9}};
			if (enemy == 3) route.MySteps = {RouteStep{.MyKind = RouteStepKind::DISAPPEAR},
				RouteStep{.MyKind = RouteStepKind::WAIT, .MyWaitSeconds = 2},
				RouteStep{.MyKind = RouteStepKind::APPEAR, .MyPosition = WorldPoint{.MyX = 10, .MyY = 9}}};
			input.MySpawns.emplace_back(EnemySpawn{.MyOwnerId = enemy == 1 ? "two" : "one",
				.MyDefinition = std::move(definition), .MyRoute = std::move(route)});
		}
		input.MyColdWinds.emplace_back(ColdWindDefinition{.MyPlayerId = "one", .MyBondId = "kjeragShip",
			.MyInterval = scene == 5 ? 0.001 : 0.47, .MyBaseDuration = scene == 4 ? -1.0 : 0.2,
			.MyDurationPerLayer = 0.3, .MyFirstDelay = scene == 0 ? 0.0 : scene == 1 ? 0.13 : -0.1,
			.MyOwnerOnly = scene == 2});
		Battle initial(std::move(input));
		// 队列内不捕获 this：注册后、启动前移动，再在有循环计时器时移动赋值。
		Battle battle(std::move(initial));
		if (scene) std::cout << ',';
		std::cout << '[';
		for (unsigned tick = 0; tick < 300; ++tick)
		{
			if (tick == 1) (void)battle.ApplyStatus(1, CombatStatus::RESIST, 3);
			if (tick == 50) battle.SetBondLayers("one", "kjeragShip", 5);
			if (tick == 110 && scene == 3) (void)battle.CancelColdWind(1);
			if (tick == 170) battle.SetBondLayers("one", "kjeragShip", 0);
			if (tick == 190) { Battle moved(std::move(battle)); battle = std::move(moved); }
			battle.Step();
			if (tick) std::cout << ',';
			std::cout << '[' << battle.ColdWindGusts(1);
			for (const auto& unit : battle.Units())
				std::cout << ',' << unit.MyStatuses.MyRemaining[static_cast<std::size_t>(CombatStatus::COLD)] << ','
					<< unit.MyStatuses.MyRemaining[static_cast<std::size_t>(CombatStatus::FREEZE)] << ','
					<< unit.MyStats.MyAttackSpeed << ',' << unit.MyStats.MyResistance << ',' << unit.MyHidden;
			std::cout << ']';
		}
		std::cout << ']';
	}
	std::cout << ']';
}
