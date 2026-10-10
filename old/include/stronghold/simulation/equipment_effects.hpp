#ifndef STRONGHOLD_SIMULATION_EQUIPMENT_EFFECTS_HPP
#define STRONGHOLD_SIMULATION_EQUIPMENT_EFFECTS_HPP
#include <span>
#include <string>
#include <string_view>
#include <stronghold/simulation/attributes.hpp>

namespace Stronghold
{
	enum class EquipmentEffectKind
	{
		DISTANCE_DAMAGE, SKILL_ATTACK_STACK, DEPLOY_BOND_SP, FRONT_HEALTH, DEPLOY_STUN,
		ATTACK_SPEED_STACK, FIRST_DAMAGE_STEALTH, BLOCK_DAMAGE, ATTACK_SELF_HEAL,
		DECAY_DAMAGE, AMMO_REFILL, HEALTH_CONTROL_IMMUNITY, HEAL_SHIELD, REVIVE,
		ATTACK_PALSY, ATTACK_COLD, WEAKNESS, SOLVENT, ARCANE_SILENCE, SIDE_ATTACK_SPEED,
		FLAT_DAMAGE_REDUCTION, TRENCH_COUNTER, PARTNER_HEAL, PARTNER_TRUE_DAMAGE, EXTRA_BULLET,
		GAINED_ATTACK_SPEED, COLD_DAMAGE, SKILL_COMPASS, STEALTH_CHARGE, KNIGHT_CREED, HAMMER_BURN, HAMMER_UNDYING, HAMMER_SPEED, HAMMER_TREMBLE, STEAM_HEART
	};

	struct EquipmentParameters
	{
		EquipmentEffectKind MyKind{};
		double MyValue{};
		double MyExtra{};
		double MyProbability{1};
		double MyDuration{};
		double MyInterval{};
		double MyThreshold{};
		unsigned MyMaximum{};
	};

	struct EquipmentEffect
	{
		std::string MyKey{};
		EquipmentParameters MyParameters{};
		std::string MyPartner{};
	};
	struct EquipmentStatTemplate
	{
		std::string_view MyItem{};
		std::string_view MyBuff{};
		std::span<const AttributeChange> MyModifiers{};
	};

	struct EquipmentEffectTemplate
	{
		std::string_view MyItem{};
		EquipmentParameters MyParameters{};
		std::string_view MyPartner{};
	};

	struct EquipmentTemplate
	{
		std::string_view MyId{};
		unsigned MyTier{};
		std::span<const EquipmentStatTemplate> MyStats{};
		std::span<const EquipmentEffectTemplate> MyEffects{};
	};
}
#endif
