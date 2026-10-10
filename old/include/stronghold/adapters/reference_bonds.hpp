#ifndef STRONGHOLD_ADAPTERS_REFERENCE_BONDS_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_BONDS_HPP
#include <stronghold/domain/bonds.hpp>
#include <stronghold/simulation/bond_effects.hpp>

namespace Stronghold
{
	struct BondRosterRecord
	{
		std::string_view MyId{};
		std::string_view MyBaseId{};
		bool MyGolden{};
		std::span<const std::string_view> MyBonds{};
	};

	struct BondItemRecord
	{
		std::string_view MyId{};
		BondItem MyItem{};
	};

	[[nodiscard]] std::span<const CoreBondEffect> ReferenceCoreBondEffects() noexcept;
	[[nodiscard]] std::span<const AddonBondEffect> ReferenceAddonBondEffects() noexcept;

	// 全部 span 和字符串具有静态生命周期。盟约按 identifier/ID 顺序，其余表按 ID 排序。
	[[nodiscard]] std::span<const BondRule> ReferenceBondRules() noexcept;
	[[nodiscard]] std::span<const BondRosterRecord> ReferenceBondRoster() noexcept;
	[[nodiscard]] std::span<const BondItemRecord> ReferenceBondItems() noexcept;
}
#endif
