#include <iomanip>
#include <iostream>
#include <source_location>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;
	void Check(bool _condition, const std::source_location _where = std::source_location::current())
	{ if (!_condition) throw std::runtime_error(std::to_string(_where.line())); }

	CombatDefinition Ally()
	{
		return CombatDefinition{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 500, .MyBlockCount = 1, .MyRedeploySeconds = 1, .MyDeploymentCost = 3}, .MyAttack = AttackProfile{.MyDisabled = true}};
	}

	CombatDefinition Token()
	{
		return CombatDefinition{.MyId = "token", .MyStats = CombatStats{.MyMaxHealth = 200, .MyAttack = 40, .MyBaseAttackTime = 0.5, .MyBlockCount = 1}};
	}

	EnemySpawn Enemy(bool _counted = true)
	{
		return EnemySpawn{.MyOwnerId = "one", .MyDefinition = CombatDefinition{.MyId = "enemy", .MyStats = CombatStats{
			.MyMaxHealth = 1000, .MyAttack = 30, .MyBaseAttackTime = 0.5, .MyMoveSpeed = 1.5}},
			.MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = 10, .MyY = 9}, .MyEnd = WorldPoint{.MyX = 3, .MyY = 9}}, .MyCounted = _counted};
	}

	BattleInput Input()
	{
		return BattleInput{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {AllyDeployment{
			.MyPieceUid = 1, .MyDefinition = Ally(), .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}}}}},
			.MySpawns = {Enemy()}, .MyAutoFinish = false, .MyField = FieldDefinition{}};
	}

	// 在伤害链内部创建大量敌军，同时访问源/目标引用，覆盖原 vector 重分配会悬空的路径。
	struct SummoningHandler final : CustomOperator<SummoningHandler>
	{
		bool MyDone{};
		void OnDamaged(Battle& _battle, ContentEvent& _event)
		{
			if (MyDone) return;
			MyDone = true;
			const auto& source = _battle.Unit(_event.MyHandlerUnit);
			const auto& target = _battle.Unit(_event.MyTarget);
			for (unsigned i = 0; i < 128; ++i) (void)_battle.SpawnEnemy(Enemy(false));
			Check(&source == &_battle.Unit(_event.MyHandlerUnit));
			Check(&target == &_battle.Unit(_event.MyTarget));
		}
	};

	void Core()
	{
		ContentRegistry registry;
		const auto custom = registry.Register<SummoningHandler>("summoning");
		registry.Seal();
		auto input = Input();
		input.MyContentRegistry = std::cref(registry);
		input.MyPlayers.front().MyUnits.front().MyDefinition.MyContent = custom;
		Battle battle(std::move(input));
		battle.Step();
		const auto& original = battle.Unit(1);
		Check(std::isgreater(battle.DealDamage(1, 2, 1, DamageType::TRUE_DAMAGE), 0));
		Check(battle.Units().size() == 130 && &original == &battle.Unit(1));
		Check(battle.Result().MyTotal == 1 && battle.ContentErrors().empty());
		battle.Retreat(1);
		Check(!battle.CanDeploy(WorldPoint{.MyX = 5, .MyY = 9}));
		Check(battle.SpawnToken(TokenSpawn{.MyDefinition = Token(), .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}, .MyOwnerUnit = 1}) == 0);
		Check(battle.Redeploy(1));
		const auto token = battle.SpawnToken(TokenSpawn{.MyDefinition = Token(), .MyPosition = WorldPoint{.MyX = 6, .MyY = 9}, .MyOwnerUnit = 1, .MyDuration = 0.1});
		Check(token && battle.Unit(token).MyKind == UnitKind::TOKEN);
		battle.Advance(5);
		Check(!battle.Unit(token).MyAlive && battle.Unit(token).MyRemoved);
		Check(!battle.Redeploy(token));
		Check(battle.Result().MyPlayers.front().MyDeaths == 0);
		Check(battle.ContentErrors().empty());
	}

	void Trace()
	{
		std::cout << std::setprecision(17) << '[';
		for (unsigned scene = 0; scene < 6; ++scene)
		{
			auto input = Input();
			if (scene == 2) input.MyDevices.emplace_back(DeviceSpawn{.MyId = "crate", .MyPosition = WorldPoint{.MyX = 9, .MyY = 9}, .MyHealth = 80, .MyObstacle = true});
			if (scene >= 4)
			{
				input.MyDevices.emplace_back(DeviceSpawn{.MyId = "trap_1105_accrate", .MyPosition = WorldPoint{.MyX = 9, .MyY = 9}, .MyHealth = 100, .MyObstacle = true});
				input.MySpawns.front().MyDefinition.MyAttack.MyDisabled = true;
				for (int row = 0; row < FieldRows; ++row)
					for (int col = 0; col < FieldColumns; ++col)
						if (row != 9) input.MyField->MyTiles[static_cast<std::size_t>(FieldGrid::Key(row, col))] = FieldTile{.MyWalkable = false, .MyLow = false, .MyBuild = FieldBuild::NONE};
			}
			Battle battle(std::move(input));
			UnitId token{}, device{};
			if (scene) std::cout << ',';
			std::cout << '[';
			for (unsigned tick = 0; tick < 240; ++tick)
			{
				if (tick == 10 && scene < 4) token = battle.SpawnToken(TokenSpawn{.MyDefinition = Token(), .MyPosition = WorldPoint{.MyX = 8, .MyY = 9}, .MyOwnerUnit = 1, .MyDuration = scene == 0 ? 2.0 : 0.0});
				if (tick == 15 && scene == 1) device = battle.SpawnDevice(DeviceSpawn{.MyId = "crate", .MyPosition = WorldPoint{.MyX = 9, .MyY = 9}, .MyHealth = 80, .MyObstacle = true});
				if (scene == 3)
				{
					if (tick == 15) Check(battle.Relocate(1, WorldPoint{.MyX = 6, .MyY = 9}));
					if (tick == 18) battle.SetDownAtHome(1, true);
					if (tick == 60) Check(battle.MoveRedeploy(1, WorldPoint{.MyX = 7, .MyY = 9}, true));
					if (tick == 100) Check(battle.Relocate(1, WorldPoint{.MyX = 8, .MyY = 9}));
					if (tick == 110) (void)battle.LoseHealth(0, 1, 10000);
				}
				if (tick == 20) (void)battle.LoseHealth(0, 1, 10000);
				if (tick == 25) Check(battle.SpawnToken(TokenSpawn{.MyDefinition = Token(), .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}, .MyOwnerUnit = 1}) == 0);
				if (tick == 35) { auto spawn = Enemy(false); if (scene >= 4) spawn.MyDefinition.MyAttack.MyDisabled = true; (void)battle.SpawnEnemy(std::move(spawn)); }
				if (tick == 90 && scene != 0 && scene < 4) battle.Retreat(token, true);
				if (tick == 25 && scene == 5) (void)battle.Displace(3, WorldPoint{.MyX = 1}, 2);
				if (tick == 100 && device) (void)battle.LoseHealth(0, device, 10000);
				battle.Step();
				if (tick) std::cout << ',';
				std::cout << '[';
				bool first = true;
				for (const auto& unit : battle.Units())
				{
					if (!first) std::cout << ',';
					first = false;
					std::cout << '[' << unit.MyHealth << ',' << unit.MyAlive << ',' << unit.MyRemoved << ',' << unit.MyPosition.MyX << ',' << unit.MyPosition.MyY << ',' << unit.MyBlockedBy << ',' << (unit.MyBody ? unit.MyBody->MyX : -1) << ',' << (unit.MyBody ? unit.MyBody->MyY : -1) << ',' << unit.MyDeploySequence << ']';
				}
				std::cout << ']';
			}
			std::cout << ']';
		}
		std::cout << ']';
	}
}

int main(int _argc, char**)
{
	try { if (_argc > 1) Trace(); else Core(); return 0; }
	catch (const std::exception& _error) { std::cerr << _error.what() << '\n'; return 1; }
}
