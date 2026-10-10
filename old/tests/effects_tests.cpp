#include <iomanip>
#include <iostream>
#include <source_location>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	void Check(bool _condition, const std::source_location _where = std::source_location::current())
	{
		if (!_condition) throw std::runtime_error(std::string(_where.file_name()) + ":" + std::to_string(_where.line()));
	}

	bool Close(double _a, double _b)
	{ return std::islessequal(std::abs(_a - _b), 1e-9 * std::max({1.0, std::abs(_a), std::abs(_b)})); }

	Battle MakeBattle()
	{
		CombatDefinition ally{.MyId = "ally", .MyStats = {.MyMaxHealth = 1000, .MyAttack = 100, .MyDefense = 50, .MyResistance = 30, .MyBlockCount = 1}, .MyAttack = {.MyDisabled = true}};
		CombatDefinition enemy{.MyId = "enemy", .MyStats = {.MyMaxHealth = 2000, .MyAttack = 200, .MyDefense = 200, .MyResistance = 50, .MySpRecovery = 0}, .MyAttack = {.MyDisabled = true}};
		BattleInput input{.MyTimeLimit = 60, .MyAutoFinish = false};
		input.MyPlayers.emplace_back("one", std::vector<AllyDeployment>{AllyDeployment{.MyPieceUid = 1, .MyDefinition = std::move(ally), .MyPosition = {.MyX = 5, .MyY = 9}}});
		input.MySpawns.emplace_back(0, "one", std::move(enemy), CombatRoute{.MyStart = {.MyX = 10, .MyY = 9}, .MyEnd = {.MyX = 0, .MyY = 9}});
		Battle battle(std::move(input));
		battle.Step();
		return battle;
	}

	BuffDefinition Buff(std::string _key, double _duration, Attribute _attribute, double _value)
	{
		return BuffDefinition{.MyKey = std::move(_key), .MyDuration = _duration,
			.MyModifiers = std::vector<AttributeChange>{AttributeChange{.MyAttribute = _attribute, .MyValue = _value}}};
	}

	void BuffLifecycle()
	{
		auto battle = MakeBattle();
		(void)battle.LoseHealth(0, 1, 500);
		auto health = Buff("health", 2, Attribute::HEALTH_PERCENT, 1);
		const auto first = battle.AddBuff(1, health);
		Check(Close(battle.Unit(1).MyHealth, 1000) && Close(battle.Unit(1).MyStats.MyMaxHealth, 2000));
		health.MyRefresh = BuffRefresh::KEEP;
		health.MyDuration = 5;
		Check(battle.AddBuff(1, health) == first);
		Check(Close(battle.Unit(1).MyBuffs.front().MyRemaining, 2));
		health.MyRefresh = BuffRefresh::EXTEND;
		Check(battle.AddBuff(1, health) == first);
		Check(Close(battle.Unit(1).MyBuffs.front().MyRemaining, 5));
		health.MyRefresh = BuffRefresh::STACK;
		health.MyMaxStacks = 3;
		health.MyDuration = 0.1;
		(void)battle.AddBuff(1, health);
		Check(Close(battle.Unit(1).MyStats.MyMaxHealth, 3000) && Close(battle.Unit(1).MyHealth, 1500));
		battle.Advance(3);
		Check(Close(battle.Unit(1).MyStats.MyMaxHealth, 1000) && Close(battle.Unit(1).MyHealth, 500));
		auto independent = Buff("independent", 1, Attribute::ATTACK_FLAT, 10);
		independent.MyRefresh = BuffRefresh::INDEPENDENT;
		independent.MyMaxStacks = 2;
		const auto oldest = battle.AddBuff(1, independent);
		(void)battle.AddBuff(1, independent);
		(void)battle.AddBuff(1, independent);
		Check(!battle.RemoveBuff(1, oldest));
		Check(Close(battle.Unit(1).MyStats.MyAttack, 120));
		Check(battle.RemoveBuff(1, "independent") == 2);
		auto dot = BuffDefinition{.MyKey = "dot", .MySource = 1, .MyDuration = 0.1, .MyInterval = 0.05,
			.MyTickEffects = {BuffEffect{.MyAmount = 7}}, .MyExpireEffects = {BuffEffect{.MyAmount = 11}},
			.MyRemoveEffects = {BuffEffect{.MyAmount = 100}}};
		(void)battle.AddBuff(2, dot);
		battle.Advance(3);
		Check(Close(battle.Unit(2).MyHealth, 1975));
		(void)battle.AddBuff(2, dot);
		Check(battle.RemoveBuff(2, "dot") == 1 && Close(battle.Unit(2).MyHealth, 1875));
	}

	void StatusInteractions()
	{
		auto battle = MakeBattle();
		Check(battle.ApplyStatus(2, CombatStatus::RESIST, 10));
		Check(battle.ApplyStatus(2, CombatStatus::COLD, 4));
		Check(Close(battle.Unit(2).MyStats.MyAttackSpeed, 70));
		Check(battle.ApplyStatus(2, CombatStatus::COLD, 2));
		Check(!battle.Unit(2).MyStatuses.Has(CombatStatus::COLD));
		Check(battle.Unit(2).MyStatuses.Has(CombatStatus::FREEZE));
		Check(Close(battle.Unit(2).MyStats.MyResistance, 35));
		Check(Close(battle.Unit(2).MyStatuses.MyRemaining[static_cast<std::size_t>(CombatStatus::FREEZE)], 2));
		Check(battle.ApplyStatus(2, CombatStatus::WEAKEN, StatusApplication{.MyDuration = 0.1, .MyValue = 0.8}));
		Check(battle.ApplyStatus(2, CombatStatus::WEAKEN, StatusApplication{.MyDuration = 1, .MyValue = 0.3}));
		Check(Close(battle.Unit(2).MyStats.MyAttack, 40));
		battle.Advance(3);
		Check(Close(battle.Unit(2).MyStats.MyAttack, 140));
		(void)battle.RemoveStatus(2, CombatStatus::WEAKEN);
		Check(Close(battle.Unit(2).MyStats.MyAttack, 200));
		Check(battle.ApplyStatus(2, CombatStatus::SLEEP, 1));
		Check(Close(battle.DealDamage(1, 2, 100, DamageType::TRUE_DAMAGE), 0));
		Check(Close(battle.DealDamage(1, 2, DamageInfo{.MyAmount = 100, .MyType = DamageType::TRUE_DAMAGE, .MyHitSleep = true}), 100));
	}

	void DamageModifiers()
	{
		auto battle = MakeBattle();
		(void)battle.AddBuff(1, Buff("penetration", 10, Attribute::DEFENSE_IGNORE_PERCENT, 0.5));
		(void)battle.AddBuff(1, Buff("damage", 10, Attribute::DAMAGE_DEALT_MULTIPLIER, 2));
		(void)battle.ApplyStatus(2, CombatStatus::FRAGILE, 10);
		Check(Close(battle.DealDamage(1, 2, 300, DamageType::PHYSICAL), 520));
		Check(Close(battle.DealDamage(1, 2, 100, DamageType::ELEMENTAL), 200));
		Check(Close(battle.DealDamage(1, 2, DamageInfo{.MyAmount = 300, .MySourceless = true}), 130));
		(void)battle.AddBuff(2, Buff("dodge", 10, Attribute::PHYSICAL_DODGE, 1));
		Check(Close(battle.DealDamage(1, 2, 100, DamageType::PHYSICAL), 0));
		battle.AddShield(2, Shield{.MyHealth = 500});
		(void)battle.AddBuff(2, BuffDefinition{.MyKey = "barrier", .MyDuration = 10, .MyShield = {.MyHits = 1}});
		Check(Close(battle.DealDamage(1, 2, 100, DamageType::TRUE_DAMAGE), 0));
		Check(Close(battle.Unit(2).MyShields.front().MyHealth, 500));
		(void)battle.LoseHealth(0, 1, 200);
		(void)battle.ApplyStatus(1, CombatStatus::HEAL_FREE, 10);
		Check(Close(battle.Heal(1, 1, 100), 0));
		(void)battle.AddBuff(1, Buff("regen", 1, Attribute::HEALTH_REGEN, 30));
		battle.Advance(3);
		Check(Close(battle.Unit(1).MyHealth, 803));
	}

	void PrintStats(const CombatStats& _stats)
	{
		const std::array values{
			_stats.MyMaxHealth, _stats.MyAttack, _stats.MyDefense, _stats.MyResistance, _stats.MyAttackSpeed,
			_stats.MyBaseAttackTime, static_cast<double>(_stats.MyBlockCount), _stats.MyMoveSpeed,
			static_cast<double>(_stats.MyRangeExtend), _stats.MyBlockRadiusScale,
			static_cast<double>(_stats.MyPermanentRangeExtend), _stats.MyMass, _stats.MyExtraTargets, _stats.MyTaunt,
			_stats.MyPhysicalDodge, _stats.MyArtsDodge, _stats.MyDefenseIgnoreFlat, _stats.MyDefenseIgnorePercent,
			_stats.MyResistanceIgnoreFlat, _stats.MyResistanceIgnorePercent, _stats.MyDamageDealtMultiplier,
			_stats.MyPhysicalDealtMultiplier, _stats.MyArtsDealtMultiplier, _stats.MyDamageTakenMultiplier,
			_stats.MyPhysicalTakenMultiplier, _stats.MyArtsTakenMultiplier, _stats.MyTrueTakenMultiplier,
			_stats.MyElementTakenMultiplier, _stats.MyElementalTakenMultiplier, _stats.MyHealingDealtMultiplier,
			_stats.MyHealingTakenMultiplier, _stats.MyAttackScaleMultiplier, _stats.MySpRecovery,
			_stats.MySpCostFlat, _stats.MyRedeployMultiplier, _stats.MyHealthRegen};
		std::cout << '[';
		for (std::size_t i = 0; i < values.size(); ++i) { if (i) std::cout << ','; std::cout << values[i]; }
		std::cout << ']';
	}

	void AttributeProbe()
	{
		Random random(8751);
		const CombatStats base{.MyMaxHealth = 1000, .MyAttack = 100, .MyDefense = 200, .MyResistance = 30,
			.MyMoveSpeed = 2, .MyBlockCount = 2, .MyTaunt = 1, .MySpRecovery = 1, .MyHealthRegen = 5, .MyMass = 2};
		std::cout << '[';
		for (int sample = 0; sample < 256; ++sample)
		{
			if (sample) std::cout << ',';
			AttributeModifiers modifiers;
			std::cout << "{\"mods\":[";
			for (std::size_t i = 0; i < static_cast<std::size_t>(Attribute::COUNT); ++i)
			{
				if (i) std::cout << ',';
				const auto value = i >= static_cast<std::size_t>(Attribute::ATTACK_MULTIPLIER) ? 0.5 + random.Next() : random.Next() * 2 - 0.5;
				const auto stacks = random.Index(4) + 1;
				const bool permanent = i % 2 == 0;
				const std::array changes{AttributeChange{.MyAttribute = static_cast<Attribute>(i), .MyValue = value}};
				modifiers.Add(changes, stacks, permanent);
				std::cout << '[' << value << ',' << stacks << ',' << permanent << ']';
			}
			std::cout << "],\"stats\":";
			PrintStats(ResolveStats(base, modifiers));
			std::cout << '}';
		}
		std::cout << ']';
	}

	void Trace()
	{
		auto battle = MakeBattle();
		(void)battle.LoseHealth(0, 1, 500);
		std::cout << '[';
		for (int tick = 0; tick < 150; ++tick)
		{
			if (tick == 0) (void)battle.AddBuff(1, Buff("health", 1, Attribute::HEALTH_PERCENT, 1));
			if (tick == 5) (void)battle.ApplyStatus(2, CombatStatus::RESIST, 3);
			if (tick == 6) (void)battle.ApplyStatus(2, CombatStatus::COLD, 2);
			if (tick == 7) (void)battle.ApplyStatus(2, CombatStatus::COLD, 4);
			if (tick == 10) (void)battle.ApplyStatus(2, CombatStatus::WEAKEN, StatusApplication{.MyDuration = 0.5, .MyValue = 0.8});
			if (tick == 11) (void)battle.ApplyStatus(2, CombatStatus::WEAKEN, StatusApplication{.MyDuration = 2, .MyValue = 0.3});
			if (tick == 15) (void)battle.ApplyStatus(2, CombatStatus::DEFENSE_DOWN, StatusApplication{.MyDuration = 1, .MyValue = 0.5});
			if (tick == 20) (void)battle.ApplyStatus(2, CombatStatus::RESISTANCE_DOWN, StatusApplication{.MyDuration = 2, .MyValue = 25});
			if (tick == 30) (void)battle.AddBuff(1, Buff("regen", 2, Attribute::HEALTH_REGEN, 60));
			if (tick == 40) (void)battle.ApplyStatus(1, CombatStatus::HEAL_FREE, 2);
			battle.Step();
			if (tick) std::cout << ',';
			std::cout << '[';
			for (const auto& unit : battle.Units())
			{
				if (unit.MyId != 1) std::cout << ',';
				std::cout << '[' << unit.MyHealth << ',';
				PrintStats(unit.MyStats);
				std::cout << ']';
			}
			std::cout << ']';
		}
		std::cout << ']';
	}
}

int main(int _argc, char** _argv)
{
	try
	{
		std::cout << std::setprecision(17);
		if (_argc > 1 && std::string_view(_argv[1]) == "attributes") AttributeProbe();
		else if (_argc > 1 && std::string_view(_argv[1]) == "trace") Trace();
		else { BuffLifecycle(); StatusInteractions(); DamageModifiers(); std::cout << "3 effect test groups passed\n"; }
	}
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

