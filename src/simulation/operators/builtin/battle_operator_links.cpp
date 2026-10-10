#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct LinkScratchGuard
		{
			std::size_t& MyDepth;

			~LinkScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::FlamtlDodge(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MyTarget || _event.MyCancel) return;
		const auto& unit = Unit(_event.MyTarget);
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<FlamtlKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || unit.MyOperatorHooksReleased) return;
		if (kit->MySkill == FlamtlSkillKind::EVADE)
		{
			if (_event.MyDamage.MyType != DamageType::PHYSICAL || !_event.MyDamage.MyIsAttack || !_event.MySource || Unit(_event.MySource).MySide != UnitSide::ENEMY ||
				!std::ranges::any_of(unit.MyBuffs, [](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == "flamtl:evade"; })) return;
			(void)RemoveBuff(unit.MyId, "flamtl:evade");
		}
		else if (kit->MySkill == FlamtlSkillKind::FLAME)
		{
			if (!unit.MySkill.MyActive || !_event.MyDamage.MyCanDodge || (_event.MyDamage.MyType != DamageType::PHYSICAL && _event.MyDamage.MyType != DamageType::ARTS) ||
				!std::isless(_MyRandom.Next(), kit->MyProbability)) return;
		}
		else return;
		_event.MyCancel = true;
		ContentEvent dodge{.MyKind = ContentEventKind::DODGED, .MySource = _event.MySource, .MyTarget = unit.MyId, .MyDamage = _event.MyDamage};
		NotifyContent(dodge);
	}

	void BattleCore::FlamtlBeforeAttack(CombatUnit& _unit, std::vector<UnitId>& _targets)
	{
		if (!_unit.MyRiposte) return;
		_unit.MyRiposte = false; _unit.MyRiposteNow = true;
		const auto& profile = EffectiveAttack(_unit);
		for (const auto id : _unit.MyBlocking)
			if (TargetableEnemy(Unit(id), profile) && !std::ranges::contains(_targets, id)) _targets.push_back(id);
	}

	void BattleCore::FlamtlSkill(UnitId _unit, const FlamtlKit& _kit, const ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::ATTACK && unit.MyRiposteNow)
		{
			unit.MyRiposteNow = false;
			for (const auto id : _event.MyTargets)
				if (Unit(id).MyAlive) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * unit.MyStats.MyAttackScaleMultiplier, .MyIsAttack = true});
		}
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			if (_kit.MySkill == FlamtlSkillKind::FLAME)
			{
				unit.MyFlameGiven = 0; unit.MyFlameAccumulator = 0;
				unit.MyFlameStep = unit.MyDefinition.MySkill.MyDuration / std::max(1U, _kit.MyPulses);
				return;
			}
			if (unit.MyOwner != NoPlayer) (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyDp);
			if (_kit.MySkill == FlamtlSkillKind::EVADE) { (void)AddBuff(_unit, {.MyKey = "flamtl:evade"}); return; }
			auto& scratch = AcquireAttackScratch(); const LinkScratchGuard guard{.MyDepth = _MyAttackDepth};
			OperatorEnemiesInGrid(_unit, _kit.MyRange, scratch.MyTargets, false);
			const AttackProfile priority{};
			SortOperatorTargets(_unit, scratch.MyTargets, _kit.MyTargets, &priority);
			for (const auto id : scratch.MyTargets)
			{
				for (unsigned hit = 0; hit < 2 && Unit(id).MyAlive; ++hit)
					(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyScale,
						.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
				if (Unit(id).MyAlive) (void)ApplyStatus(id, CombatStatus::STUN, _kit.MyStun, _unit);
			}
			for (const auto id : _MyAllyIds)
			{
				const auto& ally = Unit(id);
				if (!ally.MyAlive || ally.MyHidden || ally.MyKind == UnitKind::DEVICE || !OperatorInGrid(_unit, id, _kit.MyRange) ||
					(id != _unit && ally.MyStatuses.Has(CombatStatus::ISOLATED))) continue;
				(void)AddBuff(id, {.MyKey = "flamtl:redPine", .MySource = _unit, .MyDuration = _kit.MyDodgeDuration,
					.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::PHYSICAL_DODGE, .MyValue = _kit.MyDodge}}});
			}
		}
		if (_kit.MySkill != FlamtlSkillKind::FLAME || unit.MyOwner == NoPlayer) return;
		if (_event.MyKind == ContentEventKind::SKILL_TICK)
		{
			unit.MyFlameAccumulator += _event.MyDelta;
			while (unit.MyFlameGiven < _kit.MyPulses && std::isgreaterequal(unit.MyFlameAccumulator + 1e-9, unit.MyFlameStep))
			{
				unit.MyFlameAccumulator -= unit.MyFlameStep; ++unit.MyFlameGiven;
				(void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyDp);
			}
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING && _event.MySkillReason == SkillReason::DURATION && unit.MyFlameGiven < _kit.MyPulses)
			(void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyDp * (_kit.MyPulses - unit.MyFlameGiven));
	}

	void BattleCore::SetExtraRange(UnitId _unit, std::span<const int> _keys)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (std::ranges::equal(unit.MyExtraRangeKeys, _keys)) return;
		unit.MyExtraRangeKeys.assign(_keys.begin(), _keys.end());
		RefreshRange(unit);
	}

	void BattleCore::FartthBeforeAttack(CombatUnit& _unit, std::vector<UnitId>& _targets)
	{
		if (_unit.MyExtraRangeKeys.empty()) return;
		const auto own = RangeMask(RulePosition(_unit), _unit.MyFacing, _unit.MyDefinition.MyRange, _unit.MyStats.MyRangeExtend);
		const auto eligible = [&](UnitId _id)
		{
			const auto& enemy = Unit(_id);
			return BodyInRange(enemy, own) || (enemy.MyBlockedBy && Unit(enemy.MyBlockedBy).MySide == UnitSide::ALLY && Unit(enemy.MyBlockedBy).MyAlive);
		};
		if (std::ranges::all_of(_targets, eligible)) return;
		auto& scratch = AcquireAttackScratch(); const LinkScratchGuard guard{.MyDepth = _MyAttackDepth};
		const auto& profile = EffectiveAttack(_unit);
		GenericEnemies(_unit.MyId, scratch.MyTargets, 0, false);
		for (const auto id : _unit.MyBlocking)
			if (TargetableEnemy(Unit(id), profile) && !std::ranges::contains(scratch.MyTargets, id)) scratch.MyTargets.push_back(id);
		std::erase_if(scratch.MyTargets, [&](UnitId _id) { return !eligible(_id) || !TargetableEnemy(Unit(_id), profile); });
		SortOperatorTargets(_unit.MyId, scratch.MyTargets, static_cast<unsigned>(std::max<std::size_t>(1, _targets.size())), &profile);
		_targets.assign(scratch.MyTargets.begin(), scratch.MyTargets.end());
	}

	void BattleCore::FartthSkill(UnitId _unit, const FartthKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY) unit.MyFartthHurtAt = -std::numeric_limits<double>::infinity();
		if (_kit.MySkill == FartthSkillKind::ALLIED)
		{
			if (_event.MyKind == ContentEventKind::SKILL_TICK)
			{
				unit.MyNextExtraRangeKeys.clear();
				for (const auto id : _MyEnemyIds)
				{
					const auto& enemy = Unit(id);
					if (!enemy.MyAlive || enemy.MyHidden || !enemy.MyBlockedBy || Unit(enemy.MyBlockedBy).MySide != UnitSide::ALLY || !Unit(enemy.MyBlockedBy).MyAlive) continue;
					const auto r = static_cast<int>(std::floor(enemy.MyPosition.MyY + 0.5)), c = static_cast<int>(std::floor(enemy.MyPosition.MyX + 0.5));
					if (r >= 0 && r < FieldRows && c >= 0 && c < FieldColumns) unit.MyNextExtraRangeKeys.push_back(FieldGrid::Key(r, c));
				}
				std::ranges::sort(unit.MyNextExtraRangeKeys);
				SetExtraRange(_unit, unit.MyNextExtraRangeKeys);
			}
			else if (_event.MyKind == ContentEventKind::SKILL_ENDING) SetExtraRange(_unit, {});
		}
		if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyDamage.MyIsAttack && !_event.MyDamage.MyIsSplash && _event.MyTarget &&
			Unit(_event.MyTarget).MySide == UnitSide::ENEMY && Unit(_event.MyTarget).MyAlive && std::isgreater(Unit(_event.MyTarget).MyHealth, 0) && std::isgreater(_kit.MySurvivorSp, 0))
			(void)GainSp(_unit, _kit.MySurvivorSp);
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE) return;
		if (unit.MySkill.MyActive && _event.MyDamage.MyType == DamageType::PHYSICAL) _event.MyDamage.MyCanDodge = false;
		if (!_event.MyDamage.MyIsAttack || !_event.MyTarget) return;
		const auto& target = Unit(_event.MyTarget);
		if (_kit.MySkill == FartthSkillKind::LINE && unit.MySkill.MyActive && !BodyInRange(target, unit.MyBaseTriggerMask, RulePosition(target))) _event.MyDamage.MyMultiplier *= _kit.MyFarScale;
		if (std::isgreater(_kit.MyDistanceScale, 0))
		{
			const auto distance = Distance(RulePosition(unit), RulePosition(target));
			_event.MyDamage.MyMultiplier *= 1 + _kit.MyDistanceScale * std::clamp((distance - _kit.MyMinDistance) / std::max(1e-6, _kit.MyMaxDistance - _kit.MyMinDistance), 0.0, 1.0);
		}
	}
}
