#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::TrackDeferredDamage(CombatUnit& _unit, std::uint64_t _sequence)
	{
		if (_unit.MyDeferredHitTick != Tick()) { _unit.MyDeferredHits.clear(); _unit.MyDeferredHitTick = Tick(); }
		_unit.MyDeferredHits.push_back(_sequence);
	}

	bool Battle::ConsumeDeferredDamage(CombatUnit& _unit, std::uint64_t _sequence)
	{
		const auto found = std::ranges::find(_unit.MyDeferredHits, _sequence);
		if (found == _unit.MyDeferredHits.end()) return false;
		_unit.MyDeferredHits.erase(found);
		return true;
	}

	void Battle::RosesaObserve(ContentEvent& _event)
	{
		if (!_event.MyTarget || (_event.MyKind != ContentEventKind::BEFORE_DAMAGE && _event.MyKind != ContentEventKind::DAMAGED)) return;
		const auto& target = Unit(_event.MyTarget);
		for (std::size_t i = 0, count = _MyRosesas.size(); i < count; ++i)
		{
			auto& unit = _MyUnits[Index(_MyRosesas[i])];
			if (unit.MyOperatorHooksReleased) continue;
			const auto& kit = std::get<RosesaKit>(*unit.MyDefinition.MyOperatorKit);
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE)
			{
				if (!unit.MyAlive || !unit.MySkill.MyActive || unit.MyDefinition.MySkill.MyKind == SkillKind::PASSIVE ||
					target.MySide != UnitSide::ALLY || target.MyKind != UnitKind::OPERATOR ||
					(_event.MyDamage.MyType != DamageType::PHYSICAL && _event.MyDamage.MyType != DamageType::ARTS) || !InRuleRange(unit.MyId, target.MyId)) continue;
				TrackDeferredDamage(unit, _event.MyDamage.MySequence);
				_event.MyDamage.MyMultiplier *= kit.MyDamageScale;
			}
			else
			{
				if (!std::isgreater(_event.MyAmount, 0) || !target.MyAlive || !ConsumeDeferredDamage(unit, _event.MyDamage.MySequence)) continue;
				const auto total = _event.MyAmount * (1 - kit.MyDamageScale) / std::max(1e-6, kit.MyDamageScale);
				const auto perTick = total / std::max(1.0, std::floor(kit.MyDelayDuration / kit.MyDelayInterval + 0.5));
				const auto source = _event.MySource && Unit(_event.MySource).MySide == UnitSide::ENEMY ? _event.MySource : 0;
				(void)AddBuff(target.MyId, {.MyKey = "rosesa:dot", .MySource = source, .MyDuration = kit.MyDelayDuration + 1e-6, .MyMaxStacks = 1000,
					.MyRefresh = BuffRefresh::INDEPENDENT, .MyInterval = kit.MyDelayInterval, .MyTickEffects = {{.MyKind = BuffEffectKind::HEALTH_LOSS, .MyAmount = perTick}}});
			}
		}
	}

	void Battle::GvialObserve(ContentEvent& _event, bool _late)
	{
		if (!_event.MyTarget) return;
		auto& unit = _MyUnits[Index(_event.MyTarget)];
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<GvialKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || unit.MyOperatorHooksReleased) return;
		if (_late)
		{
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && kit->MySkill == GvialSkillKind::DEFER && unit.MySkill.MyActive)
			{
				_event.MyDamage.MyMultiplier *= 1 - kit->MyDeferral;
				TrackDeferredDamage(unit, _event.MyDamage.MySequence);
			}
			return;
		}
		if (_event.MyKind == ContentEventKind::BEFORE_HEAL)
			_event.MyAmount *= std::isless(unit.MyHealth / unit.MyStats.MyMaxHealth, kit->MyHealthThreshold) ? kit->MyLowHealthHealingScale : kit->MyHealingScale;
		else if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyDamage.MyType == DamageType::PHYSICAL &&
			std::isgreater(kit->MyReductionThreshold, 0) && std::isgreater(kit->MyPhysicalReduction, 0) && std::isgreater(unit.MyHealth / unit.MyStats.MyMaxHealth, kit->MyReductionThreshold))
			_event.MyDamage.MyMultiplier *= 1 - kit->MyPhysicalReduction;
		else if (_event.MyKind == ContentEventKind::DAMAGED && kit->MySkill == GvialSkillKind::DEFER && ConsumeDeferredDamage(unit, _event.MyDamage.MySequence) && std::isgreater(_event.MyAmount, 0))
			unit.MyGvialDebt += _event.MyAmount * kit->MyDeferral / (1 - kit->MyDeferral);
	}

	void Battle::GvialSkill(UnitId _unit, const GvialKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyDamage.MyIsAttack && _event.MyTarget && !_kit.MyHiddenVariant && Unit(_event.MyTarget).MyBlockedBy == _unit && std::isgreater(_kit.MyBlockedScale, 0))
			_event.MyDamage.MyMultiplier *= _kit.MyBlockedScale;
		if (_kit.MyHiddenVariant && _event.MyKind == ContentEventKind::DEPLOY) OperatorModuleTick(_unit);
		if (_kit.MySkill != GvialSkillKind::DEFER) return;
		if (_event.MyKind == ContentEventKind::SKILL_START) unit.MyGvialDebt = 0;
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			const auto debt = std::exchange(unit.MyGvialDebt, 0);
			if (_event.MySkillReason == SkillReason::DEATH || !unit.MyAlive || !std::isgreater(debt, 0)) return;
			(void)AddBuff(_unit, {.MyKey = "gvial:bleed", .MyDuration = _kit.MyDelayDuration + 1e-6, .MyMaxStacks = 10, .MyRefresh = BuffRefresh::INDEPENDENT,
				.MyInterval = _kit.MyDelayInterval, .MyTickEffects = {{.MyKind = BuffEffectKind::HEALTH_LOSS, .MyAmount = debt * _kit.MyDelayInterval / _kit.MyDelayDuration}}});
		}
	}
}
