#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_equipment.hpp>
#include <stronghold/adapters/reference_choices.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	void Snapshot(const Battle& _battle, std::size_t _receipt)
	{
		std::cout << '[';
		for (std::size_t i = 0; i < _battle.Units().size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& u = _battle.Units()[i];
			std::cout << '[' << u.MyHealth << ',' << u.MyStats.MyMaxHealth << ',' << u.MyStats.MyAttack << ',' << u.MyStats.MyAttackSpeed << ','
				<< u.MyStats.MyMoveSpeed << ',' << u.MyPosition.MyX << ',' << u.MyPosition.MyY << ',' << _battle.SpTotal(u.MyId) << ',' << u.MySkill.MyActive << ',' << u.MySkill.MyAmmoLeft << ',' << u.MyAlive;
			for (const auto status : {CombatStatus::STEALTH, CombatStatus::STUN, CombatStatus::COLD, CombatStatus::FREEZE, CombatStatus::LEVITATE, CombatStatus::SILENCE, CombatStatus::TREMBLE})
				std::cout << ',' << u.MyStatuses.Has(status);
			std::cout << ',' << (u.MyStatuses.Has(CombatStatus::PALSY) ? u.MyStatuses.MyValues[static_cast<std::size_t>(CombatStatus::PALSY)] : 0);
			int shields = 0; for (const auto& buff : u.MyBuffs) shields += buff.MyDefinition.MyShield.MyHits;
			std::cout << ',' << u.MyElements.MyGauges[static_cast<std::size_t>(Element::BURN)] << ',' << u.MyBlocking.size() << ',' << shields << ',';
			if (std::isfinite(u.MyRespawnAt)) std::cout << u.MyRespawnAt; else std::cout << "null";
			std::cout << ']';
		}
		std::cout << ",["; bool first = true;
		for (const auto& unit : _battle.Units()) for (const auto& grant : _battle.LentEquipment(unit.MyId))
		{
			if (!first) std::cout << ',';
			first = false; std::cout << '[' << grant.MyUnit << ',' << std::quoted(grant.MyItem) << ',' << grant.MyUntil << ']';
		}
		std::cout << "]," << _receipt << ',' << _battle.RandomState() << ']';
	}
}

