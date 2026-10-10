#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct ControlScratchGuard
		{
			std::size_t& MyDepth;

			~ControlScratchGuard() { --MyDepth; }
		};

		constexpr std::array<RangeOffset, 9> Nine{{
			{.MyRow = 1, .MyColumn = -1}, {.MyRow = 1}, {.MyRow = 1, .MyColumn = 1},
			{.MyColumn = -1}, {}, {.MyColumn = 1},
			{.MyRow = -1, .MyColumn = -1}, {.MyRow = -1}, {.MyRow = -1, .MyColumn = 1}}};
	}

	void BattleCore::PhilaeElementTalent(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::ELEMENT_HIT || !_event.MyTarget) return;
		const auto& unit = Unit(_event.MyTarget);
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<PhilaeKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || unit.MyOperatorHooksReleased) return;
		if (_event.MyElementHit.MyElement == Element::APOPTOSIS && !unit.MySkill.MyActive) (void)GainSp(unit.MyId, kit->MyApoptosisSp);
		_event.MyElementHit.MyMultiplier *= 1 - kit->MyElementResistance;
	}

	void BattleCore::PhilaeObserve(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::ELEMENT_HIT)
		{
			if (_event.MySource && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
			{
				const auto& source = Unit(_event.MySource);
				const auto* kit = source.MyDefinition.MyOperatorKit ? std::get_if<PhilaeKit>(source.MyDefinition.MyOperatorKit) : nullptr;
				if (kit && !source.MyOperatorHooksReleased && !source.MyBlocking.empty() && std::isgreater(kit->MyElementScale, 1))
					_event.MyElementHit.MyMultiplier *= kit->MyElementScale;
			}
			if (!_event.MyTarget) return;
			auto& unit = _MyUnits[Index(_event.MyTarget)];
			if (!unit.MySkill.MyActive || unit.MyOperatorHooksReleased || !std::isgreater(unit.MyPhilaeBarrier, 0)) return;
			AbsorbElement(_event, unit.MyPhilaeBarrier);
			return;
		}
		if (_event.MyKind != ContentEventKind::DAMAGED || !_event.MyTarget) return;
		auto& unit = _MyUnits[Index(_event.MyTarget)];
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<PhilaeKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || !kit->MyCounter || !unit.MyAlive || !unit.MySkill.MyActive || unit.MyOperatorHooksReleased) return;
		if (_event.MyElement)
		{
			if (!std::ranges::any_of(unit.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "skill:philae_rage"; }))
				(void)AddBuff(unit.MyId, {.MyKey = "skill:philae_rage", .MyModifiers = std::vector<AttributeChange>{
					{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit->MyRageAttack}}});
			return;
		}
		const auto tags = _event.MyDamage.MyTags;
		if (HasTag(tags, DamageTag::HP_LOSS) || HasTag(tags, DamageTag::COUNTER) || HasTag(tags, DamageTag::REFLECT) || std::isless(Time(), unit.MyPhilaeCounterAt)) return;
		unit.MyPhilaeCounterAt = Time() + kit->MyCounterCooldown;
		auto& scratch = AcquireAttackScratch(); const ControlScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (enemy.MyAlive && !enemy.MyHidden && !enemy.Flying() && OperatorInGrid(unit.MyId, id, Nine)) scratch.MyTargets.push_back(id);
		}
		for (const auto id : scratch.MyTargets)
		{
			(void)DealDamage(unit.MyId, id, {.MyAmount = unit.MyStats.MyAttack * kit->MyCounterScale, .MyType = DamageType::ARTS,
				.MyCanDodge = false, .MyTags = DamageTag::SKILL | DamageTag::COUNTER, .MyIsSkill = true});
			if (Unit(id).MyAlive && std::isgreater(kit->MyCounterElement, 0))
				(void)DealElement(unit.MyId, id, {.MyElement = Element::APOPTOSIS, .MyAmount = unit.MyStats.MyAttack * kit->MyCounterElement,
					.MyTags = static_cast<DamageTags>(DamageTag::SKILL)});
		}
	}

	void BattleCore::PhilaeSkill(UnitId _unit, const PhilaeKit& _kit, const ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			if (_kit.MyCounter) unit.MyPhilaeCounterAt = -std::numeric_limits<double>::infinity();
			else { (void)ReduceElement(_unit, 1e12); unit.MyPhilaeBarrier = _kit.MyBarrier; }
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			if (_kit.MyCounter) (void)RemoveBuff(_unit, "skill:philae_rage");
			else unit.MyPhilaeBarrier = 0;
		}
	}

	void BattleCore::ForcerSkill(UnitId _unit, const ForcerKit& _kit)
	{
		const auto& unit = Unit(_unit);
		auto& scratch = AcquireAttackScratch(); const ControlScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (enemy.MyAlive && !enemy.MyHidden && !enemy.MyStatuses.Has(CombatStatus::UNTARGETABLE) &&
				(_kit.MyHasRange ? OperatorInGrid(_unit, id, _kit.MyRange) : InRuleRange(_unit, id))) scratch.MyTargets.push_back(id);
		}
		const auto forward = RotateOffset({.MyColumn = 1}, unit.MyFacing);
		for (const auto id : scratch.MyTargets)
		{
			const bool near = std::isless(Distance(Unit(id).MyPosition, unit.MyPosition), 0.25);
			const auto expected = PushDistance(id, _kit.MyForce - (near ? 2 : 0), true);
			const auto moved = Push(id, _kit.MyForce, {.MyFrom = unit.MyPosition,
				.MyDirection = {.MyX = static_cast<double>(forward.MyColumn), .MyY = static_cast<double>(forward.MyRow)},
				.MyFromFacing = unit.MyFacing, .MyFixedAngle = true, .MyEffect = true});
			const bool wall = std::isgreater(expected, 0) && std::isless(moved + 0.05, expected);
			(void)ApplyStatus(id, CombatStatus::STUN, wall ? _kit.MyWallStun : _kit.MyDirectStun, _unit);
		}
		scratch.MySeen.assign(scratch.MyTargets.begin(), scratch.MyTargets.end());
		for (const auto id : scratch.MyTargets)
		{
			const auto point = Unit(id).MyPosition;
			for (std::size_t i = 0, count = _MyEnemyIds.size(); i < count; ++i)
			{
				const auto other = _MyEnemyIds[i]; const auto& enemy = Unit(other);
				const auto distance = BodyDistance(enemy, point);
				if (!enemy.MyAlive || enemy.MyHidden || std::isgreater(distance * distance, 0.36 + 1e-9) || std::ranges::contains(scratch.MySeen, other)) continue;
				scratch.MySeen.push_back(other);
				(void)ApplyStatus(other, CombatStatus::STUN, _kit.MyBrushStun, _unit);
			}
		}
	}

	void BattleCore::MintSkill(UnitId _unit, const MintKit& _kit, const ContentEvent& _event)
	{
		const auto& unit = Unit(_unit);
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			if (std::islessgreater(_kit.MyKeepDefense, 0) || std::islessgreater(_kit.MyKeepResistance, 0))
				(void)AddBuff(_unit, {.MyKey = "trait:mint_keep", .MyModifiers = std::vector<AttributeChange>{
					{.MyAttribute = Attribute::DEFENSE_PERCENT, .MyValue = _kit.MyKeepDefense},
					{.MyAttribute = Attribute::RESISTANCE_FLAT, .MyValue = _kit.MyKeepResistance}}});
			(void)AddBuff(_unit, {.MyKey = "talent:mint_taunt", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::TAUNT, .MyValue = _kit.MyTaunt}}});
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			if (_kit.MyVortex && _event.MySkillReason != SkillReason::DEATH && unit.MyAlive && std::isgreater(_kit.MyEndScale, 0))
			{
				auto& scratch = AcquireAttackScratch(); const ControlScratchGuard guard{.MyDepth = _MyAttackDepth};
				GenericEnemies(_unit, scratch.MyTargets);
				for (const auto id : scratch.MyTargets) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyEndScale,
					.MyType = DamageType::ARTS, .MyTags = DamageTag::SKILL | DamageTag::BURST, .MyIsSkill = true});
			}
			(void)RemoveBuff(_unit, "trait:mint_keep"); (void)RemoveBuff(_unit, "talent:mint_taunt");
		}
		else return;
		if (unit.MySkillAura != NoPlayer) RefreshOperatorAura(unit.MySkillAura);
	}

	void BattleCore::MostmaSkill(UnitId _unit, const MostmaKit& _kit, const ContentEvent& _event)
	{
		if (_kit.MySkill != MostmaSkillKind::TIME_LOCK) return;
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::SKILL_START || _event.MyKind == ContentEventKind::SKILL_ENDING)
		{ unit.MyTimeLocked.clear(); unit.MyTimeLockAccumulator = 0; }
		if (_event.MyKind != ContentEventKind::SKILL_TICK) return;
		auto& scratch = AcquireAttackScratch();
		struct Guard
		{
			std::size_t& MyDepth;

			~Guard() { --MyDepth; }
		};
		const Guard guard{.MyDepth = _MyAttackDepth};
		const auto collect = [&]
		{
			scratch.MyTargets.clear();
			for (const auto id : _MyEnemyIds) if (Unit(id).MyAlive && !Unit(id).MyHidden && InRuleRange(_unit, id)) scratch.MyTargets.push_back(id);
		};
		collect();
		for (const auto id : scratch.MyTargets)
		{
			if (std::ranges::contains(unit.MyTimeLocked, id)) continue;
			unit.MyTimeLocked.push_back(id);
			(void)ApplyStatus(id, CombatStatus::STUN, std::max(0.01, unit.MySkill.MyTimeLeft), _unit);
		}
		unit.MyTimeLockAccumulator += _event.MyDelta;
		while (std::isgreaterequal(unit.MyTimeLockAccumulator, 1 - 1e-9))
		{
			unit.MyTimeLockAccumulator -= 1;
			collect();
			for (const auto id : scratch.MyTargets)
				(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyDamageScale, .MyType = DamageType::ARTS,
					.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
		}
	}

}
