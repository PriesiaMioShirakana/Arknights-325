#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		struct FrostScratchGuard
		{
			std::size_t& MyDepth;

			~FrostScratchGuard() { --MyDepth; }
		};
	}

	void Battle::SetOperatorAttribute(UnitId _unit, std::string_view _key, bool _on, Attribute _attribute, double _value)
	{
		const std::array modifiers{AttributeChange{.MyAttribute = _attribute, .MyValue = _value}};
		SetOperatorModifiers(_unit, _key, _on, modifiers);
	}

	void Battle::GnosisBeforeAttack(CombatUnit& _unit, std::vector<UnitId>& _targets)
	{
		auto& scratch = AcquireAttackScratch(); const FrostScratchGuard guard{.MyDepth = _MyAttackDepth};
		const auto profile = EffectiveAttack(_unit);
		GenericEnemies(_unit.MyId, scratch.MyTargets, 0, false);
		std::erase_if(scratch.MyTargets, [&](UnitId _id) { return !TargetableEnemy(Unit(_id), profile); });
		for (const auto id : _unit.MyBlocking)
			if (TargetableEnemy(Unit(id), profile) && !std::ranges::contains(scratch.MyTargets, id)) scratch.MyTargets.push_back(id);
		if (scratch.MyTargets.size() <= 1) return;
		SortOperatorTargets(_unit.MyId, scratch.MyTargets, 0, &profile);
		const auto count = std::min(static_cast<double>(scratch.MyTargets.size()), std::max(1.0, std::floor(static_cast<double>(profile.MyMaxTargets) + _unit.MyStats.MyExtraTargets)));
		_targets.clear(); _targets.reserve(static_cast<std::size_t>(count));
		for (const auto frozen : {false, true})
			for (const auto id : scratch.MyTargets)
				if (_targets.size() < static_cast<std::size_t>(count) && Unit(id).MyStatuses.Has(CombatStatus::FREEZE) == frozen) _targets.push_back(id);
	}

	void Battle::GnosisSkill(UnitId _unit, const GnosisKit& _kit, const ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyDamage.MyIsAttack && !_event.MyDamage.MyIsSplash && _event.MyTarget &&
			Unit(_event.MyTarget).MySide == UnitSide::ENEMY && Unit(_event.MyTarget).MyAlive)
			(void)ApplyStatus(_event.MyTarget, CombatStatus::COLD, _kit.MyAttackCold, _unit);
		if (_kit.MySkill == GnosisSkillKind::THOUGHT) return;
		if (_kit.MySkill == GnosisSkillKind::BURST)
		{
			if (_event.MyKind != ContentEventKind::SKILL_START) return;
			const bool charged = unit.MyDefinition.MySkill.MyMaxCharges > 1 && unit.MySkill.MyCharges + 1 >= unit.MyDefinition.MySkill.MyMaxCharges;
			auto& scratch = AcquireAttackScratch(); const FrostScratchGuard guard{.MyDepth = _MyAttackDepth};
			GenericEnemies(_unit, scratch.MyTargets);
			for (const auto id : scratch.MyTargets)
			{
				(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyScale, .MyType = DamageType::ARTS,
					.MyTags = DamageTag::SKILL | DamageTag::BURST, .MyIsSkill = true});
				if (Unit(id).MyAlive) (void)ApplyStatus(id, CombatStatus::COLD, _kit.MyCold, _unit);
				if (charged && Unit(id).MyAlive) (void)ApplyStatus(id, CombatStatus::COLD, _kit.MyCold, _unit);
			}
			return;
		}
		if (_event.MyKind == ContentEventKind::SKILL_START) unit.MyHypothermia.clear();
		else if (_event.MyKind == ContentEventKind::SKILL_TICK)
		{
			for (const auto id : _MyEnemyIds)
			{
				auto& enemy = _MyUnits[Index(id)];
				if (!enemy.MyAlive || enemy.MyHidden || !InRuleRange(_unit, id) || !enemy.MyStatuses.Has(CombatStatus::FREEZE)) continue;
				if (!std::ranges::contains(unit.MyHypothermia, id)) unit.MyHypothermia.push_back(id);
				auto& remaining = enemy.MyStatuses.MyRemaining[static_cast<std::size_t>(CombatStatus::FREEZE)];
				remaining = std::max(remaining, unit.MySkill.MyTimeLeft);
			}
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			auto& scratch = AcquireAttackScratch(); const FrostScratchGuard guard{.MyDepth = _MyAttackDepth};
			if (_event.MySkillReason != SkillReason::DEATH && unit.MyAlive)
			{
				for (const auto id : _MyEnemyIds)
					if (Unit(id).MyAlive && !Unit(id).MyHidden && InRuleRange(_unit, id) && Unit(id).MyStatuses.Has(CombatStatus::FREEZE)) scratch.MyTargets.push_back(id);
				for (const auto id : unit.MyHypothermia)
					if (Unit(id).MyAlive && Unit(id).MyStatuses.Has(CombatStatus::FREEZE) && !std::ranges::contains(scratch.MyTargets, id)) scratch.MyTargets.push_back(id);
			}
			unit.MyHypothermia.clear();
			for (const auto id : scratch.MyTargets)
			{
				(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyScale, .MyType = DamageType::ARTS,
					.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
				if (Unit(id).MyAlive) (void)RemoveStatus(id, CombatStatus::FREEZE);
			}
		}
	}

	void Battle::GnosisAura(UnitId _unit, bool _resist)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<GnosisKit>(*unit.MyDefinition.MyOperatorKit);
		if (_resist)
		{
			if (std::isless(Time() - unit.MyDeployedAt, kit.MyResistDelay - 1e-9) || !std::isgreater(kit.MyResist, 0)) return;
			for (const auto id : _MyAllyIds)
			{
				const auto& ally = Unit(id);
				if (ally.MyAlive && ally.MyKind == UnitKind::OPERATOR && ally.MyOwner == unit.MyOwner && ally.MyDefinition.MyIdentity.MyNationId == "kjerag")
					(void)ApplyStatus(id, CombatStatus::RESIST, {.MyDuration = 0.25, .MySource = _unit, .MyValue = kit.MyResist});
			}
			return;
		}
		bool elite = false;
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (!enemy.MyAlive || enemy.MyHidden || !InRuleRange(_unit, id)) continue;
			elite |= enemy.MySpawnTag == EnemySpawnTag::BOSS || enemy.MyDefinition.MyElite || enemy.MyDefinition.MyLeader;
			const auto scale = enemy.MyStatuses.Has(CombatStatus::FREEZE) ? kit.MyFreezeFragile : enemy.MyStatuses.Has(CombatStatus::COLD) ? kit.MyColdFragile : 0;
			if (std::islessgreater(scale, 0)) (void)ApplyStrongest(id, "gnosis:fragile", 0.15, {.MyValue = scale, .MyAttribute = Attribute::DAMAGE_TAKEN_MULTIPLIER}, _unit);
		}
		if (std::isgreater(kit.MySpRecovery, 0)) SetOperatorAttribute(_unit, "gnosis:module", elite, Attribute::SP_RECOVERY_FLAT, kit.MySpRecovery);
	}

	void Battle::LionhdSkill(UnitId _unit, const LionhdKit& _kit)
	{
		auto& scratch = AcquireAttackScratch(); const FrostScratchGuard guard{.MyDepth = _MyAttackDepth};
		GenericEnemies(_unit, scratch.MyTargets);
		for (const auto id : scratch.MyTargets)
		{
			(void)DealDamage(_unit, id, {.MyAmount = Unit(_unit).MyStats.MyAttack * _kit.MyScale, .MyType = DamageType::ARTS,
				.MyTags = DamageTag::SKILL | DamageTag::BURST, .MyIsSkill = true});
			ApplyResistanceCut(_unit, id, "lionhd:res", _kit.MyDuration, _kit.MyResistance);
		}
	}

	void Battle::LionhdPresence(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<LionhdKit>(*unit.MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch(); const FrostScratchGuard guard{.MyDepth = _MyAttackDepth};
		GenericEnemies(_unit, scratch.MyTargets, 0, false);
		const auto count = std::min(kit.MyMaxStacks, static_cast<double>(scratch.MyTargets.size()));
		SetOperatorAttribute(_unit, "lionhd:t1", std::isgreater(count, 0), Attribute::ATTACK_PERCENT, kit.MyAttack * count);
	}

	void Battle::ReckprObserve(const ContentEvent& _event)
	{
		for (std::size_t i = 0, count = _MyReckprs.size(); i < count; ++i)
		{
			const auto& source = Unit(_MyReckprs[i]);
			if (source.MyOperatorHooksReleased) continue;
			const auto& kit = std::get<ReckprKit>(*source.MyDefinition.MyOperatorKit);
			if (_event.MyKind == ContentEventKind::SKILL_START && _event.MyUnit && _event.MyUnit != source.MyId && source.MyAlive)
			{
				const auto& ally = Unit(_event.MyUnit);
				if (ally.MySide != UnitSide::ALLY || ally.MyKind != UnitKind::OPERATOR || !InRuleRange(source.MyId, ally.MyId) || ((!kit.MySharedGuard || std::isless(kit.MyProbability, 1)) && !std::isless(_MyRandom.Next(), kit.MyProbability))) continue;
				if (kit.MySharedGuard || !source.MySkill.MyActive || source.MyDefinition.MySkill.MyKind == SkillKind::PASSIVE) (void)GainSp(source.MyId, kit.MySp);
				if (!kit.MySharedGuard || std::islessgreater(kit.MyAttackSpeed, 0)) (void)AddBuff(source.MyId, {.MyKey = kit.MySharedGuard ? "reckpr:learn" : "reckpr:t1", .MyDuration = kit.MyDuration, .MyMaxStacks = kit.MySharedGuard ? 1U : kit.MyMaxStacks,
					.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = kit.MyAttackSpeed}}});
			}
			else if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyTarget && std::isgreater(_event.MyAmount, 0) && !HasTag(_event.MyDamage.MyTags, DamageTag::HP_LOSS))
			{
				const auto& target = Unit(_event.MyTarget);
				if (target.MyAlive && target.MySide == UnitSide::ALLY && (!kit.MySharedGuard || (!_event.MyElement && std::isgreater(target.MyHealth, 0))) && std::ranges::any_of(target.MyBuffs, [&](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == source.MyGuardHealKey && (!kit.MySharedGuard || _buff.MyDefinition.MySource == source.MyId); }))
					(void)Heal(source.MyId, target.MyId, kit.MyGuardHeal);
			}
		}
	}
}
