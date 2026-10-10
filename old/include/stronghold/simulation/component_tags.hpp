#ifndef STRONGHOLD_SIMULATION_COMPONENT_TAGS_HPP
#define STRONGHOLD_SIMULATION_COMPONENT_TAGS_HPP
#include <cstdint>

namespace Stronghold
{
	enum class ComponentKind { BUILTIN, CUSTOM_SKILL, CUSTOM_OPERATOR, CUSTOM_SELECTOR, CUSTOM_OPERATION, BUFF, MECHANISM, CUSTOM_UNIT, CUSTOM_ENEMY };

	struct ComponentReference
	{
		ComponentKind MyKind{ComponentKind::BUILTIN};
		std::uint32_t MyRegistration{};
	};
}
#endif
