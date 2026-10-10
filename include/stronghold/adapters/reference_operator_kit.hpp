#ifndef STRONGHOLD_ADAPTERS_REFERENCE_OPERATOR_KIT_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_OPERATOR_KIT_HPP
#include <stronghold/adapters/reference_generic_skill.hpp>
#include <stronghold/simulation/operator_kits.hpp>

namespace Stronghold
{
	struct OperatorKitRecord
	{
		OperatorKitDefinition MyRules{};
		const GenericSkillRecord* MySkill{}; // 无覆盖时继续使用所选技能的通用回退。
		std::optional<TargetPriority> MyPriority{};
		bool MyChainNoFalloff{};
		std::optional<double> MySluggish{};
		bool MyHealing{};
		std::optional<double> MyBaseSluggish{};
		std::optional<TargetPriority> MyBasePriority{};
		bool MySkillNoHeal{};
		std::span<const GenericTalentRecord> MyTalents{};
		std::span<const RangeOffset> MyBaseRange{};
	};
}
#endif
