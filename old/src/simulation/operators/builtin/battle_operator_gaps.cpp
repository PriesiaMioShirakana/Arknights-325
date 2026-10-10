#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct GapScratchGuard
		{
			std::size_t& MyDepth;

			~GapScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::QiubaiHit(UnitId _unit, UnitId _target, const QiubaiKit& _kit)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto& target = Unit(_target);
		if (std::isgreater(_kit.MyModuleArts, 0)) (void)DealDamage(_unit, _target, {.MyAmount = unit.MyStats.MyAttack * _kit.MyModuleArts, .MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::MODULE)});
		const bool sluggish = target.MyStatuses.Has(CombatStatus::SLUGGISH), bind = target.MyStatuses.Has(CombatStatus::BIND);
		if ((sluggish || bind) && target.MyAlive && std::isgreater(_kit.MyGapScale, 0))
			(void)DealDamage(_unit, _target, {.MyAmount = unit.MyStats.MyAttack * _kit.MyGapScale * (sluggish && bind ? _kit.MyBothScale : 1), .MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::TALENT)});
		if (!target.MyAlive) return;
		if (std::isgreater(_kit.MyFirstBind, 0) && !std::ranges::contains(unit.MyQiubaiBound, _target))
		{ unit.MyQiubaiBound.push_back(_target); (void)ApplyStatus(_target, CombatStatus::BIND, _kit.MyFirstBind, _unit); return; }
		if (std::isgreater(_kit.MyBindChance, 0) && std::isless(_MyRandom.Next(), _kit.MyBindChance)) (void)ApplyStatus(_target, CombatStatus::BIND, _kit.MyBindDuration, _unit);
	}

	void BattleCore::QiubaiBurst(UnitId _unit, UnitId _target)
	{
		const auto& unit = Unit(_unit); const auto& kit = std::get<QiubaiKit>(*unit.MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch(); const GapScratchGuard guard{.MyDepth = _MyAttackDepth};
		FoesInRadius(Unit(_target).MyPosition, 1.2, scratch.MyTargets, true);
		for (const auto id : scratch.MyTargets)
			(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * kit.MyBurstScale, .MyType = DamageType::ARTS,
				.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSplash = id != _target, .MyIsSkill = true});
	}

	void BattleCore::QiubaiSkill(UnitId _unit, const QiubaiKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_kit.MySkill == QiubaiSkillKind::SNOW)
		{
			if (_event.MyKind == ContentEventKind::SKILL_START) unit.MyQiubaiStacks = 0;
			if (_event.MyKind == ContentEventKind::SKILL_ENDING) { (void)RemoveBuff(_unit, "qiubai:snow"); unit.MyQiubaiStacks = 0; }
			if (_event.MyKind == ContentEventKind::ATTACK && unit.MySkill.MyActive && unit.MyQiubaiStacks < _kit.MyMaxStacks)
			{
				++unit.MyQiubaiStacks;
				(void)AddBuff(_unit, {.MyKey = "qiubai:snow", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = _kit.MyStackSpeed * unit.MyQiubaiStacks}}});
			}
			return;
		}
		if (_kit.MySkill != QiubaiSkillKind::SHADOW || (_event.MyKind != ContentEventKind::SKILL_START && _event.MyKind != ContentEventKind::SKILL_TICK && _event.MyKind != ContentEventKind::SKILL_ENDING) ||
			(_event.MyKind == ContentEventKind::SKILL_ENDING && (_event.MySkillReason == SkillReason::DEATH || !unit.MyAlive))) return;
		auto& scratch = AcquireAttackScratch(); const GapScratchGuard guard{.MyDepth = _MyAttackDepth};
		const AttackProfile filter{.MyCanHitFlying = true};
		for (const auto id : _MyEnemyIds) if (TargetableEnemy(Unit(id), filter) && !Unit(id).Flying() && InRuleRange(_unit, id)) scratch.MyTargets.push_back(id);
		for (const auto id : scratch.MyTargets)
		{
			if (_event.MyKind == ContentEventKind::SKILL_TICK) (void)ApplyStatus(id, CombatStatus::SLUGGISH, 0.25, _unit);
			else (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * (_event.MyKind == ContentEventKind::SKILL_START ? _kit.MyStartScale : _kit.MyEndScale),
				.MyType = _event.MyKind == ContentEventKind::SKILL_START ? DamageType::ARTS : DamageType::PHYSICAL, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
		}
	}
}
