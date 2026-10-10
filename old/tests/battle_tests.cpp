#include <iostream>
#include <limits>
#include <source_location>
#include <stronghold/simulation/battle.hpp>
#include <type_traits>

namespace
{
	using namespace Stronghold;

	static_assert(!std::is_copy_constructible_v<Battle> && !std::is_copy_assignable_v<Battle>);
	static_assert(std::is_move_constructible_v<Battle> && std::is_move_assignable_v<Battle>);

	void Check(bool _condition, const std::source_location _where = std::source_location::current())
	{
		if (!_condition)
			throw std::runtime_error(std::string(_where.file_name()) + ":" + std::to_string(_where.line()));
	}

	template <class _Callable>
	void Throws(_Callable _callable)
	{
		bool threw = false;
		try
		{
			_callable();
		}
		catch (const std::exception&)
		{
			threw = true;
		}
		Check(threw);
	}

	CombatDefinition Definition(std::string _id)
	{
		CombatDefinition definition;
		definition.MyId = std::move(_id);
		definition.MyStats.MyMaxHealth = 100;
		definition.MyStats.MyAttack = 20;
		definition.MyStats.MyBlockCount = 1;
		return definition;
	}

	BattleInput Input()
	{
		BattleInput input;
		input.MyPlayers.emplace_back("one", std::vector<AllyDeployment>{{1, Definition("guard"), {5, 9}}});
		input.MySpawns.emplace_back(0, "one", Definition("enemy"), CombatRoute{{5.5, 9}, {}, {0, 9}});
		return input;
	}

	void ValidationAndClock()
	{
		auto input = Input();
		input.MyAutoFinish = false;
		input.MySpawns.clear();
		Battle battle(input);
		battle.Advance(300);
		Check(battle.Tick() == 300 && battle.Time() == 10);
		battle.ForceEnd();
		battle.Advance(100);
		Check(battle.Time() == 10 && battle.Result().MyReason == BattleEndReason::FORCED);
		Throws([&] { (void)battle.Unit(0); });
		Throws([&] { (void)battle.Unit(2); });
		auto bad = input;
		bad.MyPlayers[0].MyUnits.push_back(bad.MyPlayers[0].MyUnits[0]);
		Throws([&] { Battle invalid(bad); });
		bad = input;
		bad.MyPlayers[0].MyUnits[0].MyDefinition.MyStats.MyAttack = std::numeric_limits<double>::quiet_NaN();
		Throws([&] { Battle invalid(bad); });
		bad = Input();
		bad.MySpawns[0].MyOwnerId = "missing";
		Throws([&] { Battle invalid(bad); });
	}

	void BlockingAndOrder()
	{
		auto input = Input();
		input.MyPlayers[0].MyUnits[0].MyFacing = Facing::LEFT;
		input.MySpawns[0].MyDefinition.MyStats.MyMaxHealth = 20;
		Battle battle(input);
		battle.Step();
		// The enemy blocks this tick after its attack check. The ally may strike its blocker behind its facing.
		Check(battle.Unit(1).MyHealth == 100 && !battle.Unit(2).MyAlive);
		Check(battle.Unit(1).MyBlocking.empty());
		Check(battle.Result().MyReason == BattleEndReason::CLEARED && battle.Result().MyKilled == 1);
		Check(battle.Unit(1).MyTotals.MyDamage == 20 && battle.Unit(1).MyTotals.MyKills == 1);
		const auto events = battle.DrainEvents();
		Check(events[0].MyKind == BattleEventKind::DEPLOYED && events[1].MyKind == BattleEventKind::SPAWNED);
		Check(events[2].MyKind == BattleEventKind::BLOCKED && events[3].MyKind == BattleEventKind::ATTACKED);
		Check(battle.DrainEvents().empty());
		input.MyPlayers[0].MyUnits[0].MyGroundPassable = false;
		Battle fenced(input);
		fenced.Step();
		Check(fenced.Unit(2).MyBlockedBy == 0 && fenced.Unit(2).MyHealth == 20);
		input.MyPlayers[0].MyUnits[0].MyGroundPassable = true;
		input.MySpawns[0].MyDefinition.MyBlockWeight = 2;
		Battle heavy(input);
		heavy.Step();
		Check(heavy.Unit(2).MyBlockedBy == 0);
	}

