#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_bonds.hpp>
#include <stronghold/adapters/reference_map_characters.hpp>
#include <stronghold/adapters/reference_stage.hpp>
#include <stronghold/adapters/reference_summons.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	void Snapshot(const Battle& _battle)
	{
		std::cout << "[[";
		for (const auto& u : _battle.Units())
		{
			if (u.MyId > 1) std::cout << ',';
			std::cout << '[' << std::quoted(u.MyDefinition.MyId) << ',' << u.MyPosition.MyX << ',' << u.MyPosition.MyY << ',' << u.MyHealth << ','
				<< u.MyStats.MyMaxHealth << ',' << u.MyStats.MyAttack << ',' << u.MyStats.MyDamageTakenMultiplier << ',' << u.MyAlive << ',' << u.MyRemoved << ','
				<< _battle.SpTotal(u.MyId) << ',' << u.MySkill.MyActive << ',' << u.MySkill.MyTimeLeft << ',' << u.MyTotals.MyAttacks << ',' << u.MyTotals.MyDamage << ','
				<< u.MyElements.MyGauges[static_cast<unsigned>(Element::BURN)] << ',' << u.MyStats.MyElementalTakenMultiplier << ',' << u.MyStats.MyResistance << ']';
		}
		std::cout << "]," << _battle.RandomState() << ']';
	}

	BattleInput Input(unsigned _mode, unsigned _tier, unsigned _scene, bool _map)
	{
		const bool boss = _mode >= 2, multi = _mode % 2 != 0;
		BattleInput input{.MyTimeLimit = 100, .MyAutoFinish = false, .MyBossBattle = boss, .MyField = FieldDefinition{}, .MyLayerGainsEnabled = true};
		input.MyYanyouDefinition = MakeYanyouDefinition();
		for (unsigned player = 0; player < (multi ? 2U : 1U); ++player)
		{
			auto& p = input.MyPlayers.emplace_back(BattlePlayerInput{.MyPlayerId = player ? "q" : "p", .MyMirrorDeployment = boss && player != 0, .MyRightHalf = player != 0});
			p.MyBonds = {{"yanShip", _map ? 30.0 : 0.0, !_map && _scene == 3 ? 9 : 3, _tier > 0, !_map && _scene == 3 ? 0 : _tier}, {"maniShip", 0, 1, true, 1}};
			for (const auto& effect : ReferenceCoreBondEffects()) if (effect.MyKind == CoreBondKind::YAN) p.MyCoreBonds.push_back(effect);
			for (unsigned i = 0; i < 3; ++i)
			{
				CombatDefinition definition{.MyId = "a" + std::to_string(player * 3 + i),
					.MyStats = CombatStats{.MyMaxHealth = 1000 + i * 100.0, .MyAttack = 100 + i * 50.0, .MyBlockCount = 0, .MyRedeploySeconds = 70}, .MyAttack = AttackProfile{.MyDisabled = true}};
				definition.MyIdentity = {.MyBaseChess = definition.MyId, .MyMeleePosition = true, .MyBonds = {i == 2 || (!_map && _scene == 4) ? "maniShip" : "yanShip"}};
				definition.MyInitialBuffs.push_back({.MyKey = "fixture", .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, 0.5}}, .MyPersistent = true, .MyAllowDead = true});
				const double x = player ? (boss ? 15.0 - i : 13.0 + i) : 5.0 + i, y = boss ? 3 : 10;
				p.MyUnits.push_back({.MyPieceUid = player * 3 + i + 1, .MyDefinition = std::move(definition), .MyPosition = {x, y},
					.MyFacing = boss && player ? Facing::LEFT : Facing::RIGHT,
					.MyCarry = !_map && _scene == 5 && i < 2 ? std::optional(CarryState{.MyDown = true}) : std::nullopt,
					.MyDeferred = ((_map && _scene == 1) || (!_map && _scene == 6)) && i == 0});
			}
		}
		return input;
	}
}

