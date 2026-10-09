#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::GladyTide(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MySource || !_event.MyTarget || Unit(_event.MySource).MySide != UnitSide::ENEMY ||
			!std::ranges::contains(Unit(_event.MySource).MyDefinition.MyEnemyTags, "seamonster") ||
			(_event.MyDamage.MyType != DamageType::PHYSICAL && _event.MyDamage.MyType != DamageType::ARTS)) return;
		if (!std::ranges::any_of(Unit(_event.MyTarget).MyBuffs, [](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == "glady:tide"; })) return;
		for (const auto id : _MyGladys)
			if (!Unit(id).MyOperatorHooksReleased) _event.MyDamage.MyMultiplier *= 1 - std::get<GladyKit>(*Unit(id).MyDefinition.MyOperatorKit).MySeaReduction;
	}

	void Battle::GladyDragDamage(UnitId _unit, UnitId _target, const GladyKit& _kit, double _moved)
	{
		if (!std::isgreater(_moved, 0) || !std::isgreater(_kit.MyDragDamage, 0) || !Unit(_target).MyAlive) return;
		(void)DealDamage(_unit, _target, {.MyAmount = _kit.MyDragDamage * _moved / _kit.MyDragDistance, .MyType = DamageType::ARTS,
			.MyTags = static_cast<DamageTags>(DamageTag::DRAG), .MyIsSkill = true});
	}

	void Battle::GladyPull(UnitId _unit, UnitId _target, const GladyKit& _kit)
	{
		if (!_target || !Unit(_target).MyAlive) return;
		auto force = _kit.MyForce;
		if (std::isgreater(_kit.MyFarRadius, 0) && std::isgreater(_kit.MyFarForce, 0) &&
			std::isgreater(Distance(RulePosition(Unit(_unit)), RulePosition(Unit(_target))), _kit.MyFarRadius + 1e-9)) force += _kit.MyFarForce;
		GladyDragDamage(_unit, _target, _kit, PullToFront(_target, _unit, force));
	}

	void Battle::GladySkill(UnitId _unit, const GladyKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyDamage.MyIsAttack && _event.MyTarget &&
			Unit(_event.MyTarget).MySide == UnitSide::ENEMY && std::islessequal(Unit(_event.MyTarget).MyStats.MyMass, _kit.MyMassLimit))
			_event.MyDamage.MyMultiplier *= _kit.MyMassScale;
		if (_kit.MySkill != GladySkillKind::TORNADO) return;
		if (_event.MyKind != ContentEventKind::SKILL_START && _event.MyKind != ContentEventKind::SKILL_TICK && _event.MyKind != ContentEventKind::SKILL_ENDING) return;
		auto& scratch = AcquireAttackScratch();
		struct Guard
		{
			std::size_t& MyDepth;

			~Guard() { --MyDepth; }
		};
		const Guard guard{.MyDepth = _MyAttackDepth};
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			GenericEnemies(_unit, scratch.MyTargets, 0, true);
			const auto origin = RulePosition(unit);
			std::ranges::sort(scratch.MyTargets, [&](UnitId _a, UnitId _b)
			{
				const auto a = BodyDistance(Unit(_a), origin), b = BodyDistance(Unit(_b), origin);
				return std::islessgreater(a, b) ? std::isgreater(a, b) : _a < _b;
			});
			unit.MyTornado.reset(); unit.MyTornadoAccumulator = 0;
			if (scratch.MyTargets.empty()) return;
			const auto target = scratch.MyTargets.front(); unit.MyTornado = Unit(target).MyPosition;
			(void)ApplyStatus(target, CombatStatus::BIND, unit.MyDefinition.MySkill.MyDuration, _unit);
			return;
		}
		if (!unit.MyTornado) return;
		const auto center = *unit.MyTornado;
		const AttackProfile filter{.MyCanHitFlying = true, .MyHitSleep = true};
		for (const auto id : _MyEnemyIds)
			if (TargetableEnemy(Unit(id), filter) && std::islessequal(BodyDistance(Unit(id), center), 1.5 + 1e-9)) scratch.MyTargets.push_back(id);
		if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			unit.MyTornado.reset();
			if (_event.MySkillReason == SkillReason::DEATH || !unit.MyAlive) return;
			for (const auto id : scratch.MyTargets) GladyPull(_unit, id, _kit);
			return;
		}
		for (const auto id : scratch.MyTargets) (void)AddBuff(id, {.MyKey = unit.MyTornadoSlowKey, .MyDuration = 0.25,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::MOVE_MULTIPLIER, .MyValue = _kit.MyMoveMultiplier}}});
		unit.MyTornadoAccumulator += _event.MyDelta;
		if (std::isless(unit.MyTornadoAccumulator, _kit.MyInterval - 1e-9)) return;
		unit.MyTornadoAccumulator -= _kit.MyInterval;
		for (const auto id : scratch.MyTargets)
		{
			if (!Unit(id).MyAlive || Unit(id).MyStatuses.Has(CombatStatus::UNTARGETABLE)) continue;
			(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyScale, .MyType = DamageType::ARTS,
				.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
			if (Unit(id).MyAlive) GladyDragDamage(_unit, id, _kit, Pull(id, _kit.MyForce, {.MyTo = center, .MyStopRadius = 0.05}));
		}
	}
}
