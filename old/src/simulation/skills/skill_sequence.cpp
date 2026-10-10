#include "skill_sequence.hpp"
#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void SkillSequence::Start(Battle& _battle, UnitId _unit, SkillSequenceDefinition _definition)
	{
		if (!std::ranges::is_sorted(_definition.MyHitTimes) || std::ranges::any_of(_definition.MyHitTimes, [](double _time)
			{ return !std::isfinite(_time) || std::isless(_time, 0); })) throw std::invalid_argument("invalid skill hit sequence");
		_MyDefinition = std::move(_definition);
		_MyElapsed = 0; _MyNext = 0; _MyTarget = 0;
		const auto& unit = _battle.Unit(_unit);
		_MyActivation = unit.MySkill.MyActivations; _MyDeployment = unit.MyDeploySequence;
		_MyTargets.reserve(_battle.Units().size());
	}

	UnitId SkillSequence::Target(Battle& _battle, UnitId _unit)
	{
		EffectExecutor::Select(_battle, {.MySource = _unit, .MyOwner = _battle.Unit(_unit).MyOwner}, _MyDefinition.MySelector, _MyTargets);
		if (_MyDefinition.MyRetainTarget && _MyTarget && std::ranges::find(_MyTargets, _MyTarget, &EffectTarget::MyUnit) != _MyTargets.end()) return _MyTarget;
		_MyTarget = _MyTargets.empty() ? 0 : _MyTargets.front().MyUnit;
		return _MyTarget;
	}

	void SkillSequence::Tick(Battle& _battle, UnitId _unit, double _delta)
	{
		const auto activation = _MyActivation, deployment = _MyDeployment;
		const auto current = [&]()
		{
			const auto& unit = _battle.Unit(_unit);
			return !_battle.Finished() && unit.MyAlive && unit.MySkill.MyActive &&
				unit.MySkill.MyActivations == activation && unit.MyDeploySequence == deployment;
		};
		if (!current()) return;
		_MyElapsed += _delta;
		if (_MyNext < _MyDefinition.MyHitTimes.size() && _MyDefinition.MyEndWhenEmpty && !Target(_battle, _unit))
		{
			_battle.EndSkill(_unit, SkillReason::NO_TARGET); return;
		}
		while (current() && _MyNext < _MyDefinition.MyHitTimes.size() && std::isgreaterequal(_MyElapsed + 1e-9, _MyDefinition.MyHitTimes[_MyNext]))
		{
			const auto target = Target(_battle, _unit);
			if (!target)
			{
				if (_MyDefinition.MyEndWhenEmpty) { _battle.EndSkill(_unit, SkillReason::NO_TARGET); return; }
				++_MyNext; continue;
			}
			const EffectContext context{.MySource = _unit, .MyOwner = _battle.Unit(_unit).MyOwner};
			const auto damage = _MyDefinition.MyDamage;
			const auto lastStatus = _MyDefinition.MyLastHitStatus;
			const bool last = ++_MyNext == _MyDefinition.MyHitTimes.size();
			EffectExecutor::Apply(_battle, context, {.MyUnit = target}, damage);
			if (!current()) return;
			if (last && lastStatus && _battle.Unit(target).MyAlive) EffectExecutor::Apply(_battle, context, {.MyUnit = target}, *lastStatus);
		}
	}

	void SkillSequence::Clear() noexcept
	{
		_MyNext = _MyDefinition.MyHitTimes.size(); _MyTarget = 0;
	}
}
