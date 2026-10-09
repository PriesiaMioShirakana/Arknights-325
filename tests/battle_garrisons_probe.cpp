#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_choices.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	void Snapshot(const Battle& _battle)
	{
		std::cout << "[[";
		for (UnitId id = 1; id <= 7; ++id)
		{
			if (id > 1) std::cout << ',';
			const auto& u = _battle.Unit(id); const auto& s = u.MyStats;
			std::cout << '[' << u.MyHealth << ',' << u.MyAlive << ',' << s.MyMaxHealth << ',' << s.MyAttack << ',' << s.MyDefense << ',' << s.MyAttackSpeed << ','
				<< s.MyRedeployMultiplier << ',' << s.MySpRecovery << ',' << s.MyHealthRegen << ',' << _battle.SpTotal(id) << ',' << u.MySkill.MyActive << ','
				<< u.MySkill.MyAmmoLeft << ',' << u.MyProfession.MyDoll << ',' << u.MyTotals.MyDamage << ",[";
			bool first = true;
			for (const auto& buff : u.MyBuffs) if (buff.MyDefinition.MyKey.starts_with("gar:"))
			{
				if (!first) std::cout << ',';
				first = false; std::cout << '[' << std::quoted(buff.MyDefinition.MyKey) << ',';
				if (std::isfinite(buff.MyRemaining)) std::cout << buff.MyRemaining; else std::cout << "null";
				std::cout << ']';
			}
			std::cout << "]]";
		}
		std::cout << "],[";
		for (std::size_t i = 0; i < _battle.Players().size(); ++i)
		{
			if (i) std::cout << ',';
			std::cout << '['; bool first = true;
			for (const auto& b : _battle.Players()[i].MyBonds) { if (!first) std::cout << ','; first = false; std::cout << b.MyLayers; }
			std::cout << ']';
		}
		std::cout << "],[";
		for (UnitId id = 8; id <= _battle.Units().size(); ++id) { if (id > 8) std::cout << ','; std::cout << _battle.Unit(id).MyHealth; }
		std::cout << "]," << _battle.RandomState() << ']';
	}
}

