#ifndef STRONGHOLD_SIMULATION_ATTRIBUTES_HPP
#define STRONGHOLD_SIMULATION_ATTRIBUTES_HPP
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace Stronghold
{
	struct CombatStats
	{
		double MyMaxHealth{1};
		double MyAttack{};
		double MyDefense{};
		double MyResistance{};
		double MyAttackSpeed{100};
		double MyBaseAttackTime{1};
		double MyMoveSpeed{}; // Upstream movement attribute; tiles/s = attribute * 0.5.
		int MyBlockCount{};
		double MyTaunt{};
		double MyRedeploySeconds{30};
		double MyDeploymentCost{};

		double MySpRecovery{1};
		double MyHealthRegen{};
		double MyMass{};
		double MyElementalResistance{};
		double MyElementResistance{};
		double MyDefenseIgnorePercent{};
		double MyDefenseIgnoreFlat{};
		double MyResistanceIgnorePercent{};
		double MyResistanceIgnoreFlat{};
		double MyPhysicalDodge{};
		double MyArtsDodge{};
		double MyDamageDealtMultiplier{1};
		double MyPhysicalDealtMultiplier{1};
		double MyArtsDealtMultiplier{1};
		double MyDamageTakenMultiplier{1};
		double MyPhysicalTakenMultiplier{1};
		double MyArtsTakenMultiplier{1};
		double MyTrueTakenMultiplier{1};
		double MyElementTakenMultiplier{1};
		double MyElementalTakenMultiplier{1};
		double MyHealingDealtMultiplier{1};
		double MyHealingTakenMultiplier{1};
		double MyAttackScaleMultiplier{1};
		double MyRedeployMultiplier{1};
		double MySpCostFlat{};
		double MyExtraTargets{};
		int MyRangeExtend{};
		int MyPermanentRangeExtend{};
		double MyBlockRadiusScale{};

		[[nodiscard]] double AttackInterval() const noexcept
		{ return MyBaseAttackTime * 100 / std::clamp(MyAttackSpeed, 20.0, 600.0); }
	};


	enum class Attribute
	{
		ATTACK_FLAT,
		ATTACK_PERCENT,
		ATTACK_FINAL,
		DEFENSE_FLAT,
		DEFENSE_PERCENT,
		HEALTH_FLAT,
		HEALTH_PERCENT,
		RESISTANCE_FLAT,
		ATTACK_SPEED,
		ATTACK_TIME_PERCENT,
		BLOCK_COUNT,
		RANGE_EXTEND,
		DEFENSE_IGNORE_FLAT,
		DEFENSE_IGNORE_PERCENT,
		RESISTANCE_IGNORE_FLAT,
		RESISTANCE_IGNORE_PERCENT,
		PHYSICAL_DODGE,
		ARTS_DODGE,
		SP_RECOVERY_FLAT,
		EXTRA_TARGETS,
		TAUNT,
		HEALTH_REGEN,
		HEALTH_REGEN_RATIO,
		SP_COST_FLAT,
		MOVE_FLAT,
		MASS_FLAT,
		BLOCK_RADIUS_SCALE,
		ATTACK_MULTIPLIER,
		DEFENSE_MULTIPLIER,
		HEALTH_MULTIPLIER,
		RESISTANCE_MULTIPLIER,
		MOVE_MULTIPLIER,
		DAMAGE_DEALT_MULTIPLIER,
		DAMAGE_TAKEN_MULTIPLIER,
		PHYSICAL_TAKEN_MULTIPLIER,
		ARTS_TAKEN_MULTIPLIER,
		TRUE_TAKEN_MULTIPLIER,
		ELEMENT_TAKEN_MULTIPLIER,
		ELEMENTAL_TAKEN_MULTIPLIER,
		HEALING_DEALT_MULTIPLIER,
		HEALING_TAKEN_MULTIPLIER,
		SP_RECOVERY_MULTIPLIER,
		REDEPLOY_MULTIPLIER,
		ATTACK_SCALE_MULTIPLIER,
		PHYSICAL_DEALT_MULTIPLIER,
		ARTS_DEALT_MULTIPLIER,
		COUNT
	};

	struct AttributeChange
	{
		Attribute MyAttribute{};
		double MyValue{};
	};

	struct AttributeModifiers
	{
		std::array<double, static_cast<std::size_t>(Attribute::COUNT)> MyValues{};
		double MyPermanentRangeExtend{};
		std::array<double, 2> MyDodgeMiss{1, 1};
		std::array<double, 2> MyDodgeSingle{};
		std::array<unsigned, 2> MyDodgeSources{};

		AttributeModifiers() noexcept
		{
			std::fill(MyValues.begin() + static_cast<std::size_t>(Attribute::ATTACK_MULTIPLIER), MyValues.end(), 1);
		}

		[[nodiscard]] double At(Attribute _attribute) const noexcept
		{ return MyValues[static_cast<std::size_t>(_attribute)]; }

		void Add(std::span<const AttributeChange> _changes, unsigned _stacks = 1, bool _permanent = false);
	};

	[[nodiscard]] CombatStats ResolveStats(const CombatStats& _base, const AttributeModifiers& _modifiers);
}
#endif
