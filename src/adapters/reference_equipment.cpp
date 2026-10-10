#include <stronghold/adapters/reference_equipment.hpp>

namespace Stronghold
{
	void AppendEquipmentEffects(CombatDefinition& _definition, std::span<const std::string> _items)
	{
		AppendEquipmentStats(_definition, _items);
		const auto rules = ReferenceEquipmentEffects();
		_definition.MyEquipmentEffects.reserve(_definition.MyEquipmentEffects.size() + _items.size());
		for (const auto& item : _items)
		{
			auto rule = std::ranges::lower_bound(rules, item, {}, &EquipmentEffectRule::MyItem);
			for (; rule != rules.end() && rule->MyItem == item; ++rule)
				_definition.MyEquipmentEffects.push_back({"item:" + item, rule->MyParameters, std::string(rule->MyPartner)});
		}
	}

	void AppendEquipmentStats(CombatDefinition& _definition, std::span<const std::string> _items)
	{
		const auto rules = ReferenceEquipmentStats();
		_definition.MyInitialBuffs.reserve(_definition.MyInitialBuffs.size() + _items.size() * 2);
		for (const auto& item : _items)
		{
			auto rule = std::ranges::lower_bound(rules, item, {}, &EquipmentStatRule::MyItem);
			for (; rule != rules.end() && rule->MyItem == item; ++rule)
				_definition.MyInitialBuffs.push_back(BuffDefinition{.MyKey = "item:" + item + ":stat:" + std::string(rule->MyBuff),
					.MyModifiers = std::vector<AttributeChange>(rule->MyModifiers.begin(), rule->MyModifiers.end()), .MyPersistent = true, .MyAllowDead = true});
		}
	}
}
