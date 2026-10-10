#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_ally.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	CombatDefinition Buddy(bool _support = false)
	{
		CombatDefinition d{.MyId = "buddy", .MyStats = {.MyMaxHealth = 20000, .MyAttack = 100, .MyBlockCount = 0, .MyRedeploySeconds = 100}, .MyAttack = {.MyDisabled = true}};
		d.MyIdentity.MyBonds = {"lateranoShip"};
		d.MyOperatorProfession = _support ? OperatorProfession::SUPPORT : OperatorProfession::WARRIOR;
		d.MySkill = {.MyKind = SkillKind::AMMO, .MySpType = SpType::NONE, .MyTrigger = SkillTrigger::NEVER, .MySpCost = 1, .MyInitialSp = 1, .MyAmmo = 4, .MyManual = false};
		return d;
	}

	void Snapshot(const Battle& b)
	{
		std::cout << '[' << b.RandomState() << ',' << b.Players()[0].MyDp << ",[";
		for (std::size_t i = 0; i < b.Units().size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& u = b.Units()[i]; const auto& s = u.MyStats;
			std::cout << '[' << u.MyHealth << ',' << u.MyAlive << ',' << s.MyAttack << ',' << s.MyDefense << ',' << s.MyMaxHealth << ',' << s.MyResistance << ','
				<< s.MyAttackSpeed << ',' << s.MyBaseAttackTime << ',' << s.MyTaunt << ',' << b.SpTotal(u.MyId) << ',' << u.MySkill.MyAmmoLeft << ',' << u.MySkill.MyAmmoMax << ',' << u.MySkill.MyActive << ',';
			if (std::isfinite(u.MySkill.MyTimeLeft)) std::cout << u.MySkill.MyTimeLeft; else std::cout << "null";
			std::cout << ',' << u.MyTotals.MyAttacks << ',' << u.MyBlockedBy << ',' << u.MyAttackCooldown << ',' << u.MyStatuses.MyRemaining[static_cast<std::size_t>(CombatStatus::SLUGGISH)] << ','
				<< u.MyStatuses.Has(CombatStatus::STEALTH) << ',' << u.MyStatuses.Has(CombatStatus::REVEAL) << ',' << u.MyStatuses.Has(CombatStatus::STUN) << ','
				<< u.MyStatuses.Has(CombatStatus::DISARM) << ',' << u.MyTotals.MyHealing << ',' << u.MyTotals.MyDamage << ",[";
			for (std::size_t j = 0; j < u.MyElements.MyGauges.size(); ++j) { if (j) std::cout << ','; std::cout << u.MyElements.MyGauges[j]; }
			std::cout << "]," << u.MyStatuses.Has(CombatStatus::SILENCE) << ',' << u.MyStatuses.Has(CombatStatus::BURST_LOCK) << ',' << u.MyStatuses.Has(CombatStatus::NO_HEAL) << ',' << s.MyHealthRegen << ',' << u.MyTinmanZones << ','
				<< u.MyProfession.MyStored << ',' << u.MyStatuses.MyRemaining[static_cast<std::size_t>(CombatStatus::BIND)] << ',' << s.MyBlockCount << ',' << s.MyPhysicalTakenMultiplier << ',' << s.MyArtsTakenMultiplier << ',' << u.MyRemoved << ','
				<< u.MyPosition.MyX << ',' << u.MyPosition.MyY << ',' << u.MyDefinition.MyStats.MyDeploymentCost << ',' << u.MyWildmnCostReduction << ',';
			const auto block = std::ranges::find(u.MyBuffs, std::string_view("liskam:block"), [](const auto& _buff) { return std::string_view(_buff.MyDefinition.MyKey); });
			std::cout << (block == u.MyBuffs.end() ? 0 : block->MyRemaining) << ']';
		}
		std::cout << "]]";
	}

	void Run(std::string_view id, int skill, std::string_view module, unsigned seed, unsigned scene)
	{
		const bool stress = scene >= 2, dual = scene % 2 != 0, mixed = scene >= 4;
		const auto& record = ReferenceOperator(id);
		auto source = record.Loadout(skill, module == "-" ? "" : module).MyBody.MakeKitDefinition();
		if (!source.MyOperatorKit) throw std::runtime_error("missing operator kit");
		source.MyIdentity.MyBonds.assign(record.MyBonds.begin(), record.MyBonds.end());
		BattleInput input{.MyPlayers = {{.MyPlayerId = "p", .MyUnits = {{.MyPieceUid = 1, .MyDefinition = source, .MyPosition = {5, 9}},
			{.MyPieceUid = 2, .MyDefinition = Buddy(mixed), .MyPosition = {stress ? 6.0 : 4.0, 9}}, {.MyPieceUid = 3, .MyDefinition = Buddy(), .MyPosition = {stress ? 6.0 : 4.0, 10}}}}}, .MyTimeLimit = 200, .MyAutoFinish = false, .MySeed = seed};
		if (dual)
		{
			auto clone = source;
			if (mixed)
			{
				std::string other(id); other.back() = other.back() == 'a' ? 'b' : 'a';
				const auto& alternate = ReferenceOperator(other); clone = alternate.Loadout(skill, "").MyBody.MakeKitDefinition();
				clone.MyIdentity.MyBonds.assign(alternate.MyBonds.begin(), alternate.MyBonds.end());
			}
			input.MyPlayers[0].MyUnits.push_back({.MyPieceUid = 4, .MyDefinition = std::move(clone), .MyPosition = {5, 10}});
		}
		input.MyPlayers.push_back({.MyPlayerId = "q", .MyUnits = {{.MyPieceUid = 5, .MyDefinition = Buddy(mixed), .MyPosition = stress ? WorldPoint{6, 8} : WorldPoint{12, 9}}}});
		if (scene == 12 || scene == 13) input.MyPlayers[0].MyUnits[2].MyDefinition.MyStats.MyDeploymentCost = 11;
		Battle b(std::move(input)); b.Start();
		std::vector<UnitId> enemies;
		for (unsigned i = 0; i < 5; ++i)
		{
			CombatDefinition e{.MyId = "enemy", .MyStats = {.MyMaxHealth = 2000000, .MyAttack = 10, .MyDefense = 40, .MyResistance = 20, .MyMoveSpeed = 0, .MyMass = 1},
				.MyAttack = {.MyDisabled = true, .MyRanged = i == 1, .MyEnemyRange = i == 1 ? 2.0 : 0.0}};
			if (i % 2 == 0) e.MyEnemyTags = {"seamonster"};
			e.MyElite = i == 2; e.MyLeader = i == 3;
			e.MyFlying = scene >= 6 && i == 0;
			const WorldPoint point{i == 0 ? 5.4 : 6.0 + (i % 3), 9.0 + (i / 3)};
			enemies.push_back(b.SpawnEnemy({.MyOwnerId = "p", .MyDefinition = std::move(e), .MyRoute = {.MyStart = point, .MyEnd = {0, 9}}}));
		}
		if (!stress) (void)b.LoseHealth(0, enemies[1], 1500000);
		(void)b.ApplyStatus(enemies[4], CombatStatus::STEALTH, 60);
		const auto allyCount = b.Units().size() - enemies.size();
		if (stress) for (UnitId i = 1; i <= allyCount; ++i) (void)b.LoseHealth(0, i, b.Unit(i).MyStats.MyMaxHealth * (b.Unit(i).MyDefinition.MyId == "buddy" ? 0.6 : 0.25));
		std::cout << '['; Snapshot(b);
		for (unsigned tick = 0; tick < 2400; ++tick)
		{
			if (scene == 8 || scene == 9)
			{
				if (tick == 0 || tick == 400) for (const auto enemy : enemies) (void)b.ApplyStatus(enemy, CombatStatus::BIND, 12);
				if (tick == 10) (void)b.ApplyStatus(1, CombatStatus::DISARM, 3);
				if (tick == 120) (void)b.ApplyStatus(1, CombatStatus::STUN, 3);
				if (tick == 300) (void)b.RemoveStatus(enemies[0], CombatStatus::BIND);
				if (tick == 550) (void)b.RemoveStatus(enemies[1], CombatStatus::BIND);
			}
			if (tick % 150 == 0)
				for (UnitId i = 1; i <= allyCount; ++i) { b.SetSpTotal(i, 1000); (void)b.ActivateSkill(i); }
			if (tick % 17 == 0)
			{
				const auto& u = b.Unit(1);
				const bool healing = u.MySkill.MyActive && u.MyDefinition.MySkill.MyAttack && u.MyDefinition.MySkill.MyAttack->MyHealing;
				const std::array<UnitId, 1> targets{stress ? (healing ? UnitId{2} : enemies[(tick / 17) % 2]) : enemies[1]}; (void)b.ForceAttack(1, targets);
			}
			if (tick % 91 == 0 && b.Unit(1).MyAlive && b.Unit(1).MyHealth > 400) (void)b.LoseHealth(0, 1, 100);
			if (!stress && (tick == 450 || tick == 1350)) for (UnitId i = 1; i <= allyCount; ++i) b.EndSkill(i);
			if (tick == (stress ? 1500U : 800U)) b.Retreat(1);
			if (tick == (stress ? 1560U : 900U)) (void)b.Redeploy(1, true);
			if (tick == (stress ? 1800U : 1000U) && dual) b.Retreat(4);
			if (tick == 1200) (void)b.LoseHealth(0, enemies[1], 10000000);
			if (tick == (stress ? 1900U : 1600U)) b.Retreat(2);
			if (tick == (stress ? 1950U : 1650U)) (void)b.Redeploy(2, true);
			if (stress)
			{
				if (tick == 120) (void)b.LoseHealth(0, enemies[1], 1500000);
				if (tick % 73 == 0)
				{
					constexpr std::array<UnitId, 3> allies{2, 3, 1}; const auto target = allies[(tick / 73) % 3], n = (tick / 73) % 8;
					if (b.Unit(target).MyAlive && b.Unit(target).MyHealth > 400)
					{
						if (n == 6) (void)b.LoseHealth(enemies[2], target, 35);
						else if (n == 7) (void)b.DealElement(enemies[2], target, {.MyElement = Element::NEURAL, .MyAmount = 35});
						else (void)b.DealDamage(enemies[2], target, {.MyAmount = 35, .MyType = DamageType::TRUE_DAMAGE, .MySourceless = n == 5,
							.MyTags = n == 3 ? static_cast<DamageTags>(DamageTag::COUNTER) : n == 4 ? static_cast<DamageTags>(DamageTag::REFLECT) : DamageTags{}, .MyIsAttack = n == 2});
					}
				}
				if (tick % 89 == 0 && b.Unit(1).MyAlive)
				{
					const auto n = (tick / 89) % 4;
					(void)b.DealDamage(1, enemies[0], {.MyAmount = 130, .MyType = DamageType::PHYSICAL,
						.MyTags = n == 2 ? static_cast<DamageTags>(DamageTag::CHAIN) : DamageTags{}, .MyIsAttack = n != 3, .MyIsSplash = n == 1});
				}
				if (tick % 137 == 0)
				{
					(void)b.Heal(static_cast<UnitId>(allyCount), 2, 20); (void)b.Heal(static_cast<UnitId>(allyCount), 2, 7, {.MyRegen = true});
					(void)b.Heal(1, 1, 9, {.MySelf = true});
				}
				if (tick == 300) (void)b.AddBuff(enemies[0], {.MyKey = "test:dodge", .MyDuration = 2, .MyModifiers = std::vector<AttributeChange>{{Attribute::PHYSICAL_DODGE, 1}}});
				if (tick == 330) (void)b.ApplyStatus(1, CombatStatus::STUN, 2);
				if (tick == 660 || tick == 870)
				{
					StatusFlags flags; flags.set(static_cast<std::size_t>(tick == 660 ? CombatStatus::HEAL_FREE : CombatStatus::ISOLATED));
					(void)b.AddBuff(tick == 660 ? 2 : 3, {.MyKey = tick == 660 ? "test:noheal" : "test:isolated", .MyDuration = 3, .MyFlags = flags});
				}
			}
			if (mixed)
			{
				if (tick == 450) (void)b.LoseHealth(0, enemies[0], 10000000);
				if (tick == 600) (void)b.AddBuff(1, {.MyKey = "test:power", .MyDuration = 2, .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, 0.5}}});
				if (tick % 137 == 0) (void)b.Heal(static_cast<UnitId>(allyCount), 1, 100);
				if (tick % 193 == 0 && b.Unit(1).MyAlive && b.Unit(1).MyHealth > 500) (void)b.DealDamage(enemies[2], 1, {.MyAmount = 500, .MyType = DamageType::PHYSICAL, .MyIsAttack = true});
				if (dual && tick == 1000) b.Retreat(4);
				if (dual && tick == 1100) (void)b.Redeploy(4, true);
			}
			if (scene >= 6)
			{
				if (tick == 30 || tick == 900)
				{
					(void)b.DealElement(2, enemies[2], {.MyElement = Element::NECROSIS, .MyAmount = 5000});
					(void)b.DealElement(2, enemies[3], {.MyElement = Element::APOPTOSIS, .MyAmount = 5000});
				}
				if (tick % 101 == 0)
					for (const auto tags : {static_cast<DamageTags>(DamageTag::DOT), static_cast<DamageTags>(DamageTag::BURST), DamageTags{}})
						(void)b.DealDamage(2, enemies[2], {.MyAmount = 100, .MyType = DamageType::TRUE_DAMAGE, .MyCanDodge = false, .MyTags = tags});
				if (tick == 2000) b.Retreat(1, true);
				if (dual && tick == 2100) { (void)b.Redeploy(4, true); b.Retreat(4, true); }
				if ((scene == 8 || scene == 9) && (tick == 2050 || tick == 2200))
				{
					const std::array<UnitId, 1> targets{enemies[0]}; (void)b.ForceAttack(2, targets);
				}
			}
			if (scene >= 10)
			{
				if (tick == 200 || tick == 201) (void)b.ApplyStrongest(1, "protect", tick == 200 ? 1 : 3,
					{.MyValue = tick == 200 ? 0.5 : 0.1, .MyAttribute = Attribute::PHYSICAL_TAKEN_MULTIPLIER, .MyScale = -1, .MyOffset = 1,
						.MySecondAttribute = Attribute::ARTS_TAKEN_MULTIPLIER}, 2);
				if (tick == 205) for (const auto type : {DamageType::PHYSICAL, DamageType::ARTS, DamageType::TRUE_DAMAGE})
					(void)b.DealDamage(enemies[2], 1, {.MyAmount = 100, .MyType = type, .MyCanDodge = false});
				if (tick == 250) (void)b.Heal(1, 1, b.Unit(1).MyStats.MyMaxHealth, {.MySelf = true});
			}
			if (scene == 12 || scene == 13)
			{
				if (tick == 20 || tick == 600 || tick == 1990) b.Retreat(3);
				if ((tick >= 50 && tick <= 200 && tick % 30 == 20) || tick == 650 || tick == 680 || tick == 1995)
				{ b.Retreat(1); (void)b.Redeploy(1, true); }
				if (tick == 240 || tick == 720 || tick == 2200) (void)b.Redeploy(3, false);
			}
			if (scene == 14 || scene == 15)
			{
				if (tick == 1 || tick == 601)
					for (const auto ally : {UnitId{2}, UnitId{3}, static_cast<UnitId>(allyCount)}) { b.EndSkill(ally); b.SetSpTotal(ally, 0); }
				if (tick % 31 == 0 && b.Unit(1).MyAlive && std::isgreater(b.Unit(1).MyHealth, 300))
				{
					const auto n = (tick / 31) % 5;
					(void)b.DealDamage(enemies[2], 1, {.MyAmount = n == 4 ? 0.0 : 70.0, .MyType = n == 0 ? DamageType::PHYSICAL : n == 1 ? DamageType::ARTS : DamageType::TRUE_DAMAGE,
						.MyCanDodge = false, .MyNoSp = n == 3, .MyTags = static_cast<DamageTags>(DamageTag::DOT)});
				}
			}
			b.Step(); if (tick % 30 == 0) { std::cout << ','; Snapshot(b); }
		}
		std::cout << "]\n";
		if (!b.ContentErrors().empty()) throw std::runtime_error("operator content error");
	}
}

int main()
{
	try
	{
		std::cout << std::setprecision(17);
		std::string id, module; int skill; unsigned seed, scene;
		while (std::cin >> id >> skill >> module >> seed >> scene) Run(id, skill, module, seed, scene);
	}
	catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
