#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_bonds.hpp>
#include <stronghold/adapters/reference_equipment.hpp>
#include <stronghold/adapters/reference_choices.hpp>
#include <stronghold/adapters/reference_summons.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	void Snapshot(const Battle& _battle)
	{
		std::cout << "[[";
		for (std::size_t i = 0; i < _battle.Units().size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& u = _battle.Units()[i]; const auto& s = u.MyStats;
			std::cout << '[' << u.MyHealth << ',' << u.MyAlive << ',' << s.MyMaxHealth << ',' << s.MyAttack << ',' << s.MyDefense << ',' << s.MyAttackSpeed << ','
				<< s.MyRedeployMultiplier << ',' << s.MyDefenseIgnorePercent << ',' << s.MyResistanceIgnorePercent << ',' << s.MyDamageDealtMultiplier << ','
				<< s.MyPhysicalTakenMultiplier << ',' << s.MyArtsTakenMultiplier << ',' << s.MyDamageTakenMultiplier << ',' << _battle.SpTotal(u.MyId) << ','
				<< _battle.SpCost(u.MyId) << ',' << u.MySkill.MyActive << ',' << u.MyPosition.MyX << ',' << u.MyPosition.MyY << ',' << u.MyIndomitableFree << ','
				<< u.MyProfession.MyDoll << ',' << u.MyProfession.MyDollSwitching << ',';
			if (std::isfinite(u.MyRespawnAt)) std::cout << u.MyRespawnAt; else std::cout << "null";
			std::cout << ',' << u.MySkill.MyAmmoLeft << ',' << u.MySkill.MyAmmoMax << ",[";
			bool first = true;
			for (const auto& buff : u.MyBuffs) if (buff.MyDefinition.MyKey == "bond:sargon")
			{ if (!first) std::cout << ','; first = false; std::cout << buff.MyRemaining; }
			std::cout << "],["; first = true;
			for (const auto& grant : _battle.LentEquipment(u.MyId))
			{ if (!first) std::cout << ','; first = false; std::cout << '[' << std::quoted(grant.MyItem) << ',' << grant.MyUntil << ']'; }
			std::cout << "]]";
		}
		std::cout << "],[";
		for (std::size_t i = 0; i < CoreBondIds.size(); ++i) { if (i) std::cout << ','; std::cout << _battle.BondLayers("p", CoreBondIds[i]); }
		std::cout << "]," << _battle.Players()[0].MyDeaths << ',' << _battle.RandomState() << ']';
	}
}

