#ifndef STRONGHOLD_SIMULATION_BUILTIN_SKILL_HPP
#define STRONGHOLD_SIMULATION_BUILTIN_SKILL_HPP
#include <stronghold/simulation/skill.hpp>

namespace Stronghold
{
	class BuiltinSkill final : public SkillBase
	{
	public:
		using SkillBase::SkillBase;

	protected:
		bool CanActivate(SkillReason _reason) const override;
		void BeforeStart(SkillReason _reason) override;
		void OnStart(SkillReason _reason) override;
		void AfterStart(SkillReason _reason) override;
	};
}
#endif
