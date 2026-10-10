#ifndef STRONGHOLD_SIMULATION_BUILTIN_SKILL_HPP
#define STRONGHOLD_SIMULATION_BUILTIN_SKILL_HPP
#include <stronghold/simulation/skill.hpp>
#include "skill_sequence.hpp"

namespace Stronghold
{
	class BuiltinSkill final : public SkillBase
	{
	public:
		using SkillBase::SkillBase;
		void TickBuiltin(double _delta);

	protected:
		bool CanActivate(SkillReason _reason) const override;
		void BeforeStart(SkillReason _reason) override;
		void OnStart(SkillReason _reason) override;
		void AfterStart(SkillReason _reason) override;
		void OnEnding(SkillReason _reason) override;

	private:
		SkillSequence _MySequence;
		mutable std::vector<EffectTarget> _MyCandidates;
		std::uint64_t _MyCastDeployment{};
		unsigned _MyCasts{};
	};
}
#endif
