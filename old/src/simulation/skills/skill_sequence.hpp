#ifndef STRONGHOLD_SIMULATION_SKILL_SEQUENCE_HPP
#define STRONGHOLD_SIMULATION_SKILL_SEQUENCE_HPP
#include <stronghold/simulation/effects.hpp>

namespace Stronghold
{
	struct SkillSequenceDefinition
	{
		std::span<const double> MyHitTimes{};
		SelectorDefinition MySelector{};
		DamageOperation MyDamage{};
		std::optional<StatusOperation> MyLastHitStatus{};
		bool MyRetainTarget{true};
		bool MyEndWhenEmpty{true};
	};

	// 按技能自身时间执行独立命中，目标离开时重选；激活版本阻止回调重放旧技能。
	// 每个内置技能按值拥有此状态，配置中的只读 spans 由内容目录持有。
	class SkillSequence final
	{
	public:
		void Start(Battle& _battle, UnitId _unit, SkillSequenceDefinition _definition);
		void Tick(Battle& _battle, UnitId _unit, double _delta);
		void Clear() noexcept;

	private:
		[[nodiscard]] UnitId Target(Battle& _battle, UnitId _unit);
		SkillSequenceDefinition _MyDefinition{};
		std::vector<EffectTarget> _MyTargets;
		double _MyElapsed{};
		std::size_t _MyNext{};
		UnitId _MyTarget{};
		std::uint64_t _MyActivation{};
		std::uint64_t _MyDeployment{};
	};
}
#endif
