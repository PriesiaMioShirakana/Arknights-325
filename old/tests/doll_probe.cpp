#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;
	// 普通自定义不死优先于职业切换，即使没有更改 HP 也应阻止进入替身。
	struct Survive final : CustomBond<Survive>
	{
		void OnFatal(Battle& _battle, ContentEvent& _event)
		{ if (std::isless(_battle.Time(), 0.2)) _event.MyPrevented = true; }
	};
}

int main()
{
	using namespace Stronghold;
	ContentRegistry registry;
	const auto survive = registry.Register<Survive>("survive"); registry.Seal();
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 8; ++scene)
	{
		CombatDefinition source{.MyId = "source", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyAttack = 100,
			.MyBaseAttackTime = 0.5, .MyBlockCount = 2, .MyRedeploySeconds = 1}, .MyRange = {RangeOffset{}, RangeOffset{.MyColumn = 1}},
			.MySkill = SkillDefinition{.MyKind = scene == 4 ? SkillKind::PASSIVE : SkillKind::DURATION,
				.MyTrigger = SkillTrigger::NEVER, .MySpCost = 10, .MyInitialSp = 10, .MyDuration = 50,
				.MyModifiers = {AttributeChange{.MyAttribute = Attribute::ATTACK_FLAT, .MyValue = 7}}},
			.MyProfession = ProfessionDefinition{.MyKind = ProfessionTrait::DOLLKEEPER,
				.MyDollHealthMultiplier = scene == 3 ? 1.6 : 1.0, .MyDollNoAttack = scene == 2}};
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {
			AllyDeployment{.MyPieceUid = 1, .MyDefinition = std::move(source), .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}}}}},
			.MyAutoFinish = false, .MyContentRegistry = std::cref(registry)};
		if (scene == 5) input.MyContentBindings.emplace_back(ContentBinding{.MyContent = survive, .MyPlayerId = "one"});
		input.MySpawns.emplace_back(EnemySpawn{.MyOwnerId = "one", .MyDefinition = CombatDefinition{.MyId = "enemy",
			.MyStats = CombatStats{.MyMaxHealth = 1000000, .MyMoveSpeed = 0}, .MyAttack = AttackProfile{.MyDisabled = true}},
			.MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = 5.4, .MyY = 9}, .MyEnd = WorldPoint{.MyY = 9}}});
		Battle battle(std::move(input)); battle.Start();
		(void)battle.ActivateSkill(1, true);
		(void)battle.AddBuff(1, BuffDefinition{.MyKey = "external", .MyModifiers = std::vector{
			AttributeChange{.MyAttribute = Attribute::ATTACK_FLAT, .MyValue = 13}}});
		if (scene) std::cout << ',';
		std::cout << '[';
		for (unsigned tick = 0; tick < 750; ++tick)
		{
			if (tick == 1) (void)battle.ApplyStatus(1, CombatStatus::SLOW, 10);
			if (tick == 2)
			{
				if (scene == 1) (void)battle.EnterDoll(1);
				else (void)battle.LoseHealth(0, 1, 10000);
			}
			if (tick == 10 || tick == 640) (void)battle.LoseHealth(0, 1, 10000);
			if (tick == 15 && scene == 1) (void)battle.RemoveBuff(1, "trait:dollSwitching");
			if (tick == 20) (void)battle.ApplyStatus(1, CombatStatus::FREEZE, 0.3, 0, true);
			if (tick == 21) (void)battle.ApplyStatus(1, CombatStatus::STUN, 0.3, 0, true);
			if (tick == 22) (void)battle.ApplyStatus(1, CombatStatus::SLEEP, 0.3, 0, true);
			if (tick == 23) (void)battle.Heal(1, 1, 200, HealOptions{.MySelf = true});
			if (tick == 50 && scene == 6) (void)battle.LoseHealth(0, 1, 10000);
			if (tick == 60 && scene == 1) (void)battle.RemoveBuff(1, "trait:substitute");
			if (tick == 80) (void)battle.MoveRedeploy(1, WorldPoint{.MyX = 5, .MyY = 10});
			if (tick == 100) (void)battle.MoveRedeploy(1, WorldPoint{.MyX = 5, .MyY = 9});
			if (tick == 150 && scene == 7) battle.Retreat(1);
			if (tick == 160 && scene == 7) (void)battle.Redeploy(1);
			battle.Step();
			const auto& unit = battle.Unit(1);
			if (tick) std::cout << ',';
			std::cout << '[' << unit.MyHealth << ',' << unit.MyStats.MyMaxHealth << ',' << unit.MyStats.MyAttack << ',' << unit.MyStats.MyBlockCount << ','
				<< unit.MyProfession.MyDoll << ',' << unit.MyProfession.MyDollSwitching << ',' << unit.MyAlive << ',' << unit.MySkill.MyActive << ','
				<< battle.SpTotal(1) << ',' << unit.MyTotals.MyAttacks << ',' << battle.Players()[0].MyDeaths;
			for (const auto status : {CombatStatus::STUN, CombatStatus::SLOW, CombatStatus::NO_HEAL, CombatStatus::HEAL_FREE,
				CombatStatus::ISOLATED, CombatStatus::DISARM, CombatStatus::NO_SP, CombatStatus::INVULNERABLE}) std::cout << ',' << unit.MyStatuses.Has(status);
			std::cout << ']';
		}
		std::cout << ']';
	}
	std::cout << ']';
}