int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17);
	for (std::string item; std::cin >> item;)
	{
		if (item == "@catalog")
		{
			std::cout << '['; bool first = true;
			for (const auto& entry : ReferenceEquipmentTemplates()) { if (!first) std::cout << ','; first = false; std::cout << std::quoted(entry.MyId); }
			std::cout << "]\n"; continue;
		}
		unsigned scene = 0, seed = 0; std::cin >> scene >> seed;
		const bool lending = scene >= 100; if (lending) scene -= 100;
		std::size_t receipt = 0;
		constexpr std::array<std::string_view, 4> hammerItems{"chess_item_4_09_e_a", "chess_item_3_09_e_a", "chess_item_3_10_e_a", "chess_item_2_03_e_a"};
		const bool steam = item.starts_with("chess_item_6_05");
		const bool hammer = steam || std::ranges::any_of(hammerItems, [&](std::string_view _id) { _id.remove_suffix(1); return item.starts_with(_id); });
		const std::string bond = hammer ? "victoriaShip" : item.starts_with("chess_item_6_02") ? "lateranoShip" : item.starts_with("chess_item_6_03") ? "yanShip" :
			item.starts_with("chess_item_6_06") ? "kjeragShip" : item.starts_with("chess_item_6_07") ? "sargonShip" : item.starts_with("chess_item_6_11") ? "siracusaShip" : item.starts_with("chess_item_6_10") ? "kazimierzShip" : "egirShip";
		const bool signature = !hammer && (item.starts_with("chess_item_5_09") || (item.starts_with("chess_item_6_") && !item.starts_with("chess_item_6_04")));
		std::string partner;
		for (const auto& rule : ReferenceEquipmentEffects()) if (rule.MyItem == item && !rule.MyPartner.empty()) partner = rule.MyPartner;
		CombatDefinition ally{.MyId = "op", .MyStats = CombatStats{.MyMaxHealth = 3000, .MyAttack = 100, .MyDefense = 80,
			.MyResistance = 20, .MyBlockCount = 1, .MyRedeploySeconds = 70},
			.MyAttack = AttackProfile{.MyDamageType = scene == 3 || (hammer && scene >= 16) ? DamageType::ARTS : DamageType::PHYSICAL, .MyDisabled = true,
				.MyHealing = scene == 4, .MyMaxTargets = 2}, .MyRange = {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}}};
		ally.MyIdentity.MyMeleePosition = scene != 22;
		ally.MyIdentity.MyBonds = {scene == 8 ? "other" : scene == 9 || scene == 10 ? "maniShip" : bond};
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "p"}, BattlePlayerInput{.MyPlayerId = "q"}}, .MyAutoFinish = false, .MySeed = seed};
		input.MyEquipmentTemplates = ReferenceEquipmentTemplates();
		input.MyPlayers[0].MyGainedChess = scene % 5;
		if (scene >= 12) ally.MyRange = {{0, 4}, {0, 3}, {0, 0}, {0, 1}, {0, 2}, {0, 5}};
		if (scene == 9 || scene == 10) input.MyPlayers[0].MyBonds = {{.MyId = bond, .MyActive = true}, {.MyId = "maniShip", .MyActive = scene == 9}};
		constexpr std::array<WorldPoint, 4> positions{{{5, 5}, {6, 5}, {5, 6}, {6, 6}}};
		for (unsigned i = 0; i < 4; ++i)
		{
			auto definition = ally;
			if (scene >= 6 && i >= 2) definition.MyId = "enemy_ally";
			if (i == 0)
			{
				definition.MyIdentity.MyItems = {item};
				if (!partner.empty() && (scene == 7 || scene >= 11)) definition.MyIdentity.MyItems.push_back(partner + "_a");
				definition.MySkill = SkillDefinition{.MyKind = scene % 2 ? SkillKind::DURATION : SkillKind::AMMO,
					.MyTrigger = SkillTrigger::NEVER, .MySpCost = 100, .MyInitialSp = scene == 14 ? 100.0 : 1.0, .MyDuration = 0.2, .MyAmmo = 4};
				definition.MySkill.MyActivateOnDeploy = scene == 14;
			}
			if ((signature || lending) && i > 0) definition.MySkill = SkillDefinition{.MyKind = lending && scene % 2 == 0 ? SkillKind::AMMO : SkillKind::DURATION, .MyTrigger = SkillTrigger::NEVER, .MySpCost = 100, .MyDuration = 0.2, .MyAmmo = 4};
			if (i == 1 && (scene == 6 || (lending && scene == 2))) definition.MyIdentity.MyItems = {item};
			if (lending && i == 2 && scene == 5) definition.MyIdentity.MyItems = {item};
			if (lending && i == 1 && scene == 4) definition.MyIdentity.MyItems = {"chess_item_6_05_e_a"};
			if (hammer && scene >= 16)
			{
				if (i == 0 && !steam) definition.MyIdentity.MyItems.push_back("chess_item_6_05_e_a");
				if (i == 0 && steam && scene % 2 == 0) definition.MyIdentity.MyItems.emplace_back(hammerItems[(scene - 16) / 2]);
				if (i == 1 && steam && scene % 2 != 0) definition.MyIdentity.MyItems.emplace_back(hammerItems[(scene - 16) / 2]);
				if (i == 0 && scene == 23) definition.MyIdentity.MyItems.push_back(item);
			}
			AppendEquipmentEffects(definition, definition.MyIdentity.MyItems);
			auto position = positions[i]; if (i == 1 && scene == 0) position.MyX = 4;
			input.MyPlayers[i == 3 ? 1 : 0].MyUnits.push_back(AllyDeployment{.MyPieceUid = i + 1, .MyDefinition = std::move(definition),
				.MyPosition = position, .MyFacing = scene == 11 ? Facing::UP : scene == 5 ? Facing::LEFT : Facing::RIGHT,
				.MyKind = i == 2 && (scene == 3 || scene == 7) ? UnitKind::TOKEN : UnitKind::OPERATOR, .MyDeferred = i == 2 && scene == 1});
		}
		if (scene == 5) input.MyPlayers[0].MyBandEffects = MakeBandBattleEffects("band_ermengard");
		auto enemy = ally; enemy.MyId = "enemy"; enemy.MyStats.MyMaxHealth = 300000; enemy.MyStats.MyBlockCount = 0; enemy.MyStats.MyMoveSpeed = scene == 15 ? 2 : 0;
		for (unsigned i = 0; i < 2; ++i)
			input.MySpawns.push_back({.MyOwnerId = "p", .MyDefinition = enemy,
				.MyRoute = CombatRoute{.MyStart = {i ? 9.0 : 5.0, 5}, .MyEnd = {0, 5}}});
		Battle battle(std::move(input)); battle.Start(); battle.Step();
		std::cout << '['; Snapshot(battle, receipt);
		for (unsigned tick = 0; tick < 900; ++tick)
		{
			if (lending)
			{
				if (tick == 0 || tick == 30 || tick == 120 || tick == 300) receipt = battle.LendEquipment(1, 2, scene % 2 ? 6 : 5, 2.5);
				if (scene == 5 && tick == 45) receipt = battle.LendEquipment(3, 2, 6, 2.5);
				if (tick % 30 == 0 && battle.Unit(2).MyAlive) { (void)battle.GainSp(2, 100); (void)battle.ActivateSkill(2); }
				if (tick % 5 == 0) { const std::array<UnitId, 2> targets{5, 6}; (void)battle.ForceAttack(2, targets); }
				if (tick % 30 == 1) (void)battle.DealDamage(5, 2, 300, DamageType::ARTS);
				if (tick % 90 == 4) (void)battle.LoseHealth(0, 2, 100000);
				if (tick % 90 == 5) (void)battle.Redeploy(2);
				if (tick % 60 == 20) (void)battle.ApplyStatus(2, CombatStatus::STEALTH, 1, 2);
				if (scene == 6 && tick == 70) (void)battle.Retreat(2);
				if (scene == 6 && tick == 100) (void)battle.Redeploy(2);
			}
			if (tick % 30 == 0 && battle.Unit(1).MyAlive) { (void)battle.GainSp(1, 100); (void)battle.ActivateSkill(1); }
			if (tick % 5 == 0 && (scene < 6 || tick < 30 || signature || hammer)) { const std::array<UnitId, 2> targets = scene == 4 ? std::array<UnitId, 2>{2, 3} : std::array<UnitId, 2>{5, 6}; (void)battle.ForceAttack(1, targets); }
			if (scene < 12 && tick % 30 == 1) { (void)battle.DealDamage(5, 1, scene >= 6 ? 700 : 140, DamageType::PHYSICAL); (void)battle.DealDamage(6, 1, scene >= 6 ? 600 : 130, DamageType::ARTS); }
			if (scene >= 6 && scene < 12 && tick % 17 == 6)
			{
				(void)battle.DealDamage(3, 5, 50, DamageType::TRUE_DAMAGE);
				(void)battle.DealDamage(4, 6, 50, DamageType::TRUE_DAMAGE);
				(void)battle.DealDamage(5, 1, DamageInfo{.MyAmount = 450, .MyType = DamageType::TRUE_DAMAGE, .MyTags = static_cast<DamageTags>(DamageTag::ITEM)});
			}
			if (scene >= 6 && tick % 90 == 8) (void)battle.Heal(2, 1, 10000);
			if (tick % 15 == 2)
			{
				(void)battle.Heal(1, 2, 55); (void)battle.Heal(1, 1, 20, HealOptions{.MySelf = true});
				(void)battle.Heal(1, 1, 20, HealOptions{.MyRegen = true});
			}
			if (tick % 60 == 3)
			{
				for (const auto status : {CombatStatus::STUN, CombatStatus::COLD, CombatStatus::COLD, CombatStatus::LEVITATE})
					(void)battle.ApplyStatus(1, status, 0.4, 5);
			}
			if (scene != 2 && scene < 12 && tick % 90 == 4) (void)battle.LoseHealth(0, 1, 100000);
			if (scene != 2 && scene < 12 && tick % 90 == 5) (void)battle.Redeploy(1);
			if (hammer && scene >= 16)
			{
				if (tick % 30 == 6) (void)battle.DealDamage(1, 6, 600, DamageType::ARTS);
				if (tick % 180 == 4) (void)battle.LoseHealth(0, 1, 100000);
				if (tick % 180 == 5) (void)battle.Redeploy(1);
				if (tick % 180 == 30) (void)battle.Retreat(2);
				if (tick % 180 == 60) (void)battle.Redeploy(2);
				if (scene == 21 && tick % 150 == 50) { (void)battle.Retreat(1); (void)battle.Redeploy(1); }
			}
			if (signature && tick % 180 == 20) (void)battle.ApplyStatus(1, CombatStatus::STEALTH, 2, 1);
			if (signature && tick % 60 == 10)
			{
				(void)battle.ApplyStatus(5, CombatStatus::COLD, 1.5, 1);
				(void)battle.ApplyStatus(6, scene % 2 ? CombatStatus::COLD : CombatStatus::FREEZE, 1.5, 1);
			}
			if (signature && tick == 15) (void)battle.LoseHealth(0, 1, 1000);
			battle.Step();
			if (tick % 10 == 0 || tick % 90 == 4 || tick % 90 == 5) { std::cout << ','; Snapshot(battle, receipt); }
		}
		std::cout << "]\n";
		if (!battle.ContentErrors().empty()) return 1;
	}
}
