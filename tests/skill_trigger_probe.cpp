#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

// 额外范围不会让治疗技能对敌人自动施放；具来源的范围实时追踪位置、增距和离场状态。
int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 8; ++scene)
	{
		const bool nearby = scene == 5 || scene == 6;
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {
			AllyDeployment{.MyPieceUid = 1, .MyDefinition = CombatDefinition{.MyId = "owner", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyAttack = 1, .MyBaseAttackTime = 0.3},
				.MyAttack = AttackProfile{.MyOnlyDuringSkill = scene == 5 || scene == 7},
				.MySkill = SkillDefinition{.MyKind = SkillKind::DURATION, .MySpCost = 1, .MyInitialSp = 1, .MyDuration = 0.4, .MyManual = false, .MyHealSkill = scene == 3}},
				.MyPosition = WorldPoint{.MyX = 5, .MyY = 9}},
			AllyDeployment{.MyPieceUid = 2, .MyDefinition = CombatDefinition{.MyId = "helper", .MyStats = CombatStats{.MyMaxHealth = 1000}, .MyAttack = AttackProfile{.MyDisabled = true}},
				.MyPosition = WorldPoint{.MyX = 10, .MyY = 9}}}}},
			.MySpawns = {EnemySpawn{.MyOwnerId = "one", .MyDefinition = CombatDefinition{.MyId = "enemy", .MyStats = CombatStats{.MyMaxHealth = 100000},
				.MyAttack = AttackProfile{.MyDisabled = true}, .MyFlying = scene == 1 || scene == 2 || scene == 6},
				.MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = nearby ? 6.0 : 11.0, .MyY = 9}, .MyEnd = WorldPoint{.MyY = 9}}}}, .MyAutoFinish = false};
		Battle battle(std::move(input));
		SkillTriggerArea area{.MySourceUnit = scene == 0 || scene == 4 || scene == 7 ? 2U : 0U, .MyCanHitFlying = scene != 2};
		area.MyMask.set(static_cast<std::size_t>(FieldGrid::Key(9, 11)));
		std::uint64_t range = nearby ? 0 : battle.AddSkillTriggerRange(1, area);
		if (scene == 6) battle.SetSkillTrigger(1, SkillTrigger::CUSTOM_RANGE);
		if (scene) std::cout << ',';
		std::cout << '[';
		for (unsigned tick = 0; tick < 300; ++tick)
		{
			if (tick == 20) (void)battle.Relocate(2, WorldPoint{.MyX = 8, .MyY = 9});
			if (tick == 25) (void)battle.AddBuff(2, BuffDefinition{.MyKey = "extend",
				.MyModifiers = std::vector<AttributeChange>{AttributeChange{.MyAttribute = Attribute::RANGE_EXTEND, .MyValue = 2}}, .MyPersistent = true});
			if (tick == 40) (void)battle.RemoveSkillTriggerRange(1, range);
			if (tick == 60 && !nearby) range = battle.AddSkillTriggerRange(1, area);
			if (tick == 80) battle.SetSkillTrigger(1, SkillTrigger::ACTIVE_RANGE, {RangeOffset{.MyColumn = 6}});
			if (tick == 110) battle.Retreat(2, true);
			if (tick == 120) battle.SetSkillTrigger(1, SkillTrigger::SKILL_RANGE, {RangeOffset{.MyColumn = 6}});
			if (tick == 140) (void)battle.ApplyStatus(3, CombatStatus::UNTARGETABLE, 1);
			if (tick == 170) battle.SetSkillTrigger(1, SkillTrigger::DEFAULT);
			if (tick == 190 && range)
			{
				area.MySourceUnit = 0; area.MyCanHitFlying = true;
				if (!battle.UpdateSkillTriggerRange(1, range, area)) return 1;
			}
			if (tick == 220) (void)battle.ApplyStatus(3, CombatStatus::SLEEP, 1);
			if (tick == 260) battle.SetSkillTrigger(1, SkillTrigger::SP_FULL);
			battle.Step();
			if (tick) std::cout << ',';
			const auto& unit = battle.Unit(1);
			const auto& skill = unit.MySkill;
			std::cout << '[' << skill.MyActivations << ',' << skill.MySp << ',' << skill.MyCharges << ',' << skill.MyActive << ',' << skill.MyTimeLeft << ',' << unit.MyTotals.MyAttacks << ',' << battle.Unit(3).MyHealth << ']';
		}
		std::cout << ']';
	}
	std::cout << ']';
}
