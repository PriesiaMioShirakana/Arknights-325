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

	AllyDeployment Ally(WorldPoint _point)
	{
		CombatDefinition definition{.MyId = "operator"};
		definition.MyStats.MyMaxHealth = 1000;
		definition.MyStats.MyAttack = 100;
		definition.MyStats.MyBlockCount = 0;
		definition.MyStats.MyMoveSpeed = 0;
		definition.MyStats.MyRedeploySeconds = 100;
		definition.MyAttack.MyDamageType = DamageType::TRUE_DAMAGE;
		definition.MySkill = {.MyKind = SkillKind::DURATION, .MySpType = SpType::NONE, .MyTrigger = SkillTrigger::NEVER,
			.MySpCost = 1, .MyInitialSp = 1, .MyDuration = 30, .MyManual = false, .MyAttack = definition.MyAttack};
		return {.MyPieceUid = 1 + static_cast<std::uint64_t>(_point.MyY * FieldColumns + _point.MyX),
			.MyDefinition = std::move(definition), .MyPosition = _point};
	}

	BattleInput Input(RulePositionMode _mode, WorldPoint _enemy = {6, 9})
	{
		CombatDefinition enemy{.MyId = "enemy"};
		enemy.MyStats.MyMaxHealth = 10000; enemy.MyStats.MyMoveSpeed = 0; enemy.MyAttack.MyDisabled = true;
		BattleInput input{.MyPlayers = {{.MyPlayerId = "p", .MyUnits = {Ally({5, 9})}}},
			.MySpawns = {{.MyOwnerId = "p", .MyDefinition = std::move(enemy), .MyRoute = {.MyStart = _enemy, .MyEnd = {0, 9}}}},
			.MyAutoFinish = false, .MyRulePositionMode = _mode};
		return input;
	}

	Battle Start(BattleInput _input)
	{
		auto spawns = std::move(_input.MySpawns); _input.MySpawns.clear();
		Battle battle(std::move(_input)); battle.Start();
		for (auto& spawn : spawns) (void)battle.SpawnEnemy(std::move(spawn));
		return battle;
	}

	void SkillTriggers(RulePositionMode _mode)
	{
		for (const auto trigger : {SkillTrigger::DEFAULT, SkillTrigger::SEARCH, SkillTrigger::CUSTOM_RANGE,
			SkillTrigger::SKILL_RANGE, SkillTrigger::ACTIVE_RANGE})
			for (const bool nearHome : {false, true})
			{
				auto input = Input(_mode, {6, nearHome ? 9.0 : 12.0});
				auto& skill = input.MyPlayers[0].MyUnits[0].MyDefinition.MySkill;
				skill.MyTrigger = trigger; skill.MyTriggerRange = {{0, 1}};
				auto battle = Start(std::move(input));
				Check(battle.Relocate(1, {5, 12})); battle.Step();
				const bool expected = nearHome == (_mode == RulePositionMode::INITIAL);
				Check((battle.Unit(1).MySkill.MyActivations == 1) == expected);
				if (expected) Check(Close(battle.Unit(2).MyHealth, 9900));
			}
	}

	void HealingTriggersAndTargets(RulePositionMode _mode)
	{
		for (const bool nearHome : {false, true})
		{
			auto input = Input(_mode, {18, 9});
			auto& healer = input.MyPlayers[0].MyUnits[0].MyDefinition;
			healer.MyAttack.MyDisabled = true;
			healer.MySkill.MyTrigger = SkillTrigger::SKILL_RANGE;
			healer.MySkill.MyTriggerRange = {{0, 1}};
			healer.MySkill.MyTriggerAllies = true; healer.MySkill.MyHealSkill = true;
			healer.MySkill.MyAttack->MyHealing = true;
			input.MyPlayers[0].MyUnits.push_back(Ally({nearHome ? 6.0 : 8.0, 9}));
			auto battle = Start(std::move(input));
			Check(battle.Relocate(1, {5, 12})); Check(battle.MoveRedeploy(2, {6, nearHome ? 15.0 : 12.0}));
			Check(Close(battle.LoseHealth(0, 2, 500), 500)); battle.Step();
			const bool expected = nearHome == (_mode == RulePositionMode::INITIAL);
			Check((battle.Unit(1).MySkill.MyActivations == 1) == expected);
			Check(Close(battle.Unit(2).MyHealth, expected ? 600 : 500));
		}
	}

	void HealingSecondaryEffects(RulePositionMode _mode, bool _medic)
	{
		auto input = Input(_mode, {18, 9});
		auto& definition = input.MyPlayers[0].MyUnits[0].MyDefinition;
		definition.MySkill.MyAttack->MyHealing = true;
		if (_medic) definition.MyMedic = MedicKitDefinition{.MySkillExtraHeal = 0.75};
		else definition.MySkill.MyAttack->MyHealChainCount = 2;
		input.MyPlayers[0].MyUnits.push_back(Ally({6, 9}));
		input.MyPlayers[0].MyUnits.push_back(Ally({7, 9}));
		auto battle = Start(std::move(input));
		Check(battle.Relocate(3, {12, 9}));
		(void)battle.LoseHealth(0, 2, 200); (void)battle.LoseHealth(0, 3, 500);
		Check(battle.ActivateSkill(1)); Check(battle.ForceAttack(1));
		Check(Close(battle.Unit(3).MyHealth, _mode == RulePositionMode::INITIAL ? 575 : 500));
	}

	void SourceRangeAndAbsoluteRange(RulePositionMode _mode)
	{
		for (const bool absolute : {false, true})
		{
			auto input = Input(_mode, {7, 9});
			auto& source = input.MyPlayers[0].MyUnits[0].MyDefinition;
			source.MyAttack.MyDisabled = true; source.MySkill.MyTrigger = SkillTrigger::SEARCH;
			input.MyPlayers[0].MyUnits.push_back(Ally({6, 9}));
			auto battle = Start(std::move(input));
			SkillTriggerArea area{.MySourceUnit = absolute ? 0U : 2U};
			area.MyMask.set(static_cast<std::size_t>(FieldGrid::Key(9, 7)));
			(void)battle.AddSkillTriggerRange(1, area);
			Check(battle.Relocate(2, {6, 12})); battle.Step();
			Check((battle.Unit(1).MySkill.MyActivations == 1) == (absolute || _mode == RulePositionMode::INITIAL));
		}
	}

	void SkillRangeAndRedeploy(RulePositionMode _mode)
	{
		auto input = Input(_mode); auto& source = input.MyPlayers[0].MyUnits[0].MyDefinition;
		source.MyRange = {{0, 0}}; source.MySkill.MyRange = {{0, 1}};
		auto battle = Start(std::move(input));
		Check(battle.Relocate(1, {5, 12})); Check(battle.ActivateSkill(1));
		Check(battle.ForceAttack(1) == (_mode == RulePositionMode::INITIAL));
		battle.EndSkill(1); Check(!battle.ForceAttack(1));
		battle.Retreat(1); Check(battle.Redeploy(1, true, WorldPoint{5, 15}));
		Check(Close(battle.Unit(1).MyHome.MyY, 9)); Check(battle.ActivateSkill(1));
		Check(battle.ForceAttack(1) == (_mode == RulePositionMode::INITIAL));
		// 敌人仍按当前位置受击，初始模式不会把其冻结在出生点。
		Check(Close(battle.Displace(2, {0, 1}, 4), 4));
		Check(!battle.InRuleRange(1, 2));
	}

	void NormalAttackAndProjectile(RulePositionMode _mode)
	{
		auto input = Input(_mode, {6, 12});
		auto normal = Start(std::move(input)); Check(normal.Relocate(1, {5, 12}));
		Check(normal.ForceAttack(1)); Check(Close(normal.Unit(2).MyHealth, 9900));

		input = Input(_mode);
		auto& source = input.MyPlayers[0].MyUnits[0].MyDefinition;
		source.MySkill.MyKind = SkillKind::INSTANT;
		source.MyTraitFrontRange = std::vector<RangeOffset>{{0, 1}};
		source.MySkill.MyAttack->MyScaling = AttackScaling::FRONT;
		source.MySkill.MyAttack->MyConditionalScale = 2;
		source.MySkill.MyAttack->MyRanged = true; source.MySkill.MyAttack->MyProjectileSpeed = 12;
		source.MyAttack.MyDisabled = true;
		auto ranged = Start(std::move(input)); Check(ranged.Relocate(1, {5, 12}));
		Check(ranged.ActivateSkill(1)); const std::array<UnitId, 1> target{2};
		Check(ranged.ForceAttack(1, target)); Check(!ranged.Unit(1).MySkill.MyActive);
		ranged.Advance(30); Check(Close(ranged.Unit(2).MyHealth, _mode == RulePositionMode::INITIAL ? 9800 : 9900));
	}

	void MapEdgeAndRangeChanges(RulePositionMode _mode)
	{
		auto input = Input(_mode, {19, 9});
		auto& source = input.MyPlayers[0].MyUnits[0];
		source.MyPosition = {18, 9}; source.MyDefinition.MyRange = {{0, 1}};
		auto battle = Start(std::move(input)); Check(battle.Relocate(1, {20, 9}));
		Check(battle.RuleRange(1).test(static_cast<std::size_t>(FieldGrid::Key(9, 19))) == (_mode == RulePositionMode::INITIAL));
		(void)battle.AddBuff(1, {.MyKey = "extend", .MyModifiers = std::vector<AttributeChange>{{Attribute::RANGE_EXTEND, 1}}});
		Check(battle.RuleRange(1).test(static_cast<std::size_t>(FieldGrid::Key(9, 20))) == (_mode == RulePositionMode::INITIAL));
		Check(battle.RemoveBuff(1, "extend") == 1);
		Check(!battle.RuleRange(1).test(static_cast<std::size_t>(FieldGrid::Key(9, 20))));
		Check(battle.Relocate(1, {5, 9}));
		Check(battle.RuleRangeKeys(1).size() == 1);
	}

	void MovingSummonRange(RulePositionMode _mode)
	{
		auto input = Input(_mode, {10, 9}); auto& source = input.MyPlayers[0].MyUnits[0];
		source.MyKind = UnitKind::TOKEN;
		source.MyDefinition.MyYanyou = YanyouKitDefinition{};
		source.MyDefinition.MySkill.MyTrigger = SkillTrigger::SEARCH;
		auto battle = Start(std::move(input)); battle.Advance(270);
		Check(battle.Unit(1).MyPosition.MyX > 8);
		Check((battle.Unit(1).MySkill.MyActivations == 1) == (_mode == RulePositionMode::CURRENT));
		Check(battle.InRuleRange(1, 2) == (_mode == RulePositionMode::CURRENT));
	}

	void GenericSkillRanges(RulePositionMode _mode)
	{
		constexpr std::array<GenericStatusEffect, 1> statuses{{{CombatStatus::STUN, 2}}};
		constexpr std::array<AttributeChange, 1> debuff{{{Attribute::DEFENSE_PERCENT, -0.5}}};
		const GenericSkillEffects effects{.MyDebuffKey = "generic:range", .MyStartStatuses = statuses, .MyDebuff = debuff,
			.MyAura = true, .MyStartBurst = 2, .MyEndBurst = 3};
		auto input = Input(_mode);
		auto& source = input.MyPlayers[0].MyUnits[0].MyDefinition;
		source.MyAttack.MyDisabled = true; source.MySkill.MyAttack->MyDisabled = true;
		source.MyGenericSkill = &effects;
		input.MySpawns[0].MyDefinition.MyStats.MyDefense = 100;
		auto battle = Start(std::move(input)); Check(battle.Relocate(1, {5, 12}));
		Check(battle.ActivateSkill(1));
		const bool initial = _mode == RulePositionMode::INITIAL;
		Check(Close(battle.Unit(2).MyHealth, initial ? 9800 : 10000));
		Check(battle.Unit(2).MyStatuses.Has(CombatStatus::STUN) == initial);
		battle.Step();
		Check(Close(battle.Unit(2).MyStats.MyDefense, initial ? 50 : 100));
		battle.EndSkill(1);
		Check(Close(battle.Unit(2).MyHealth, initial ? 9500 : 10000));
		battle.Advance(30);
		Check(Close(battle.Unit(2).MyStats.MyDefense, 100));
	}

	void GenericSkillHealing(RulePositionMode _mode)
	{
		for (const bool all : {false, true})
		{
			const GenericSkillEffects effects{.MyHealAlly = 2, .MyHealAll = all};
			auto input = Input(_mode);
			auto& source = input.MyPlayers[0].MyUnits[0].MyDefinition;
			source.MyGenericSkill = &effects; source.MySkill.MyAttack->MyGenericHit = true;
			input.MyPlayers[0].MyUnits.push_back(Ally({6, 9}));
			auto battle = Start(std::move(input));
			Check(battle.Relocate(1, {5, 12})); Check(battle.Relocate(2, {6, 15}));
			Check(Close(battle.LoseHealth(0, 2, 500), 500));
			Check(battle.ActivateSkill(1)); const std::array<UnitId, 1> targets{3};
			Check(battle.ForceAttack(1, targets));
			Check(Close(battle.Unit(2).MyHealth, _mode == RulePositionMode::INITIAL ? 700 : 500));
		}
	}

	void GenericSkillCounter(RulePositionMode _mode)
	{
		const GenericSkillEffects effects{.MyCounter = true, .MyCounterScale = 2, .MyCounterType = DamageType::TRUE_DAMAGE, .MyCounterAround = true};
		auto input = Input(_mode);
		input.MyPlayers[0].MyUnits[0].MyDefinition.MyGenericSkill = &effects;
		auto battle = Start(std::move(input)); Check(battle.Relocate(1, {5, 12}));
		Check(battle.ActivateSkill(1));
		Check(Close(battle.DealDamage(2, 1, {.MyAmount = 20, .MyType = DamageType::TRUE_DAMAGE, .MyIsAttack = true}), 20));
		Check(Close(battle.Unit(2).MyHealth, _mode == RulePositionMode::INITIAL ? 9800 : 10000));
	}

	void OperatorSkillPositions(RulePositionMode _mode)
	{
		constexpr std::array<RangeOffset, 1> grid{{{0, 1}}};
		const OperatorKitDefinition texas = TexasKit{.MyInitialDp = 2, .MyDp = 3, .MyScale = 2, .MyStun = 2, .MyRange = grid, .MySwordRain = true};
		const OperatorKitDefinition prove = ProveKit{.MyHealthDrop = 0.2, .MyScalePerDrop = 0.2, .MyFrontProbability = 1, .MyCriticalScale = 2};
		const OperatorKitDefinition caper = CaperKit{.MyNearScale = 1.5};
		for (const bool home : {false, true})
		{
			const bool inRange = home == (_mode == RulePositionMode::INITIAL);
			for (const auto* kit : {&texas, &prove, &caper})
			{
				auto input = Input(_mode, {6, home ? 9.0 : 12.0});
				input.MyInitialDp = 0;
				auto& source = input.MyPlayers[0].MyUnits[0].MyDefinition; source.MyOperatorKit = kit;
				auto battle = Start(std::move(input)); Check(battle.Relocate(1, {5, 12}));
				if (kit == &texas)
				{
					Check(Close(battle.Players()[0].MyDp, 2)); Check(battle.ActivateSkill(1));
					Check(Close(battle.Players()[0].MyDp, 5));
					Check(Close(battle.Unit(2).MyHealth, inRange ? 9600 : 10000));
					Check(battle.Unit(2).MyStatuses.Has(CombatStatus::STUN) == inRange);
				}
				else
				{
					(void)battle.LoseHealth(0, 2, 2000);
					const auto expected = kit == &prove ? (inRange ? 240.0 : 120.0) : (inRange ? 150.0 : 100.0);
					Check(Close(battle.DealDamage(1, 2, {.MyAmount = 100, .MyType = DamageType::TRUE_DAMAGE, .MyIsAttack = true}), expected));
					Check(Close(battle.DealDamage(1, 2, 100, DamageType::TRUE_DAMAGE), 100));
				}
			}
			const OperatorKitDefinition hunt = ProveKit{.MyHunt = true};
			auto input = Input(_mode, {6, home ? 9.0 : 12.0}); input.MyPlayers[0].MyUnits[0].MyDefinition.MyOperatorKit = &hunt;
			auto battle = Start(std::move(input)); Check(battle.Relocate(1, {5, 12})); Check(battle.ActivateSkill(1));
			for (unsigned tick = 0; tick < 30; ++tick) battle.Step();
			Check(battle.Unit(1).MyTotals.MyAttacks == 0);
			(void)battle.LoseHealth(0, 2, 3000); battle.Step();
			Check(battle.Unit(1).MyTotals.MyAttacks == (inRange ? 1U : 0U));
		}
	}

	void VendlaPositions(RulePositionMode _mode)
	{
		const OperatorKitDefinition kit = VendlaKit{.MyHealingScale = 1.5, .MyCounterScale = 2, .MyProtection = true};
		auto input = Input(_mode, {18, 9}); auto& player = input.MyPlayers[0];
		auto& source = player.MyUnits[0].MyDefinition;
		source.MyOperatorKit = &kit; source.MyProfession.MyKind = ProfessionTrait::INCANTATION;
		source.MyRange = {{0, 0}, {0, 1}};
		player.MyUnits.push_back(Ally({6, 9})); player.MyUnits.push_back(Ally({8, 9}));
		for (std::size_t i = 1; i < 3; ++i) player.MyUnits[i].MyDefinition.MyStats.MyMaxHealth = 2000;
		auto battle = Start(std::move(input)); Check(battle.Relocate(1, {5, 12}));
		Check(battle.Relocate(2, {6, 15})); Check(battle.Relocate(3, {6, 12}));
		(void)battle.LoseHealth(0, 1, 600); (void)battle.LoseHealth(0, 2, 1000); (void)battle.LoseHealth(0, 3, 1000);
		Check(battle.ActivateSkill(1)); const UnitId protege = _mode == RulePositionMode::INITIAL ? 2 : 3;
		Check(Close(battle.Unit(protege).MyStats.MyTaunt, 1));
		Check(Close(battle.DealDamage(4, protege, 100, DamageType::TRUE_DAMAGE), 100));
		Check(Close(battle.Unit(4).MyHealth, 9800)); Check(Close(battle.Unit(protege).MyHealth, 1050));
		Check(Close(battle.Unit(1).MyHealth, 400)); // 反击治疗只指向保护对象。
		Check(Close(battle.DealDamage(1, 4, 100, DamageType::TRUE_DAMAGE), 100));
		Check(Close(battle.Unit(1).MyHealth, 450)); // 其他伤害仍治疗范围内最低生命比例者。
		for (const auto tag : {DamageTag::COUNTER, DamageTag::REFLECT, DamageTag::HP_LOSS})
			(void)battle.DealDamage(4, protege, {.MyAmount = 10, .MyType = DamageType::TRUE_DAMAGE, .MyTags = static_cast<DamageTags>(tag)});
		(void)battle.DealDamage(4, protege, {.MyAmount = 10, .MyType = DamageType::TRUE_DAMAGE, .MySourceless = true});
		Check(Close(battle.Unit(4).MyHealth, 9700));
		battle.Retreat(1); Check(Close(battle.Unit(protege).MyStats.MyTaunt, 0));
	}

	void VendlaHealingCache()
	{
		const OperatorKitDefinition kit = VendlaKit{.MyHealingScale = 1.5};
		auto input = Input(RulePositionMode::CURRENT, {18, 9}); auto& player = input.MyPlayers[0];
		player.MyUnits[0].MyDefinition.MyOperatorKit = &kit;
		player.MyUnits[0].MyDefinition.MyRange = {{0, 0}, {0, 1}, {0, 2}};
		player.MyUnits.push_back(Ally({6, 9})); player.MyUnits.push_back(Ally({7, 9}));
		player.MyUnits[1].MyDefinition.MyStats.MyMaxHealth = 2000; player.MyUnits[2].MyDefinition.MyStats.MyMaxHealth = 1500;
		for (auto& ally : player.MyUnits) ally.MyDefinition.MyAttack.MyDisabled = true;
		auto battle = Start(std::move(input)); (void)battle.LoseHealth(0, 2, 1000); (void)battle.LoseHealth(0, 3, 1000);
		Check(Close(battle.Heal(0, 2, 100), 150));
		(void)battle.AddBuff(3, {.MyKey = "health", .MyModifiers = std::vector<AttributeChange>{{Attribute::HEALTH_FLAT, 1000}}});
		Check(Close(battle.Heal(0, 3, 100), 100)); // 本帧缓存不因最大生命变化而重选。
		battle.Step(); Check(Close(battle.Heal(0, 3, 100), 150));
		Check(Close(battle.Heal(0, 3, 100, {.MyRegen = true}), 100));
	}

	void SunbrLandedProc()
	{
		const OperatorKitDefinition kit = SunbrKit{.MyProbability = 1, .MyCriticalScale = 2, .MyStun = 2};
		auto input = Input(RulePositionMode::CURRENT); input.MyPlayers[0].MyUnits[0].MyDefinition.MyOperatorKit = &kit;
		auto battle = Start(std::move(input));
		(void)battle.AddBuff(2, {.MyKey = "dodge", .MyModifiers = std::vector<AttributeChange>{{Attribute::PHYSICAL_DODGE, 1}}});
		Check(Close(battle.DealDamage(1, 2, {.MyAmount = 100, .MyIsAttack = true}), 0));
		Check(!battle.Unit(2).MyStatuses.Has(CombatStatus::STUN)); Check(battle.RemoveBuff(2, "dodge") == 1);
		Check(Close(battle.DealDamage(1, 2, {.MyAmount = 100, .MyIsAttack = true, .MyIsSplash = true}), 100));
		Check(Close(battle.DealDamage(1, 2, {.MyAmount = 100, .MyTags = static_cast<DamageTags>(DamageTag::CHAIN), .MyIsAttack = true}), 100));
		Check(Close(battle.DealDamage(1, 2, 100, DamageType::PHYSICAL), 100));
		Check(!battle.Unit(2).MyStatuses.Has(CombatStatus::STUN));
		battle.AddShield(2, {.MyHealth = 1000});
		Check(Close(battle.DealDamage(1, 2, {.MyAmount = 100, .MyIsAttack = true}), 0));
		Check(battle.Unit(2).MyStatuses.Has(CombatStatus::STUN)); // 护盾吸收生命伤害仍属于命中。
	}

	void OperatorRevealPositions(RulePositionMode _mode)
	{
		const OperatorKitDefinition kit = UdflowKit{.MyReveal = true};
		auto input = Input(_mode);
		auto& source = input.MyPlayers[0].MyUnits[0].MyDefinition;
		source.MyOperatorKit = &kit; source.MyAttack.MyDisabled = true;
		auto battle = Start(std::move(input));
		Check(battle.ApplyStatus(2, CombatStatus::STEALTH, 30));
		Check(battle.Relocate(1, {5, 12})); battle.Advance(7);
		Check(battle.Unit(2).MyStatuses.Has(CombatStatus::REVEAL) == (_mode == RulePositionMode::INITIAL));
		battle.Retreat(1); battle.Advance(12);
		Check(!battle.Unit(2).MyStatuses.Has(CombatStatus::REVEAL));
		Check(battle.Redeploy(1, true)); battle.Advance(7);
		Check(battle.Unit(2).MyStatuses.Has(CombatStatus::REVEAL) == (_mode == RulePositionMode::INITIAL));
	}

	struct SpawnDuringReveal final : CustomBondEffect<SpawnDuringReveal>
	{
		bool MySpawned{};

		void OnStatusApplied(Battle& _battle, ContentEvent& _event)
		{
			if (MySpawned || _event.MyStatus != CombatStatus::REVEAL) return;
			MySpawned = true;
			for (unsigned i = 0; i < 8; ++i)
			{
				CombatDefinition enemy{.MyId = "spawned", .MyStats = {.MyMaxHealth = 1000, .MyMoveSpeed = 0}, .MyAttack = {.MyDisabled = true}};
				const auto id = _battle.SpawnEnemy({.MyOwnerId = "p", .MyDefinition = std::move(enemy), .MyRoute = {.MyStart = {6, 9}, .MyEnd = {0, 9}}});
				(void)_battle.ApplyStatus(id, CombatStatus::STEALTH, 30);
			}
		}
	};

	void OperatorRevealReentrancy()
	{
		ContentRegistry registry;
		const auto hook = registry.Register<SpawnDuringReveal>("reveal-spawns"); registry.Seal();
		const OperatorKitDefinition kit = UdflowKit{.MyReveal = true};
		auto input = Input(RulePositionMode::CURRENT);
		input.MyContentRegistry = std::cref(registry); input.MyContentBindings = {{hook, "p"}};
		input.MyPlayers[0].MyUnits[0].MyDefinition.MyOperatorKit = &kit;
		input.MyPlayers[0].MyUnits[0].MyDefinition.MyAttack.MyDisabled = true;
		auto battle = Start(std::move(input));
		Check(battle.ApplyStatus(2, CombatStatus::STEALTH, 30)); battle.Advance(7);
		Check(battle.Units().size() == 10);
		for (const auto& unit : battle.Units()) if (unit.MySide == UnitSide::ENEMY) Check(unit.MyStatuses.Has(CombatStatus::REVEAL));
		Check(battle.ContentErrors().empty());
	}

	void BondAura(RulePositionMode _mode)
	{
		auto input = Input(_mode, {18, 9}); auto& player = input.MyPlayers[0];
		player.MyUnits[0].MyDefinition.MyIdentity.MyBonds = {"skillfulShip"};
		player.MyUnits.push_back(Ally({6, 9}));
		player.MyBonds = {{.MyId = "skillfulShip", .MyActive = true}};
		player.MyAddonBonds = {{.MyKind = AddonBondKind::SKILLFUL, .MyParameters = {.MyAttackSpeed = 25}}};
		auto battle = Start(std::move(input));
		Check(battle.Relocate(1, {5, 12})); Check(battle.Relocate(2, {6, 15}));
		battle.Advance(9); Check(Close(battle.Unit(2).MyStats.MyAttackSpeed, _mode == RulePositionMode::INITIAL ? 125 : 100));
		(void)battle.LoseHealth(0, 1, 10000); battle.Advance(9);
		Check(Close(battle.Unit(2).MyStats.MyAttackSpeed, _mode == RulePositionMode::INITIAL ? 125 : 100));
	}

	void GarrisonPositions(RulePositionMode _mode)
	{
		constexpr std::array<std::string_view, 1> bonds{"yanShip"};
		const std::array<BattleGarrisonRule, 2> rules{{
			{.MyId = "gain", .MyKind = BattleGarrisonKind::SKILL_GAIN, .MyBonds = bonds, .MyGainAmount = GarrisonGainAmount::ROW_COUNT, .MyAmount = 1},
			{.MyId = "magic", .MyKind = BattleGarrisonKind::EXTRA_GAIN, .MyExtra = 4}}};
		auto input = Input(_mode, {18, 9}); auto& player = input.MyPlayers[0];
		input.MyGarrisonRules = {rules, bonds};
		player.MyUnits[0].MyDefinition.MyIdentity.MyGarrisons = {"gain"};
		player.MyUnits.push_back(Ally({6, 9})); player.MyUnits.push_back(Ally({4, 9}));
		player.MyUnits[2].MyDefinition.MyIdentity.MyGarrisons = {"magic"};
		player.MyBonds = {{.MyId = "yanShip", .MyActive = true}};
		auto battle = Start(std::move(input));
		Check(battle.Relocate(1, {5, 12})); Check(battle.Relocate(2, {6, 12})); Check(battle.ActivateSkill(1));
		Check(Close(battle.BondLayers("p", "yanShip"), _mode == RulePositionMode::INITIAL ? 7 : 2));
	}

	void GarrisonStatusRange(RulePositionMode _mode)
	{
		constexpr std::array<std::string_view, 1> bonds{"yanShip"};
		const std::array<BattleGarrisonRule, 1> rules{{{.MyId = "freeze", .MyKind = BattleGarrisonKind::FREEZE_GAIN, .MyBonds = bonds, .MyAmount = 3}}};
		auto input = Input(_mode); input.MyGarrisonRules = {rules, bonds};
		input.MyPlayers[0].MyUnits[0].MyDefinition.MyIdentity.MyGarrisons = {"freeze"};
		input.MyPlayers[0].MyBonds = {{.MyId = "yanShip", .MyActive = true}};
		auto battle = Start(std::move(input)); Check(battle.Relocate(1, {5, 12}));
		Check(battle.ApplyStatus(2, CombatStatus::FREEZE, 1));
		Check(Close(battle.BondLayers("p", "yanShip"), _mode == RulePositionMode::INITIAL ? 3 : 0));
	}

	void GarrisonAfterExit(RulePositionMode _mode, bool _enabled)
	{
		constexpr std::array<std::string_view, 1> bonds{"yanShip"};
		// 魔王普通／精锐的原始参数：前方特质加层额外 +1／+2。
		for (const double extra : {1.0, 2.0}) for (const bool nearHome : {false, true}) for (unsigned exit = 0; exit < 3; ++exit)
		{
			const std::array<BattleGarrisonRule, 1> rules{{{.MyId = "magic", .MyKind = BattleGarrisonKind::EXTRA_GAIN, .MyExtra = extra}}};
			auto input = Input(_mode, {18, 9}); input.MyGarrisonRules = {rules, bonds}; input.MyGarrisonEffectsAfterExit = _enabled;
			auto& player = input.MyPlayers[0]; player.MyBonds = {{.MyId = "yanShip", .MyActive = true}};
			player.MyUnits[0].MyDefinition.MyIdentity.MyGarrisons = {"magic"};
			player.MyUnits.push_back(Ally({nearHome ? 6.0 : 8.0, 9}));
			auto battle = Start(std::move(input));
			Check(battle.Relocate(1, {5, 12})); Check(battle.Relocate(2, nearHome ? WorldPoint{9, 9} : WorldPoint{6, 12}));
			const bool aligned = nearHome == (_mode == RulePositionMode::INITIAL);
			const auto gain = [&] { return battle.AddBondLayers("p", "yanShip", 1, {.MySource = 2, .MyReason = "garrison"}); };
			Check(Close(gain(), 1 + (aligned ? extra : 0)));
			if (exit == 0) (void)battle.LoseHealth(0, 1, 10000);
			else battle.Retreat(1, exit == 2);
			battle.Step(); Check(!battle.Unit(1).MyAlive);
			Check(Close(gain(), 1 + (_enabled && aligned ? extra : 0)));
			Check(Close(battle.AddBondLayers("p", "yanShip", 1, {.MySource = 2, .MyReason = "bond"}), 1));
			if (exit != 2)
			{
				Check(battle.Redeploy(1, true, WorldPoint{5, 12}));
				Check(Close(gain(), 1 + (aligned ? extra : 0))); // 再部署恢复，且不会重复安装奖励。
			}
		}
	}

	void AfterExitStatusesAndAmmo(RulePositionMode _mode, bool _enabled)
	{
		constexpr std::array<std::string_view, 1> bonds{"yanShip"};
		for (const auto scope : {GarrisonAmmoScope::FRONT, GarrisonAmmoScope::ADJACENT})
		{
			const std::array<BattleGarrisonRule, 2> rules{{
				{.MyId = "ammo", .MyKind = BattleGarrisonKind::AMMO_GAIN, .MyBonds = bonds, .MyAmmoScope = scope, .MyAmount = 2},
				{.MyId = "freeze", .MyKind = BattleGarrisonKind::FREEZE_GAIN, .MyBonds = bonds, .MyAmount = 3}}};
			auto input = Input(_mode); input.MyGarrisonRules = {rules, bonds}; input.MyGarrisonEffectsAfterExit = _enabled;
			auto& player = input.MyPlayers[0]; player.MyBonds = {{.MyId = "yanShip", .MyActive = true}};
			player.MyUnits[0].MyDefinition.MyIdentity.MyGarrisons = {"ammo", "freeze"};
			player.MyUnits.push_back(Ally({6, 9}));
			player.MyUnits[1].MyDefinition.MySkill.MyKind = SkillKind::AMMO;
			player.MyUnits[1].MyDefinition.MySkill.MyAmmo = 3;
			auto battle = Start(std::move(input));
			Check(battle.Relocate(1, {5, 12})); Check(battle.Relocate(2, {6, 12}));
			battle.Retreat(1); battle.Step();
			Check(battle.ApplyStatus(3, CombatStatus::FREEZE, 1));
			Check(battle.ActivateSkill(2)); const std::array<UnitId, 1> targets{3}; Check(battle.ForceAttack(2, targets));
			const auto expected = _enabled ? 2 + (_mode == RulePositionMode::INITIAL ? 3 : 0) : 0;
			Check(Close(battle.BondLayers("p", "yanShip"), expected));
		}
	}

	void AfterExitChainingAndDeferred(bool _enabled)
	{
		constexpr std::array<std::string_view, 1> bonds{"yanShip"};
		const std::array<BattleGarrisonRule, 2> rules{{
			{.MyId = "freeze", .MyKind = BattleGarrisonKind::FREEZE_GAIN, .MyBonds = bonds, .MyAmount = 3},
			{.MyId = "magic", .MyKind = BattleGarrisonKind::EXTRA_GAIN, .MyExtra = 2}}};
		auto input = Input(RulePositionMode::CURRENT); input.MyGarrisonRules = {rules, bonds}; input.MyGarrisonEffectsAfterExit = _enabled;
		auto& player = input.MyPlayers[0]; player.MyBonds = {{.MyId = "yanShip", .MyActive = true}};
		player.MyUnits[0].MyDefinition.MyIdentity.MyGarrisons = {"freeze"};
		player.MyUnits.push_back(Ally({4, 9})); player.MyUnits[1].MyDefinition.MyIdentity.MyGarrisons = {"magic"};
		auto battle = Start(input); battle.Retreat(1); battle.Step();
		Check(battle.ApplyStatus(3, CombatStatus::FREEZE, 1));
		Check(Close(battle.BondLayers("p", "yanShip"), _enabled ? 5 : 0));
		// 开关不会让从未部署过的魔王提供效果，也不会把退场单位计作在场目标。
		input.MyPlayers[0].MyUnits[1].MyDeferred = true;
		auto deferred = Start(std::move(input)); Check(!deferred.Unit(2).MyAlive);
		Check(Close(deferred.AddBondLayers("p", "yanShip", 1, {.MySource = 1, .MyReason = "garrison"}), 1));
	}

	void GrantedTraitSurvivesGrantor(RulePositionMode _mode, bool _enabled, bool _retain, unsigned _exit, bool _native)
	{
		constexpr std::array<std::string_view, 1> bonds{"kjeragShip"};
		const std::array<BattleGarrisonRule, 2> rules{{
			{.MyId = "freeze", .MyKind = BattleGarrisonKind::FREEZE_GAIN, .MyBonds = bonds, .MyAmount = 3, .MyMaximum = 5},
			{.MyId = "grant", .MyKind = BattleGarrisonKind::GRANT, .MyGrantedGarrison = "freeze", .MyRequiredBond = "kjeragShip"}}};
		auto input = Input(_mode, {7, 9}); input.MyGarrisonRules = {rules, bonds}; input.MyGarrisonEffectsAfterExit = _enabled;
		input.MyRetainGrantedGarrisonsAfterExit = _retain;
		auto& player = input.MyPlayers[0]; player.MyBonds = {{.MyId = "kjeragShip", .MyActive = true}};
		player.MyUnits[0].MyDefinition.MyIdentity.MyGarrisons = {"grant"};
		player.MyUnits.push_back(Ally({6, 9})); player.MyUnits[1].MyDefinition.MyIdentity.MyBonds = {"kjeragShip"};
		if (_native) player.MyUnits[1].MyDefinition.MyIdentity.MyGarrisons = {"freeze"};
		auto battle = Start(std::move(input));
		Check(battle.RetainGrantedGarrisonsAfterExit() == _retain);
		battle.Retreat(1, true); battle.Step(); // 凛御银灰永久退场，已经授予的特质依然属于接受者。
		Check(battle.ApplyStatus(3, CombatStatus::FREEZE, 1));
		Check(Close(battle.BondLayers("p", "kjeragShip"), 3));
		Check(battle.RemoveStatus(3, CombatStatus::FREEZE));
		Check(battle.Relocate(2, {6, 12}));
		if (_mode == RulePositionMode::CURRENT) Check(Close(battle.Displace(3, {0, 1}, 3), 3));
		if (_exit == 0) (void)battle.LoseHealth(0, 2, 10000);
		else battle.Retreat(2, _exit == 2);
		battle.Step();
		Check(battle.ApplyStatus(3, CombatStatus::FREEZE, 1));
		const bool kept = _retain || _native;
		Check(Close(battle.BondLayers("p", "kjeragShip"), kept && _enabled ? 5 : 3));
		if (_exit == 2) return;
		Check(battle.RemoveStatus(3, CombatStatus::FREEZE));
		Check(battle.Redeploy(2, true, WorldPoint{6, 12}));
		Check(battle.ApplyStatus(3, CombatStatus::FREEZE, 1));
		Check(Close(battle.BondLayers("p", "kjeragShip"), kept ? 5 : 3));
		Check(battle.RemoveStatus(3, CombatStatus::FREEZE));
		Check(battle.ApplyStatus(3, CombatStatus::FREEZE, 1));
		Check(Close(battle.BondLayers("p", "kjeragShip"), kept ? 5 : 3)); // 保留收益上限，撤销的授予不因重部署恢复。
	}

	void GrantedAttributesAfterExit(bool _retain, bool _native)
	{
		constexpr std::array<std::string_view, 1> bonds{"kjeragShip"};
		const std::array<BattleGarrisonRule, 5> rules{{
			{.MyId = "attack", .MyKind = BattleGarrisonKind::ATTRIBUTES_BY_BOND, .MyBonds = bonds, .MyAttack = 0.1},
			{.MyId = "grant_attack", .MyKind = BattleGarrisonKind::GRANT, .MyGrantedGarrison = "attack"},
			{.MyId = "grant_respawn", .MyKind = BattleGarrisonKind::GRANT, .MyGrantedGarrison = "respawn"},
			{.MyId = "native", .MyKind = BattleGarrisonKind::COMMON_ATTRIBUTES, .MyAttackSpeed = 7},
			{.MyId = "respawn", .MyKind = BattleGarrisonKind::REDEPLOY_BY_BOND, .MyBonds = bonds, .MyRedeploy = -0.1}}};
		auto input = Input(RulePositionMode::CURRENT, {18, 9}); input.MyGarrisonRules = {rules, bonds};
		input.MyRetainGrantedGarrisonsAfterExit = _retain;
		auto& player = input.MyPlayers[0]; player.MyBonds = {{.MyId = "kjeragShip", .MyLayers = 3, .MyActive = true}};
		player.MyUnits[0].MyDefinition.MyIdentity.MyGarrisons = {"grant_attack", "grant_respawn"};
		player.MyUnits.push_back(Ally({6, 9}));
		player.MyUnits[1].MyDefinition.MyIdentity.MyGarrisons = {"native"};
		if (_native) player.MyUnits[1].MyDefinition.MyIdentity.MyGarrisons.insert(
			player.MyUnits[1].MyDefinition.MyIdentity.MyGarrisons.end(), {"attack", "respawn"});
		auto battle = Start(std::move(input));
		Check(Close(battle.Unit(2).MyStats.MyAttack, 130));
		const auto speed = battle.Unit(2).MyStats.MyAttackSpeed;
		battle.Retreat(1, true); battle.Step();
		Check(Close(battle.Unit(2).MyStats.MyAttack, 130));
		battle.Retreat(2);
		const bool kept = _retain || _native;
		Check(Close(battle.Unit(2).MyStats.MyAttack, kept ? 130 : 100));
		Check(Close(battle.Unit(2).MyRespawnAt - battle.Unit(2).MyRemovedAt, kept ? 70 : 100));
		Check(Close(battle.Unit(2).MyStats.MyAttackSpeed, speed));
		Check(Close(battle.AddBondLayers("p", "kjeragShip", 1), 1)); battle.Step();
		Check(Close(battle.Unit(2).MyStats.MyAttack, kept ? 140 : 100));
		Check(battle.Redeploy(2, true));
		Check(Close(battle.Unit(2).MyStats.MyAttack, kept ? 140 : 100));
		battle.Retreat(2); Check(battle.Redeploy(2, true));
		Check(Close(battle.Unit(2).MyStats.MyAttackSpeed, speed));
		Check(Close(battle.Unit(2).MyStats.MyAttack, kept ? 140 : 100));
	}

	void GrantedEventTraitsAfterExit(bool _retain)
	{
		constexpr std::array<std::string_view, 1> bonds{"kjeragShip"};
		for (const auto kind : {BattleGarrisonKind::DEPLOY_GAIN, BattleGarrisonKind::SKILL_GAIN, BattleGarrisonKind::DEATH_GAIN})
		{
			const std::array<BattleGarrisonRule, 2> rules{{
				{.MyId = "effect", .MyKind = kind, .MyBonds = bonds, .MyAmount = 2},
				{.MyId = "grant", .MyKind = BattleGarrisonKind::GRANT, .MyGrantedGarrison = "effect"}}};
			auto input = Input(RulePositionMode::CURRENT, {18, 9}); input.MyGarrisonRules = {rules, bonds};
			input.MyRetainGrantedGarrisonsAfterExit = _retain;
			auto& player = input.MyPlayers[0]; player.MyBonds = {{.MyId = "kjeragShip", .MyActive = true}};
			player.MyUnits[0].MyDefinition.MyIdentity.MyGarrisons = {"grant"};
			player.MyUnits.push_back(Ally({6, 9}));
			auto battle = Start(std::move(input));
			battle.Retreat(1, true);
			Check(battle.ActivateSkill(2)); (void)battle.LoseHealth(0, 2, 10000);
			Check(Close(battle.BondLayers("p", "kjeragShip"), 2)); // 本次死亡特质先结算，再撤销授予。
			Check(battle.Redeploy(2, true)); Check(battle.ActivateSkill(2));
			(void)battle.LoseHealth(0, 2, 10000);
			Check(Close(battle.BondLayers("p", "kjeragShip"), _retain ? 4 : 2));
		}
	}
}

