#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	// 验证死亡后的最后一次漏怪通知，以及已结束战斗的结算记账。
	struct LeakHook final : CustomEnemy<LeakHook>
	{
		void OnEnemyLeak(Battle& _battle, ContentEvent& _event)
		{
			if (_event.MyHandlerUnit == _event.MyUnit) (void)_battle.AddCoins("one", 1);
		}
	};

	struct EndHook final : CustomBond<EndHook>
	{
		void OnBattleEnd(Battle& _battle, ContentEvent&) { (void)_battle.AddCoins("two", 2); }
	};

	void String(std::string_view _value) { std::cout << '"' << _value << '"'; }

	void Modifiers(const std::optional<EnemySpawnModifiers>& _mods)
	{
		if (!_mods) { std::cout << "null"; return; }
		std::cout << '[';
		for (const auto value : {_mods->MyHealth, _mods->MyAttack, _mods->MyDefense, _mods->MyResistance, _mods->MySpeed, _mods->MySupplyHealth})
		{
			if (value) std::cout << *value; else std::cout << "null";
			std::cout << ',';
		}
		String(_mods->MySlot); std::cout << ','; String(_mods->MyBountyId); std::cout << ']';
	}

	void Snapshot(const Battle& _battle)
	{
		const auto result = _battle.Result();
		std::cout << '[' << result.MyKilled << ',' << result.MyTotal << ',' << result.MyLeaked << ",[";
		for (std::size_t i = 0; i < result.MyPlayers.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& p = result.MyPlayers[i];
			std::cout << '[' << p.MyCoins << ',' << p.MyPerfect << ',' << p.MyKilled << ',' << p.MyTotal << ",[";
			for (std::size_t j = 0; j < p.MyLeaks.size(); ++j)
			{
				if (j) std::cout << ',';
				const auto& leak = p.MyLeaks[j];
				std::cout << '['; String(leak.MyEnemyId); std::cout << ','; Modifiers(leak.MyModifiers);
				std::cout << ',' << leak.MyLifeCost << ','; String(leak.MySourcePlayer);
				std::cout << ',' << static_cast<unsigned>(leak.MyTag) << ',' << leak.MyCounted << ',' << leak.MyBoss << ']';
			}
			std::cout << "]]";
		}
		std::cout << "],[";
		for (std::size_t i = 0; i < result.MyPendingEnemies.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& p = result.MyPendingEnemies[i];
			std::cout << '['; String(p.MyEnemyId); std::cout << ',' << p.MyTime << ',' << static_cast<unsigned>(p.MyTag) << ',';
			String(p.MySourcePlayer); std::cout << ']';
		}
		std::cout << "]]";
	}
}

int main()
{
	ContentRegistry registry;
	const auto leakHook = registry.Register<LeakHook>("leak");
	const auto endHook = registry.Register<EndHook>("end");
	registry.Seal();
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 16; ++scene)
	{
		const CombatDefinition ally{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 100, .MyBlockCount = 0}, .MyAttack = AttackProfile{.MyDisabled = true}};
		BattleInput input{.MyPlayers = {
			BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {AllyDeployment{.MyPieceUid = 1, .MyDefinition = ally, .MyPosition = WorldPoint{.MyX = 5, .MyY = 3}}}},
			BattlePlayerInput{.MyPlayerId = "two", .MyUnits = {AllyDeployment{.MyPieceUid = 2, .MyDefinition = ally, .MyPosition = WorldPoint{.MyX = 15, .MyY = 3}}}, .MyMirrorDeployment = true}},
			.MyTimeLimit = 0.5, .MyAutoFinish = false, .MyContentRegistry = std::cref(registry)};
		if (scene == 12) input.MyContentBindings.emplace_back(ContentBinding{.MyContent = endHook, .MyPlayerId = "two"});
		EnemySpawn spawn{.MyOwnerId = "one", .MyDefinition = CombatDefinition{.MyId = "enemy_test",
			.MyStats = CombatStats{.MyMaxHealth = 200, .MyMoveSpeed = scene >= 8 && scene <= 11 && scene != 9 ? 60.0 : 0.0},
			.MyAttack = AttackProfile{.MyDisabled = true}, .MyContent = leakHook},
			.MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = 15, .MyY = 9}, .MyEnd = WorldPoint{.MyX = 16, .MyY = 9}},
			.MyLifeCost = 3, .MyCounted = scene != 10 && scene != 11, .MyTag = scene == 10 ? EnemySpawnTag::BOSS : EnemySpawnTag::BOUNTY,
			.MyModifiers = EnemySpawnModifiers{.MyHealth = 2, .MyAttack = 1.2, .MyDefense = 1.1, .MyResistance = 0.8, .MySpeed = 1,
				.MySupplyHealth = 1.6, .MySlot = "S", .MyBountyId = "card"},
			.MySourcePlayer = scene == 8 || scene == 9 ? "leaker" : "",
			.MyBounty = BountyReward{.MyCoins = 17, .MyOwnerId = scene == 4 ? "one" : scene == 6 ? "" : "away"}};
		input.MySpawns.emplace_back(spawn);
		if (scene == 9)
		{
			spawn.MyTime = 4; input.MySpawns.emplace_back(spawn);
			spawn.MyTime = 5; spawn.MyCounted = false; spawn.MyTag = EnemySpawnTag::PART; input.MySpawns.emplace_back(std::move(spawn));
		}
		Battle battle(std::move(input)); battle.Start(); battle.Step();
		const UnitId target = 3;
		UnitId source = scene == 0 ? 1 : scene == 1 ? 2 : 0;
		if (scene == 2) source = battle.SpawnToken(TokenSpawn{.MyDefinition = ally, .MyPosition = WorldPoint{.MyX = 16, .MyY = 3}, .MyOwnerUnit = 2});
		if (scene == 5)
		{
			auto enemy = ally; enemy.MyId = "enemy_ally";
			source = battle.SpawnEnemy(EnemySpawn{.MyOwnerId = "one", .MyDefinition = std::move(enemy),
			.MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = 4, .MyY = 9}, .MyEnd = WorldPoint{.MyX = 3, .MyY = 9}}, .MyCounted = false});
		}
		if (scene < 7 || scene >= 12) (void)battle.LoseHealth(source, target, 100000);
		if (scene == 14) (void)battle.LoseHealth(source, target, 100000); // 重复致死调用不得重复支付。
		if (scene == 13)
		{
			(void)battle.AddCoins("one", 2.5); (void)battle.AddCoins("one", -1);
			(void)battle.AddCoins("one", std::numeric_limits<double>::quiet_NaN()); (void)battle.AddCoins("missing", 5);
		}
		if (scene) std::cout << ',';
		std::cout << '['; Snapshot(battle);
		battle.Advance(30); std::cout << ','; Snapshot(battle); std::cout << ']';
		if (!battle.ContentErrors().empty()) return 1;
		std::array<double, 2> paid{};
		for (const auto& event : battle.DrainEvents())
			if (event.MyKind == BattleEventKind::COINS_GAINED) paid.at(event.MyPlayer) += event.MyAmount;
		for (std::size_t i = 0; i < paid.size(); ++i)
			if (std::islessgreater(paid[i], battle.Players()[i].MyCoins)) return 2;
	}
	std::cout << ']';
}
