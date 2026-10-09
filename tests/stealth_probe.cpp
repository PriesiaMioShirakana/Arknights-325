#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

int main()
{
	using namespace Stronghold;
	CombatDefinition ally{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 10000, .MyAttack = 1, .MyBaseAttackTime = 0.1, .MyBlockCount = 1},
		.MyRange = {RangeOffset{}, RangeOffset{.MyColumn = 1}}};
	BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {AllyDeployment{
		.MyPieceUid = 1, .MyDefinition = std::move(ally), .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}}}}}, .MyAutoFinish = false};
	input.MySpawns.emplace_back(0, "one", CombatDefinition{.MyId = "enemy", .MyStats = CombatStats{.MyMaxHealth = 10000}, .MyAttack = AttackProfile{.MyDisabled = true}},
		CombatRoute{.MyStart = WorldPoint{.MyX = 5.4, .MyY = 9}, .MyEnd = WorldPoint{.MyX = 0, .MyY = 9}});
	Battle battle(std::move(input));
	battle.Step();
	StatusFlags stealth;
	stealth.set(static_cast<std::size_t>(CombatStatus::STEALTH));
	(void)battle.AddBuff(2, BuffDefinition{.MyKey = "source-a", .MyFlags = stealth});
	std::cout << std::setprecision(17) << '[';
	for (unsigned tick = 0; tick < 240; ++tick)
	{
		if (tick == 5 || tick == 45 || tick == 95) (void)battle.ApplyStatus(1, CombatStatus::NO_BLOCK, 1);
		if (tick == 15) (void)battle.AddBuff(2, BuffDefinition{.MyKey = "source-b", .MyFlags = stealth, .MyStealthRestore = 0});
		if (tick == 25) (void)battle.RemoveBuff(2, "source-b");
		if (tick == 50) (void)battle.AddBuff(2, BuffDefinition{.MyKey = "source-c", .MyFlags = stealth, .MyStealthRestore = 1});
		if (tick == 55) (void)battle.RemoveBuff(2, "source-a");
		if (tick == 120) (void)battle.Relocate(1, WorldPoint{.MyX = 6, .MyY = 9});
		if (tick == 125) (void)battle.ApplyStatus(1, CombatStatus::NO_BLOCK, 5);
		battle.Step();
		if (tick) std::cout << ',';
		const auto& enemy = battle.Unit(2);
		std::cout << '[' << enemy.MyHealth << ',' << enemy.MyBlockedBy << ',' << battle.Unit(1).MyTotals.MyAttacks << ']';
	}
	std::cout << ']';
	return 0;
}
