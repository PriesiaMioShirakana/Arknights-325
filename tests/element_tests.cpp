#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	BattleInput Input()
	{
		CombatDefinition ally{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 100000, .MyAttack = 100, .MyDefense = 300, .MyResistance = 30},
			.MyAttack = AttackProfile{.MyDisabled = true},
			.MySkill = SkillDefinition{.MyKind = SkillKind::CHARGES, .MyTrigger = SkillTrigger::NEVER, .MySpCost = 10, .MyInitialSp = 25, .MyMaxCharges = 3}};
		CombatDefinition enemy{.MyId = "enemy", .MyStats = CombatStats{.MyMaxHealth = 100000, .MyAttack = 200, .MyDefense = 500, .MyResistance = 30, .MyMoveSpeed = 0},
			.MyAttack = AttackProfile{.MyDisabled = true}, .MyLeader = true};
		ally.MyStats.MyElementResistance = enemy.MyStats.MyElementResistance = 10;
		ally.MyStats.MyElementalResistance = enemy.MyStats.MyElementalResistance = 20;
		return BattleInput{
			.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {AllyDeployment{.MyPieceUid = 1, .MyDefinition = std::move(ally), .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}}}}},
			.MySpawns = {EnemySpawn{.MyOwnerId = "one", .MyDefinition = std::move(enemy), .MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = 10, .MyY = 9}, .MyEnd = WorldPoint{.MyY = 9}}}},
			.MyAutoFinish = false};
	}

	void Trace()
	{
		constexpr std::array Keys{"neuralBurst", "erosionBurst", "burnBurst", "apoptosisBurst", "necrosisBurst"};
		std::cout << std::setprecision(17) << '[';
		for (unsigned caseIndex = 0; caseIndex < 10; ++caseIndex)
		{
			Battle battle(Input());
			battle.Step();
			const auto target = caseIndex < 5 ? 1U : 2U;
			const auto source = target == 1 ? 2U : 1U;
			const auto element = static_cast<Element>(caseIndex % 5);
			(void)battle.AddBuff(source, BuffDefinition{.MyKey = "source-multiplier", .MyModifiers = std::vector<AttributeChange>{AttributeChange{.MyAttribute = Attribute::DAMAGE_DEALT_MULTIPLIER, .MyValue = 5}}});
			if (caseIndex) std::cout << ',';
			std::cout << '[';
			for (unsigned tick = 0; tick < 600; ++tick)
			{
				if (tick == 0) (void)battle.DealElement(source, target, ElementHit{.MyElement = element, .MyAmount = 400});
				if (tick == 1) (void)battle.ReduceElement(target, 20);
				if (tick == 2 || tick == 61) (void)battle.DealElement(source, target, ElementHit{.MyElement = element, .MyAmount = 3000});
				if (tick == 10 || tick == 62) (void)battle.DealElement(source, target, ElementHit{.MyElement = Element::BURN, .MyAmount = 2000});
				if (tick == 20) (void)battle.ReduceElement(target, 10000);
				if (tick == 60) (void)battle.RemoveBuff(target, Keys[caseIndex % 5]);
				battle.Step();
				const auto& unit = battle.Unit(target);
				if (tick) std::cout << ',';
				std::cout << '[' << unit.MyHealth << ',' << unit.MyStats.MyAttack << ',' << unit.MyStats.MyDefense << ',' << unit.MyStats.MyResistance << ',' << battle.SpTotal(target);
				for (const auto gauge : unit.MyElements.MyGauges) std::cout << ',' << gauge;
				std::cout << ',' << unit.MyStatuses.Has(CombatStatus::STUN) << ',' << unit.MyStatuses.MyValues[static_cast<std::size_t>(CombatStatus::PALSY)] << ',' << unit.MyStatuses.Has(CombatStatus::NO_SP) << ',' << unit.MyStatuses.Has(CombatStatus::BURST_LOCK);
				const auto view = battle.ElementView(target);
				std::cout << ',' << (view ? static_cast<int>(view->MyElement) : -1) << ',' << (view ? view->MyFill : 0) << ',' << (view ? view->MyCooldownEnd : 0) << ',' << (view ? view->MyCooldown : 0) << ']';
			}
			std::cout << ']';
		}
		std::cout << ']';
	}
}

int main()
{
	try { Trace(); return 0; }
	catch (const std::exception& _error) { std::cerr << _error.what() << '\n'; return 1; }
}