int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17);
	for (std::string command; std::cin >> command;)
	{
		if (command == "map")
		{
			std::string stage; unsigned mode = 0, tier = 0, occupied = 0; std::cin >> stage >> mode >> tier >> occupied;
			auto input = Input(mode, tier, occupied, true);
			const auto& map = ReferenceBattleStage(stage);
			std::ranges::copy(map.MyTiles, input.MyField->MyTiles.begin());
			input.MyField->MyRect = mode >= 2 ? FieldRect{0, 5, 0, 20} : FieldRect{9, 12, 0, mode % 2 ? 20 : 10};
			for (const auto& slot : map.MyMapCharacters) input.MyMapCharacterTiles.push_back(slot.MyTile.MyPosition);
			if (occupied) input.MyMapCharacters = MakeMapCharacterVariants(map);
			Battle battle(std::move(input)); battle.Start(); Snapshot(battle); std::cout << '\n';
			if (!battle.ContentErrors().empty()) return 1;
			continue;
		}
		unsigned scene = 0, seed = 0; std::cin >> scene >> seed;
		const unsigned tier = scene == 0 ? 1U : scene == 1 || scene == 20 ? 2U : 3U;
		auto input = Input(scene == 8 || scene == 9 ? 1 : 0, tier, scene, false); input.MySeed = seed;
		for (unsigned i = 0; i < 3; ++i)
		{
			CombatDefinition enemy{.MyId = "enemy_enemy", .MyStats = CombatStats{.MyMaxHealth = scene == 16 ? 4000.0 : 1000000.0, .MyAttack = 30, .MyDefense = 100, .MyResistance = 20,
				.MyMoveSpeed = scene == 7 || scene == 8 ? 0.4 : 0}, .MyAttack = AttackProfile{.MyDisabled = true}, .MyFlying = scene == 18};
			if (scene == 19) enemy.MyHitArea = HitArea{.MyWidth = 2, .MyHeight = 2};
			const WorldPoint start{i == 0 ? 14.0 : 13.0, 9.0 + i};
			input.MySpawns.push_back({.MyTime = scene == 20 ? 40.0 : 0.0, .MyOwnerId = "p", .MyDefinition = std::move(enemy), .MyRoute = {.MyStart = start, .MyEnd = {0, start.MyY}}});
		}
		Battle battle(std::move(input)); battle.Start(); std::cout << '['; Snapshot(battle);
		std::vector<UnitId> tokens;
		for (const auto& unit : battle.Units()) if (unit.MyDefinition.MyYanyou) tokens.push_back(unit.MyId);
		for (unsigned tick = 0; tick < 2400; ++tick)
		{
			UnitId enemy = 0; for (const auto& unit : battle.Units()) if (unit.MySide == UnitSide::ENEMY && unit.MyAlive) { enemy = unit.MyId; break; }
			if (tick == 60) { (void)battle.AddBondLayers("p", "yanShip", 40); if (battle.Players().size() > 1) (void)battle.AddBondLayers("q", "yanShip", 80); }
			if (tick % 100 == 10 && enemy) for (const auto id : tokens)
			{ (void)battle.DealElement(enemy, id, ElementHit{.MyElement = Element::BURN, .MyAmount = 300}); (void)battle.DealDamage(enemy, id, 500, DamageType::PHYSICAL); }
			if (scene == 10 && tick == 510) for (const auto id : tokens) (void)battle.ApplyStatus(id, CombatStatus::SILENCE, 5);
			if (scene == 11 && tick == 510) for (const auto id : tokens) (void)battle.ApplyStatus(id, CombatStatus::STUN, 4);
			if (scene == 12 && tick == 600 && enemy) (void)battle.LoseHealth(0, enemy, 100000000);
			if (scene == 13 && tick == 600 && enemy) (void)battle.ApplyStatus(enemy, CombatStatus::STEALTH, 6);
			if (scene == 14 && tick == 200) for (const auto id : tokens) (void)battle.AddBuff(id, {.MyKey = "attack", .MyDuration = 5, .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, 0.7}}});
			if (scene == 15 && tick == 200) for (const auto id : tokens) (void)battle.AddBuff(id, {.MyKey = "range", .MyDuration = 2, .MyModifiers = std::vector<AttributeChange>{{Attribute::RANGE_EXTEND, 1}}});
			if (scene == 17 && tick == 650 && !tokens.empty()) (void)battle.LoseHealth(0, tokens.front(), 100000000);
			if (scene == 21 && (tick == 300 || tick == 600) && enemy) (void)battle.ApplyStatus(enemy + 2, CombatStatus::TAUNT, StatusApplication{.MyDuration = 15, .MyValue = 10});
			if (scene == 22 && tick == 450) for (const auto id : tokens) (void)battle.ApplyStatus(id, CombatStatus::NO_MOVE, 8);
			if (scene == 23 && (tick == 2 || tick == 200)) for (const auto id : tokens) { (void)battle.GainSp(id, 15); (void)battle.ActivateSkill(id); }
			if (tick % 15 == 0) { std::cout << ','; Snapshot(battle); }
			battle.Step();
			if (tick % 15 == 0) { std::cout << ','; Snapshot(battle); }
		}
		std::cout << "]\n";
		if (!battle.ContentErrors().empty()) return 1;
	}
}