	void DamageAndHealing()
	{
		auto input = Input();
		input.MyAutoFinish = false;
		input.MyPlayers[0].MyUnits[0].MyDefinition.MyAttack.MyDisabled = true;
		input.MySpawns[0].MyDefinition.MyAttack.MyDisabled = true;
		input.MySpawns[0].MyDefinition.MyStats.MyDefense = 1000;
		Battle battle(input);
		battle.Step();
		battle.AddShield(2, {10, 0, 15});
		battle.AddShield(2, {0, 1, 1});
		Check(battle.DealDamage(1, 2, 100, DamageType::PHYSICAL) == 0);
		Check(battle.Unit(2).MyShields[0].MyHealth == 10);
		Check(battle.DealDamage(1, 2, 100, DamageType::PHYSICAL) == 0);
		Check(battle.Unit(2).MyShields[0].MyHealth == 5);
		Check(battle.DealDamage(1, 2, 30, DamageType::TRUE_DAMAGE) == 25);
		Check(battle.Heal(1, 2, 1000) == 25);
		Check(battle.DealDamage(1, 2, 1000, DamageType::TRUE_DAMAGE) == 100);
		Check(battle.Heal(1, 2, 100) == 0);
		Check(battle.Players()[0].MyDamage == 125);
		Throws([&] { battle.DealDamage(1, 2, -1, DamageType::PHYSICAL); });
		Throws([&] { battle.AddShield(1, {-1, 0, 1}); });
		battle.ForceEnd();
		Check(battle.DealDamage(1, 1, 1000, DamageType::TRUE_DAMAGE) == 0);
		Check(battle.Unit(1).MyHealth == 100);
	}

	void RoutesAndTimeout()
	{
		auto input = Input();
		input.MyPlayers[0].MyUnits.clear();
		auto& spawn = input.MySpawns[0];
		spawn.MyTime = 0.1;
		spawn.MyDefinition.MyStats.MyMoveSpeed = 2;
		spawn.MyDefinition.MyAttack.MyDisabled = true;
		spawn.MyRoute = {
			{10, 9},
			{{RouteStepKind::DISAPPEAR, {}}, {RouteStepKind::WAIT, {}, 0.1}, {RouteStepKind::APPEAR, {1, 9}}},
			{0, 9}
		};
		Battle battle(input);
		battle.Advance(3);
		Check(battle.Units().empty());
		battle.Step();
		Check(battle.Unit(1).MyHidden);
		battle.Advance(100);
		Check(battle.Finished() && battle.Result().MyLeaked == 1 && battle.Result().MyPlayers[0].MyLifeLost == 1);
		auto later = input.MySpawns[0];
		later.MyTime = 5;
		input.MySpawns.push_back(later);
		input.MyTimeLimit = 0.5;
		Battle timed(input);
		timed.Advance(1000);
		const auto result = timed.Result();
		Check(result.MyReason == BattleEndReason::TIMEOUT && result.MyTime == 0.5);
		Check(result.MyLeaked == 1 && result.MyUnspawned == 1 && result.MyTotal == 1);
		Check(result.MyPlayers[0].MyTotal == 1);
	}

