#include <array>
#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

// 状态机跨正常攻击、缴械、强制攻击、免费移动、死亡重部署验证，弹道模式另外执行一遍。
int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 6; ++scene)
		for (unsigned ranged = 0; ranged < 2; ++ranged)
		{
			const auto kind = static_cast<ProfessionTrait>(scene + 1);
			AttackProfile attack{.MyCanHitFlying = true, .MyRanged = ranged != 0, .MyMaxTargets = scene == 2 ? 2U : 1U,
				.MyProjectileSpeed = 11, .MyHits = 2, .MyOnlyDuringSkill = scene == 3,
				.MyScaling = scene == 0 ? AttackScaling::HUNTER : scene == 1 ? AttackScaling::FUNNEL : AttackScaling::NONE,
				.MyConditionalScale = 1.3};
			CombatDefinition source{.MyId = "source", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyAttack = 100,
				.MyDefense = 20, .MyResistance = 5, .MyBaseAttackTime = 0.2, .MyBlockCount = 2, .MyRedeploySeconds = 0.5},
				.MyAttack = attack, .MyRange = {RangeOffset{}, RangeOffset{.MyColumn = 1}, RangeOffset{.MyColumn = 2}},
				.MySkill = SkillDefinition{.MyKind = SkillKind::DURATION, .MyTrigger = SkillTrigger::NEVER,
					.MySpCost = 2, .MyInitialSp = 2, .MyDuration = 0.7, .MyManual = false},
				.MyProfession = ProfessionDefinition{.MyKind = kind, .MyAmmoMax = 3, .MyFunnelInitial = 0.3,
					.MyFunnelDelta = 0.2, .MyFunnelMax = 1.1, .MyStoreMax = 4, .MyGuardDefense = 2.5, .MyGuardResistance = 25, .MyDodge = 0.4}};
			BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {AllyDeployment{
				.MyPieceUid = 1, .MyDefinition = std::move(source), .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}}}}}, .MyAutoFinish = false};
			for (unsigned i = 0; i < 2; ++i)
				input.MySpawns.emplace_back(EnemySpawn{.MyTime = 2, .MyOwnerId = "one",
					.MyDefinition = CombatDefinition{.MyId = "enemy", .MyStats = CombatStats{.MyMaxHealth = 1000000, .MyDefense = 10, .MyMoveSpeed = 0}, .MyAttack = AttackProfile{.MyDisabled = true}},
					.MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = 5.4 + i * 1.2, .MyY = 9}, .MyEnd = WorldPoint{.MyY = 9}}});
			Battle battle(std::move(input)); battle.Start();
			if (scene || ranged) std::cout << ',';
			std::cout << '[';
			for (unsigned tick = 0; tick < 540; ++tick)
			{
				if (tick == 20 || tick == 220) (void)battle.ApplyStatus(1, CombatStatus::DISARM, 0.6);
				if (tick == 100 || tick == 150) (void)battle.ActivateSkill(1, true);
				if (tick == 120) (void)battle.ApplyStatus(2, CombatStatus::UNTARGETABLE, 1);
				if (tick == 140) (void)battle.MoveRedeploy(1, WorldPoint{.MyX = 5, .MyY = 10});
				if (tick == 200) (void)battle.MoveRedeploy(1, WorldPoint{.MyX = 5, .MyY = 9});
				if (tick == 280) (void)battle.ForceAttack(1);
				if (tick == 300) (void)battle.LoseHealth(0, 1, 10000);
				if (tick == 400) (void)battle.ApplyStatus(1, CombatStatus::STUN, 0.5);
				battle.Step();
				const auto& unit = battle.Unit(1);
				const auto& state = unit.MyProfession;
				if (tick) std::cout << ',';
				std::cout << '[' << unit.MyHealth << ',' << unit.MyTotals.MyDamage << ',' << unit.MyTotals.MyAttacks << ','
					<< (scene == 0 ? state.MyAmmo : 0) << ',' << (scene == 0 ? state.MyReloadAccumulator : 0) << ','
					<< (scene == 1 ? state.MyFunnelScale : 0) << ',' << state.MyStored << ',' << unit.MyStats.MyDefense << ','
					<< unit.MyStats.MyResistance << ',' << unit.MyStats.MyBlockCount << ',' << unit.MyStats.MyTaunt << ','
					<< unit.MyStats.MyPhysicalDodge << ',' << unit.MyStats.MyArtsDodge << ',' << unit.MyBlocking.size() << ']';
			}
			std::cout << ']';
		}
	std::cout << ']';
}