int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17);
	for (std::string gid; std::cin >> gid;)
	{
		unsigned scene = 0, seed = 0; std::cin >> scene >> seed;
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "p", .MyMirrorDeployment = scene == 2}, BattlePlayerInput{.MyPlayerId = "q", .MyRightHalf = true}},
			.MyTimeLimit = 200, .MyAutoFinish = false, .MySeed = seed, .MyField = FieldDefinition{}, .MyLayerGainsEnabled = scene != 3};
		input.MyGarrisonRules = ReferenceBattleGarrisons();
		for (auto& player : input.MyPlayers)
			for (std::size_t i = 0; i < input.MyGarrisonRules.MyBondOrder.size(); ++i)
				player.MyBonds.push_back({std::string(input.MyGarrisonRules.MyBondOrder[i]), (scene == 2 || scene >= 4) ? 300.0 : scene == 1 ? 37.0 : 0.0, 3, (scene != 1 || i % 3 != 0) && (scene != 5 || i >= 8), 1});
		std::array<WorldPoint, 7> positions{{{5, 10}, {6, 10}, {7, 10}, {5, 9}, {5, 11}, {15, 10}, {4, 10}}};
		if (scene == 9) positions[5] = {7, 9};
		for (unsigned i = 0; i < positions.size(); ++i)
		{
			CombatDefinition def{.MyId = "a" + std::to_string(i), .MyStats = CombatStats{.MyMaxHealth = 1000, .MyAttack = 100, .MyDefense = 50,
				.MyResistance = 20, .MyBlockCount = 0, .MyRedeploySeconds = 1}, .MyAttack = AttackProfile{.MyDisabled = true},
				.MySkill = SkillDefinition{.MyKind = scene == 11 ? SkillKind::PASSIVE : SkillKind::AMMO, .MySpType = SpType::TIME, .MyTrigger = SkillTrigger::NEVER, .MySpCost = 10, .MyAmmo = 3}};
			def.MyRange.clear(); for (int r = -3; r <= 3; ++r) for (int c = -3; c <= 3; ++c) def.MyRange.push_back({r, c});
			def.MyIdentity.MyTier = i + 1;
			for (const auto id : input.MyGarrisonRules.MyBondOrder) def.MyIdentity.MyBonds.emplace_back(id);
			if (i == 0 || i == 1 || i == 4 || i == 5) def.MyIdentity.MyGarrisons = {gid, gid};
			if (gid == "@mixed" && (i == 0 || i == 1 || i == 4 || i == 5)) def.MyIdentity.MyGarrisons = {"garrison_145_a", "garrison_144_a", "garrison_160_a", "garrison_159_a", "garrison_72_a", "garrison_95_a", "garrison_148_a", "garrison_108_a", "garrison_106_a", "garrison_90_a", "garrison_42_a", "garrison_22_a", "garrison_24_a", "garrison_40_a", "garrison_125_a", "garrison_28_a", "garrison_05_a", "garrison_115_a", "garrison_101_a", "garrison_18_a", "garrison_01_a", "garrison_126_a", "garrison_118_b"};
			if (scene == 5 && (i == 1 || i == 2)) def.MyIdentity.MyBonds = {"maniShip"};
			if (i == 6) def.MyIdentity.MyGarrisons = {"garrison_59_b"};
			if ((scene == 2 || scene >= 4) && i == 0) def.MyProfession = {.MyKind = ProfessionTrait::DOLLKEEPER, .MyDollDuration = 1};
			input.MyPlayers[i == 5 ? 1 : 0].MyUnits.push_back({.MyPieceUid = i + 1, .MyDefinition = std::move(def), .MyPosition = positions[i],
				.MyFacing = scene == 1 && i < 2 ? Facing::UP : Facing::RIGHT, .MyKind = i == 4 ? UnitKind::TOKEN : UnitKind::OPERATOR,
				.MyCarry = scene == 6 && i == 0 ? std::optional(CarryState{.MyDown = true}) : std::nullopt, .MyDeferred = scene == 6 && i == 1});
		}
		for (unsigned i = 0; i < 24; ++i)
		{
			CombatDefinition enemy{.MyId = "enemy", .MyStats = CombatStats{.MyMaxHealth = 1000000, .MyAttack = 10, .MyDefense = 80, .MyResistance = 40},
				.MyAttack = AttackProfile{.MyDisabled = true}};
			enemy.MyEnemyTags = {i % 2 ? "drone" : "seamonster"};
			input.MySpawns.push_back({.MyOwnerId = "p", .MyDefinition = std::move(enemy), .MyRoute = {.MyStart = {7, 10}, .MyEnd = {0, 10}}});
		}
		Battle battle(std::move(input)); battle.Start(); battle.Step(); std::cout << '['; Snapshot(battle);
		for (unsigned tick = 0; tick < (scene == 4 ? 3630U : 360U); ++tick)
		{
			UnitId enemy = 0; for (UnitId id = 8; id <= battle.Units().size(); ++id) if (battle.Unit(id).MyAlive) { enemy = id; break; }
			if (tick < 360 && tick % 10 == 0) for (UnitId id = 1; id <= 7; ++id) { battle.EndSkill(id); battle.SetSpTotal(id, 10); (void)battle.ActivateSkill(id); }
			if (enemy && tick < 360)
			{
				if (tick % 3 == 0) for (UnitId id = 1; id <= 7; ++id) { const std::array<UnitId, 1> targets{enemy}; (void)battle.ForceAttack(id, targets); }
				if (scene == 10 && tick % 10 == 1) (void)battle.AddBuff(enemy, {.MyKey = "ward", .MyDuration = 0.2, .MyStatus = CombatStatus::FREEZE});
				if (tick % 10 == 1) { (void)battle.ApplyStatus(enemy, CombatStatus::FREEZE, 2); (void)battle.ApplyStatus(enemy, CombatStatus::FREEZE, 2); (void)battle.ApplyStatus(enemy, CombatStatus::STUN, 2); }
				if (tick % 10 == 2) (void)battle.ApplyStatus(enemy, CombatStatus::FREEZE, StatusApplication{.MyDuration = 2, .MyReenter = true});
				if (scene != 10 && tick % 10 == 3) { (void)battle.ApplyStatus(enemy, CombatStatus::BIND, 1); (void)battle.ApplyStatus(enemy, CombatStatus::SLUGGISH, 1); }
				if (scene == 10 && tick % 10 == 3) (void)battle.AddBuff(enemy, {.MyKey = "sluggish", .MyDuration = 1});
				if (tick % 10 == 4) (void)battle.DealDamage(1, enemy, DamageInfo{.MyAmount = 170, .MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::DOT)});
				if (tick % 13 == 12) (void)battle.LoseHealth(1, enemy, 10000000);
			}
			if (tick < 360 && tick % 40 == 6) (void)battle.ApplyStatus(2, CombatStatus::SLEEP, 0.3);
			if (tick < 360 && tick % 50 == 7) (void)battle.LoseHealth(1, 2, 10000000);
			if (tick < 360 && tick % 50 == 8) (void)battle.Redeploy(2);
			if (scene == 7 && tick == 0) { (void)battle.AddBuff(1, {.MyKey = "school", .MyDuration = 100, .MyModifiers = std::vector<AttributeChange>{{Attribute::PHYSICAL_DEALT_MULTIPLIER, 2}, {Attribute::ARTS_DEALT_MULTIPLIER, 0.2}}}); }
			if (scene == 8 && tick == 0) for (const auto id : {2U, 3U, 6U}) (void)battle.Relocate(id, {5.0 + id, 6});
			if (scene == 8 && tick == 150) { (void)battle.Relocate(2, {6, 10}); (void)battle.Relocate(3, {7, 10}); (void)battle.Relocate(6, {4, 10}); }
			if (tick == 90) (void)battle.EnterDoll(1);
			if (tick == 130) battle.Retreat(1);
			if (tick == 132) (void)battle.Redeploy(1);
			if (tick == 60 || tick == 200) { (void)battle.AddBondLayers("p", "yanShip", 51); (void)battle.AddBondLayers("q", "sargonShip", 100); }
			if (tick == 180) { battle.SetBondLayers("p", "sargonShip", 600); (void)battle.AddBondLayers("p", "sargonShip", 1); }
			if (tick == 210) { battle.SetBondLayers("p", "sargonShip", 0); (void)battle.AddBondLayers("p", "yanShip", 1); }
			if (scene != 4 && tick == 230) for (const auto id : ReferenceBattleGarrisons().MyBondOrder) battle.SetBondLayers("p", id, 0);
			if (tick == 231) (void)battle.AddBondLayers("p", "yanShip", 1);
			if ((tick < 360 && tick % 10 == 0) || (tick >= 360 && tick % 60 == 0)) { std::cout << ','; Snapshot(battle); }
			battle.Step();
			if ((tick < 360 && tick % 10 == 0) || (tick >= 360 && tick % 60 == 0)) { std::cout << ','; Snapshot(battle); }
		}
		std::cout << "]\n";
		if (!battle.ContentErrors().empty()) return 1;
	}
}
