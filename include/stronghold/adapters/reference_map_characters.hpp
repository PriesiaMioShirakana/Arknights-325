#ifndef STRONGHOLD_ADAPTERS_REFERENCE_MAP_CHARACTERS_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_MAP_CHARACTERS_HPP
#include <stronghold/adapters/reference_ally.hpp>
#include <stronghold/adapters/reference_stage.hpp>

namespace Stronghold
{
	[[nodiscard]] CombatDefinition MakeMapMedicDefinition(const AllyRecord& _body);
	[[nodiscard]] std::vector<MapCharacterVariant> MakeMapCharacterVariants(const StageRecord& _stage);
}
#endif