int main()
{
	try
	{
		Check(BattleInput{}.MyRulePositionMode == RulePositionMode::CURRENT);
		Check(!BattleInput{}.MyGarrisonEffectsAfterExit);
		Check(BattleInput{}.MyRetainGrantedGarrisonsAfterExit);
		for (const auto mode : {RulePositionMode::CURRENT, RulePositionMode::INITIAL})
		{
			SkillTriggers(mode); HealingTriggersAndTargets(mode); HealingSecondaryEffects(mode, false); HealingSecondaryEffects(mode, true);
			SourceRangeAndAbsoluteRange(mode); SkillRangeAndRedeploy(mode); NormalAttackAndProjectile(mode); MapEdgeAndRangeChanges(mode);
			MovingSummonRange(mode);
			GenericSkillRanges(mode); GenericSkillHealing(mode); GenericSkillCounter(mode);
			OperatorRevealPositions(mode);
			OperatorSkillPositions(mode); VendlaPositions(mode);
			BondAura(mode); GarrisonPositions(mode); GarrisonStatusRange(mode);
			for (const bool enabled : {false, true})
			{
				GarrisonAfterExit(mode, enabled); AfterExitStatusesAndAmmo(mode, enabled);
				for (const bool retain : {false, true}) for (unsigned exit = 0; exit < 3; ++exit) for (const bool native : {false, true})
					GrantedTraitSurvivesGrantor(mode, enabled, retain, exit, native);
			}
		}
		for (const bool enabled : {false, true}) AfterExitChainingAndDeferred(enabled);
		OperatorRevealReentrancy();
		VendlaHealingCache(); SunbrLandedProc();
		for (const bool retain : {false, true})
		{
			for (const bool native : {false, true}) GrantedAttributesAfterExit(retain, native);
			GrantedEventTraitsAfterExit(retain);
		}
		auto invalid = Input(static_cast<RulePositionMode>(-1)); bool rejected = false;
		try { Battle battle(std::move(invalid)); } catch (const std::invalid_argument&) { rejected = true; }
		Check(rejected); std::cout << "initial/current position rules passed\n"; return 0;
	}
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
