#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		struct StormScratchGuard
		{
			std::size_t& MyDepth;

			~StormScratchGuard() { --MyDepth; }
		};
	}

	void Battle::PasngrSkill(UnitId _unit, const PasngrKit& _kit, ContentEvent& _event)
	{
		const auto& unit = Unit(_unit);
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
		{
			const auto& target = Unit(_event.MyTarget);
			if (_event.MyDamage.MyIsAttack && std::isgreaterequal(target.MyHealth / target.MyStats.MyMaxHealth, _kit.MyHealthThreshold - 1e-9))
				(void)AddBuff(target.MyId, {.MyKey = unit.MyPasngrEnhanceKey, .MySource = _unit, .MyDuration = _kit.MyEnhanceDuration});
			if (std::ranges::any_of(target.MyBuffs, [&](const auto& _buff) { return _buff.MyDefinition.MyKey == unit.MyPasngrEnhanceKey; })) _event.MyDamage.MyMultiplier *= _kit.MyEnhanceScale;
		}
		if (_event.MyKind != ContentEventKind::SKILL_START || _kit.MySkill != PasngrSkillKind::STORM) return;
		auto& scratch = AcquireAttackScratch(); const StormScratchGuard guard{.MyDepth = _MyAttackDepth};
		const auto range = _kit.MyRange.empty() ? RuleRange(unit) : RangeMask(RulePosition(unit), unit.MyFacing, _kit.MyRange, unit.MyStats.MyRangeExtend);
		const AttackProfile filter{.MyCanHitFlying = true};
		UnitId target = 0;
		for (const auto id : _MyEnemyIds)
			if (TargetableEnemy(Unit(id), filter) && BodyInRange(Unit(id), range) && (!target || std::isgreater(Unit(id).MyHealth, Unit(target).MyHealth))) target = id;
		if (!target) return;
		Schedule({.MyAt = Time() + _kit.MyInterval, .MyKind = ScheduledKind::PASNGR_STORM, .MySource = _unit,
			.MyInterval = _kit.MyInterval, .MyVersion = unit.MyDeploySequence, .MyPoint = Unit(target).MyPosition, .MyRemaining = _kit.MyStrikes});
	}

	void Battle::PasngrStorm(UnitId _unit, WorldPoint _point)
	{
		const auto& unit = Unit(_unit); const auto& kit = std::get<PasngrKit>(*unit.MyDefinition.MyOperatorKit);
		const auto& profile = unit.MyDefinition.MyAttack; const AttackProfile filter{.MyCanHitFlying = true};
		const auto falloff = kit.MyFalloff.value_or(profile.MyChainFalloff), sluggish = kit.MySluggish.value_or(profile.MyChainSluggish);
		const auto radius = std::isgreater(profile.MyChainRadius, 0) ? profile.MyChainRadius : 1.7;
		auto& scratch = AcquireAttackScratch(); const StormScratchGuard guard{.MyDepth = _MyAttackDepth};
		FoesInRadius(_point, 1.5, scratch.MyTargets);
		std::erase_if(scratch.MyTargets, [&](UnitId _id) { return !TargetableEnemy(Unit(_id), filter); });
		if (scratch.MyTargets.empty()) return;
		UnitId target = scratch.MyTargets[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MyTargets.size()))];
		for (unsigned n = 0; n < kit.MyChainCount && target; ++n)
		{
			scratch.MySeen.push_back(target);
			(void)DealDamage(_unit, target, {.MyAmount = unit.MyStats.MyAttack * kit.MyStormScale * std::pow(1 - falloff, n), .MyType = DamageType::ARTS,
				.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsAttack = true, .MyIsSkill = true});
			if (std::isgreater(sluggish, 0) && Unit(target).MyAlive) (void)ApplyStatus(target, CombatStatus::SLUGGISH, sluggish, _unit);
			const auto point = Unit(target).MyPosition; FoesInRadius(point, radius, scratch.MyTargets);
			UnitId next = 0; double best = std::numeric_limits<double>::infinity();
			for (const auto id : scratch.MyTargets)
			{
				if (std::ranges::contains(scratch.MySeen, id) || !TargetableEnemy(Unit(id), filter)) continue;
				const auto distance = BodyDistance(Unit(id), point);
				if (std::isless(distance, best - 1e-9)) { best = distance; next = id; }
			}
			target = next;
		}
	}
}
