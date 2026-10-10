#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::RunBuffProgram(UnitId _source, UnitId _target, const EffectProgram* _program, double _delta)
	{
		if (!_program) return;
		const std::array targets{_target};
		EffectExecutor::Execute(_MyView, {.MySource = _source, .MyOwner = Unit(_target).MyOwner,
			.MyEventUnit = _target, .MyEventTarget = _target, .MyProvided = targets, .MyDelta = _delta}, *_program);
	}
}
