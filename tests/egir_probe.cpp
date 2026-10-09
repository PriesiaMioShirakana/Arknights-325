#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_bonds.hpp>
#include <stronghold/adapters/reference_choices.hpp>
#include <stronghold/adapters/reference_equipment.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	void Snapshot(const Battle& _battle)
	{
		std::cout << "[[";
		for (UnitId id = 1; id <= 9; ++id)
		{
			if (id > 1) std::cout << ',';
			const auto& u = _battle.Unit(id); const auto& s = u.MyStats;
			std::cout << '[' << u.MyHealth << ',' << u.MyAlive << ',' << s.MyMaxHealth << ',' << s.MyAttack << ',' << s.MyDefense << ',' << s.MyBlockCount << ','
				<< _battle.SpTotal(id) << ',' << u.MyPosition.MyX << ',' << u.MyPosition.MyY << ',' << u.MyProfession.MyDoll << ',' << u.MyProfession.MyDollSwitching << ',';
			if (std::isfinite(u.MyRespawnAt)) std::cout << u.MyRespawnAt; else std::cout << "null";
			std::cout << ']';
		}
		std::cout << "],[" << _battle.BondLayers("p", "egirShip") << ',' << _battle.BondLayers("q", "egirShip") << "],["
			<< _battle.Players()[0].MyDeaths << ',' << _battle.Players()[1].MyDeaths << "]," << _battle.RandomState() << ']';
	}
}

