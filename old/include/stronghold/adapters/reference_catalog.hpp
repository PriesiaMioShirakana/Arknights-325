#ifndef STRONGHOLD_ADAPTERS_REFERENCE_CATALOG_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_CATALOG_HPP
#include <stronghold/domain/catalog.hpp>

namespace Stronghold
{
	// Generated at build time; no Node/Python dependency at runtime.
	[[nodiscard]] Catalog ReferenceCatalog();

	[[nodiscard]] const std::map<std::string, StageBoards, std::less<>>& ReferenceStages();

	[[nodiscard]] std::string_view ReferenceDataFingerprint() noexcept;
} // namespace Stronghold
#endif
