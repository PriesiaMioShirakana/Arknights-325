#ifndef STRONGHOLD_DOMAIN_PREPARATION_BONDS_HPP
#define STRONGHOLD_DOMAIN_PREPARATION_BONDS_HPP
#include <cstdint>
#include <span>
#include <string_view>

namespace Stronghold
{
	enum class PreparationBondKind { PREP_LAYERS, COIN_MILESTONE, DISCOUNT, REFRESH_CHANCE, ITEM_MILESTONE };

	struct PreparationBondEffect
	{
		std::string_view MyBond{};
		PreparationBondKind MyKind{};
		std::int64_t MyStep{};
		std::int64_t MyCount{};
		std::int64_t MyHighCount{};
		std::int64_t MyHighStep{};
		double MyBaseChance{};
		double MyChancePerLayer{};
		std::string_view MyPool{};
		std::string_view MyDiscountBond{};
	};
}
#endif