int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17);
	for (unsigned scene = 0, seed = 0; std::cin >> scene >> seed;)
	{
		const bool mirror = scene == 3;
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "p", .MyMirrorDeployment = mirror},
			BattlePlayerInput{.MyPlayerId = "q", .MyMirrorDeployment = mirror, .MyRightHalf = true}},
			.MyTimeLimit = 30, .MyAutoFinish = false, .MySeed = seed, .MyField = FieldDefinition{}, .MyLayerGainsEnabled = scene != 15};
		for (std::size_t player = 0; player < 2; ++player)
		{
			auto& p = input.MyPlayers[player];
			p.MyBonds.push_back({"egirShip", scene == 2 ? 60.0 : 0.0, scene == 22 ? 5 : 3,
				!(player == 1 && (scene == 12 || scene == 13)) && !(player == 0 && scene == 23), scene == 22 ? 0U : scene == 14 && player == 0 ? 1U : 2U});
			for (const auto& effect : ReferenceCoreBondEffects()) if (effect.MyKind == CoreBondKind::EGIR) p.MyCoreBonds.push_back(effect);
			if (scene == 13 || scene == 16) p.MyBonds.push_back({.MyId = "maniShip", .MyActive = true});
			if (scene == 10)
			{
				p.MyBonds.push_back({"indomShip", 200, 3, true, 2});
				for (const auto& effect : ReferenceAddonBondEffects()) if (effect.MyKind == AddonBondKind::INDOMITABLE) p.MyAddonBonds.push_back(effect);
			}
			if (scene == 9) p.MyBandEffects = MakeBandBattleEffects("band_ermengard");
		}
		constexpr std::array<WorldPoint, 9> positions{{{2, 10}, {3, 10}, {4, 10}, {5, 10}, {6, 10}, {3, 11}, {7, 10}, {8, 10}, {9, 10}}};
		constexpr std::array<WorldPoint, 4> cycle{{{2, 10}, {3, 10}, {3, 11}, {2, 11}}};
		constexpr std::array<Facing, 4> directions{Facing::RIGHT, Facing::UP, Facing::LEFT, Facing::DOWN};
		for (unsigned i = 0; i < positions.size(); ++i)
		{
			CombatDefinition definition{.MyId = "a" + std::to_string(i),
				.MyStats = CombatStats{.MyMaxHealth = scene == 1 || scene == 2 ? 7000.0 : scene == 20 ? 5000.0 : 1000.0,
					.MyAttack = 100 + i * 50.0, .MyDefense = scene == 4 ? 5000.0 : scene == 20 ? 0.0 : 650.0,
					.MyBlockCount = 1 + static_cast<int>(i % 2), .MyRedeploySeconds = 1}, .MyAttack = AttackProfile{.MyDisabled = true},
				.MySkill = SkillDefinition{.MyKind = SkillKind::DURATION, .MySpType = SpType::TIME, .MyTrigger = SkillTrigger::NEVER, .MySpCost = 100, .MyInitialSp = 25, .MyDuration = 1}};
			definition.MyIdentity = {.MyBaseChess = definition.MyId, .MyTier = 1 + i % 6, .MyMeleePosition = true};
			definition.MyIdentity.MyBonds = {(scene == 13 && i >= 6) || (scene == 16 && (i == 1 || i == 2)) ? "maniShip" : "egirShip"};
			if (scene == 17 && i == 1) definition.MyIdentity.MyBonds.clear();
			definition.MyInitialBuffs.push_back({.MyKey = "initial", .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, 0.5}}, .MyPersistent = true, .MyAllowDead = true});
			if (scene == 21) definition.MyInitialBuffs.push_back({.MyKey = "mitigation", .MyModifiers = std::vector<AttributeChange>{{Attribute::DAMAGE_TAKEN_MULTIPLIER, 0.1}}, .MyPersistent = true, .MyShield = {.MyHealth = 10000}, .MyAllowDead = true});
			if (scene == 8) { definition.MyIdentity.MyItems = {"chess_item_4_12_e_a"}; AppendEquipmentEffects(definition, definition.MyIdentity.MyItems); }
			if (scene == 11) definition.MyProfession = {.MyKind = ProfessionTrait::DOLLKEEPER, .MyDollDuration = 1};
			auto position = scene == 18 && i < 4 ? cycle[i] : positions[i];
			if (scene == 18 && i == 5) position = {6, 11};
			if (mirror) position.MyX = 20 - position.MyX;
			const auto facing = scene == 18 && i < 4 ? directions[i] : i == 5 ? Facing::DOWN : mirror || scene == 19 ? Facing::LEFT : Facing::RIGHT;
			input.MyPlayers[i < 6 ? 0 : 1].MyUnits.push_back({.MyPieceUid = i + 1, .MyDefinition = std::move(definition), .MyPosition = position, .MyFacing = facing,
				.MyKind = scene == 5 && i == 2 ? UnitKind::TOKEN : UnitKind::OPERATOR,
				.MyCarry = scene == 7 && (i == 1 || i == 6) ? std::optional(CarryState{.MyHealthRatio = 0.4, .MySp = 37, .MyDown = true}) : std::nullopt,
				.MyDeferred = scene == 6 && i == 2, .MyOwnerPieceUid = scene == 5 && i == 2 ? 1U : 0U});
		}
		Battle battle(std::move(input)); battle.Start(); std::cout << '['; Snapshot(battle);
		for (unsigned tick = 0; tick < 90; ++tick)
		{
			if (tick == 2) for (UnitId id = 1; id <= 9; ++id) (void)battle.AddBuff(id, {.MyKey = "later", .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, 1}}, .MyPersistent = true, .MyAllowDead = true});
			if (tick == 5) { (void)battle.AddBondLayers("p", "egirShip", 60); (void)battle.AddBondLayers("q", "egirShip", 60); }
			if (tick == 15) battle.Retreat(1);
			if (tick == 16) (void)battle.Redeploy(1);
			if (tick % 9 == 3) (void)battle.LoseHealth(0, 1 + tick / 9 % 9, 100000);
			if (tick % 20 == 7) for (UnitId id = 1; id <= 9; ++id) (void)battle.Redeploy(id);
			if (tick % 30 == 11) for (UnitId id = 1; id <= 9; ++id) (void)battle.Heal(id, id, 10000);
			if (tick % 3 == 0) { std::cout << ','; Snapshot(battle); }
			battle.Step();
			if (tick % 3 == 0) { std::cout << ','; Snapshot(battle); }
		}
		std::cout << "]\n";
		if (!battle.ContentErrors().empty()) return 1;
	}
}
