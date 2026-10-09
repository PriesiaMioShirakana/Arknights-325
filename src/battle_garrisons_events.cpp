#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	bool Battle::GarrisonAmmoTarget(const BattleGarrisonRuntime& _state, UnitId _unit) const
	{
		if (!_unit) return false;
		const auto& holder = Unit(_state.MyUnit); const auto& target = Unit(_unit);
		const auto scope = _MyInput.MyGarrisonRules.MyEffects[_state.MyRule].MyAmmoScope;
		if (scope == GarrisonAmmoScope::SELF) return _unit == _state.MyUnit;
		if (scope == GarrisonAmmoScope::ADJACENT && _unit == _state.MyUnit) return true;
		if (!GarrisonSourceActive(holder.MyId) || !GarrisonOnField(_unit) || target.MyOwner != holder.MyOwner || target.MyKind != UnitKind::OPERATOR || _unit == holder.MyId) return false;
		const auto point = RulePosition(target), origin = RulePosition(holder);
		if (scope == GarrisonAmmoScope::ADJACENT) return std::abs(point.MyX - origin.MyX) + std::abs(point.MyY - origin.MyY) == 1;
		const auto offset = RotateOffset(RangeOffset{0, 1}, holder.MyFacing);
		return point.MyX == origin.MyX + offset.MyColumn && point.MyY == origin.MyY + offset.MyRow;
	}

	void Battle::NotifyGarrisons(ContentEvent& _event, bool _late)
	{
		if (_MyGarrisons.empty()) return;
		if (_event.MyKind != ContentEventKind::SKILL_START && _event.MyKind != ContentEventKind::BEFORE_KILL &&
			_event.MyKind != ContentEventKind::DEATH && _event.MyKind != ContentEventKind::TICK && _event.MyKind != ContentEventKind::AMMO_USED &&
			_event.MyKind != ContentEventKind::STATUS_APPLIED && _event.MyKind != ContentEventKind::DEPLOY &&
			_event.MyKind != ContentEventKind::LAYER_GAIN && _event.MyKind != ContentEventKind::BEFORE_DAMAGE) return;
		if (_late)
		{
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MySource && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
				for (const auto index : _MyGarrisonGroups[static_cast<unsigned>(BattleGarrisonKind::WEAKNESS)])
					if (!_MyGarrisons[index].MyRetired && _MyGarrisons[index].MyUnit == _event.MySource) { RetypeWeakness(_event.MyDamage, _event.MySource, _event.MyTarget, true); break; }
			return;
		}
		for (const auto kind : _MyGarrisonOrder)
			for (const auto index : _MyGarrisonGroups[static_cast<unsigned>(kind)])
			{
				auto& state = _MyGarrisons[index]; const auto& rule = _MyInput.MyGarrisonRules.MyEffects[state.MyRule]; const auto& unit = Unit(state.MyUnit);
				if (state.MyRetired) continue;
				bool gain = false;
				switch (kind)
				{
				case BattleGarrisonKind::SKILL_GAIN:
					gain = _event.MyKind == ContentEventKind::SKILL_START && _event.MyUnit == unit.MyId;
					break;
				case BattleGarrisonKind::KILL_GAIN:
					if (_event.MyKind == ContentEventKind::BEFORE_KILL && _event.MySource == unit.MyId && _event.MyTarget &&
						(Unit(_event.MyTarget).MySide == UnitSide::ENEMY || (rule.MyAlliedKills && Unit(_event.MyTarget).MyKind == UnitKind::OPERATOR)))
					{ ++state.MyCount; gain = std::fmod(state.MyCount, rule.MyEvery) == 0; }
					break;
				case BattleGarrisonKind::DEATH_GAIN:
					gain = _event.MyKind == ContentEventKind::DEATH && _event.MyUnit == unit.MyId && _event.MyRemovalReason == RemovalReason::KILLED;
					if (_event.MyKind == ContentEventKind::TICK && rule.MyDollSwap && state.MyDoll != unit.MyProfession.MyDoll)
					{ state.MyDoll = unit.MyProfession.MyDoll; gain = GarrisonOnField(unit.MyId); }
					break;
				case BattleGarrisonKind::AMMO_GAIN:
					if (_event.MyKind == ContentEventKind::AMMO_USED && GarrisonAmmoTarget(state, _event.MyUnit))
					{ ++state.MyCount; gain = std::fmod(state.MyCount, rule.MyEvery) == 0; }
					break;
				case BattleGarrisonKind::FREEZE_GAIN:
				case BattleGarrisonKind::SLEEP_STUN_GAIN:
					if (_event.MyKind == ContentEventKind::STATUS_APPLIED && _event.MyStatusEntered && _event.MyTarget && GarrisonSourceActive(unit.MyId) && InRuleRange(unit.MyId, _event.MyTarget))
					{
						const auto& target = Unit(_event.MyTarget);
						if (kind == BattleGarrisonKind::FREEZE_GAIN && target.MySide == UnitSide::ENEMY && _event.MyStatus == CombatStatus::FREEZE) gain = _MyRandom.Next() < rule.MyProbability;
						if (kind == BattleGarrisonKind::SLEEP_STUN_GAIN && (target.MySide == UnitSide::ENEMY || target.MyKind == UnitKind::OPERATOR))
							gain = _event.MyStatus == CombatStatus::SLEEP || _event.MyStatus == CombatStatus::STUN;
					}
					break;
				case BattleGarrisonKind::DEPLOY_GAIN:
					gain = _event.MyKind == ContentEventKind::DEPLOY && _event.MyUnit == unit.MyId;
					break;
				case BattleGarrisonKind::EXTRA_GAIN:
					if (_event.MyKind == ContentEventKind::LAYER_GAIN && _event.MyReason == "garrison" && _event.MySource && _event.MySource != unit.MyId &&
						_event.MySourceTile && _event.MyPlayer == unit.MyOwner && GarrisonSourceActive(unit.MyId))
					{
						const auto& source = Unit(_event.MySource); const auto offset = RotateOffset(RangeOffset{0, 1}, unit.MyFacing);
						const auto origin = RulePosition(unit);
						if (source.MyKind == UnitKind::OPERATOR && source.MyOwner == unit.MyOwner &&
							_event.MySourceTile->MyX == origin.MyX + offset.MyColumn && _event.MySourceTile->MyY == origin.MyY + offset.MyRow) _event.MyAmount += rule.MyExtra;
					}
					break;
				case BattleGarrisonKind::TAG_DAMAGE:
				case BattleGarrisonKind::STATUS_DAMAGE:
					if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MySource == unit.MyId && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
					{
						const auto& target = Unit(_event.MyTarget);
						if (kind == BattleGarrisonKind::TAG_DAMAGE && !HasTag(_event.MyDamage.MyTags, DamageTag::DOT) && std::ranges::contains(target.MyDefinition.MyEnemyTags, rule.MyEnemyTag)) _event.MyDamage.MyAmount *= rule.MyAttack;
						const auto hasBuff = [&](std::string_view key) { return std::ranges::any_of(target.MyBuffs, [&](const CombatBuff& b) { return b.MyDefinition.MyKey == key; }); };
						if (kind == BattleGarrisonKind::STATUS_DAMAGE && state.MySteps > 0 &&
							((rule.MyBound && (target.MyStatuses.Has(CombatStatus::BIND) || hasBuff("bind"))) ||
							 (rule.MySluggish && (target.MyStatuses.MyRemaining[static_cast<unsigned>(CombatStatus::SLUGGISH)] > 0 || hasBuff("sluggish")))))
							_event.MyDamage.MyMultiplier *= 1 + rule.MyDamageScale * state.MySteps;
					}
					break;
				default: break;
				}
				if (gain) GainGarrisonLayers(index);
			}
		if (_MyGarrisonReaders && _event.MyKind == ContentEventKind::LAYER_GAIN && !_MyGarrisonPending)
		{
			_MyGarrisonPending = true; Schedule({.MyAt = Time(), .MyKind = ScheduledKind::GARRISON_REFRESH});
		}
		if (_event.MyKind == ContentEventKind::DEPLOY)
			for (const auto index : _MyGarrisonGroups[static_cast<unsigned>(BattleGarrisonKind::DEPLOY_ATTRIBUTES)])
			{
				auto& state = _MyGarrisons[index];
				if (state.MyRetired || state.MyUnit != _event.MyUnit) continue;
				state.MySteps = GarrisonSteps(state); ApplyGarrisonAttributes(index, _MyInput.MyGarrisonRules.MyEffects[state.MyRule].MyDuration);
			}
	}
}
