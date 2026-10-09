#include <charconv>
#include <iomanip>
#include <iostream>
#include <stronghold/core/random.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	struct StatusCommand
	{
		std::uint64_t MyTick{};
		UnitId MyTarget{};
		CombatStatus MyStatus{};
		double MySeconds{};
		bool MyRemove{};
	};

	constexpr std::array<StatusCommand, 19> StatusCommands{{
		{1, 2, CombatStatus::STUN, 0.5},
		{20, 4, CombatStatus::STUN, 0.8},
		{30, 1, CombatStatus::DISARM, 1},
		{35, 1, CombatStatus::DISARM, 0.1},
		{45, 1, CombatStatus::DISARM, 2},
		{60, 5, CombatStatus::UNBLOCKABLE, 2},
		{75, 6, CombatStatus::NO_MOVE, 0.5},
		{85, 6, CombatStatus::STUN, 0.6},
		{100, 1, CombatStatus::DISARM, 0, true},
		{110, 7, CombatStatus::STUN, 0.5},
		{130, 2, CombatStatus::STUN, 0.3},
		{140, 2, CombatStatus::STUN, 2},
		{150, 2, CombatStatus::STUN, 0, true},
		{200, 9, CombatStatus::NO_MOVE, 2},
		{225, 9, CombatStatus::NO_MOVE, 0, true},
		{240, 10, CombatStatus::DISARM, 1},
		{245, 10, CombatStatus::UNBLOCKABLE, 1},
		{260, 3, CombatStatus::DISARM, 1},
		{270, 1, CombatStatus::STUN, 0.05}
	}};

	void String(std::string_view _value)
	{
		std::cout << '"';
		for (const auto c : _value)
		{
			if (c == '"' || c == '\\') std::cout << '\\';
			std::cout << c;
		}
		std::cout << '"';
	}

	CombatDefinition Definition(std::string _id, double _health, double _attack)
	{
		CombatDefinition definition;
		definition.MyId = std::move(_id);
		definition.MyStats.MyMaxHealth = _health;
		definition.MyStats.MyAttack = _attack;
		return definition;
	}

	BattleInput Scenario(std::uint32_t _seed)
	{
		Random random(_seed);
		BattleInput input;
		input.MyTimeLimit = 12;
		input.MyInitialDp = 2;
		input.MyDpPerSecond = 1.5;
		auto shooter = Definition("shooter", 350, 80);
		shooter.MyStats.MyBaseAttackTime = 1.1;
		shooter.MyAttack.MyCanHitFlying = true;
		shooter.MyAttack.MyRanged = true;
		shooter.MyAttack.MyProjectileSpeed = 14;
		shooter.MyAttack.MyPriority = static_cast<TargetPriority>(_seed % 7);
		shooter.MyAttack.MyMaxTargets = 1 + _seed % 2;
		shooter.MyRange.clear();
		for (int row = -2; row <= 2; ++row)
			for (int column = 0; column <= 5; ++column) shooter.MyRange.emplace_back(row, column);
		auto guard = Definition("guard", 380, 95);
		guard.MyStats.MyDefense = 40;
		guard.MyStats.MyBlockCount = 2;
		guard.MyStats.MyBaseAttackTime = 1.2;
		guard.MyStats.MyRedeploySeconds = 1;
		guard.MyStats.MyDeploymentCost = 4;
		auto medic = Definition("medic", 300, 45);
		medic.MyStats.MyBaseAttackTime = 1.8;
		medic.MyAttack.MyHealing = true;
		medic.MyRange = shooter.MyRange;
		input.MyPlayers.emplace_back(
			"one",
			std::vector<AllyDeployment>{
				{11, shooter, {5, 11}}, {12, guard, {6, 10}, Facing::LEFT}, {13, medic, {4, 11}}
			}
		);
		for (int i = 0; i < 12; ++i)
		{
			// Keep the established stream order explicit; argument evaluation order varies between toolchains.
			const auto attack = 90 + random.Index(81);
			const auto health = 250 + random.Index(401);
			auto enemy = Definition("enemy" + std::to_string(i), health, attack);
			enemy.MyStats.MyDefense = random.Index(41);
			enemy.MyStats.MyResistance = random.Index(31);
			enemy.MyStats.MyBaseAttackTime = 1 + random.Index(4) * 0.25;
			enemy.MyStats.MyAttackSpeed = i % 3 == 0 ? 180 : 100;
			enemy.MyStats.MyMoveSpeed = 1.5 + random.Index(4) * 0.5;
			enemy.MyStats.MyTaunt = i % 4 == 0 ? 1 : 0;
			enemy.MyFlying = i % 5 == 0;
			enemy.MyBlockWeight = i % 4 == 0 ? 2 : 1;
			enemy.MyAttack.MyDamageType = i % 3 == 0 ? DamageType::ARTS : DamageType::PHYSICAL;
			enemy.MyAttack.MyRanged = i % 3 == 0;
			enemy.MyAttack.MyEnemyRange = 2.5;
			enemy.MyAttack.MyAnimationDuration = 0.8;
			enemy.MyAttack.MyAnimationHit = 0.3;
			enemy.MyAttack.MyAttackWhileMoving = i % 6 == 0;
			const auto row = i % 2 == 0 ? 10.0 : 9.0;
			CombatRoute route{{12, row}, {}, {2, row}};
			if (i % 4 == 1) route.MySteps = {{RouteStepKind::MOVE, {10, row}}, {RouteStepKind::WAIT, {}, 0.2}};
			if (i % 4 == 2)
				route.MySteps = {
					{RouteStepKind::MOVE, {10, row}},
					{RouteStepKind::DISAPPEAR, {}},
					{RouteStepKind::WAIT, {}, 0.3},
					{RouteStepKind::APPEAR, {8, row}}
				};
			input.MySpawns.emplace_back(
				i == 11 ? 30.0 : i * 0.35, "one", std::move(enemy), std::move(route), 1 + i % 2
			);
		}
		return input;
	}

	void Point(WorldPoint _point) { std::cout << '[' << _point.MyX << ',' << _point.MyY << ']'; }

	void DefinitionJson(const CombatDefinition& _definition)
	{
		const auto& stats = _definition.MyStats;
		const auto& attack = _definition.MyAttack;
		std::cout << "{\"id\":";
		String(_definition.MyId);
		std::cout << ",\"stats\":{\"maxHp\":" << stats.MyMaxHealth << ",\"atk\":" << stats.MyAttack
				  << ",\"def\":" << stats.MyDefense << ",\"res\":" << stats.MyResistance
				  << ",\"aspd\":" << stats.MyAttackSpeed << ",\"bat\":" << stats.MyBaseAttackTime
				  << ",\"moveSpeed\":" << stats.MyMoveSpeed << ",\"blockCnt\":" << stats.MyBlockCount
				  << ",\"tauntLevel\":" << stats.MyTaunt << ",\"respawnTime\":" << stats.MyRedeploySeconds
				  << ",\"cost\":" << stats.MyDeploymentCost << "},\"attack\":[" << static_cast<int>(attack.MyDamageType)
				  << ',' << attack.MyDisabled << ',' << attack.MyHealing << ',' << attack.MyCanHitFlying << ','
				  << attack.MyBlockFlying << ',' << attack.MyRanged << ',' << attack.MyAttackWhileMoving << ','
				  << attack.MyMaxTargets << ',' << static_cast<int>(attack.MyPriority) << ','
				  << attack.MyProjectileSpeed << ',' << attack.MyEnemyRange << ',' << attack.MyAnimationDuration << ',';
		if (attack.MyAnimationHit) std::cout << *attack.MyAnimationHit;
		else std::cout << "null";
		std::cout << "],\"flying\":" << _definition.MyFlying << ",\"weight\":" << _definition.MyBlockWeight
				  << ",\"range\":[";
		for (std::size_t i = 0; i < _definition.MyRange.size(); ++i)
		{
			if (i) std::cout << ',';
			std::cout << '[' << _definition.MyRange[i].MyRow << ',' << _definition.MyRange[i].MyColumn << ']';
		}
		std::cout << "]}";
	}

	void InputJson(const BattleInput& _input, std::span<const StatusCommand> _commands)
	{
		std::cout << "{\"limit\":" << _input.MyTimeLimit << ",\"dp\":[" << _input.MyInitialDp << ','
				  << _input.MyDpPerSecond << ',' << _input.MyMaxDp << "],\"units\":[";
		const auto& player = _input.MyPlayers.front();
		for (std::size_t i = 0; i < player.MyUnits.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& unit = player.MyUnits[i];
			std::cout << "{\"uid\":" << unit.MyPieceUid << ",\"position\":";
			Point(unit.MyPosition);
			std::cout << ",\"dir\":" << static_cast<int>(unit.MyFacing) << ",\"definition\":";
			DefinitionJson(unit.MyDefinition);
			std::cout << '}';
		}
		std::cout << "],\"spawns\":[";
		for (std::size_t i = 0; i < _input.MySpawns.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& spawn = _input.MySpawns[i];
			std::cout << "{\"time\":" << spawn.MyTime << ",\"life\":" << spawn.MyLifeCost << ",\"definition\":";
			DefinitionJson(spawn.MyDefinition);
			std::cout << ",\"start\":";
			Point(spawn.MyRoute.MyStart);
			std::cout << ",\"end\":";
			Point(spawn.MyRoute.MyEnd);
			std::cout << ",\"steps\":[";
			for (std::size_t j = 0; j < spawn.MyRoute.MySteps.size(); ++j)
			{
				if (j) std::cout << ',';
				const auto& step = spawn.MyRoute.MySteps[j];
				std::cout << '[' << static_cast<int>(step.MyKind) << ',' << step.MyPosition.MyX << ','
						  << step.MyPosition.MyY << ',' << step.MyWaitSeconds << ']';
			}
			std::cout << "]}";
		}
		std::cout << "],\"effects\":[";
		for (std::size_t i = 0; i < _commands.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& command = _commands[i];
			std::cout << '[' << command.MyTick << ',' << command.MyTarget << ',' << static_cast<int>(command.MyStatus)
				<< ',' << command.MySeconds << ',' << command.MyRemove << ']';
		}
		std::cout << "]}";
	}

	void Snapshot(const Battle& _battle)
	{
		const auto result = _battle.Result();
		std::cout << "{\"tick\":" << _battle.Tick() << ",\"reason\":" << static_cast<int>(result.MyReason)
				  << ",\"counts\":[" << result.MyKilled << ',' << result.MyLeaked << ',' << result.MyTotal << ','
				  << result.MyUnspawned << "],\"players\":[";
		for (std::size_t i = 0; i < _battle.Players().size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& player = _battle.Players()[i];
			std::cout << '[' << player.MyDp << ',' << player.MyTotal << ',' << player.MyKilled << ',' << player.MyLeaked
					  << ',' << player.MyDeaths << ',' << player.MyLifeLost << ',' << player.MyDamage << ','
					  << player.MyHealing << ']';
		}
		std::cout << "],\"units\":[";
		bool first = true;
		for (const auto& unit : _battle.Units())
		{
			if (!first) std::cout << ',';
			first = false;
			const auto& totals = unit.MyTotals;
			std::cout << '[' << unit.MyId << ',' << unit.MyPosition.MyX << ',' << unit.MyPosition.MyY << ','
					  << unit.MyHealth << ',' << unit.MyAlive << ',' << unit.MyHidden << ',' << unit.MyBlockedBy << ','
					  << unit.MyAttackCooldown << ',' << unit.MyDeploySequence << ',' << totals.MyDamage << ','
					  << totals.MyHealing << ',' << totals.MyTaken << ',' << totals.MyAttacks << ',' << totals.MyKills;
			for (std::size_t i = 0; i < 4; ++i) std::cout << ',' << unit.MyStatuses.MyRemaining[i];
			std::cout << ']';
		}
		std::cout << "]}";
	}
}

