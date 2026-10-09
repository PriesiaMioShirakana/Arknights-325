#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_equipment.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	void Snapshot(const Stronghold::Battle& _battle)
	{
		const auto& u = _battle.Unit(1); const auto& s = u.MyStats;
		std::cout << '[' << s.MyMaxHealth << ',' << s.MyAttack << ',' << s.MyDefense << ',' << s.MyResistance << ',' << s.MyAttackSpeed << ','
			<< s.MySpRecovery << ',' << s.MyRedeployMultiplier << ',' << s.MyResistanceIgnorePercent << ',' << s.MyTaunt << ','
			<< u.MyHealth << ',' << _battle.SpTotal(1) << ',' << u.MyAlive << ']';
	}
}

int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17);
	for (unsigned count = 0; std::cin >> count;)
	{
		std::vector<std::string> items(count); for (auto& item : items) std::cin >> item;
		CombatDefinition definition{.MyId = "op", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyAttack = 100, .MyDefense = 200,
			.MyResistance = 20, .MyAttackSpeed = 100, .MyBlockCount = 0, .MyRedeploySeconds = 70},
			.MyAttack = AttackProfile{.MyDisabled = true}, .MySkill = SkillDefinition{.MyKind = SkillKind::DURATION,
				.MyTrigger = SkillTrigger::NEVER, .MySpCost = 100, .MyInitialSp = 3, .MyDuration = 1}};
		definition.MyInitialBuffs.push_back(BuffDefinition{.MyKey = "fixture", .MyModifiers = std::vector<AttributeChange>{
			{Attribute::ATTACK_PERCENT, 0.25}, {Attribute::HEALTH_PERCENT, 0.15}}, .MyPersistent = true, .MyAllowDead = true});
		AppendEquipmentStats(definition, items);
		Battle battle(BattleInput{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "p", .MyUnits = {
			AllyDeployment{.MyPieceUid = 1, .MyDefinition = std::move(definition), .MyPosition = {5, 10}}}}}, .MyAutoFinish = false});
		std::cout << '['; Snapshot(battle);
		battle.Start(); std::cout << ','; Snapshot(battle);
		battle.Advance(30); std::cout << ','; Snapshot(battle);
		(void)battle.LoseHealth(0, 1, 100000); std::cout << ','; Snapshot(battle);
		(void)battle.Redeploy(1); std::cout << ','; Snapshot(battle);
		battle.Advance(30); std::cout << ','; Snapshot(battle);
		std::cout << "]\n";
	}
}
