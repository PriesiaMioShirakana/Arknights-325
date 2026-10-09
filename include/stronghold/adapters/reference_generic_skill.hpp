#ifndef STRONGHOLD_ADAPTERS_REFERENCE_GENERIC_SKILL_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_GENERIC_SKILL_HPP
#include <stronghold/simulation/combat_types.hpp>

namespace Stronghold
{
	struct GenericSkillRecord
	{
		SkillKind MyKind{};
		std::optional<double> MyDuration{};
		double MyAmmo{};
		bool MyActivateOnDeploy{};
		std::optional<SkillTrigger> MyTrigger{};
		std::span<const AttributeChange> MyModifiers{};
		std::span<const RangeOffset> MyRange{};
		int MyRangeExtend{};
		std::optional<unsigned> MyMaxTargets{};
		bool MyHasAttack{};
		std::optional<DamageType> MyDamageType{};
		std::optional<double> MyAttackScale{};
		std::optional<double> MyHealScale{};
		std::optional<unsigned> MyHits{};
		std::optional<double> MySplashRadius{};
		bool MyNoAttack{};
		bool MyOnHit{};
		GenericSkillEffects MyEffects{};
	};

	struct GenericTalentRecord
	{
		std::string_view MyKey{};
		std::span<const AttributeChange> MyModifiers{};
	};
}
#endif
