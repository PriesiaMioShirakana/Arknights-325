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

	template <class _Callable>
	void Throws(_Callable _callable)
	{
		bool threw = false;
		try { _callable(); } catch (const std::exception&) { threw = true; }
		Check(threw);
	}

	bool Close(double _a, double _b) { return std::islessequal(std::abs(_a - _b), 1e-9); }

	// 五类 CRTP 派生各自持有状态；测试通过战斗结果观察调用，不依赖共享可变计数器。
	struct OperatorHandler final : CustomOperator<OperatorHandler>
	{
		void OnDeploy(Battle& _battle, ContentEvent& _event)
		{
			(void)_battle.AddBuff(_event.MyHandlerUnit, BuffDefinition{.MyKey = "custom-attack",
				.MyModifiers = std::vector<AttributeChange>{AttributeChange{.MyAttribute = Attribute::ATTACK_FLAT, .MyValue = 10}}});
		}

		void OnBeforeDamage(Battle&, ContentEvent& _event)
		{
			if (_event.MyTarget == _event.MyHandlerUnit) _event.MyDamage.MyAmount *= 0.5;
		}
	};

	struct EnemyHandler final : CustomEnemy<EnemyHandler>
	{
		void OnDeploy(Battle& _battle, ContentEvent& _event)
		{ _battle.AddShield(_event.MyHandlerUnit, Shield{.MyHealth = 5}); }
	};

	struct BondHandler final : CustomBond<BondHandler>
	{
		void OnBattleStart(Battle& _battle, ContentEvent& _event)
		{
			for (const auto& unit : _battle.Units())
				if (unit.MySide == UnitSide::ALLY && unit.MyOwner == _event.MyHandlerOwner)
					(void)_battle.AddBuff(unit.MyId, BuffDefinition{.MyKey = "custom-defense",
						.MyModifiers = std::vector<AttributeChange>{AttributeChange{.MyAttribute = Attribute::DEFENSE_FLAT, .MyValue = 20}}});
		}
	};

	struct BondEffectHandler final : CustomBondEffect<BondEffectHandler>
	{
		void OnTick(Battle& _battle, ContentEvent&)
		{ (void)_battle.DealDamage(0, 2, 1, DamageType::TRUE_DAMAGE); }
	};

	struct BuffHandler final : CustomBuff<BuffHandler>
	{
		unsigned MyTicks{};

		void OnTick(Battle& _battle, ContentEvent& _event)
		{
			(void)_battle.RemoveBuff(_event.MyHandlerUnit, _event.MyBuff);
			// 自删除后的这一行必须仍可安全访问派生对象，析构延迟到最外层回调返回。
			++MyTicks;
			(void)_battle.Heal(_event.MyHandlerUnit, _event.MyHandlerUnit, MyTicks);
		}

		void OnRemove(Battle& _battle, ContentEvent& _event)
		{ (void)_battle.Heal(_event.MyHandlerUnit, _event.MyHandlerUnit, 2); }
	};

	struct ThrowingHandler final : CustomBond<ThrowingHandler>
	{
		void OnTick(Battle&, ContentEvent&) { throw std::runtime_error("expected custom failure"); }
	};

	struct ReentrantHandler final : CustomBondEffect<ReentrantHandler>
	{
		void OnTick(Battle& _battle, ContentEvent&) { _battle.Step(); }
	};

	BattleInput Input(const ContentRegistry& _registry, ContentReference _operator = {}, ContentReference _enemy = {})
	{
		CombatDefinition ally{.MyId = "ally", .MyStats = {.MyMaxHealth = 1000, .MyAttack = 100},
			.MyAttack = {.MyDisabled = true}, .MyContent = _operator};
		CombatDefinition enemy{.MyId = "enemy", .MyStats = {.MyMaxHealth = 1000},
			.MyAttack = {.MyDisabled = true}, .MyContent = _enemy};
		BattleInput input{.MyTimeLimit = 60, .MyAutoFinish = false, .MyContentRegistry = std::cref(_registry)};
		input.MyPlayers.emplace_back("one", std::vector<AllyDeployment>{AllyDeployment{
			.MyPieceUid = 1, .MyDefinition = std::move(ally), .MyPosition = {.MyX = 5, .MyY = 9}}});
		input.MySpawns.emplace_back(0, "one", std::move(enemy), CombatRoute{
			.MyStart = {.MyX = 10, .MyY = 9}, .MyEnd = {.MyX = 0, .MyY = 9}});
		return input;
	}

	void RegistrationAndDispatch()
	{
		ContentRegistry registry(7);
		const auto op = registry.Register<OperatorHandler>("operator");
		const auto enemy = registry.Register<EnemyHandler>("enemy");
		const auto buff = registry.Register<BuffHandler>("buff");
		const auto bond = registry.Register<BondHandler>("bond");
		const auto effect = registry.Register<BondEffectHandler>("effect");
		Throws([&] { (void)registry.Register<OperatorHandler>("operator"); });
		Throws([&] { Battle invalid(Input(registry, op)); });
		registry.Seal();
		Throws([&] { (void)registry.Register<OperatorHandler>("another"); });
		Throws([&] { Battle invalid(Input(registry, enemy)); });
		Check(registry.Find(ContentTag::CUSTOM_BUFF, "buff").MyRegistration == buff.MyRegistration);
		auto input = Input(registry, op, enemy);
		input.MyContentBindings.emplace_back(bond, "one");
		input.MyContentBindings.emplace_back(effect, "one");
		Battle battle(std::move(input));
		battle.Step();
		Check(Close(battle.Unit(1).MyStats.MyAttack, 110) && Close(battle.Unit(1).MyStats.MyDefense, 20));
		Check(Close(battle.Unit(2).MyShields.front().MyHealth, 4));
		Check(Close(battle.DealDamage(2, 1, 100, DamageType::TRUE_DAMAGE), 50));
		(void)battle.AddBuff(1, BuffDefinition{.MyKey = "self-removing", .MyDuration = 1, .MyContent = buff});
		battle.Step();
		Check(Close(battle.Unit(1).MyHealth, 953));
		Check(battle.RemoveBuff(1, "self-removing") == 0);
		Check(battle.ContentErrors().empty());
		Battle builtin(Input(registry));
		builtin.Step();
		Check(Close(builtin.Unit(1).MyStats.MyAttack, 100) && builtin.Unit(2).MyShields.empty());
	}

	void ErrorIsolation()
	{
		ContentRegistry registry;
		const auto throwing = registry.Register<ThrowingHandler>("throwing");
		const auto reentrant = registry.Register<ReentrantHandler>("reentrant");
		registry.Seal();
		auto input = Input(registry);
		input.MyContentBindings.emplace_back(throwing, "one");
		Battle battle(std::move(input));
		battle.Advance(100);
		Check(battle.Finished() && battle.ContentErrors().size() == 64);
		input = Input(registry);
		input.MyContentBindings.emplace_back(reentrant, "one");
		Battle recursive(std::move(input));
		recursive.Step();
		Check(recursive.Tick() == 1 && recursive.ContentErrors().size() == 1);
	}

	struct FatalHandler final : CustomOperator<FatalHandler>
	{
		unsigned MyFatalCount{};
		unsigned MyKillCount{};

		void OnBeforeDamage(Battle&, ContentEvent& _event)
		{
			if (_event.MyDamage.MySourceless) Check(_event.MySource == 0 && _event.MyCredit == 2);
		}

		void OnFatal(Battle&, ContentEvent& _event)
		{ _event.MyPrevented = ++MyFatalCount == 1; }

		void OnBeforeKill(Battle& _battle, ContentEvent& _event)
		{
			if (++MyKillCount == 1) (void)_battle.Heal(_event.MyHandlerUnit, _event.MyHandlerUnit, 10);
		}
	};

	void DamageLifecycle()
	{
		ContentRegistry registry;
		const auto handler = registry.Register<FatalHandler>("fatal");
		registry.Seal();
		auto input = Input(registry, handler);
		auto& skill = input.MyPlayers[0].MyUnits[0].MyDefinition.MySkill;
		skill = SkillDefinition{.MyKind = SkillKind::INSTANT, .MySpType = SpType::HURT, .MyTrigger = SkillTrigger::NEVER, .MySpCost = 10};
		Battle battle(std::move(input));
		battle.Step();
		(void)battle.AddBuff(1, BuffDefinition{.MyKey = "one-hit", .MyShield = Shield{.MyHits = 1}});
		Check(Close(battle.DealDamage(2, 1, 100, DamageType::TRUE_DAMAGE), 0));
		Check(Close(battle.SpTotal(1), 1) && battle.Unit(1).MyBuffs.empty());
		(void)battle.DealDamage(2, 1, DamageInfo{.MyAmount = 2000, .MyType = DamageType::TRUE_DAMAGE, .MySourceless = true});
		Check(battle.Unit(1).MyAlive && Close(battle.Unit(1).MyHealth, 1));
		(void)battle.DealDamage(2, 1, 2000, DamageType::TRUE_DAMAGE);
		Check(battle.Unit(1).MyAlive && Close(battle.Unit(1).MyHealth, 10));
		(void)battle.DealDamage(2, 1, 2000, DamageType::TRUE_DAMAGE);
		Check(!battle.Unit(1).MyAlive && battle.ContentErrors().empty());
	}

	// 同步状态回调内取消当前风、注册零延迟新风。新动作不能混入本帧到期快照。
	struct WindHandler final : CustomBond<WindHandler>
	{
		bool MyStarted{};
		void OnBeforeStatus(Battle& _battle, ContentEvent& _event)
		{
			if (MyStarted || _event.MyStatus != CombatStatus::COLD) return;
			MyStarted = true;
			Check(_battle.CancelColdWind(1));
			Check(_battle.StartColdWind(ColdWindDefinition{.MyInterval = 50, .MyBaseDuration = 2, .MyFirstDelay = 0}) == 2);
		}
	};

	void WindScheduling()
	{
		ContentRegistry registry;
		const auto handler = registry.Register<WindHandler>("wind");
		registry.Seal();
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one"}},
			.MySpawns = {EnemySpawn{.MyOwnerId = "one", .MyDefinition = CombatDefinition{.MyId = "enemy",
				.MyStats = CombatStats{.MyMaxHealth = 1000}, .MyAttack = AttackProfile{.MyDisabled = true}},
				.MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = 10, .MyY = 9}, .MyEnd = WorldPoint{.MyY = 9}}}},
			.MyAutoFinish = false, .MyContentRegistry = std::cref(registry),
			.MyContentBindings = {ContentBinding{.MyContent = handler, .MyPlayerId = "one"}}};
		Battle battle(std::move(input));
		Throws([&] { (void)battle.StartColdWind(ColdWindDefinition{}); });
		Throws([&] { (void)battle.StartColdWind(ColdWindDefinition{.MyPlayerId = "unknown", .MyInterval = 1}); });
		Check(battle.StartColdWind(ColdWindDefinition{.MyInterval = BattleClock::StepSeconds, .MyBaseDuration = 2, .MyFirstDelay = 0}) == 1);
		battle.Step(); // 第一次吹风发生在出生队列之前，场上无人，仍计一次。
		Check(battle.ColdWindGusts(1) == 1 && !battle.Unit(1).MyStatuses.Has(CombatStatus::COLD));
		battle.Step();
		Check(battle.ColdWindGusts(1) == 2 && battle.ColdWindGusts(2) == 0);
		Check(battle.Unit(1).MyStatuses.Has(CombatStatus::COLD) && !battle.Unit(1).MyStatuses.Has(CombatStatus::FREEZE));
		battle.Step();
		Check(battle.ColdWindGusts(1) == 2 && battle.ColdWindGusts(2) == 1);
		Check(battle.Unit(1).MyStatuses.Has(CombatStatus::FREEZE) && battle.ContentErrors().empty());
		Check(!battle.CancelColdWind(1) && !battle.CancelColdWind(0));
		battle.ForceEnd();
		Check(battle.StartColdWind(ColdWindDefinition{.MyInterval = 1}) == 0);
	}

}

int main()
{
	try { RegistrationAndDispatch(); ErrorIsolation(); DamageLifecycle(); WindScheduling(); std::cout << "4 custom content test groups passed\n"; }
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
