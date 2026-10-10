#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_bonds.hpp>
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
			std::cout << '[' << u.MyHealth << ',' << u.MyAlive << ',' << u.MyStats.MyAttackSpeed << ',' << u.MyStatuses.Has(CombatStatus::STEALTH) << ','
				<< u.MyStatuses.MyRemaining[static_cast<unsigned>(CombatStatus::FEAR)] << ',' << u.MyPosition.MyX << ',' << u.MyPosition.MyY << ',';
			if (std::isfinite(u.MySiracusaStealthEnd)) std::cout << u.MySiracusaStealthEnd; else std::cout << "null";
			std::cout << ",["; bool first = true;
			for (const auto& buff : u.MyBuffs) if (buff.MyDefinition.MyKey == "bond:siracusa" || buff.MyDefinition.MyKey == "bond:siracusa:stealth")
			{ if (!first) std::cout << ','; first = false; std::cout << '[' << std::quoted(buff.MyDefinition.MyKey) << ',' << buff.MyRemaining << ']'; }
			std::cout << "]]";
		}
		std::cout << "]," << _battle.RandomState() << ']';
	}
}

int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17);
	for (unsigned scene = 0, seed = 0; std::cin >> scene >> seed;)
	{
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "p"}, BattlePlayerInput{.MyPlayerId = "q"}},
			.MyTimeLimit = 90, .MyAutoFinish = false, .MySeed = seed, .MyField = FieldDefinition{}, .MyLayerGainsEnabled = true};
		for (unsigned player = 0; player < 2; ++player)
		{
			auto& p = input.MyPlayers[player];
			p.MyBonds.push_back({"siracusaShip", scene == 23 ? 30.0 : 0.0, scene == 2 ? 6 : 3, scene != 0, scene == 2 ? 0U : scene == 1 || scene == 22 ? 1U : 2U});
			for (const auto& effect : ReferenceCoreBondEffects()) if (effect.MyKind == CoreBondKind::SIRACUSA) p.MyCoreBonds.push_back(effect);
			if (scene == 18) p.MyBonds.push_back({.MyId = "maniShip", .MyActive = true});
		}
		CombatDefinition base{.MyId = "a", .MyStats = CombatStats{.MyMaxHealth = 1000000, .MyAttack = 100, .MyBlockCount = 0, .MyRedeploySeconds = 1},
			.MyAttack = AttackProfile{.MyDisabled = true, .MySplashRadius = scene == 7 ? 2.0 : 0.0}};
		for (unsigned i = 0; i < 4; ++i)
		{
			auto definition = base; definition.MyId += std::to_string(i);
			definition.MyIdentity = {.MyBaseChess = definition.MyId, .MyBonds = {scene == 18 && i % 2 ? "maniShip" : "siracusaShip"}};
			input.MyPlayers[i / 2].MyUnits.push_back({.MyPieceUid = i + 1, .MyDefinition = std::move(definition), .MyPosition = {5.0 + i % 2, 10.0 + i / 2}});
		}
		for (unsigned i = 0; i < 3; ++i)
		{
			auto enemy = base; enemy.MyId = "enemy"; enemy.MyStats.MyMaxHealth = scene == 20 ? 8000.0 : 1000000000.0; enemy.MyAttack.MySplashRadius = 0;
			const WorldPoint start{7.0 + i * 0.2, 10};
			input.MySpawns.push_back({.MyOwnerId = "p", .MyDefinition = std::move(enemy), .MyRoute = {.MyStart = start, .MyEnd = {0, 10}}});
		}
		Battle battle(std::move(input)); battle.Start(); battle.Step();
		if (scene == 19) for (UnitId id = 5; id <= 7; ++id) (void)battle.ApplyStatus(id, CombatStatus::RESIST, StatusApplication{.MyDuration = 90, .MyValue = 0.5});
		std::cout << '['; Snapshot(battle);
		for (unsigned tick = 0; tick < 2100; ++tick)
		{
			if ((scene == 3 && tick == 60) || (scene == 4 && tick == 1200)) for (UnitId id = 1; id <= 4; ++id) (void)battle.RemoveBuff(id, "bond:siracusa:stealth");
			if (scene == 5 && tick == 300) (void)battle.LoseHealth(0, 1, 10000000);
			if (scene == 5 && tick == 360) (void)battle.Redeploy(1);
			if ((scene == 6 || scene == 22) && tick == 1500) for (UnitId id = 1; id <= 4; ++id) (void)battle.ApplyStatus(id, CombatStatus::STEALTH, 10);
			if (scene == 21 && tick == 600) for (UnitId id = 1; id <= 4; ++id) (void)battle.ApplyStatus(id, CombatStatus::STUN, 40);
			if (scene == 23 && tick == 60) { (void)battle.AddBondLayers("p", "siracusaShip", 70); (void)battle.AddBondLayers("q", "siracusaShip", 100); }
			if (tick % 3 == 0)
			{
				for (UnitId id = 1; id <= 4; ++id) if (battle.Unit(id).MyAlive)
				{
					if (scene == 7) { const std::array<UnitId, 1> targets{5}; (void)battle.ForceAttack(id, targets); continue; }
					if (scene == 17) { (void)battle.DealElement(id, 5, ElementHit{.MyElement = Element::BURN, .MyAmount = 0.01}); continue; }
					constexpr std::array tags{DamageTag::DOT, DamageTag::PERIODIC, DamageTag::ADDITION, DamageTag::ITEM, DamageTag::HP_LOSS, DamageTag::BOND};
					(void)battle.DealDamage(id, 5 + id % 3, DamageInfo{.MyAmount = 3, .MyType = scene == 15 ? DamageType::ELEMENTAL : DamageType::ARTS,
						.MySourceless = scene == 14, .MyTags = scene >= 8 && scene <= 13 ? static_cast<DamageTags>(tags[scene - 8]) : 0,
						.MyIsAttack = tick % 2 == 0, .MyIsSplash = scene == 16});
				}
			}
			if (tick % 30 == 0) { std::cout << ','; Snapshot(battle); }
			battle.Step();
			if (tick % 30 == 0) { std::cout << ','; Snapshot(battle); }
		}
		std::cout << "]\n";
		if (!battle.ContentErrors().empty()) return 1;
	}
}
