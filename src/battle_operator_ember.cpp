#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::SurtrFatal(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::FATAL || !_event.MyUnit || _event.MyPrevented) return;
		auto& unit = _MyUnits[Index(_event.MyUnit)];
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<SurtrKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || unit.MyOperatorHooksReleased) return;
		_event.MyPrevented = true;
		if (unit.MySurtrEmber) return;
		unit.MySurtrEmber = true;
		StatusFlags flags; flags.set(static_cast<std::size_t>(CombatStatus::NO_HEAL)); flags.set(static_cast<std::size_t>(CombatStatus::HEAL_FREE));
		(void)AddBuff(unit.MyId, {.MyKey = "surtr:ember", .MyFlags = flags, .MyStatus = CombatStatus::HEAL_FREE});
		Schedule({.MyAt = Time() + kit->MyEmberDuration, .MyKind = ScheduledKind::SURTR_RETREAT, .MySource = unit.MyId, .MyVersion = unit.MyDeploySequence});
	}

	void Battle::SurtrSkill(UnitId _unit, const SurtrKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY) unit.MySurtrEmber = false;
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _kit.MySkill == SurtrSkillKind::GIANT && unit.MySurtrSolo && unit.MySkill.MyActive && _event.MyDamage.MyIsAttack && !_event.MyDamage.MyIsSplash)
			_event.MyDamage.MyAmount *= _kit.MySoloScale;
		if (_kit.MySkill != SurtrSkillKind::TWILIGHT) return;
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			unit.MySurtrTime = 0; unit.MySurtrAccumulator = 0;
			(void)Heal(_unit, _unit, unit.MyStats.MyMaxHealth, {.MySelf = true, .MyIgnoreHealFree = true});
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK && std::isgreater(_kit.MyPeakLoss, 0))
		{
			unit.MySurtrTime += _event.MyDelta;
			PeriodicHealthLoss(_unit, unit.MySurtrAccumulator, _event.MyDelta, _kit.MyInterval, _kit.MyPeakLoss * std::min(1.0, unit.MySurtrTime / _kit.MyRamp), true);
		}
	}
}