	void RedeployAndProjectiles()
	{
		auto input = Input();
		input.MyAutoFinish = false;
		input.MyInitialDp = 0;
		auto& ally = input.MyPlayers[0].MyUnits[0];
		ally.MyDefinition.MyAttack.MyDisabled = true;
		ally.MyDefinition.MyStats.MyDeploymentCost = 3;
		ally.MyDefinition.MyStats.MyRedeploySeconds = 0.1;
		input.MySpawns[0].MyDefinition.MyStats.MyAttack = 100;
		input.MySpawns[0].MyDefinition.MyStats.MyBaseAttackTime = 30;
		Battle battle(input);
		battle.Advance(2);
		Check(!battle.Unit(1).MyAlive && battle.Unit(2).MyBlockedBy == 0);
		battle.Advance(60);
		Check(!battle.Unit(1).MyAlive);
		battle.Advance(30);
		Check(battle.Unit(1).MyAlive && battle.Unit(1).MyHealth == 100);
		Check(battle.Unit(1).MyDeploySequence > battle.Unit(2).MyDeploySequence);
		Check(battle.Players()[0].MyDeaths == 1 && battle.Players()[0].MyDp < 0.1);

		input = Input();
		auto& shooter = input.MyPlayers[0].MyUnits[0];
		shooter.MyDefinition.MyAttack.MyRanged = true;
		shooter.MyDefinition.MyAttack.MyProjectileSpeed = 10;
		shooter.MyDefinition.MyRange = {{0, 4}};
		shooter.MyDefinition.MyStats.MyRedeploySeconds = 30;
		input.MySpawns[0].MyRoute.MyStart = {9, 9};
		input.MySpawns[0].MyDefinition.MyAttack.MyDisabled = true;
		Battle shots(input);
		shots.Step();
		shots.DealDamage(2, 1, 100, DamageType::TRUE_DAMAGE);
		shots.Advance(15);
		Check(!shots.Unit(1).MyAlive && shots.Unit(2).MyHealth == 80);
		Check(shots.Unit(1).MyTotals.MyDamage == 20);

		// An incoming shot belongs to the target's old life; an immediate redeploy must not inherit it.
		input.MyAutoFinish = false;
		input.MyPlayers[0].MyUnits[0].MyDefinition.MyAttack.MyDisabled = true;
		input.MyPlayers[0].MyUnits[0].MyDefinition.MyStats.MyRedeploySeconds = 0;
		input.MySpawns[0].MyDefinition.MyAttack.MyDisabled = false;
		input.MySpawns[0].MyDefinition.MyAttack.MyRanged = true;
		input.MySpawns[0].MyDefinition.MyAttack.MyEnemyRange = 5;
		input.MySpawns[0].MyDefinition.MyStats.MyBaseAttackTime = 30;
		Battle oldShot(input);
		oldShot.Step();
		Check(oldShot.Unit(2).MyTotals.MyAttacks == 1);
		oldShot.DealDamage(0, 1, 100, DamageType::TRUE_DAMAGE);
		oldShot.Advance(20);
		Check(oldShot.Unit(1).MyAlive && oldShot.Unit(1).MyHealth == 100);
		Check(oldShot.Players()[0].MyDeaths == 1);
	}

	void MultiplePlayers()
	{
		auto input = Input();
		input.MyAutoFinish = false;
		auto ally = Definition("passive");
		ally.MyAttack.MyDisabled = true;
		input.MyPlayers[0].MyUnits = {{1, ally, {5, 9}}, {2, ally, {7, 9}}};
		input.MyPlayers.emplace_back("two", std::vector<AllyDeployment>{{1, ally, {12, 9}}, {2, ally, {14, 9}}}, true);
		input.MySpawns[0].MyOwnerId = "two";
		input.MySpawns[0].MyRoute.MyStart = {18, 9};
		input.MySpawns[0].MyDefinition.MyAttack.MyDisabled = true;
		auto leaker = input.MySpawns[0];
		leaker.MyOwnerId = "one";
		leaker.MyLifeCost = 2;
		leaker.MyDefinition.MyStats.MyMoveSpeed = 2;
		leaker.MyRoute.MyStart = leaker.MyRoute.MyEnd = {20, 9};
		input.MySpawns.emplace_back(std::move(leaker));
		Battle battle(input);
		battle.Step();
		Check(battle.Unit(1).MyDeploySequence == 1 && battle.Unit(4).MyDeploySequence == 2);
		Check(battle.Unit(2).MyDeploySequence == 3 && battle.Unit(3).MyDeploySequence == 4);
		// 漏怪按到达的右半场归属；出生和击杀计数仍保留原玩家。
		Check(battle.Players()[1].MyLeaked == 1 && battle.Players()[1].MyLifeLost == 2);
		Check(battle.DealDamage(1, 5, 100, DamageType::TRUE_DAMAGE) == 100);
		Check(battle.Players()[0].MyDamage == 100 && battle.Players()[0].MyKilled == 0);
		Check(battle.Players()[1].MyKilled == 1 && battle.Players()[1].MyDamage == 0);
	}

