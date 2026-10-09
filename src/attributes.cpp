#include <limits>
#include <stdexcept>
#include <stronghold/simulation/attributes.hpp>

namespace Stronghold
{
	void AttributeModifiers::Add(std::span<const AttributeChange> _changes, unsigned _stacks, bool _permanent)
	{
		if (_stacks == 0 || _stacks > 1000000) throw std::invalid_argument("invalid modifier stack count");
		for (const auto& change : _changes)
			if (static_cast<std::size_t>(change.MyAttribute) >= MyValues.size() || !std::isfinite(change.MyValue))
				throw std::invalid_argument("invalid attribute modifier");
		for (const auto& change : _changes)
		{
			auto& value = MyValues[static_cast<std::size_t>(change.MyAttribute)];
			if (change.MyAttribute >= Attribute::ATTACK_MULTIPLIER)
				value *= _stacks == 1 ? change.MyValue : std::pow(change.MyValue, _stacks);
			else if ((change.MyAttribute == Attribute::PHYSICAL_DODGE || change.MyAttribute == Attribute::ARTS_DODGE) && change.MyValue > 0)
			{
				const auto index = change.MyAttribute == Attribute::PHYSICAL_DODGE ? 0U : 1U;
				const auto chance = std::min(1.0, change.MyValue);
				MyDodgeMiss[index] *= std::pow(1 - chance, _stacks);
				MyDodgeSingle[index] = chance;
				MyDodgeSources[index] = std::min(2U, MyDodgeSources[index] + std::min(2U, _stacks));
			}
			else
				value += change.MyValue * _stacks;
			if (_permanent && change.MyAttribute == Attribute::RANGE_EXTEND)
				MyPermanentRangeExtend += change.MyValue * _stacks;
		}
	}

