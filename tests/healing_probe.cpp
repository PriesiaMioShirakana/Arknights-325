#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;
	struct HealHandler final : CustomBond<HealHandler>
	{
		void OnBeforeHeal(Battle&, ContentEvent& _event) { _event.MyAmount *= 0.5; }
	};
}

// 比较治疗倍率、再生忽略倍率/回调改量、禁疗、自愈、穿透与溢出盾的完整时间序列。
int main()
{
	using namespace Stronghold;
	ContentRegistry registry;
	const auto handler = registry.Register<HealHandler>("healing"); registry.Seal();
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 6; ++scene)
	{
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {
			AllyDeployment{.MyPieceUid = 1, .MyDefinition = CombatDefinition{.MyId = "source", .MyStats = CombatStats{.MyMaxHealth = 1000}, .MyAttack = AttackProfile{.MyDisabled = true}}, .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}},
			AllyDeployment{.MyPieceUid = 2, .MyDefinition = CombatDefinition{.MyId = "target", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyHealthRegen = 7.5}, .MyAttack = AttackProfile{.MyDisabled = true, .MyNoHeal = scene == 5}}, .MyPosition = WorldPoint{.MyX = 6, .MyY = 9}}}}},
			.MyAutoFinish = false, .MyContentRegistry = std::cref(registry), .MyContentBindings = {ContentBinding{.MyContent = handler, .MyPlayerId = "one"}}};
		Battle battle(std::move(input)); battle.Start();
		(void)battle.LoseHealth(0, 2, 500);
		(void)battle.AddBuff(1, BuffDefinition{.MyKey = "dealt", .MyModifiers = std::vector<AttributeChange>{AttributeChange{.MyAttribute = Attribute::HEALING_DEALT_MULTIPLIER, .MyValue = 2}}});
		(void)battle.AddBuff(2, BuffDefinition{.MyKey = "taken", .MyModifiers = std::vector<AttributeChange>{AttributeChange{.MyAttribute = Attribute::HEALING_TAKEN_MULTIPLIER, .MyValue = 0.3}}});
		if (scene) std::cout << ',';
		std::cout << '[';
		for (unsigned tick = 0; tick < 180; ++tick)
		{
			if (tick == 20) (void)battle.ApplyStatus(2, CombatStatus::HEAL_FREE, 2);
			if (tick == 30) (void)battle.ApplyStatus(2, CombatStatus::NO_HEAL, 1);
			if (tick == 100 || tick == 150) (void)battle.DealDamage(0, 2, 300, DamageType::TRUE_DAMAGE);
			double healed = 0;
			if (tick % 13 == 0) healed = battle.Heal(1, 2, 200, HealOptions{.MySelf = scene == 2, .MyRegen = scene == 1,
				.MyIgnoreHealFree = scene == 4, .MyThrough = scene == 3, .MyOverheal = true, .MyOverhealDuration = tick == 169 ? 0.0 : 0.8});
			battle.Step();
			double shield = 0;
			for (const auto& buff : battle.Unit(2).MyBuffs) shield += buff.MyDefinition.MyShield.MyHealth;
			if (tick) std::cout << ',';
			std::cout << '[' << healed << ',' << battle.Unit(2).MyHealth << ',' << shield << ',' << battle.Unit(1).MyTotals.MyHealing << ',' << battle.Unit(2).MyTotals.MyHealing << ']';
		}
		if (!battle.ContentErrors().empty()) return 1;
		std::cout << ']';
	}
	std::cout << ']';
}
