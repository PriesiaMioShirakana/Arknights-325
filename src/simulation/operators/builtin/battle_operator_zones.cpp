#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct OperatorZoneScratchGuard
		{
			std::size_t& MyDepth;

			~OperatorZoneScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::TinmanStartZone(UnitId _unit, const TinmanKit& _kit)
	{
		auto& source = _MyUnits[Index(_unit)];
		auto& scratch = AcquireAttackScratch(); const OperatorZoneScratchGuard guard{_MyAttackDepth};
		GenericEnemies(_unit, scratch.MyTargets);
		const auto ground = std::ranges::find_if(scratch.MyTargets, [&](UnitId id) { return !Unit(id).Flying(); });
		const auto target = ground != scratch.MyTargets.end() ? *ground : scratch.MyTargets.empty() ? UnitId{} : scratch.MyTargets.front();
		const auto origin = RulePosition(source); const auto forward = RotateOffset({0, 1}, source.MyFacing);
		const auto point = target ? Unit(target).MyPosition : WorldPoint{origin.MyX + forward.MyColumn, origin.MyY + forward.MyRow};
		const auto interval = _kit.MyWeakZone ? 0.25 : 1.0;
		++source.MyTinmanZones;
		const auto sequence = _kit.MyWeakZone ? 0 : ++source.MyTinmanZoneSequence;
		// 队列保存施放快照，按单位计数跨退场保留；循环复用同一个值类型动作。
		Schedule({.MyAt = Time(), .MyKind = ScheduledKind::TINMAN_ZONE, .MySource = _unit, .MyHandle = sequence, .MyInterval = interval,
			.MyPoint = point, .MyAmount = source.MyStats.MyAttack,
			.MyRemaining = static_cast<unsigned>(std::max(1.0, std::floor(_kit.MyDuration / interval + 0.5)))});
	}

	void BattleCore::TinmanZonePulse(UnitId _unit, WorldPoint _point, double _attack, std::uint64_t _sequence, bool _damage)
	{
		const auto& kit = std::get<TinmanKit>(*Unit(_unit).MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch(); const OperatorZoneScratchGuard guard{_MyAttackDepth};
		const AttackProfile filter{.MyHitSleep = true};
		for (const auto id : _MyEnemyIds)
			if (TargetableEnemy(Unit(id), filter) && !Unit(id).Flying() && BodyDistance(Unit(id), _point) <= kit.MyRadius + 1e-9) scratch.MyTargets.push_back(id);
		for (const auto id : scratch.MyTargets)
		{
			if (kit.MyWeakZone && kit.MyWeaken > 0)
				(void)ApplyStatus(id, CombatStatus::WEAKEN, {.MyDuration = 0.3, .MySource = _unit, .MyValue = kit.MyWeaken});
			if (kit.MyWitherScale > 1)
				(void)AddBuff(id, {.MyKey = "tinman:wither", .MySource = _unit, .MyDuration = kit.MyWeakZone ? 0.3 : 1.05,
					.MyStrength = BuffStrength{.MyValue = kit.MyWitherScale}});
			if (!kit.MyWeakZone || (_damage && kit.MyDamageScale > 0))
				(void)DealDamage(_unit, id, {.MyAmount = _attack * kit.MyDamageScale, .MyType = DamageType::ARTS, .MyCanDodge = false,
					.MyTags = DamageTag::DOT | DamageTag::ZONE, .MyIsSkill = true});
		}
		if (kit.MyWeakZone || !(kit.MyRegenRatio > 0)) return;
		scratch.MyTargets.clear();
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id); const auto point = RulePosition(ally);
			const double dx = point.MyX - _point.MyX, dy = point.MyY - _point.MyY;
			if (ally.MyAlive && !ally.MyHidden && ally.MyKind != UnitKind::DEVICE && (id == _unit || !ally.MyStatuses.Has(CombatStatus::ISOLATED)) &&
				dx * dx + dy * dy <= kit.MyRadius * kit.MyRadius + 1e-9) scratch.MyTargets.push_back(id);
		}
		const auto key = "tinman:zone:" + std::to_string(_unit) + ":" + std::to_string(_sequence);
		for (const auto id : scratch.MyTargets)
			(void)AddBuff(id, {.MyKey = key, .MySource = _unit, .MyDuration = 1 + 2 * BattleClock::StepSeconds,
				.MyModifiers = std::vector<AttributeChange>{{Attribute::HEALTH_REGEN, _attack * kit.MyRegenRatio}}});
	}
}
