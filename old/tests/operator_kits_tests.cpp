#include <stronghold/simulation/operator_kits.hpp>
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

	bool Close(double _left, double _right) { return std::abs(_left - _right) < 1e-8; }

	AllyDeployment Ally(UnitId _id, WorldPoint _point, const OperatorKitDefinition* _kit = nullptr)
	{
		CombatDefinition definition{.MyId = "operator", .MyStats = {.MyMaxHealth = 1000, .MyAttack = 100}, .MyAttack = {.MyDisabled = true}};
		definition.MySkill = {.MyKind = SkillKind::DURATION, .MySpType = SpType::NONE, .MyTrigger = SkillTrigger::NEVER,
			.MySpCost = 1, .MyInitialSp = 1, .MyDuration = 30, .MyManual = false};
		definition.MyOperatorKit = _kit;
		return {.MyPieceUid = _id, .MyDefinition = std::move(definition), .MyPosition = _point};
	}

	BattleInput Input(const OperatorKitDefinition& _kit, RulePositionMode _mode = RulePositionMode::CURRENT)
	{
		return {.MyPlayers = {{.MyPlayerId = "p", .MyUnits = {Ally(1, {5, 9}, &_kit), Ally(2, {6, 9})}}},
			.MyAutoFinish = false, .MyRulePositionMode = _mode};
	}

	UnitId Enemy(Battle& _battle, WorldPoint _point, double _hp = 10000, bool _elite = false)
	{
		CombatDefinition definition{.MyId = "enemy", .MyStats = {.MyMaxHealth = _hp}, .MyAttack = {.MyDisabled = true}, .MyElite = _elite};
		return _battle.SpawnEnemy({.MyOwnerId = "p", .MyDefinition = std::move(definition), .MyRoute = {.MyStart = _point, .MyEnd = {0, 9}}});
	}

	void EstellHealing(RulePositionMode _mode)
	{
		constexpr std::array<RangeOffset, 1> grid{{{0, 1}}};
		const OperatorKitDefinition kit = EstellKit{.MyHealRatio = 0.1, .MyHealthThreshold = 0.5, .MyPhysicalReduction = 0.2, .MyRange = grid, .MyHasRange = true};
		for (const bool home : {false, true})
		{
			auto input = Input(kit, _mode); input.MyPlayers[0].MyUnits[0].MyDefinition.MySkill.MyFlags.set(static_cast<std::size_t>(CombatStatus::NO_HEAL));
			Battle battle(std::move(input)); battle.Start(); Check(battle.Relocate(1, {5, 12}));
			const auto enemy = Enemy(battle, {6, home ? 9.0 : 12.0});
			Check(battle.ActivateSkill(1)); (void)battle.LoseHealth(0, 1, 500);
			Check(Close(battle.Heal(2, 1, 100), 0));
			Check(Close(battle.DealDamage(enemy, 1, 100, DamageType::PHYSICAL), 100)); // 恰好半血不减伤。
			Check(Close(battle.Heal(1, 1, 400, {.MySelf = true}), 400));
			Check(Close(battle.DealDamage(enemy, 1, 100, DamageType::PHYSICAL), 80));
			Check(Close(battle.DealDamage(enemy, 1, 100, DamageType::ARTS), 100));
			const auto health = battle.Unit(1).MyHealth;
			(void)battle.LoseHealth(0, enemy, 10000);
			Check(Close(battle.Unit(1).MyHealth, health + (home == (_mode == RulePositionMode::INITIAL) ? 100 : 0)));
			const auto retired = Enemy(battle, {6, home ? 9.0 : 12.0}); const auto healed = battle.Unit(1).MyHealth;
			battle.Retreat(retired, true); Check(Close(battle.Unit(1).MyHealth, healed));
		}
	}

	void PodegoHealingAndSp(RulePositionMode _mode)
	{
		const OperatorKitDefinition healKit = PodegoKit{.MyHealing = true};
		const OperatorKitDefinition spKit = PodegoKit{.MySpPerSecond = 0.2};
		for (const bool home : {false, true})
		{
			const bool selected = home == (_mode == RulePositionMode::INITIAL);
			auto input = Input(healKit, _mode); auto& source = input.MyPlayers[0].MyUnits[0].MyDefinition;
			source.MySkill.MyHealSkill = true; source.MySkill.MyAttack = AttackProfile{.MyHealing = true};
			input.MyPlayers[0].MyUnits[1].MyPosition = {home ? 6.0 : 8.0, 9};
			Battle heal(std::move(input)); heal.Start(); Check(heal.Relocate(1, {5, 12}));
			Check(heal.Relocate(2, {6, home ? 15.0 : 12.0})); (void)heal.LoseHealth(0, 2, 500);
			heal.Step(); Check(heal.Unit(1).MySkill.MyActive == selected); // 没有敌人也能由帧末检查启动。
			heal.Step(); Check(Close(heal.Unit(2).MyHealth, selected ? 600 : 500));
			for (const bool stealth : {false, true})
			{
				auto spInput = Input(spKit, _mode); auto& skill = spInput.MyPlayers[0].MyUnits[0].MyDefinition.MySkill;
				skill.MySpType = SpType::TIME; skill.MyInitialSp = 0; skill.MySpCost = 100;
				Battle sp(std::move(spInput)); sp.Start(); Check(sp.Relocate(1, {5, 12}));
				const auto enemy = Enemy(sp, {6, home ? 9.0 : 12.0});
				if (stealth) (void)sp.ApplyStatus(enemy, CombatStatus::STEALTH, 10);
				sp.Advance(30); Check(Close(sp.SpTotal(1), selected && !stealth ? 1.2 : 1));
			}
		}
	}

	void PodegoAuraOwnership()
	{
		const OperatorKitDefinition weak = PodegoKit{.MyAuraAttack = 0.07}, strong = PodegoKit{.MyAuraAttack = 0.11};
		auto input = Input(weak); auto& allies = input.MyPlayers[0].MyUnits;
		allies[0].MyDefinition.MyOperatorProfession = OperatorProfession::SUPPORT;
		allies[1].MyDefinition.MyOperatorKit = &strong; allies[1].MyDefinition.MyOperatorProfession = OperatorProfession::SUPPORT;
		allies.push_back(Ally(3, {7, 9}));
		auto other = Ally(4, {12, 9}); other.MyDefinition.MyOperatorProfession = OperatorProfession::SUPPORT;
		auto token = Ally(5, {12, 10}); token.MyDefinition.MyOperatorProfession = OperatorProfession::SUPPORT; token.MyKind = UnitKind::TOKEN;
		input.MyPlayers.push_back({.MyPlayerId = "q", .MyUnits = {other, token}});
		Battle battle(std::move(input)); battle.Start(); battle.Step();
		Check(Close(battle.Unit(1).MyStats.MyAttack, 111)); Check(Close(battle.Unit(4).MyStats.MyAttack, 111));
		Check(Close(battle.Unit(3).MyStats.MyAttack, 100)); Check(Close(battle.Unit(5).MyStats.MyAttack, 100));
		battle.Retreat(2, true); battle.Advance(33);
		Check(Close(battle.Unit(4).MyStats.MyAttack, 107));
		StatusFlags isolated; isolated.set(static_cast<std::size_t>(CombatStatus::ISOLATED));
		(void)battle.AddBuff(4, {.MyKey = "isolated", .MyDuration = 4, .MyFlags = isolated});
		(void)battle.AddBuff(1, {.MyKey = "isolated", .MyDuration = 4, .MyFlags = isolated});
		battle.Advance(30); Check(Close(battle.Unit(4).MyStats.MyAttack, 100)); Check(Close(battle.Unit(1).MyStats.MyAttack, 107));
	}

	void PodegoZoneLifetime(RulePositionMode _mode)
	{
		const OperatorKitDefinition kit = PodegoKit{.MyZoneDuration = 3, .MyZoneScale = 2};
		for (const bool home : {false, true})
		{
			Battle battle(Input(kit, _mode)); battle.Start(); Check(battle.Relocate(1, {5, 12}));
			const auto enemy = Enemy(battle, {6, home ? 9.0 : 12.0});
			(void)battle.AddBuff(1, {.MyKey = "power", .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, 1}}});
			Check(battle.ActivateSkill(1)); battle.Retreat(1, true);
			battle.Advance(61); const bool hit = home == (_mode == RulePositionMode::INITIAL);
			Check(Close(battle.Unit(enemy).MyHealth, hit ? 8800 : 10000));
			Check(battle.Unit(enemy).MyStatuses.Has(CombatStatus::SILENCE) == hit);
			battle.Advance(60); Check(Close(battle.Unit(enemy).MyHealth, hit ? 8800 : 10000));
			Check(!battle.Unit(enemy).MyStatuses.Has(CombatStatus::SILENCE));
		}
	}

	double Gauge(const Battle& _battle, UnitId _unit, Element _element)
	{ return _battle.Unit(_unit).MyElements.MyGauges[static_cast<std::size_t>(_element)]; }

	void PithstElements()
	{
		const OperatorKitDefinition kit = PithstKit{.MyElementRatio = 1, .MyEliteElementRatio = 2};
		Battle battle(Input(kit)); battle.Start(); const auto normal = Enemy(battle, {6, 9}), elite = Enemy(battle, {7, 9}, 10000, true);
		(void)battle.DealDamage(1, normal, 10, DamageType::TRUE_DAMAGE); (void)battle.DealDamage(1, elite, 10, DamageType::TRUE_DAMAGE);
		for (const auto element : {Element::NEURAL, Element::BURN, Element::APOPTOSIS})
		{
			Check(Close(Gauge(battle, normal, element), 100)); Check(Close(Gauge(battle, elite, element), 200));
		}
		(void)battle.DealDamage(1, normal, 10, DamageType::ELEMENTAL);
		(void)battle.DealDamage(1, normal, {.MyAmount = 10, .MyTags = static_cast<DamageTags>(DamageTag::BURST)});
		Check(Close(Gauge(battle, normal, Element::NEURAL), 100));
		battle.AddShield(normal, {.MyHits = 1}); (void)battle.DealDamage(1, normal, 10, DamageType::TRUE_DAMAGE);
		Check(Close(Gauge(battle, normal, Element::NEURAL), 100));
		const auto dying = Enemy(battle, {8, 9}, 1); (void)battle.DealDamage(1, dying, 10, DamageType::TRUE_DAMAGE);
		Check(!battle.Unit(dying).MyAlive); Check(Close(Gauge(battle, dying, Element::NEURAL), 0));
		for (const auto element : {Element::NEURAL, Element::BURN, Element::APOPTOSIS})
			(void)battle.DealElement(0, normal, {.MyElement = element, .MyAmount = 850});
		(void)battle.DealDamage(1, normal, 10, DamageType::TRUE_DAMAGE);
		Check(battle.Unit(normal).MyStatuses.Has(CombatStatus::BURST_LOCK));
		Check(Close(Gauge(battle, normal, Element::NEURAL), 1000));
		Check(Close(Gauge(battle, normal, Element::BURN), 950)); Check(Close(Gauge(battle, normal, Element::APOPTOSIS), 950));
		const auto lingering = Enemy(battle, {9, 9}); battle.Retreat(1, true);
		(void)battle.DealDamage(1, lingering, {.MyAmount = 10, .MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::DOT)});
		Check(Close(Gauge(battle, lingering, Element::NEURAL), 100)); // 永久退场当帧尚未释放监听，仍可附加元素。
		battle.Step(); const auto afterRelease = Gauge(battle, lingering, Element::NEURAL);
		(void)battle.DealDamage(1, lingering, {.MyAmount = 10, .MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::DOT)});
		Check(Close(Gauge(battle, lingering, Element::NEURAL), afterRelease)); // 永久移除者的监听在帧末释放。
	}

	BattleInput TinmanInput(const OperatorKitDefinition& _kit, RulePositionMode _mode = RulePositionMode::CURRENT)
	{
		auto input = Input(_kit, _mode);
		input.MyPlayers[0].MyUnits[0].MyDefinition.MySkill = {.MyKind = SkillKind::INSTANT, .MySpType = SpType::TIME,
			.MyTrigger = SkillTrigger::NEVER, .MySpCost = 100, .MyInitialSp = 100, .MyManual = false};
		return input;
	}

	void TinmanLifetime(RulePositionMode _mode)
	{
		const OperatorKitDefinition kit = TinmanKit{.MyDuration = 3, .MyRadius = 0.9, .MyDamageScale = 2, .MyZone = true};
		for (const bool home : {false, true})
		{
			Battle battle(TinmanInput(kit, _mode)); battle.Start(); Check(battle.Relocate(1, {5, 12}));
			const auto enemy = Enemy(battle, {6, home ? 9.0 : 12.0});
			(void)battle.AddBuff(1, {.MyKey = "power", .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, 1}}});
			Check(battle.ActivateSkill(1)); Check(battle.Unit(1).MyTinmanZones == 1);
			battle.Retreat(1, true); battle.Advance(61);
			Check(Close(battle.Unit(enemy).MyHealth, home == (_mode == RulePositionMode::INITIAL) ? 8800 : 10000));
			Check(battle.Unit(1).MyTinmanZones == 0); battle.Advance(60);
			Check(Close(battle.Unit(enemy).MyHealth, home == (_mode == RulePositionMode::INITIAL) ? 8800 : 10000));
		}
	}

	void TinmanRegeneration()
	{
		const OperatorKitDefinition kit = TinmanKit{.MyDuration = 3, .MyRadius = 1.5, .MyDamageScale = 1, .MyRegenRatio = 0.5,
			.MyWitherScale = 1.25, .MySpPerSecond = 0.1, .MyZone = true};
		auto input = TinmanInput(kit); input.MyPlayers[0].MyUnits.push_back(Ally(3, {6, 10}));
		input.MyPlayers.push_back({.MyPlayerId = "q", .MyUnits = {Ally(4, {6, 8})}});
		Battle battle(std::move(input)); battle.Start();
		const auto ground = Enemy(battle, {6, 9});
		CombatDefinition flying{.MyId = "flying", .MyStats = {.MyMaxHealth = 10000}, .MyAttack = {.MyDisabled = true}, .MyFlying = true};
		const auto air = battle.SpawnEnemy({.MyOwnerId = "p", .MyDefinition = std::move(flying), .MyRoute = {.MyStart = {5.4, 9}, .MyEnd = {0, 9}}});
		StatusFlags noHeal; noHeal.set(static_cast<std::size_t>(CombatStatus::NO_HEAL)); noHeal.set(static_cast<std::size_t>(CombatStatus::HEAL_FREE));
		StatusFlags isolated; isolated.set(static_cast<std::size_t>(CombatStatus::ISOLATED));
		(void)battle.AddBuff(2, {.MyKey = "noheal", .MyFlags = noHeal});
		(void)battle.AddBuff(1, {.MyKey = "isolated", .MyFlags = isolated});
		(void)battle.AddBuff(3, {.MyKey = "isolated", .MyFlags = isolated});
		for (const auto id : {1U, 2U, 3U, 4U}) (void)battle.LoseHealth(0, id, 500);
		Check(battle.ActivateSkill(1));
		(void)battle.AddBuff(1, {.MyKey = "power", .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, 1}}});
		Check(battle.ActivateSkill(1, true)); battle.Step();
		Check(battle.Unit(1).MyTinmanZones == 2); Check(Close(battle.SpTotal(1), 1.1 / 30));
		Check(Close(battle.Unit(ground).MyHealth, 9625)); Check(Close(battle.Unit(air).MyHealth, 10000));
		for (const auto id : {1U, 2U, 4U}) { Check(Close(battle.Unit(id).MyStats.MyHealthRegen, 150)); Check(battle.Unit(id).MyHealth > 500); }
		Check(Close(battle.Unit(3).MyStats.MyHealthRegen, 0));
		battle.Retreat(1, true); battle.Advance(60);
		Check(battle.Unit(1).MyTinmanZones == 0); Check(Close(battle.Unit(2).MyStats.MyHealthRegen, 150));
		battle.Advance(33); Check(Close(battle.Unit(2).MyStats.MyHealthRegen, 0));
	}

	void TinmanWither()
	{
		const OperatorKitDefinition kit = TinmanKit{.MyDuration = 2, .MyRadius = 2, .MyWitherScale = 1.5, .MyZone = true};
		Battle battle(TinmanInput(kit)); battle.Start();
		const auto necrosis = Enemy(battle, {6, 9}), apoptosis = Enemy(battle, {7, 9});
		Check(battle.ActivateSkill(1)); battle.Step(); battle.Retreat(1, true);
		for (const auto tag : {DamageTag::DOT, DamageTag::NECROSIS, DamageTag::APOPTOSIS})
			Check(Close(battle.DealDamage(2, necrosis, {.MyAmount = 100, .MyType = DamageType::TRUE_DAMAGE, .MyTags = static_cast<DamageTags>(tag)}), 150));
		Check(Close(battle.DealDamage(2, necrosis, {.MyAmount = 100, .MyType = DamageType::TRUE_DAMAGE, .MyTags = static_cast<DamageTags>(DamageTag::BURST)}), 100));
		Check(Close(battle.DealDamage(0, necrosis, 100, DamageType::TRUE_DAMAGE), 100));
		(void)battle.DealElement(2, necrosis, {.MyElement = Element::NECROSIS, .MyAmount = 1000});
		(void)battle.DealElement(2, apoptosis, {.MyElement = Element::APOPTOSIS, .MyAmount = 1000});
		const auto before = battle.Unit(necrosis).MyHealth, beforeA = battle.Unit(apoptosis).MyHealth;
		battle.Advance(30);
		Check(Close(battle.Unit(necrosis).MyHealth, before - 150)); Check(Close(battle.Unit(apoptosis).MyHealth, beforeA - 1200));
		battle.Advance(34);
		Check(Close(battle.DealDamage(2, necrosis, {.MyAmount = 100, .MyType = DamageType::TRUE_DAMAGE, .MyTags = static_cast<DamageTags>(DamageTag::DOT)}), 100));
	}

	void TinmanWeakZone()
	{
		const OperatorKitDefinition kit = TinmanKit{.MyDuration = 1.25, .MyRadius = 1, .MyDamageScale = 1, .MyWeaken = 0.25,
			.MyWitherScale = 1.2, .MyZone = true, .MyWeakZone = true};
		Battle battle(TinmanInput(kit)); battle.Start(); const auto enemy = Enemy(battle, {6, 9});
		Check(battle.ActivateSkill(1)); battle.Step(); Check(Close(battle.Unit(enemy).MyHealth, 9880));
		Check(battle.Unit(enemy).MyStatuses.Has(CombatStatus::WEAKEN));
		battle.Retreat(1, true); battle.Advance(29); Check(Close(battle.Unit(enemy).MyHealth, 9880));
		battle.Step(); Check(Close(battle.Unit(enemy).MyHealth, 9760)); Check(battle.Unit(1).MyTinmanZones == 0);
		battle.Advance(10); Check(!battle.Unit(enemy).MyStatuses.Has(CombatStatus::WEAKEN));
	}

	void IndigoEnergyAndRetarget()
	{
		const OperatorKitDefinition kit = IndigoKit{.MyProbability = 1, .MyBindDuration = 100, .MySkillProbabilityScale = 2};
		auto input = Input(kit); auto& definition = input.MyPlayers[0].MyUnits[0].MyDefinition;
		definition.MyAttack = {.MyDamageType = DamageType::ARTS};
		definition.MyProfession.MyKind = ProfessionTrait::MYSTIC;
		definition.MySkill.MyAttack = definition.MyAttack; definition.MySkill.MyAttack->MyAttackScale = 0.4;
		definition.MySkill.MyRange = {{0, 0}, {0, 1}, {0, 2}};
		Battle battle(std::move(input)); battle.Start(); const auto enemy = Enemy(battle, {6, 9});
		(void)battle.ApplyStatus(enemy, CombatStatus::BIND, 100); battle.Advance(120);
		Check(battle.Unit(1).MyProfession.MyStored == 3); Check(battle.Unit(1).MyTotals.MyAttacks == 0);
		const auto before = battle.RandomState(); const std::array<UnitId, 1> target{enemy};
		Check(!battle.ForceAttack(1, target)); Check(battle.Unit(1).MyProfession.MyStored == 3); Check(battle.RandomState() == before);
		Check(battle.ActivateSkill(1)); (void)battle.RemoveStatus(enemy, CombatStatus::BIND);
		Check(battle.ForceAttack(1, target)); Check(Close(battle.Unit(enemy).MyHealth, 9840));
		Check(battle.Unit(1).MyProfession.MyStored == 0); Check(battle.Unit(enemy).MyStatuses.Has(CombatStatus::BIND));
		Random expected(before); (void)expected.Next(); Check(battle.RandomState() == expected.State());
		const auto second = Enemy(battle, {7, 9}); Check(battle.ForceAttack(1, target));
		Check(Close(battle.Unit(second).MyHealth, 9960)); Check(Close(battle.Unit(enemy).MyHealth, 9840));
		(void)expected.Next(); Check(battle.RandomState() == expected.State());
		const auto dying = Enemy(battle, {8, 9}, 1); const std::array<UnitId, 1> dead{dying};
		Check(battle.ForceAttack(1, dead)); Check(!battle.Unit(dying).MyAlive); Check(battle.RandomState() == expected.State());
		const auto shielded = Enemy(battle, {8, 10}); const std::array<UnitId, 1> shieldTarget{shielded};
		battle.AddShield(shielded, {.MyHits = 1}); Check(battle.ForceAttack(1, shieldTarget));
		Check(Close(battle.Unit(shielded).MyHealth, 10000)); Check(battle.Unit(shielded).MyStatuses.Has(CombatStatus::BIND));
		(void)expected.Next(); Check(battle.RandomState() == expected.State());
	}

	void IndigoBlockedTarget()
	{
		const OperatorKitDefinition kit = IndigoKit{};
		auto input = Input(kit); auto& definition = input.MyPlayers[0].MyUnits[0].MyDefinition;
		definition.MyRange = {{0, 3}}; definition.MyAttack = {.MyDamageType = DamageType::ARTS};
		definition.MyStats.MyBlockCount = 1;
		definition.MyProfession.MyKind = ProfessionTrait::MYSTIC;
		Battle battle(std::move(input)); battle.Start(); const auto enemy = Enemy(battle, {5.4, 9});
		battle.Step(); Check(battle.Unit(enemy).MyBlockedBy == 1); Check(Close(battle.Unit(enemy).MyHealth, 9900));
		(void)battle.ApplyStatus(enemy, CombatStatus::BIND, 10); battle.Advance(31);
		Check(battle.Unit(1).MyProfession.MyStored == 1); Check(Close(battle.Unit(enemy).MyHealth, 9900));
		const auto bound = Enemy(battle, {8, 9}); (void)battle.ApplyStatus(bound, CombatStatus::BIND, 10);
		(void)battle.RemoveStatus(enemy, CombatStatus::BIND); const std::array<UnitId, 1> target{bound};
		Check(battle.ForceAttack(1, target)); Check(Close(battle.Unit(enemy).MyHealth, 9700));
		Check(Close(battle.Unit(bound).MyHealth, 10000));
	}

	void IndigoMazePosition(RulePositionMode _mode)
	{
		const OperatorKitDefinition kit = IndigoKit{.MyDamageScale = 2, .MyMaze = true};
		Battle battle(Input(kit, _mode)); battle.Start(); Check(battle.Relocate(1, {5, 12}));
		const auto home = Enemy(battle, {6, 9}), current = Enemy(battle, {6, 12});
		(void)battle.ApplyStatus(home, CombatStatus::BIND, 10); (void)battle.ApplyStatus(current, CombatStatus::BIND, 10);
		Check(battle.ActivateSkill(1)); battle.Advance(15);
		const auto hit = _mode == RulePositionMode::INITIAL ? home : current, missed = hit == home ? current : home;
		Check(Close(battle.Unit(hit).MyHealth, 9800)); Check(Close(battle.Unit(missed).MyHealth, 10000));
		(void)battle.AddBuff(1, {.MyKey = "power", .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, 1}}});
		(void)battle.ApplyStatus(1, CombatStatus::STUN, 10); battle.Advance(15);
		Check(Close(battle.Unit(hit).MyHealth, 9400)); battle.EndSkill(1); battle.Advance(15);
		Check(Close(battle.Unit(hit).MyHealth, 9400));
	}

	void IndigoNormalAttackPosition(RulePositionMode _mode)
	{
		const OperatorKitDefinition kit = IndigoKit{};
		auto input = Input(kit, _mode); auto& definition = input.MyPlayers[0].MyUnits[0].MyDefinition;
		definition.MyAttack = {.MyDamageType = DamageType::ARTS}; definition.MyProfession.MyKind = ProfessionTrait::MYSTIC;
		Battle battle(std::move(input)); battle.Start(); Check(battle.Relocate(1, {5, 12}));
		const auto home = Enemy(battle, {6, 9}), current = Enemy(battle, {6, 12});
		(void)battle.ApplyStatus(home, CombatStatus::BIND, 10); battle.Step();
		Check(Close(battle.Unit(current).MyHealth, 9900)); Check(Close(battle.Unit(home).MyHealth, 10000));
		const std::array<UnitId, 1> target{home}; Check(battle.ForceAttack(1, target));
		Check(Close(battle.Unit(current).MyHealth, 9800));
		Check(battle.ActivateSkill(1)); Check(battle.ForceAttack(1, target) == (_mode == RulePositionMode::CURRENT));
	}

	void IndigoDelayedHit()
	{
		const OperatorKitDefinition kit = IndigoKit{.MyProbability = 0.5, .MyBindDuration = 4, .MySkillProbabilityScale = 2};
		for (const bool retreat : {false, true})
		{
			auto input = Input(kit); auto& definition = input.MyPlayers[0].MyUnits[0].MyDefinition;
			definition.MyAttack = {.MyDamageType = DamageType::ARTS, .MyDisabled = true, .MyRanged = true, .MyProjectileSpeed = 3};
			definition.MySkill.MyAttack = definition.MyAttack; definition.MySkill.MyAttack->MyAttackScale = 0.4;
			Battle battle(std::move(input)); battle.Start(); const auto enemy = Enemy(battle, {8, 9});
			Check(battle.ActivateSkill(1)); const std::array<UnitId, 1> target{enemy};
			Check(battle.ForceAttack(1, target));
			if (retreat) battle.Retreat(1, true); else battle.EndSkill(1);
			battle.Advance(32); Check(Close(battle.Unit(enemy).MyHealth, 9960));
			Random expected(1); Check(expected.Next() > 0.5); Check(battle.RandomState() == expected.State());
			Check(!battle.Unit(enemy).MyStatuses.Has(CombatStatus::BIND)); // 命中时技能已结束，只用基础概率。
		}
	}

	void UtageBreach()
	{
		const OperatorKitDefinition kit = UtageKit{.MyHealthLoss = 0.5, .MyBreach = true};
		for (const double health : {1.0, 2.0, 1000.0})
		{
			auto input = Input(kit); auto& source = input.MyPlayers[0].MyUnits[0].MyDefinition;
			source.MyStats.MyMaxHealth = health; source.MySkill.MyActivateOnDeploy = true;
			source.MySkill.MySpType = SpType::NONE; source.MySkill.MySpCost = 0;
			Battle battle(std::move(input)); battle.Start();
			Check(Close(battle.Unit(1).MyHealth, health == 1 ? 1 : health / 2)); Check(battle.Unit(1).MySkill.MyActive);
			const auto enemy = Enemy(battle, {7, 9});
			(void)battle.AddBuff(enemy, {.MyKey = "guard", .MyModifiers = std::vector<AttributeChange>{{Attribute::DEFENSE_FLAT, 80}, {Attribute::RESISTANCE_FLAT, 50}}});
			Check(Close(battle.DealDamage(1, enemy, {.MyAmount = 100, .MyType = DamageType::PHYSICAL, .MyIsAttack = true}), 50));
			Check(Close(battle.DealDamage(1, enemy, 100, DamageType::PHYSICAL), 20));
			(void)battle.AddBuff(2, {.MyKey = "guard", .MyModifiers = std::vector<AttributeChange>{{Attribute::DEFENSE_FLAT, 80}, {Attribute::RESISTANCE_FLAT, 50}}});
			Check(Close(battle.DealDamage(1, 2, {.MyAmount = 100, .MyType = DamageType::PHYSICAL, .MyIsAttack = true}), 20));
			battle.EndSkill(1);
			Check(Close(battle.DealDamage(1, enemy, {.MyAmount = 100, .MyType = DamageType::PHYSICAL, .MyIsAttack = true}), 20));
			battle.Retreat(1); Check(battle.Redeploy(1, true));
			Check(battle.Unit(1).MySkill.MyActive); Check(Close(battle.Unit(1).MyHealth, health == 1 ? 1 : health / 2));
		}
	}

	void UtageProtectionAndSpeed()
	{
		const OperatorKitDefinition kit = UtageKit{.MyMaxAttackSpeed = 100, .MyMinHealthRatio = 0.3, .MyProtectThreshold = 0.5, .MyProtection = 0.25};
		Battle battle(Input(kit)); battle.Start(); const auto enemy = Enemy(battle, {7, 9});
		(void)battle.LoseHealth(0, 1, 500);
		Check(Close(battle.DealDamage(enemy, 1, 100, DamageType::PHYSICAL), 100)); // 恰好半血，本次伤害没有庇护。
		Check(Close(battle.DealDamage(enemy, 1, 100, DamageType::PHYSICAL), 75));
		Check(Close(battle.DealDamage(enemy, 1, 100, DamageType::ARTS), 75));
		Check(Close(battle.DealDamage(enemy, 1, 100, DamageType::TRUE_DAMAGE), 100));
		(void)battle.Heal(1, 1, 1000, {.MySelf = true}); battle.Advance(2);
		Check(Close(battle.Unit(1).MyStats.MyPhysicalTakenMultiplier, 1)); Check(Close(battle.Unit(1).MyStats.MyArtsTakenMultiplier, 1));
		(void)battle.LoseHealth(0, 1, 350); battle.Step(); Check(Close(battle.Unit(1).MyStats.MyAttackSpeed, 150));
		(void)battle.Heal(1, 1, 0.1, {.MySelf = true}); battle.Step(); Check(Close(battle.Unit(1).MyStats.MyAttackSpeed, 150));
		(void)battle.LoseHealth(0, 1, 7.1); battle.Step(); Check(Close(battle.Unit(1).MyStats.MyAttackSpeed, 151));
		(void)battle.Heal(1, 1, 1000, {.MySelf = true}); battle.Step(); Check(Close(battle.Unit(1).MyStats.MyAttackSpeed, 100));
		(void)battle.LoseHealth(0, 1, 700); battle.Step(); Check(Close(battle.Unit(1).MyStats.MyAttackSpeed, 200));
		(void)battle.ApplyStrongest(1, "protect", 1, {.MyValue = 0.5, .MyAttribute = Attribute::PHYSICAL_TAKEN_MULTIPLIER,
			.MyScale = -1, .MyOffset = 1, .MySecondAttribute = Attribute::ARTS_TAKEN_MULTIPLIER}, 2);
		Check(Close(battle.DealDamage(enemy, 1, 100, DamageType::ARTS), 50));
		battle.Advance(31); Check(Close(battle.Unit(1).MyStats.MyPhysicalTakenMultiplier, 0.75)); Check(Close(battle.Unit(1).MyStats.MyArtsTakenMultiplier, 0.75));
		(void)battle.Heal(1, 1, 1000, {.MySelf = true}); battle.Advance(2);
		(void)battle.ApplyStrongest(1, "protect", 0.5, {.MyValue = 0.5, .MyAttribute = Attribute::PHYSICAL_TAKEN_MULTIPLIER,
			.MyScale = -1, .MyOffset = 1, .MySecondAttribute = Attribute::ARTS_TAKEN_MULTIPLIER}, 2);
		(void)battle.ApplyStrongest(1, "protect", 2, {.MyValue = 0.2, .MyAttribute = Attribute::PHYSICAL_TAKEN_MULTIPLIER,
			.MyScale = -1, .MyOffset = 1, .MySecondAttribute = Attribute::ARTS_TAKEN_MULTIPLIER}, 2);
		battle.Advance(16); Check(Close(battle.Unit(1).MyStats.MyPhysicalTakenMultiplier, 0.8)); Check(Close(battle.Unit(1).MyStats.MyArtsTakenMultiplier, 0.8));
		battle.Advance(60); Check(Close(battle.Unit(1).MyStats.MyPhysicalTakenMultiplier, 1)); Check(Close(battle.Unit(1).MyStats.MyArtsTakenMultiplier, 1));
	}

	void UtageRest()
	{
		const OperatorKitDefinition kit = UtageKit{.MyRest = true};
		auto input = Input(kit); auto& source = input.MyPlayers[0].MyUnits[0].MyDefinition;
		source.MyStats.MyDefense = 100; source.MyStats.MyBlockCount = 1; source.MyAttack = {.MyNoHeal = true};
		source.MySkill.MyModifiers = {{Attribute::DEFENSE_PERCENT, 1.6}, {Attribute::HEALTH_REGEN_RATIO, 0.08}, {Attribute::BLOCK_COUNT, -99}};
		source.MySkill.MyAttack = source.MyAttack; source.MySkill.MyAttack->MyDisabled = true;
		Battle battle(std::move(input)); battle.Start(); const auto enemy = Enemy(battle, {5.4, 9}); battle.Step();
		Check(battle.Unit(enemy).MyBlockedBy == 1); (void)battle.LoseHealth(0, 1, 500); Check(battle.ActivateSkill(1));
		Check(battle.Unit(enemy).MyBlockedBy == 0); Check(battle.Unit(1).MyStats.MyBlockCount == 0);
		Check(Close(battle.Unit(1).MyStats.MyDefense, 260)); Check(Close(battle.Heal(2, 1, 100), 0));
		const auto attacks = battle.Unit(1).MyTotals.MyAttacks; battle.Advance(30);
		Check(battle.Unit(1).MyTotals.MyAttacks == attacks); Check(Close(battle.Unit(1).MyHealth, 580));
		battle.EndSkill(1); Check(battle.Unit(1).MyStats.MyBlockCount == 1); Check(Close(battle.Unit(1).MyStats.MyHealthRegen, 0));
		battle.Step(); Check(battle.Unit(enemy).MyBlockedBy == 1);
	}

	void OperatorHookCleanup()
	{
		const std::array<OperatorKitDefinition, 3> kits{IndigoKit{}, VignaKit{}, ProveKit{.MyHunt = true}};
		for (const auto& kit : kits)
			for (const bool permanent : {false, true})
			{
				Battle battle(Input(kit)); battle.Start(); const auto enemy = Enemy(battle, {7, 9}, 1);
				(void)battle.LoseHealth(0, enemy, 1); const std::array<UnitId, 1> target{enemy};
				battle.Retreat(1, permanent); Check(!battle.ForceAttack(2, target)); // 本帧监听尚在。
				battle.Step(); Check(battle.ForceAttack(2, target) == permanent);
				Check(battle.Unit(2).MyTotals.MyAttacks == (permanent ? 1 : 0));
			}
	}

	void WildmnDiscounts()
	{
		for (const bool elite : {false, true})
			for (const bool permanent : {false, true})
			{
				const OperatorKitDefinition kit = WildmnKit{.MyCostCap = elite ? 5.0 : 1.0, .MyEveryDeploy = elite};
				auto input = Input(kit); auto& allies = input.MyPlayers[0].MyUnits;
				allies[1].MyDeferred = true; allies[1].MyDefinition.MyOperatorProfession = OperatorProfession::WARRIOR;
				allies[1].MyDefinition.MyStats.MyDeploymentCost = 10;
				auto cheap = allies[1]; cheap.MyPieceUid = 3; cheap.MyPosition = {7, 9}; cheap.MyDefinition.MyStats.MyDeploymentCost = 2;
				allies.push_back(std::move(cheap));
				auto token = allies[1]; token.MyPieceUid = 4; token.MyKind = UnitKind::TOKEN; token.MyPosition = {8, 9}; allies.push_back(std::move(token));
				auto other = allies[1]; other.MyPieceUid = 5; other.MyPosition = {9, 9};
				input.MyPlayers.push_back({.MyPlayerId = "q", .MyUnits = {std::move(other)}});
				Battle battle(std::move(input)); battle.Start();
				Check(Close(battle.Unit(2).MyDefinition.MyStats.MyDeploymentCost, 9));
				for (unsigned i = 0; i < 8; ++i) { battle.Retreat(1); Check(battle.Redeploy(1, true)); }
				Check(Close(battle.Unit(2).MyWildmnCostReduction, elite ? 5 : 1));
				Check(Close(battle.Unit(3).MyDefinition.MyStats.MyDeploymentCost, elite ? 0 : 1));
				Check(Close(battle.Unit(4).MyDefinition.MyStats.MyDeploymentCost, 10));
				Check(Close(battle.Unit(5).MyDefinition.MyStats.MyDeploymentCost, 10));
				battle.Retreat(1, permanent); battle.Step(); (void)battle.AddDp("p", 99);
				const auto dp = battle.Players()[0].MyDp, price = battle.Unit(2).MyDefinition.MyStats.MyDeploymentCost;
				Check(battle.Redeploy(2, false)); Check(Close(battle.Players()[0].MyDp, dp - price));
				// 永久释放全部来源监听后，原版也不再执行部署后的费用还原。
				Check(Close(battle.Unit(2).MyDefinition.MyStats.MyDeploymentCost, permanent ? price : 10));
				Check(Close(battle.Unit(2).MyWildmnCostReduction, permanent ? (elite ? 5 : 1) : 0));
			}
	}

	void WildmnSharedCap()
	{
		const OperatorKitDefinition normal = WildmnKit{}, elite = WildmnKit{.MyCostCap = 5, .MyEveryDeploy = true};
		for (const bool eliteFirst : {false, true})
		{
			auto input = Input(normal); auto& allies = input.MyPlayers[0].MyUnits;
			allies[0].MyDeferred = allies[1].MyDeferred = true;
			allies[1].MyDefinition.MyOperatorProfession = OperatorProfession::WARRIOR; allies[1].MyDefinition.MyStats.MyDeploymentCost = 10;
			allies.push_back(Ally(3, {5, 10}, &elite)); allies[2].MyDeferred = true;
			Battle battle(std::move(input)); battle.Start();
			Check(battle.Redeploy(eliteFirst ? 3 : 1, true)); Check(battle.Redeploy(eliteFirst ? 1 : 3, true));
			Check(Close(battle.Unit(2).MyWildmnCostReduction, eliteFirst ? 1 : 2));
		}
	}

	void WildmnPush(RulePositionMode _mode)
	{
		const OperatorKitDefinition kit = WildmnKit{.MyCharge = true};
		for (const bool behind : {false, true})
		{
			auto input = Input(kit, _mode); auto& definition = input.MyPlayers[0].MyUnits[0].MyDefinition;
			definition.MySkill.MyAttack = definition.MyAttack; definition.MySkill.MyAttack->MyOperatorSkillHit = true;
			definition.MySkill.MyAttack->MyHits = 3;
			Battle battle(std::move(input)); battle.Start(); Check(battle.Relocate(1, {5, 12}));
			const auto enemy = Enemy(battle, {behind ? 4.0 : 6.0, 12}); Check(battle.ActivateSkill(1));
			const std::array<UnitId, 1> target{enemy}; Check(battle.ForceAttack(1, target));
			Check(Close(battle.Unit(enemy).MyPosition.MyX, behind ? 3.56 : 8.14));
			Check(Close(battle.Unit(enemy).MyPosition.MyY, 12)); Check(Close(battle.Unit(enemy).MyHealth, 9700));
			battle.EndSkill(1); Check(battle.ForceAttack(1, target));
			Check(Close(battle.Unit(enemy).MyPosition.MyX, behind ? 3.56 : 8.14));
		}
	}

	void LiskamProtection()
	{
		const OperatorKitDefinition kit = LiskamKit{.MySp = 1, .MyDefense = true};
		auto input = Input(kit); auto& allies = input.MyPlayers[0].MyUnits;
		allies[0].MyDefinition.MySkill.MySpType = SpType::HURT;
		allies[0].MyDefinition.MySkill.MySpCost = 10; allies[0].MyDefinition.MySkill.MyInitialSp = 10;
		allies[1].MyDefinition.MySkill.MySpCost = 100; allies[1].MyDefinition.MySkill.MyInitialSp = 0;
		Battle battle(std::move(input)); battle.Start(); Check(battle.ActivateSkill(1));
		const auto blocked = [&]() { return std::ranges::any_of(battle.Unit(1).MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "liskam:block"; }); };
		Check(blocked()); const auto random = battle.RandomState();
		(void)battle.DealDamage(0, 1, 0, DamageType::TRUE_DAMAGE); (void)battle.LoseHealth(0, 1, 1);
		(void)battle.DealElement(0, 1, {.MyElement = Element::NEURAL, .MyAmount = 100});
		Check(blocked()); Check(battle.RandomState() == random); Check(Close(battle.SpTotal(2), 0));
		Check(Close(battle.DealDamage(0, 1, 100, DamageType::TRUE_DAMAGE), 0)); Check(!blocked());
		Check(Close(battle.Unit(1).MyHealth, 999)); Check(battle.RandomState() == random);
		Check(Close(battle.DealDamage(0, 1, 10, DamageType::TRUE_DAMAGE), 10));
		Check(Close(battle.SpTotal(1), 0)); Check(Close(battle.SpTotal(2), 1));
		(void)battle.DealDamage(0, 1, {.MyAmount = 10, .MyType = DamageType::TRUE_DAMAGE, .MyNoSp = true});
		Check(Close(battle.SpTotal(2), 1));
		for (const auto type : {DamageType::PHYSICAL, DamageType::ARTS, DamageType::ELEMENTAL})
		{
			battle.EndSkill(1); battle.SetSpTotal(1, 10); Check(battle.ActivateSkill(1));
			Check(blocked()); Check(Close(battle.DealDamage(0, 1, 100, type), 0)); Check(!blocked());
		}
		battle.EndSkill(1); Check(!battle.Unit(1).MyStatuses.Has(CombatStatus::STUN));
		(void)battle.DealDamage(0, 1, 10, DamageType::TRUE_DAMAGE);
		Check(Close(battle.SpTotal(1), 2)); Check(Close(battle.SpTotal(2), 2));
		battle.SetSpTotal(2, 100); Check(battle.ActivateSkill(2)); const auto beforeBusy = battle.RandomState();
		(void)battle.DealDamage(0, 1, 10, DamageType::TRUE_DAMAGE);
		Check(battle.RandomState() != beforeBusy); Check(Close(battle.SpTotal(2), 0));
	}

	void LiskamPosition(RulePositionMode _mode)
	{
		const OperatorKitDefinition kit = LiskamKit{.MySp = 1};
		auto input = Input(kit, _mode); auto other = Ally(3, {6, 12});
		other.MyDefinition.MySkill.MySpCost = 100; other.MyDefinition.MySkill.MyInitialSp = 0;
		input.MyPlayers[0].MyUnits[1].MyDefinition.MySkill = other.MyDefinition.MySkill;
		input.MyPlayers.push_back({.MyPlayerId = "q", .MyUnits = {std::move(other)}});
		Battle battle(std::move(input)); battle.Start(); Check(battle.Relocate(1, {5, 12})); Check(battle.Relocate(2, {10, 10}));
		(void)battle.DealDamage(0, 1, 1, DamageType::TRUE_DAMAGE);
		const UnitId chosen = _mode == RulePositionMode::INITIAL ? 2 : 3;
		Check(Close(battle.SpTotal(chosen), 1)); Check(Close(battle.SpTotal(chosen == 2 ? 3 : 2), 0));
		(void)battle.ApplyStatus(chosen, CombatStatus::ISOLATED, 10); const auto random = battle.RandomState();
		(void)battle.DealDamage(0, 1, 1, DamageType::TRUE_DAMAGE); Check(battle.RandomState() == random);
	}

	void LiskamHitProbability()
	{
		for (const double probability : {0.0, 1.0})
		{
			const OperatorKitDefinition kit = LiskamKit{.MyProbability = probability, .MyStun = 2, .MyArc = true};
			auto input = Input(kit); auto& definition = input.MyPlayers[0].MyUnits[0].MyDefinition;
			definition.MySkill.MyAttack = definition.MyAttack; definition.MySkill.MyAttack->MyOperatorSkillHit = true;
			Battle battle(std::move(input)); battle.Start(); Check(battle.ActivateSkill(1));
			battle.AddShield(2, {.MyHits = 1}); const std::array<UnitId, 1> target{2};
			Check(battle.ForceAttack(1, target)); Check(Close(battle.Unit(2).MyHealth, 1000));
			Check(battle.Unit(2).MyStatuses.Has(CombatStatus::STUN) == std::isgreater(probability, 0));
			Random expected(1); (void)expected.Next(); Check(battle.RandomState() == expected.State());
			// 技能命中对存活友方也抽签，零概率同样消耗随机数；普通攻击不携带该回调。
			battle.EndSkill(1); (void)battle.RemoveStatus(2, CombatStatus::STUN);
			Check(battle.ForceAttack(1, target)); Check(!battle.Unit(2).MyStatuses.Has(CombatStatus::STUN));
			Check(battle.RandomState() == expected.State());
		}
	}

	void OperatorSkillDelayedHit()
	{
		const std::array<OperatorKitDefinition, 2> kits{WildmnKit{.MyCharge = true}, LiskamKit{.MyProbability = 1, .MyStun = 2, .MySelfStun = 5, .MyArc = true}};
		for (const auto& kit : kits)
			for (const bool permanent : {false, true})
			{
				auto input = Input(kit); auto& definition = input.MyPlayers[0].MyUnits[0].MyDefinition;
				definition.MyAttack.MyRanged = true; definition.MyAttack.MyProjectileSpeed = 3;
				definition.MySkill.MyAttack = definition.MyAttack; definition.MySkill.MyAttack->MyOperatorSkillHit = true;
				definition.MySkill.MyAttack->MyHits = 3;
				Battle battle(std::move(input)); battle.Start(); const auto enemy = Enemy(battle, {8, 9});
				Check(battle.ActivateSkill(1)); const std::array<UnitId, 1> target{enemy}; Check(battle.ForceAttack(1, target));
				if (permanent) battle.Retreat(1, true); else battle.EndSkill(1);
				if (std::holds_alternative<LiskamKit>(kit)) Check(battle.Unit(1).MyStatuses.Has(CombatStatus::STUN) == !permanent);
				battle.Advance(32); Check(Close(battle.Unit(enemy).MyHealth, 9700));
				if (std::holds_alternative<WildmnKit>(kit)) Check(Close(battle.Unit(enemy).MyPosition.MyX, 10.14));
				else
				{
					Check(battle.Unit(enemy).MyStatuses.Has(CombatStatus::STUN)); Random expected(1); (void)expected.Next();
					Check(battle.RandomState() == expected.State());
				}
			}
	}
}

int main()
{
	try
	{
		for (const auto mode : {RulePositionMode::CURRENT, RulePositionMode::INITIAL})
		{
			EstellHealing(mode); PodegoHealingAndSp(mode); PodegoZoneLifetime(mode); TinmanLifetime(mode);
			IndigoMazePosition(mode); IndigoNormalAttackPosition(mode);
			WildmnPush(mode); LiskamPosition(mode);
		}
		OperatorHookCleanup(); PodegoAuraOwnership(); PithstElements(); TinmanRegeneration(); TinmanWither(); TinmanWeakZone();
		IndigoEnergyAndRetarget(); IndigoBlockedTarget(); IndigoDelayedHit();
		UtageBreach(); UtageProtectionAndSpeed(); UtageRest();
		WildmnDiscounts(); WildmnSharedCap(); LiskamProtection(); LiskamHitProbability(); OperatorSkillDelayedHit();
		std::cout << "operator kits lifecycle and position rules passed\n";
	}
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