	CombatStats ResolveStats(const CombatStats& _base, const AttributeModifiers& _modifiers)
	{
		const auto at = [&](Attribute _attribute) { return _modifiers.At(_attribute); };
		const auto finite = [](double _value, double _fallback) { return std::isfinite(_value) ? _value : _fallback; };
		const auto rounded = [&](double _value)
		{
			return static_cast<int>(std::clamp(std::floor(finite(_value, 0) + 0.5), 0.0, static_cast<double>(std::numeric_limits<int>::max())));
		};
		auto result = _base;
		result.MyMaxHealth = std::max(1.0, finite((_base.MyMaxHealth + at(Attribute::HEALTH_FLAT)) * std::max(0.0, 1 + at(Attribute::HEALTH_PERCENT)) * at(Attribute::HEALTH_MULTIPLIER), _base.MyMaxHealth));
		result.MyAttack = std::max(0.0, finite(((_base.MyAttack + at(Attribute::ATTACK_FLAT)) * std::max(0.0, 1 + at(Attribute::ATTACK_PERCENT)) + at(Attribute::ATTACK_FINAL)) * at(Attribute::ATTACK_MULTIPLIER), _base.MyAttack));
		result.MyDefense = std::max(0.0, finite((_base.MyDefense + at(Attribute::DEFENSE_FLAT)) * std::max(0.0, 1 + at(Attribute::DEFENSE_PERCENT)) * at(Attribute::DEFENSE_MULTIPLIER), _base.MyDefense));
		result.MyResistance = std::clamp(finite((_base.MyResistance + at(Attribute::RESISTANCE_FLAT)) * at(Attribute::RESISTANCE_MULTIPLIER), _base.MyResistance), 0.0, 100.0);
		result.MyAttackSpeed = std::clamp(finite(_base.MyAttackSpeed + at(Attribute::ATTACK_SPEED), 100), 20.0, 600.0);
		result.MyBaseAttackTime = finite(_base.MyBaseAttackTime * std::max(0.1, 1 + at(Attribute::ATTACK_TIME_PERCENT)), _base.MyBaseAttackTime);
		result.MyMoveSpeed = std::max(0.0, finite((_base.MyMoveSpeed + at(Attribute::MOVE_FLAT)) * at(Attribute::MOVE_MULTIPLIER), _base.MyMoveSpeed));
		result.MyBlockCount = rounded(_base.MyBlockCount + at(Attribute::BLOCK_COUNT));
		result.MyRangeExtend = rounded(at(Attribute::RANGE_EXTEND));
		result.MyPermanentRangeExtend = rounded(_modifiers.MyPermanentRangeExtend);
		result.MyBlockRadiusScale = std::max(0.0, finite(at(Attribute::BLOCK_RADIUS_SCALE), 0));
		result.MyMass = std::max(0.0, finite(_base.MyMass + at(Attribute::MASS_FLAT), 0));
		result.MyTaunt = _base.MyTaunt + at(Attribute::TAUNT);
		result.MyExtraTargets = at(Attribute::EXTRA_TARGETS);
		const auto dodge = [&](Attribute _attribute, std::size_t _index)
		{
			const auto sources = _modifiers.MyDodgeSources[_index];
			return std::clamp(at(_attribute) + (sources == 0 ? 0 : sources == 1 ? _modifiers.MyDodgeSingle[_index] : 1 - _modifiers.MyDodgeMiss[_index]), 0.0, 1.0);
		};
		result.MyPhysicalDodge = dodge(Attribute::PHYSICAL_DODGE, 0);
		result.MyArtsDodge = dodge(Attribute::ARTS_DODGE, 1);
		result.MyDefenseIgnoreFlat = at(Attribute::DEFENSE_IGNORE_FLAT);
		result.MyDefenseIgnorePercent = std::clamp(at(Attribute::DEFENSE_IGNORE_PERCENT), 0.0, 1.0);
		result.MyResistanceIgnoreFlat = at(Attribute::RESISTANCE_IGNORE_FLAT);
		result.MyResistanceIgnorePercent = std::clamp(at(Attribute::RESISTANCE_IGNORE_PERCENT), 0.0, 1.0);
		result.MyDamageDealtMultiplier = at(Attribute::DAMAGE_DEALT_MULTIPLIER);
		result.MyPhysicalDealtMultiplier = at(Attribute::PHYSICAL_DEALT_MULTIPLIER);
		result.MyArtsDealtMultiplier = at(Attribute::ARTS_DEALT_MULTIPLIER);
		result.MyDamageTakenMultiplier = at(Attribute::DAMAGE_TAKEN_MULTIPLIER);
		result.MyPhysicalTakenMultiplier = at(Attribute::PHYSICAL_TAKEN_MULTIPLIER);
		result.MyArtsTakenMultiplier = at(Attribute::ARTS_TAKEN_MULTIPLIER);
		result.MyTrueTakenMultiplier = at(Attribute::TRUE_TAKEN_MULTIPLIER);
		result.MyElementTakenMultiplier = at(Attribute::ELEMENT_TAKEN_MULTIPLIER);
		result.MyElementalTakenMultiplier = at(Attribute::ELEMENTAL_TAKEN_MULTIPLIER);
		result.MyHealingDealtMultiplier = at(Attribute::HEALING_DEALT_MULTIPLIER);
		result.MyHealingTakenMultiplier = at(Attribute::HEALING_TAKEN_MULTIPLIER);
		result.MyAttackScaleMultiplier = at(Attribute::ATTACK_SCALE_MULTIPLIER);
		result.MyRedeployMultiplier = at(Attribute::REDEPLOY_MULTIPLIER);
		result.MySpRecovery = std::max(0.0, finite((_base.MySpRecovery + at(Attribute::SP_RECOVERY_FLAT)) * at(Attribute::SP_RECOVERY_MULTIPLIER), 0));
		result.MySpCostFlat = at(Attribute::SP_COST_FLAT);
		result.MyHealthRegen = finite(_base.MyHealthRegen + at(Attribute::HEALTH_REGEN) + at(Attribute::HEALTH_REGEN_RATIO) * result.MyMaxHealth, 0);
		return result;
	}
}
