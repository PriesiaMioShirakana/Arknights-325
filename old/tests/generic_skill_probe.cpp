#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_ally.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	template <class T> void Optional(const std::optional<T>& value)
	{
		if (!value) std::cout << "null";
		else if constexpr (std::is_enum_v<T>) std::cout << static_cast<unsigned>(*value);
		else std::cout << *value;
	}

	void Grid(std::span<const RangeOffset> grid)
	{
		std::cout << '[';
		for (std::size_t i = 0; i < grid.size(); ++i) { if (i) std::cout << ','; std::cout << '[' << grid[i].MyRow << ',' << grid[i].MyColumn << ']'; }
		std::cout << ']';
	}

	void Mods(std::span<const AttributeChange> mods)
	{
		std::cout << '[';
		for (std::size_t i = 0; i < mods.size(); ++i) { if (i) std::cout << ','; std::cout << '[' << static_cast<unsigned>(mods[i].MyAttribute) << ',' << mods[i].MyValue << ']'; }
		std::cout << ']';
	}

	void Config(const AllyRecord& body)
	{
		if (!body.MyGenericSkill) std::cout << "null";
		else
		{
			const auto& s = *body.MyGenericSkill;
			std::cout << '[' << static_cast<unsigned>(s.MyKind) << ','; Optional(s.MyDuration); std::cout << ',' << s.MyAmmo << ',' << s.MyActivateOnDeploy << ',';
			Optional(s.MyTrigger); std::cout << ','; Mods(s.MyModifiers); std::cout << ','; Grid(s.MyRange);
			std::cout << ',' << s.MyRangeExtend << ','; Optional(s.MyMaxTargets); std::cout << ',' << s.MyHasAttack << ',';
			Optional(s.MyDamageType); std::cout << ','; Optional(s.MyAttackScale); std::cout << ','; Optional(s.MyHealScale); std::cout << ','; Optional(s.MyHits);
			std::cout << ','; Optional(s.MySplashRadius); std::cout << ',' << s.MyNoAttack << ',' << s.MyOnHit << ',';
			const auto definition = body.MakeGenericDefinition(); const auto& r = definition.MySkill;
			std::cout << '[' << static_cast<unsigned>(r.MyKind) << ',' << static_cast<unsigned>(r.MySpType) << ',' << static_cast<unsigned>(r.MyTrigger) << ',' << r.MySpCost << ','
				<< r.MyInitialSp << ',' << r.MyMaxCharges << ',' << r.MyDuration << ',' << r.MyAmmo << ',' << r.MyManual << ',' << r.MyActivateOnDeploy << ',' << r.MyHealSkill << ',' << r.MyTriggerAllies << ',';
			Grid(r.MyTriggerRange); std::cout << "]]";
		}
		std::cout << ",[";
		for (std::size_t i = 0; i < body.MyGenericTalents.size(); ++i)
		{
			if (i) std::cout << ',';
			std::cout << '[' << std::quoted(std::string(body.MyGenericTalents[i].MyKey)) << ','; Mods(body.MyGenericTalents[i].MyModifiers); std::cout << ']';
		}
		std::cout << ']';
	}

	void Configs()
	{
		for (const auto& op : ReferenceOperators())
		{
			for (const auto& l : op.MyLoadouts)
			{
				std::cout << "[\"chess\"," << std::quoted(std::string(op.MyId)) << ',' << l.MySkillIndex << ',' << std::quoted(std::string(l.MyModuleId)) << ',';
				Config(l.MyBody); std::cout << "]\n";
			}
			if (op.MyStandIn)
			{
				std::cout << "[\"stand\"," << std::quoted(std::string(op.MyId)) << ','; Config(op.MyStandIn->MyBody); std::cout << "]\n";
			}
			for (const auto& d : op.MyDiyChoices)
			{
				std::cout << "[\"diy\"," << std::quoted(std::string(op.MyId)) << ',' << std::quoted(std::string(d.MyCharacterId)) << ',' << d.MySkillIndex << ',' << std::quoted(std::string(d.MyModuleId)) << ',';
				Config(d.MyLoadout.MyBody); std::cout << "]\n";
			}
		}
		for (const auto& token : ReferenceTokens())
		{
			for (const auto& variant : token.MyVariants)
			{
				std::cout << "[\"token\"," << std::quoted(std::string(token.MyId)) << ',' << std::quoted(std::string(variant.MyOwnerId)) << ',' << variant.MySkillIndex << ',' << std::quoted(std::string(variant.MyModuleId)) << ',';
				Config(variant.MyBody); std::cout << "]\n";
			}
			for (const auto& variant : token.MyDiyVariants)
			{
				std::cout << "[\"diytoken\"," << std::quoted(std::string(token.MyId)) << ',' << std::quoted(std::string(variant.MyOwnerId)) << ',' << variant.MySkillIndex << ',' << std::quoted(std::string(variant.MyModuleId)) << ',';
				Config(variant.MyBody); std::cout << "]\n";
			}
			std::cout << "[\"unowned\"," << std::quoted(std::string(token.MyId)) << ','; Config(token.MyUnowned.MyBody); std::cout << "]\n";
		}
	}

	void Snapshot(const Battle& b)
	{
		std::cout << '[' << b.RandomState() << ',' << b.Players()[0].MyDp << ",[";
		for (std::size_t i = 0; i < b.Units().size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& u = b.Units()[i]; const auto& s = u.MyStats;
			std::cout << '[' << u.MyHealth << ',' << u.MyAlive << ',' << u.MyPosition.MyX << ',' << u.MyPosition.MyY << ',' << s.MyAttack << ',' << s.MyDefense << ',' << s.MyMaxHealth << ','
				<< s.MyResistance << ',' << s.MyAttackSpeed << ',' << s.MyBaseAttackTime << ',' << u.MySkill.MyActive << ',' << u.MySkill.MyPending << ',' << b.SpTotal(u.MyId) << ',' << u.MySkill.MyAmmoLeft << ',';
			if (std::isfinite(u.MySkill.MyTimeLeft)) std::cout << u.MySkill.MyTimeLeft; else std::cout << "null";
			std::cout << ',' << u.MySkill.MyActivations << ",[";
			for (std::size_t j = 0; j < 8; ++j) { if (j) std::cout << ','; std::cout << u.MyStatuses.MyRemaining[j]; }
			std::cout << "],[";
			for (std::size_t j = 0; j < u.MyElements.MyGauges.size(); ++j) { if (j) std::cout << ','; std::cout << u.MyElements.MyGauges[j]; }
			double shield = 0; for (const auto& buff : u.MyBuffs) shield += buff.MyDefinition.MyShield.MyHealth;
			std::cout << "]," << shield << ',' << u.MyAttackCooldown << ',' << u.MyTotals.MyAttacks << ']';
		}
		std::cout << "]]";
	}

	void Run(std::string_view id, int si, std::string_view module, unsigned seed, unsigned scene)
	{
		const auto& body = ReferenceOperator(id).Loadout(si, module == "-" ? "" : module).MyBody;
		auto def = body.MakeGenericDefinition();
		if (scene < 2)
		{
			def.MyStats = {.MyMaxHealth = 20000, .MyAttack = 100, .MyDefense = 50, .MyResistance = 10, .MyBlockCount = 0, .MyRedeploySeconds = 100};
			def.MyProfession = {}; def.MyRange = {{0, 0}, {0, 1}, {0, 2}, {1, 1}, {-1, 1}};
			def.MyAttack = {.MyDamageType = DamageType::PHYSICAL, .MyDisabled = true, .MyCanHitFlying = true, .MyMaxTargets = 1};
			if (scene == 1) { def.MyAttack.MyRanged = true; def.MyAttack.MyProjectileSpeed = 8; }
			// 普通职业行为另有独立差分；此处仅保留通用技能对标准攻击配置的覆盖。
			if (def.MySkill.MyAttack)
			{
				const auto& spec = *body.MyGenericSkill;
				auto a = def.MyAttack;
				a.MyDamageType = spec.MyDamageType.value_or(DamageType::PHYSICAL); a.MyAttackScale = spec.MyAttackScale.value_or(1);
				a.MyHealScale = spec.MyHealScale.value_or(1); a.MyHits = spec.MyHits.value_or(1); a.MyMaxTargets = spec.MyMaxTargets.value_or(1);
				a.MySplashRadius = spec.MySplashRadius.value_or(0); a.MyGenericHit = spec.MyOnHit;
				if (scene == 1) { a.MyRanged = true; a.MyProjectileSpeed = 8; }
				def.MySkill.MyAttack = a;
			}
		}
		def.MySkill.MyTrigger = SkillTrigger::NEVER; def.MySkill.MyManual = false;
		def.MySkill.MyTriggerAllies = false;
		BattleInput input{.MyPlayers = {{.MyPlayerId = "p", .MyUnits = {{.MyPieceUid = 1, .MyDefinition = def, .MyPosition = {5, 9}}}}}, .MyAutoFinish = false, .MySeed = seed};
		for (unsigned i = 0; i < 3; ++i)
		{
			CombatDefinition ally{.MyId = "ally", .MyStats = {.MyMaxHealth = 20000, .MyAttack = 100, .MyBlockCount = 0}, .MyAttack = {.MyDisabled = true}};
			input.MyPlayers[0].MyUnits.push_back({.MyPieceUid = i + 2, .MyDefinition = std::move(ally), .MyPosition = {i == 0 ? 6.0 : 4.0, 8.0 + i}});
		}
		Battle b(std::move(input)); b.Start();
		for (unsigned i = 0; i < 4; ++i)
		{
			CombatDefinition enemy{.MyId = "enemy", .MyStats = {.MyMaxHealth = 100000, .MyAttack = 10, .MyDefense = 20, .MyResistance = 10, .MyMoveSpeed = 0, .MyMass = 1}, .MyAttack = {.MyDisabled = true}};
			(void)b.SpawnEnemy({.MyOwnerId = "p", .MyDefinition = std::move(enemy), .MyRoute = {.MyStart = {6.0 + (i % 2), 9.0 + (i / 2)}, .MyEnd = {0, 9}}});
		}
		std::cout << '['; Snapshot(b);
		const auto enemyId = std::ranges::find(b.Units(), UnitSide::ENEMY, &CombatUnit::MySide)->MyId;
		for (unsigned tick = 0; tick < 360; ++tick)
		{
			if (tick % 30 == 0)
			{
				for (std::size_t i = 0; i < b.Units().size(); ++i)
					if (const auto& ally = b.Units()[i]; ally.MySide == UnitSide::ALLY) (void)b.LoseHealth(0, ally.MyId, 500);
				b.SetSpTotal(1, 1000); (void)b.ActivateSkill(1);
			}
			if (tick % 17 == 0) { const std::array<UnitId, 1> targets{enemyId}; (void)b.ForceAttack(1, targets); }
			if (tick % 11 == 0) (void)b.DealDamage(enemyId, 1, {.MyAmount = 20, .MyType = DamageType::TRUE_DAMAGE, .MyIsAttack = true});
			if (tick == 90) b.EndSkill(1);
			if (tick == 160) b.Retreat(1);
			if (tick == 170) (void)b.Redeploy(1, true);
			b.Step(); if (tick % 10 == 0 || tick == 90 || tick == 160 || tick == 170) { std::cout << ','; Snapshot(b); }
		}
		std::cout << "]\n";
		if (!b.ContentErrors().empty()) throw std::runtime_error("generic content error");
	}
}

int main(int argc, char** argv)
{
	std::cout << std::setprecision(17);
	if (argc > 1 && std::string_view(argv[1]) == "config") { Configs(); return 0; }
	for (std::string id, module; std::cin >> id;)
	{
		int si; unsigned seed, scene; std::cin >> si >> module >> seed >> scene;
		Run(id, si, module, seed, scene);
	}
}