	void StatusLifecycle()
	{
		auto input = Input();
		input.MyAutoFinish = false;
		input.MySpawns[0].MyDefinition.MyAttack.MyDisabled = true;
		Battle battle(input);
		const auto address = &battle.Unit(1);
		battle.Step();
		Check(address == &battle.Unit(1));
		Check(battle.Unit(2).MyBlockedBy == 1);
		const auto cooldown = battle.Unit(1).MyAttackCooldown;
		Check(battle.ApplyStatus(1, CombatStatus::STUN, 0.2));
		Check(battle.Unit(2).MyBlockedBy == 0 && battle.Unit(1).MyBlocking.empty());
		battle.Advance(2);
		Check(battle.Unit(1).MyAttackCooldown == cooldown && battle.Unit(2).MyBlockedBy == 0);
		Check(battle.ApplyStatus(2, CombatStatus::UNBLOCKABLE, 2));
		Check(battle.RemoveStatus(1, CombatStatus::STUN));
		battle.Step();
		Check(battle.Unit(1).MyAttackCooldown < cooldown && battle.Unit(2).MyBlockedBy == 0);
		Check(battle.RemoveStatus(2, CombatStatus::UNBLOCKABLE));
		battle.Step();
		Check(battle.Unit(2).MyBlockedBy == 1);
		Check(battle.ApplyStatus(1, CombatStatus::DISARM, 2));
		Check(battle.ApplyStatus(1, CombatStatus::DISARM, 0.1));
		Check(battle.Unit(1).MyStatuses.MyRemaining[1] == 2);
		const auto attacks = battle.Unit(1).MyTotals.MyAttacks;
		battle.Advance(35);
		Check(battle.Unit(1).MyTotals.MyAttacks == attacks && battle.Unit(1).MyAttackCooldown == 0);
		Check(battle.RemoveStatus(1, CombatStatus::DISARM));
		Check(!battle.RemoveStatus(1, CombatStatus::DISARM));
		Check(battle.ApplyStatus(1, CombatStatus::STUN, 0.05));
		battle.Step();
		Check(battle.Unit(1).MyTotals.MyAttacks == attacks);
		battle.Step();
		Check(!battle.Unit(1).MyStatuses.Has(CombatStatus::STUN));
		Check(battle.Unit(1).MyTotals.MyAttacks == attacks + 1);
		Throws([&] { battle.ApplyStatus(1, CombatStatus::STUN, -1); });
		Throws([&] { battle.ApplyStatus(1, CombatStatus::STUN, std::numeric_limits<double>::infinity()); });
		Throws([&] { battle.ApplyStatus(1, static_cast<CombatStatus>(99), 1); });
		Check(!battle.ApplyStatus(1, CombatStatus::STUN, 0));
		battle.ForceEnd();
		Check(!battle.ApplyStatus(1, CombatStatus::STUN, 1));

		input.MySpawns[0].MyDefinition.MyStunImmune = true;
		Battle immune(input);
		immune.Step();
		Check(!immune.ApplyStatus(2, CombatStatus::STUN, 1));
		Check(immune.ApplyStatus(2, CombatStatus::STUN, 1, 1, true));
		Check(immune.Unit(2).MyBlockedBy == 1); // Enemy stun does not make it unblockable.
		Check(immune.ApplyStatus(1, CombatStatus::DISARM, 1));
		immune.DealDamage(2, 1, 100, DamageType::TRUE_DAMAGE);
		Check(!immune.Unit(1).MyStatuses.Has(CombatStatus::DISARM));
		Check(!immune.ApplyStatus(1, CombatStatus::STUN, 1));
	}