int main(int _argc, char** _argv)
{
	try
	{
		std::uint32_t seed = 42;
		if (_argc > 1)
		{
			const std::string_view text = _argv[1];
			const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), seed);
			if (error != std::errc{} || end != text.data() + text.size()) throw std::invalid_argument("invalid seed");
		}
		const auto input = Scenario(seed);
		std::span<const StatusCommand> commands;
		if (_argc > 2 && std::string_view(_argv[2]) == "statuses") commands = StatusCommands;
		Battle battle(input);
		std::cout << std::boolalpha << std::setprecision(17) << "{\"input\":";
		InputJson(input, commands);
		std::cout << ",\"frames\":[";
		bool first = true;
		while (!battle.Finished())
		{
			for (const auto& command : commands)
			{
				if (command.MyTick != battle.Tick()) continue;
				if (command.MyRemove) battle.RemoveStatus(command.MyTarget, command.MyStatus);
				else battle.ApplyStatus(command.MyTarget, command.MyStatus, command.MySeconds);
			}
			battle.Step();
			if (!first) std::cout << ',';
			first = false;
			Snapshot(battle);
		}
		std::cout << "],\"attacks\":[";
		first = true;
		for (const auto& event : battle.DrainEvents())
		{
			if (event.MyKind != BattleEventKind::ATTACKED) continue;
			if (!first) std::cout << ',';
			first = false;
			std::cout << '[' << event.MyTick << ',' << event.MySource << ',' << event.MyTarget << ']';
		}
		std::cout << "]}\n";
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