int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17);
	for (std::string bond; std::cin >> bond;)
	{
		unsigned scene = 0, seed = 0; std::cin >> scene >> seed;
		constexpr std::array<double, 16> layers{0, 0, 0, 39, 49, 205, 250, 0, 30, 70, 300, 90, 1, 40, 50, 0};
		const double rowOffset = 0;
		const bool share = scene == 4 || scene == 7 || scene == 9;
		CombatDefinition base{.MyId = "a", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyAttack = 100, .MyDefense = 80,
			.MyResistance = 30, .MyBlockCount = 0, .MyRedeploySeconds = 1}, .MyAttack = AttackProfile{.MyDisabled = true},
			.MySkill = SkillDefinition{.MyKind = scene == 10 ? SkillKind::NONE : scene == 11 ? SkillKind::PASSIVE : scene % 2 ? SkillKind::DURATION : SkillKind::AMMO,
				.MySpType = SpType::TIME, .MyTrigger = SkillTrigger::NEVER, .MySpCost = 100, .MyDuration = 0.2, .MyAmmo = 4}};
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "p", .MyRightHalf = scene == 13},
			BattlePlayerInput{.MyPlayerId = "q", .MyMirrorDeployment = scene == 12, .MyRightHalf = true}},
			.MyTimeLimit = 90, .MyAutoFinish = false, .MySeed = seed, .MyBossBattle = false, .MyField = FieldDefinition{}, .MyLayerGainsEnabled = true};
		for (std::size_t player = 0; player < 2; ++player)
			for (const auto& effect : ReferenceCoreBondEffects())
			{
				if (effect.MyKind == CoreBondKind::SARGON && effect.MyShareEquipment != share) continue;
				const auto id = CoreBondIds[static_cast<unsigned>(effect.MyKind)];
				if (bond != "@all" && id != bond) continue;
				input.MyPlayers[player].MyBonds.push_back({std::string(id), layers[scene % layers.size()] + (player ? 70 : 0), scene == 2 ? 6 : 3, scene != 0, scene == 2 ? 0U : scene == 1 ? 1U : 2U});
				input.MyPlayers[player].MyCoreBonds.push_back(effect);
			}
		input.MyEquipmentTemplates = ReferenceEquipmentTemplates();
		input.MyYanyouDefinition = MakeYanyouDefinition();
		if (scene == 6 || scene == 7) for (auto& player : input.MyPlayers) player.MyBonds.push_back({.MyId = "maniShip", .MyActive = true});
		constexpr std::array<WorldPoint, 6> positions{{{5, 10}, {5, 11}, {6, 11}, {6, 10}, {4, 10}, {15, 10}}};
		for (unsigned i = 0; i < positions.size(); ++i)
		{
			auto definition = base; definition.MyId = "a" + std::to_string(i);
			definition.MyIdentity = {.MyBaseChess = definition.MyId, .MyGolden = i == 1 || i == 5, .MyMeleePosition = i != 2};
			if (i < 2 || i == 5)
			{
				for (const auto& active : input.MyPlayers[0].MyBonds) if (active.MyId != "maniShip") definition.MyIdentity.MyBonds.push_back(active.MyId);
			}
			else definition.MyIdentity.MyBonds.push_back("maniShip");
			if (scene == 8 && i == 0) definition.MyProfession = {.MyKind = ProfessionTrait::DOLLKEEPER, .MyDollDuration = 1};
			if (scene >= 3 && i == 0) definition.MyStats.MyBlockCount = 2;
			if (scene > 0 && i == 0) definition.MyIdentity.MyItems = {"chess_item_3_04_e_a", "chess_item_6_07_e_b"};
			if (scene > 0 && i == 1) definition.MyIdentity.MyItems = {"chess_item_2_03_e_a", "chess_item_3_01_e_a"};
			AppendEquipmentEffects(definition, definition.MyIdentity.MyItems);
			input.MyPlayers[i == 5 ? 1 : 0].MyUnits.push_back({.MyPieceUid = i + 1, .MyDefinition = std::move(definition),
				.MyPosition = {positions[i].MyX, positions[i].MyY + rowOffset}, .MyFacing = static_cast<Facing>(scene % 4),
				.MyKind = i == 4 ? UnitKind::TOKEN : UnitKind::OPERATOR, .MyDeferred = scene == 7 && i == 1, .MyOwnerPieceUid = i == 4 ? 2U : 0U});
		}
		if (scene == 9) input.MyPlayers[0].MyBandEffects = MakeBandBattleEffects("band_ermengard");
		for (unsigned i = 0; i < 3; ++i)
		{
			auto enemy = base; enemy.MyId = "enemy"; enemy.MySkill = {}; enemy.MyStats.MyMaxHealth = 1000000;
			const WorldPoint start{i == 2 ? 14.0 : i == 1 ? 5.6 : 5.4, (i == 1 ? 10.4 : 10.0) + rowOffset};
			input.MySpawns.push_back({.MyOwnerId = i == 2 ? "q" : "p", .MyDefinition = std::move(enemy), .MyRoute = {.MyStart = start, .MyEnd = {0, start.MyY}}});
			input.MyGroundRoutes.push_back(input.MySpawns.back().MyRoute);
		}
		Battle battle(std::move(input)); battle.Start(); battle.Step();
		std::array<UnitId, 3> enemies{};
		std::size_t enemyCount = 0;
		for (const auto& unit : battle.Units()) if (unit.MySide == UnitSide::ENEMY) enemies[enemyCount++] = unit.MyId;
		std::cout << '['; Snapshot(battle);
		for (unsigned tick = 0; tick < 2100; ++tick)
		{
			if (tick % 9 == 0 && tick < 300) for (UnitId id = 1; id <= 6; ++id) if (battle.Unit(id).MyAlive) { (void)battle.GainSp(id, 100); (void)battle.ActivateSkill(id); }
			if (tick % 40 == 10) for (UnitId id = 1; id <= 6; ++id) battle.EndSkill(id);
			if (tick % 3 == 0) { const std::array<UnitId, 2> targets{enemies[0], enemies[1]}; (void)battle.ForceAttack(1, targets); (void)battle.ForceAttack(2, targets); }
			if (tick == 45 || tick == 75) (void)battle.ApplyStatus(enemies[0], CombatStatus::COLD, 1.5);
			if (tick % 17 == 0) { (void)battle.DealDamage(1, enemies[0], 170, DamageType::ARTS); (void)battle.DealDamage(2, enemies[0], 130, DamageType::ARTS); }
			if (tick == 30) (void)battle.DealDamage(6, enemies[0], 140, DamageType::ARTS);
			if (tick % 31 == 3) for (UnitId id = 1; id <= 6; ++id) (void)battle.DealDamage(enemies[0], id, 230, tick % 2 ? DamageType::PHYSICAL : DamageType::ARTS);
			if (tick % 45 == 5) (void)battle.LoseHealth(0, 1, 100000);
			if (tick % 45 == 6) (void)battle.Redeploy(1);
			if (tick == 80) battle.Retreat(2);
			if (tick == 90) { (void)battle.Redeploy(2); battle.SetSpTotal(1, 100); }
			if (tick == 120) for (const auto id : CoreBondIds) { (void)battle.AddBondLayers("p", id, 25); (void)battle.AddBondLayers("p", id, 30); }
			if (tick == 150) (void)battle.LoseHealth(0, enemies[0], 550000);
			if (tick == 180) (void)battle.Relocate(3, {7, 11 + rowOffset});
			if (tick == 220) (void)battle.Relocate(3, {6, 11 + rowOffset});
			if (tick == 55) { (void)battle.DealElement(enemies[0], 1, ElementHit{.MyElement = Element::BURN, .MyAmount = 50}); (void)battle.DealDamage(enemies[0], 3, 50, DamageType::ELEMENTAL); }
			if (tick % 50 == 11) (void)battle.DealDamage(0, 1, 20, DamageType::TRUE_DAMAGE);
			if (tick % 50 == 12) (void)battle.DealDamage(enemies[0], 1, 20, DamageType::TRUE_DAMAGE);
			if (tick % 90 == 15) for (UnitId id = 1; id <= 6; ++id) (void)battle.Heal(id, id, 10000);
			if (scene == 8 && tick == 210) (void)battle.EnterDoll(1);
			if ((tick % 30 == 0 || (bond == "@all" && scene == 6 && tick < 300))) { std::cout << ','; Snapshot(battle); }
			battle.Step();
			if ((tick % 30 == 0 || (bond == "@all" && scene == 6 && tick < 300))) { std::cout << ','; Snapshot(battle); }
		}
		std::cout << "]\n";
		if (!battle.ContentErrors().empty()) return 1;
	}
}
