#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	BattleInput Input(unsigned _case)
	{
		AttackProfile profile{.MyRanged = _case == 2, .MyMaxTargets = 1, .MyProjectileSpeed = 8};
		if (_case == 0 || _case == 2) { profile.MySplashRadius = 1.1; profile.MySplashScale = 0.7; }
		if (_case == 0) profile.MyHits = 2;
		if (_case == 1) { profile.MyChainCount = 4; profile.MyChainSluggish = 0.2; }
		if (_case == 3) profile.MyAllInRange = true;
		if (_case == 4) { profile.MyHealing = true; profile.MyHealChainCount = 4; }
		if (_case == 5) { profile.MyHits = 3; profile.MyHitMultiplier = 0.5; }
		CombatDefinition ally{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyAttack = 100, .MyBaseAttackTime = 0.5}, .MyAttack = profile,
			.MyRange = {RangeOffset{}, RangeOffset{.MyRow = 1}, RangeOffset{.MyColumn = 1}, RangeOffset{.MyColumn = 2}, RangeOffset{.MyColumn = 3}}};
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {AllyDeployment{.MyPieceUid = 1, .MyDefinition = ally, .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}}}}}, .MyAutoFinish = false};
		if (_case == 4)
			for (unsigned i = 0; i < 3; ++i)
			{
				auto patient = ally;
				patient.MyAttack = AttackProfile{.MyDisabled = true};
				input.MyPlayers[0].MyUnits.emplace_back(i + 2, std::move(patient), WorldPoint{.MyX = 5.0 + i, .MyY = 10});
			}
		for (unsigned i = 0; i < 4; ++i)
		{
			CombatDefinition enemy{.MyId = "enemy", .MyStats = CombatStats{.MyMaxHealth = 10000, .MyDefense = 30, .MyMoveSpeed = 0}, .MyAttack = AttackProfile{.MyDisabled = true}};
			input.MySpawns.emplace_back(0, "one", std::move(enemy), CombatRoute{.MyStart = WorldPoint{.MyX = 6.0 + i, .MyY = 9}, .MyEnd = WorldPoint{.MyY = 9}});
		}
		return input;
	}

	void Trace()
	{
		std::cout << std::setprecision(17) << '[';
		for (unsigned caseIndex = 0; caseIndex < 6; ++caseIndex)
		{
			Battle battle(Input(caseIndex));
			battle.Step();
			if (caseIndex == 4)
				for (UnitId id = 1; id <= 4; ++id) (void)battle.LoseHealth(0, id, 100 * id);
			if (caseIndex) std::cout << ',';
			std::cout << '[';
			for (unsigned tick = 0; tick < 120; ++tick)
			{
				if (caseIndex == 2 && tick == 0) (void)battle.LoseHealth(0, 2, 100000);
				if (tick == 20) (void)battle.ForceAttack(1);
				battle.Step();
				if (tick) std::cout << ',';
				std::cout << '[';
				bool first = true;
				for (const auto& unit : battle.Units())
				{
					if (!first) std::cout << ',';
					first = false;
					std::cout << '[' << unit.MyHealth << ',' << unit.MyTotals.MyDamage << ',' << unit.MyTotals.MyHealing << ',' << unit.MyTotals.MyAttacks << ']';
				}
				std::cout << ']';
			}
			std::cout << ']';
		}
		std::cout << ']';
	}

	struct NestedAttack final : CustomOperator<NestedAttack>
	{
		bool MyDid{};
		void OnAttackHit(Battle& _battle, ContentEvent& _event)
		{
			if (MyDid) return;
			MyDid = true;
			const std::array targets{_event.MyTarget};
			(void)_battle.ForceAttack(_event.MySource, targets);
		}
	};

	void Nested()
	{
		ContentRegistry registry;
		const auto content = registry.Register<NestedAttack>("nested");
		registry.Seal();
		auto input = Input(3);
		input.MyContentRegistry = registry;
		input.MyPlayers[0].MyUnits[0].MyDefinition.MyContent = content;
		Battle battle(std::move(input));
		battle.Step();
		if (!battle.ContentErrors().empty() || battle.Unit(1).MyTotals.MyAttacks != 2 ||
			std::isgreater(std::abs(battle.Unit(1).MyTotals.MyDamage - 280), 1e-9)) throw std::runtime_error("nested attack damaged outer targets");
	}
}

int main(int _argc, char**)
{
	try { if (_argc > 1) Trace(); else { Nested(); std::cout << "nested attacks passed\n"; } return 0; }
	catch (const std::exception& _error) { std::cerr << _error.what() << '\n'; return 1; }
}