	void StatusMovementAndWindup()
	{
		auto input = Input();
		input.MyAutoFinish = false;
		input.MyPlayers[0].MyUnits[0].MyDefinition.MyAttack.MyDisabled = true;
		auto& spawn = input.MySpawns[0];
		spawn.MyRoute.MyStart = {10, 9};
		spawn.MyDefinition.MyStats.MyMoveSpeed = 2;
		spawn.MyDefinition.MyAttack.MyRanged = true;
		spawn.MyDefinition.MyAttack.MyEnemyRange = 10;
		spawn.MyDefinition.MyAttack.MyAnimationDuration = 0.6;
		spawn.MyDefinition.MyAttack.MyAnimationHit = 0.3;
		Battle battle(input);
		battle.Advance(5);
		Check(battle.Unit(2).MySwing && battle.Unit(2).MyAttackCooldown < 0.3);
		const auto cooldown = battle.Unit(2).MyAttackCooldown;
		Check(battle.ApplyStatus(2, CombatStatus::STUN, 1));
		battle.Advance(5);
		Check(!battle.Unit(2).MySwing && battle.Unit(2).MyAttackCooldown == cooldown);
		Check(battle.Unit(2).MyPosition.MyX == 10);
		Check(battle.RemoveStatus(2, CombatStatus::STUN));
		battle.Step();
		Check(battle.Unit(2).MySwing && battle.Unit(2).MyAttackCooldown == 0.3);

		spawn.MyDefinition.MyAttack.MyDisabled = true;
		Battle moving(input);
		moving.Step();
		const auto position = moving.Unit(2).MyPosition;
		Check(moving.ApplyStatus(2, CombatStatus::NO_MOVE, 0.2));
		moving.Advance(3);
		Check(moving.Unit(2).MyPosition.MyX == position.MyX);
		Check(moving.RemoveStatus(2, CombatStatus::NO_MOVE));
		moving.Step();
		Check(moving.Unit(2).MyPosition.MyX < position.MyX);
	}
	void ChoiceValidation()
	{
		auto input = Input();
		auto& effects = input.MyPlayers[0].MyChoiceEffects;
		effects.emplace_back(BattleChoiceEffect{.MyKey = "choice:test#0"});
		for (const double value : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
		{
			effects[0].MySelfHeal = value;
			Throws([&] { Battle invalid(input); });
		}
		effects[0].MySelfHeal = 0;
		effects[0].MyGate.MyKind = static_cast<ChoiceGateKind>(99);
		Throws([&] { Battle invalid(input); });
		effects[0].MyGate.MyKind = ChoiceGateKind::ALWAYS;
		effects[0].MyKey.assign(257, 'x');
		Throws([&] { Battle invalid(input); });
		effects[0].MyKey = "choice:test#0";
		effects[0].MyEnemies.emplace_back(ChoiceEnemyModifiers{.MyRank = static_cast<ChoiceEnemyRank>(99)});
		Throws([&] { Battle invalid(input); });
		effects[0].MyEnemies.clear();
		effects[0].MySelfHeal = std::numeric_limits<double>::max();
		effects.emplace_back(BattleChoiceEffect{.MyKey = "choice:test#1", .MySelfHeal = std::numeric_limits<double>::max()});
		Throws([&] { Battle invalid(input); });
	}

}

int main()
{
	try
	{
		ValidationAndClock();
		BlockingAndOrder();
		DamageAndHealing();
		RoutesAndTimeout();
		RedeployAndProjectiles();
		MultiplePlayers();
		StatusLifecycle();
		StatusMovementAndWindup();
		ChoiceValidation();
		std::cout << "9 battle test groups passed\n";
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
