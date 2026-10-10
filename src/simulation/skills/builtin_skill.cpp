#include "battle_core.hpp"

namespace Stronghold
{
	bool BuiltinSkill::CanActivate(SkillReason) const
	{
		const auto& unit = Core().Unit(Unit());
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<PapyrsKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		return !kit || !kit->MyLockSkill || Core().PapyrsTarget(Unit());
	}

	void BuiltinSkill::BeforeStart(SkillReason)
	{
		Core().DiyOperatorBeforeStart(Unit());
	}

	void BuiltinSkill::OnStart(SkillReason _reason)
	{
		if (_reason == SkillReason::PASSIVE) return;
		const auto& unit = Core().Unit(Unit());
		if (const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<Texas2Kit>(unit.MyDefinition.MyOperatorKit) : nullptr)
			Core().Texas2Start(Unit(), *kit, _reason);
	}

	void BuiltinSkill::AfterStart(SkillReason)
	{
		const auto& unit = Core().Unit(Unit());
		if (unit.MyDefinition.MyOperatorKit && std::holds_alternative<Texas2Kit>(*unit.MyDefinition.MyOperatorKit)) Core().Texas2FinishStart(Unit());
	}
}
