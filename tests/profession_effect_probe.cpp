#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;
	struct Adjustments final : CustomBond<Adjustments>
	{
		void OnMerchantPay(Battle& _battle, ContentEvent& _event)
		{
			if (std::isless(_battle.Time(), 3)) _event.MyCancel = true;
			else if (std::isless(_battle.Time(), 6)) _event.MyAmount *= 0.4;
		}
		void OnBardRegen(Battle&, ContentEvent& _event)
		{ if (_event.MyTarget == 2) _event.MyAmount *= 1.5; }
	};
}

// 每种回复的触发来源不同：普攻、多段、技能、Buff、生命流失、元素损伤不能混用。
int main()
{
	using namespace Stronghold;
	ContentRegistry registry; const auto adjustment = registry.Register<Adjustments>("adjustments"); registry.Seal();
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 8; ++scene)
	{
		CombatDefinition source{.MyId = "source", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyAttack = 100, .MyBaseAttackTime = 0.5,
			.MyBlockCount = 2, .MyRedeploySeconds = 1, .MyDeploymentCost = 0},
			.MyAttack = AttackProfile{.MyDisabled = scene == 7, .MyCanHitFlying = true, .MyNoHeal = scene < 2, .MyHits = 2,
				.MyAllInRange = scene == 1, .MyOnlyDuringSkill = scene == 6},
			.MyRange = {RangeOffset{}, RangeOffset{.MyColumn = 1}, RangeOffset{.MyColumn = 2}},
			.MySkill = SkillDefinition{.MyKind = SkillKind::DURATION, .MyTrigger = SkillTrigger::NEVER,
				.MySpCost = 2, .MyInitialSp = 2, .MyDuration = 1.2, .MyManual = false},
			.MyProfession = ProfessionDefinition{.MyKind = static_cast<ProfessionTrait>(scene + 7), .MySelfHeal = 35,
				.MyHealRatio = 0.25, .MyDpOnKill = 2, .MyHpDrain = 0.4, .MyMerchantInterval = 0.7,
				.MyMerchantCost = 4, .MyRampMax = 2, .MyRampTime = 4, .MyRampInitial = 0.5, .MyAuraRatio = 0.2}};
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {
			AllyDeployment{.MyPieceUid = 1, .MyDefinition = std::move(source), .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}},
			AllyDeployment{.MyPieceUid = 2, .MyDefinition = CombatDefinition{.MyId = "patient", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyBlockCount = 0},
				.MyAttack = AttackProfile{.MyDisabled = true, .MyNoHeal = scene == 7}}, .MyPosition = WorldPoint{.MyX = 6, .MyY = 9}}}}},
			.MyInitialDp = 9, .MyDpPerSecond = 0.5, .MyAutoFinish = false, .MyContentRegistry = std::cref(registry),
			.MyContentBindings = {ContentBinding{.MyContent = adjustment, .MyPlayerId = "one"}}};
		for (unsigned i = 0; i < 2; ++i)
			input.MySpawns.emplace_back(EnemySpawn{.MyOwnerId = "one",
				.MyDefinition = CombatDefinition{.MyId = "enemy", .MyStats = CombatStats{.MyMaxHealth = 1000000, .MyDefense = 10, .MyMoveSpeed = 0}, .MyAttack = AttackProfile{.MyDisabled = true}},
				.MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = 5.4 + 1.2 * i, .MyY = 9}, .MyEnd = WorldPoint{.MyY = 9}}});
		Battle battle(std::move(input)); battle.Step();
		(void)battle.LoseHealth(0, 1, 600); (void)battle.LoseHealth(0, 2, 500);
		if (scene) std::cout << ',';
		std::cout << '[';
		for (unsigned tick = 0; tick < 450; ++tick)
		{
			if (tick % 17 == 0) { (void)battle.LoseHealth(0, 1, 30); (void)battle.LoseHealth(0, 2, 20); }
			if (tick % 25 == 0)
				for (unsigned hit = 0; hit < 3; ++hit)
					(void)battle.DealDamage(1, 4, DamageInfo{.MyAmount = 50, .MyType = DamageType::ARTS,
						.MyTags = tick % 100 == 0 ? static_cast<DamageTags>(DamageTag::DOT) : 0, .MyTraitAlly = tick % 100 == 50 ? 2U : 0U});
			if (tick == 90) (void)battle.DealElement(1, 4, ElementHit{.MyElement = Element::BURN, .MyAmount = 50});
			if (tick == 92) (void)battle.LoseHealth(1, 4, 25);
			if (tick == 93) (void)battle.DealDamage(1, 4, 50, DamageType::ELEMENTAL);
			if (tick == 100) (void)battle.LoseHealth(1, 3, 10000000);
			if (tick == 110 || tick == 190) (void)battle.ActivateSkill(1, true);
			if (tick == 120) { (void)battle.ApplyStatus(1, CombatStatus::HEAL_FREE, 0.8); (void)battle.ApplyStatus(2, CombatStatus::HEAL_FREE, 0.8); }
			if (tick == 150) (void)battle.ApplyStatus(1, CombatStatus::STUN, 0.6);
			if (tick == 180) (void)battle.MoveRedeploy(1, WorldPoint{.MyX = 5, .MyY = 10});
			if (tick == 230) (void)battle.MoveRedeploy(1, WorldPoint{.MyX = 5, .MyY = 9});
			if (tick == 270) (void)battle.LoseHealth(0, 1, 10000);
			battle.Step();
			const auto& unit = battle.Unit(1); const auto& patient = battle.Unit(2);
			if (tick) std::cout << ',';
			std::cout << '[' << unit.MyHealth << ',' << patient.MyHealth << ',' << unit.MyTotals.MyDamage << ',' << unit.MyTotals.MyHealing << ','
				<< battle.Players()[0].MyDp << ',' << unit.MyProfession.MyRamp << ',' << unit.MyStats.MyAttack << ',' << unit.MyStats.MyBlockCount << ','
				<< unit.MyAlive << ',' << unit.MySkill.MyActive << ',' << battle.Players()[0].MyDeaths << ',' << unit.MyStats.MyHealthRegen << ',' << patient.MyStats.MyHealthRegen << ']';
		}
		if (!battle.ContentErrors().empty()) return 1;
		std::cout << ']';
	}
	std::cout << ']';
}
