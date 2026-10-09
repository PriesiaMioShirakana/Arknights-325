#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		struct ScorchScratchGuard
		{
			std::size_t& MyDepth;

			~ScorchScratchGuard() { --MyDepth; }
		};
	}

	void Battle::Reed2Scorch(UnitId _unit, UnitId _target)
	{
		const auto& unit = Unit(_unit); const auto& kit = std::get<Reed2Kit>(*unit.MyDefinition.MyOperatorKit);
		if (!Unit(_target).MyAlive) return;
		const auto duration = kit.MyDefault && unit.MySkill.MyActive ? std::max(0.1, unit.MySkill.MyTimeLeft) : kit.MyScorchDuration;
		(void)AddBuff(_target, {.MyKey = "reed2:scorch", .MySource = _unit, .MyDuration = duration, .MyRefresh = BuffRefresh::EXTEND,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyScorchAttack}}});
		if (std::isgreater(kit.MyScorchFragile, 0) && Unit(_target).MyAlive) (void)ApplyStatus(_target, CombatStatus::ARTS_FRAGILE, {.MyDuration = duration, .MySource = _unit, .MyValue = kit.MyScorchFragile});
	}

	void Battle::Reed2Kill(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_KILL || !_event.MyTarget || Unit(_event.MyTarget).MySide != UnitSide::ENEMY ||
			!std::ranges::any_of(Unit(_event.MyTarget).MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "reed2:scorch"; })) return;
		for (const auto id : _MyReed2s)
		{
			const auto& unit = Unit(id); const auto& kit = std::get<Reed2Kit>(*unit.MyDefinition.MyOperatorKit);
			if (unit.MyAlive && !unit.MyHidden && !unit.MyOperatorHooksReleased && unit.MySkill.MyActive && std::isgreater(kit.MyBurstScale, 0))
				Schedule({.MyAt = Time(), .MyKind = ScheduledKind::REED2_BURST, .MySource = id, .MyPoint = Unit(_event.MyTarget).MyPosition});
		}
	}

	void Battle::Reed2Burst(UnitId _unit, WorldPoint _point)
	{
		const auto& unit = Unit(_unit); const auto& kit = std::get<Reed2Kit>(*unit.MyDefinition.MyOperatorKit);
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) return;
		auto& scratch = AcquireAttackScratch(); const ScorchScratchGuard guard{.MyDepth = _MyAttackDepth};
		FoesInRadius(_point, kit.MyBurstRadius, scratch.MyTargets, true);
		for (const auto id : scratch.MyTargets)
		{
			(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * kit.MyBurstScale, .MyType = DamageType::ARTS, .MyTags = DamageTag::SKILL | DamageTag::SCORCH, .MyIsSplash = true, .MyIsSkill = true});
			Reed2Scorch(_unit, id);
		}
	}

	void Battle::Reed2Module(UnitId _unit)
	{
		const auto& unit = Unit(_unit); const auto& kit = std::get<Reed2Kit>(*unit.MyDefinition.MyOperatorKit);
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) return;
		auto& scratch = AcquireAttackScratch(); const ScorchScratchGuard guard{.MyDepth = _MyAttackDepth};
		HealingTargets(_unit, scratch.MyTargets, false);
		if (std::ranges::any_of(scratch.MyTargets, [&](UnitId _id) { return _id != _unit && Unit(_id).MyKind == UnitKind::OPERATOR; }))
			(void)AddBuff(_unit, {.MyKey = "reed2:corner", .MyDuration = 0.4, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::DAMAGE_DEALT_MULTIPLIER, .MyValue = kit.MyInjuredScale}}});
	}

	void Battle::Reed2Skill(UnitId _unit, const Reed2Kit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::BEFORE_HEAL && _event.MyTarget && _event.MyTarget != _unit && !_event.MyHealOptions.MyReflect)
		{
			_event.MyAmount *= _kit.MyHealBoost;
			if (std::isgreater(_kit.MyHealShare, 0) && std::isgreater(_event.MyAmount, 0)) (void)Heal(_unit, _unit, _event.MyAmount * _kit.MyHealShare, {.MySelf = true, .MyReflect = true});
		}
		if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY && Unit(_event.MyTarget).MyAlive && !_event.MyElement && !HasTag(_event.MyDamage.MyTags, DamageTag::SCORCH))
		{
			const auto chance = unit.MySkill.MyActive ? _kit.MySkillChance : _kit.MyChance;
			if (std::isgreaterequal(chance, 1) || (std::isgreater(chance, 0) && std::isless(_MyRandom.Next(), chance))) Reed2Scorch(_unit, _event.MyTarget);
		}
		if (_kit.MySkill == Reed2SkillKind::QUICK) return;
		if (_kit.MySkill == Reed2SkillKind::SEEDS)
		{
			if (_event.MyKind == ContentEventKind::SKILL_START) unit.MyReedAccumulator = 0;
			if (_event.MyKind != ContentEventKind::SKILL_TICK) return;
			unit.MyReedAccumulator += _event.MyDelta;
			if (std::isless(unit.MyReedAccumulator + 1e-9, 1)) return;
			unit.MyReedAccumulator -= 1;
			if (!std::isgreater(_kit.MyDot, 0)) return;
			auto& scratch = AcquireAttackScratch(); const ScorchScratchGuard guard{.MyDepth = _MyAttackDepth};
			for (const auto id : _MyEnemyIds) if (Unit(id).MyAlive && !Unit(id).MyHidden && std::ranges::any_of(Unit(id).MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "reed2:scorch"; })) scratch.MyTargets.push_back(id);
			for (const auto id : scratch.MyTargets) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyDot, .MyType = DamageType::ARTS, .MyTags = DamageTag::SKILL | DamageTag::SCORCH, .MyIsSkill = true});
			return;
		}
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			auto& scratch = AcquireAttackScratch(); const ScorchScratchGuard guard{.MyDepth = _MyAttackDepth};
			OperatorAlliesInRange(_unit, scratch.MyTargets);
			std::erase_if(scratch.MyTargets, [&](UnitId _id) { return Unit(_id).MyKind != UnitKind::OPERATOR; });
			std::ranges::sort(scratch.MyTargets, [&](UnitId _left, UnitId _right)
			{
				const auto& left = Unit(_left); const auto& right = Unit(_right);
				return std::tuple{_left == _unit, !left.MyGround, left.MyHealth / left.MyStats.MyMaxHealth, left.MyDeploySequence} < std::tuple{_right == _unit, !right.MyGround, right.MyHealth / right.MyStats.MyMaxHealth, right.MyDeploySequence};
			});
			if (scratch.MyTargets.size() > _kit.MyCarriers) scratch.MyTargets.resize(_kit.MyCarriers);
			unit.MyReedCarriers.clear();
			for (const auto id : scratch.MyTargets)
			{
				unit.MyReedCarriers.push_back({.MyUnit = id});
				(void)AddBuff(id, {.MyKey = unit.MyReedFireKey, .MySource = _unit, .MyDuration = unit.MySkill.MyTimeLeft});
			}
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			auto carriers = std::move(unit.MyReedCarriers); unit.MyReedCarriers.reserve(_kit.MyCarriers);
			for (const auto& carrier : carriers) (void)RemoveBuff(carrier.MyUnit, unit.MyReedFireKey);
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK)
		{
			auto& scratch = AcquireAttackScratch(); const ScorchScratchGuard guard{.MyDepth = _MyAttackDepth};
			for (auto& carrier : unit.MyReedCarriers)
			{
				if (!Unit(carrier.MyUnit).MyAlive || Unit(carrier.MyUnit).MyHidden) continue;
				carrier.MyAccumulator += _event.MyDelta;
				if (std::isgreaterequal(carrier.MyAccumulator + 1e-9, _kit.MyFireballCooldown)) { carrier.MyAccumulator -= _kit.MyFireballCooldown; scratch.MySeen.push_back(carrier.MyUnit); }
			}
			const AttackProfile filter{.MyCanHitFlying = true};
			for (const auto carrier : scratch.MySeen)
			{
				scratch.MyTargets.clear();
				for (const auto id : _MyEnemyIds) if (TargetableEnemy(Unit(id), filter) && InRuleRange(carrier, id)) scratch.MyTargets.push_back(id);
				if (scratch.MyTargets.empty()) for (const auto id : _MyEnemyIds) if (TargetableEnemy(Unit(id), filter) && InRuleRange(_unit, id)) scratch.MyTargets.push_back(id);
				SortOperatorTargets(carrier, scratch.MyTargets, 1);
				if (!scratch.MyTargets.empty()) (void)DealDamage(_unit, scratch.MyTargets.front(), {.MyAmount = unit.MyStats.MyAttack * _kit.MyFireballScale, .MyType = DamageType::ARTS, .MyTags = DamageTag::SKILL | DamageTag::FIREBALL, .MyTraitAlly = carrier, .MyIsSkill = true});
			}
		}
	}
}
