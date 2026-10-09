#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		struct LinkingScratchGuard
		{
			std::size_t& MyDepth;

			~LinkingScratchGuard() { --MyDepth; }
		};
	}

	void Battle::Halo2BeforeAttack(CombatUnit& _unit, const Halo2Kit& _kit, std::vector<UnitId>& _targets)
	{
		if (!_unit.MySkill.MyActive) return;
		if (_kit.MySkill == Halo2SkillKind::GRAVITY && _unit.MySkill.MyPending)
		{
			UnitId best = 0; double farthest = 0;
			for (const auto id : _MyEnemyIds)
			{
				if (!TargetableEnemy(Unit(id), EffectiveAttack(_unit)) || !InRuleRange(_unit.MyId, id)) continue;
				const auto distance = RemainingDistance(id);
				if (!best || std::isgreater(distance, farthest + 1e-9)) { best = id; farthest = distance; }
			}
			if (best) _targets.assign(1, best);
		}
		if (!_kit.MyDefault || _kit.MySkill != Halo2SkillKind::LINKS) return;
		const AttackProfile filter{.MyCanHitFlying = true};
		std::erase_if(_unit.MyHaloLocks, [&](UnitId _id) { return !TargetableEnemy(Unit(_id), filter) || !InRuleRange(_unit.MyId, _id); });
		for (const auto id : _targets)
			if (_unit.MyHaloLocks.size() < _kit.MyTargets && !std::ranges::contains(_unit.MyHaloLocks, id)) _unit.MyHaloLocks.push_back(id);
		_targets.assign(_unit.MyHaloLocks.begin(), _unit.MyHaloLocks.end());
	}

	void Battle::Halo2Hit(UnitId _unit, UnitId _target, const Halo2Kit& _kit)
	{
		const auto& unit = Unit(_unit); const AttackProfile filter{.MyCanHitFlying = true};
		auto& scratch = AcquireAttackScratch(); const LinkingScratchGuard guard{.MyDepth = _MyAttackDepth};
		if (_kit.MySkill == Halo2SkillKind::STARS)
		{
			auto previous = _target; auto point = Unit(previous).MyPosition;
			for (unsigned bounce = 0; bounce < _kit.MyBounces; ++bounce)
			{
				FoesInRadius(point, _kit.MyBounceRadius, scratch.MyTargets);
				UnitId best = 0; double nearest = std::numeric_limits<double>::infinity();
				for (const auto id : scratch.MyTargets)
				{
					if (id == previous || !TargetableEnemy(Unit(id), filter)) continue;
					const auto distance = BodyDistance(Unit(id), point);
					if (std::isless(distance, nearest - 1e-9)) { best = id; nearest = distance; }
				}
				if (!best) break;
				(void)DealDamage(_unit, best, {.MyAmount = unit.MyStats.MyAttack, .MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::CHAIN), .MyIsAttack = true, .MyIsSkill = true});
				const auto duration = unit.MyDefinition.MyAttack.MyOnHitStatus == CombatStatus::SLUGGISH ? unit.MyDefinition.MyAttack.MyOnHitApplication.MyDuration : 0;
				if (std::isgreater(duration, 0) && Unit(best).MyAlive) (void)ApplyStatus(best, CombatStatus::SLUGGISH, duration, _unit);
				previous = best; point = Unit(best).MyPosition;
			}
			return;
		}
		if (_kit.MySkill != Halo2SkillKind::GRAVITY) return;
		const auto point = Unit(_target).MyPosition;
		FoesInRadius(point, _kit.MyPullRadius, scratch.MyTargets);
		std::erase_if(scratch.MyTargets, [&](UnitId _id) { return _id == _target || !TargetableEnemy(Unit(_id), filter); });
		std::ranges::sort(scratch.MyTargets, [&](UnitId _left, UnitId _right)
		{
			const auto left = BodyDistance(Unit(_left), point), right = BodyDistance(Unit(_right), point);
			return std::islessgreater(left, right) ? std::isless(left, right) : Unit(_left).MySpawnSequence < Unit(_right).MySpawnSequence;
		});
		if (scratch.MyTargets.size() > _kit.MyPullTargets) scratch.MyTargets.resize(_kit.MyPullTargets);
		for (const auto id : scratch.MyTargets)
		{
			(void)Pull(id, _kit.MyPullForce, {.MyTo = point, .MyStopRadius = 0.3});
			if (Unit(id).MyAlive) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyLinkScale, .MyType = DamageType::ARTS, .MyTags = DamageTag::SKILL | DamageTag::LINK, .MyIsSkill = true});
		}
	}

	void Battle::Halo2Observe(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::STATUS_APPLIED && _event.MySource && _event.MyStatus == CombatStatus::SLUGGISH)
		{
			auto& unit = _MyUnits[Index(_event.MySource)]; const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<Halo2Kit>(unit.MyDefinition.MyOperatorKit) : nullptr;
			if (kit && unit.MyAlive && !unit.MyHidden && !unit.MyOperatorHooksReleased && unit.MyHaloStacks < kit->MyMaxStacks)
			{
				++unit.MyHaloStacks;
				(void)AddBuff(unit.MyId, {.MyKey = "halo2:model", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = kit->MyStackSpeed * unit.MyHaloStacks},
					{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = unit.MyHaloStacks >= kit->MyMaxStacks ? kit->MyFullAttack : 0}}});
			}
		}
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MyTarget || _event.MyDamage.MyType != DamageType::ARTS || HasTag(_event.MyDamage.MyTags, DamageTag::LINK)) return;
		for (const auto id : _MyHalo2s)
		{
			auto& unit = _MyUnits[Index(id)]; const auto& kit = std::get<Halo2Kit>(*unit.MyDefinition.MyOperatorKit);
			if (!kit.MyDefault || unit.MyOperatorHooksReleased || !std::isgreater(kit.MyShare, 0) || !unit.MySkill.MyActive || unit.MyHaloLocks.size() < 2 || unit.MyHaloLinking || !std::ranges::contains(unit.MyHaloLocks, _event.MyTarget)) continue;
			auto& scratch = AcquireAttackScratch(); const LinkingScratchGuard guard{.MyDepth = _MyAttackDepth};
			scratch.MyTargets.assign(unit.MyHaloLocks.begin(), unit.MyHaloLocks.end());
			unit.MyHaloLinking = true;
			struct LinkingGuard { bool& MyActive; ~LinkingGuard() { MyActive = false; } } linking{.MyActive = unit.MyHaloLinking};
			for (const auto other : scratch.MyTargets)
				if (other != _event.MyTarget && Unit(other).MyAlive) (void)DealDamage(_event.MySource ? _event.MySource : id, other, {.MyAmount = _event.MyDamage.MyAmount * kit.MyShare,
					.MyType = DamageType::ARTS, .MyCanDodge = false, .MyTags = DamageTag::SKILL | DamageTag::LINK, .MyIsSkill = true});
		}
	}

	void Battle::Halo2Pulse(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto& kit = std::get<Halo2Kit>(*unit.MyDefinition.MyOperatorKit);
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) return;
		if (unit.MyHaloStay.size() < _MyUnits.size() + 1) unit.MyHaloStay.resize(_MyUnits.size() + 1);
		const AttackProfile filter{.MyCanHitFlying = true};
		for (std::size_t i = 0; i < _MyEnemyIds.size(); ++i)
		{
			const auto id = _MyEnemyIds[i]; if (id >= unit.MyHaloStay.size()) unit.MyHaloStay.resize(_MyUnits.size() + 1);
			auto& stay = unit.MyHaloStay[id];
			if (!TargetableEnemy(Unit(id), filter) || !InRuleRange(_unit, id)) { stay = 0; continue; }
			stay += 0.25; const auto scale = std::isgreater(stay, kit.MyMatureAfter) ? kit.MyMatureFragile : kit.MyFragile;
			if (std::isgreater(scale, 1)) (void)ApplyStatus(id, CombatStatus::FRAGILE, {.MyDuration = 0.4, .MySource = _unit, .MyValue = scale - 1});
		}
	}
}
