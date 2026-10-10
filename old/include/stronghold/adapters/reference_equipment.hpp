#ifndef STRONGHOLD_ADAPTERS_REFERENCE_EQUIPMENT_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_EQUIPMENT_HPP
#include <stronghold/adapters/reference_combat.hpp>

namespace Stronghold
{
	using EquipmentStatRule = EquipmentStatTemplate;

	[[nodiscard]] std::span<const EquipmentStatRule> ReferenceEquipmentStats() noexcept;
	// 仅追加已移植的常驻属性，装备的事件／套装处理器单独安装。
	void AppendEquipmentStats(CombatDefinition& _definition, std::span<const std::string> _items);

	using EquipmentEffectRule = EquipmentEffectTemplate;

	[[nodiscard]] std::span<const EquipmentEffectRule> ReferenceEquipmentEffects() noexcept;
	[[nodiscard]] std::span<const EquipmentTemplate> ReferenceEquipmentTemplates() noexcept;
	void AppendEquipmentEffects(CombatDefinition& _definition, std::span<const std::string> _items);
}
#endif
