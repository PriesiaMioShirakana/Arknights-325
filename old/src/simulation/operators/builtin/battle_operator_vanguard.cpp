#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct VanguardScratchGuard
		{
			std::size_t& MyDepth;

			~VanguardScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::VulpisPunish(UnitId _unit, UnitId _target, const VulpisKit& _kit)
	{
		const auto& unit = Unit(_unit);
		if (_target && Unit(_target).MyAlive) (void)DealDamage(_unit, _target, {.MyAmount = unit.MyStats.MyAttack * _kit.MyDamageScale,
			.MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
		if (unit.MyOwner != NoPlayer && std::isgreater(_kit.MyDp, 0)) (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyDp);
	}

	void BattleCore::VulpisSkill(UnitId _unit, const VulpisKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyTarget && !_event.MyElement && !unit.MyVulpisBusy && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
		{
			const auto found = std::ranges::find(unit.MyVulpisMarks, _event.MyTarget, &TimedTargetMark::MyTarget);
			if (found == unit.MyVulpisMarks.end()) unit.MyVulpisMarks.push_back({.MyTarget = _event.MyTarget, .MyTime = Time()});
			else if (std::islessequal(Time() - found->MyTime, _kit.MyMarkDuration + 1e-9) && Unit(_event.MyTarget).MyAlive)
			{
				struct BusyGuard
				{
					bool& MyBusy;

					~BusyGuard() { MyBusy = false; }
				};
				unit.MyVulpisBusy = true; const BusyGuard guard{.MyBusy = unit.MyVulpisBusy};
				(void)DealDamage(_unit, _event.MyTarget, {.MyAmount = unit.MyStats.MyAttack * _kit.MyMarkScale, .MyType = DamageType::ARTS,
					.MyCanDodge = false, .MyTags = DamageTag::TALENT | DamageTag::ADDITION});
			}
		}
		if (_event.MyKind == ContentEventKind::BEFORE_KILL && _event.MySource == _unit && unit.MySkill.MyActive) unit.MyVulpisKill = true;
		if (_event.MyKind == ContentEventKind::SKILL_START && _kit.MySkill != VulpisSkillKind::PUNISH)
		{
			if (_kit.MySkill == VulpisSkillKind::CAMOUFLAGE)
			{
				(void)RemoveBuff(_unit, "vulpis:camou");
				unit.MyVulpisKill = false;
			}
			if (unit.MyOwner != NoPlayer && std::isgreater(_kit.MyDp, 0)) (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyDp);
			if (_kit.MySkill == VulpisSkillKind::TORTURE)
			{
				auto& scratch = AcquireAttackScratch(); const VanguardScratchGuard guard{.MyDepth = _MyAttackDepth};
				if (_kit.MyHasRange) OperatorEnemiesInGrid(_unit, _kit.MyRange, scratch.MyTargets);
				else GenericEnemies(_unit, scratch.MyTargets);
				if (scratch.MyTargets.size() > _kit.MyTargets) scratch.MyTargets.resize(_kit.MyTargets);
				for (const auto id : scratch.MyTargets)
				{
					const bool was = std::ranges::any_of(Unit(id).MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "sluggish"; });
					(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyDamageScale, .MyType = DamageType::ARTS,
						.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
					if (!Unit(id).MyAlive) continue;
					(void)ApplyStatus(id, CombatStatus::SLUGGISH, _kit.MySluggish, _unit);
					if (was && Unit(id).MyAlive) (void)ApplyStatus(id, CombatStatus::STUN, _kit.MyStun, _unit);
				}
			}
			else
			{
				(void)AddBuff(_unit, {.MyKey = "skill:vulpis_aspd", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = _kit.MyAttackSpeed}}});
				unit.MyVulpisDuration = unit.MySkill.MyTimeLeft;
			}
		}
		if (_kit.MySkill != VulpisSkillKind::CAMOUFLAGE) return;
		if (_event.MyKind == ContentEventKind::SKILL_TICK)
		{
			const auto found = std::ranges::find(unit.MyBuffs, "skill:vulpis_aspd", [](const auto& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
			if (found != unit.MyBuffs.end() && found->MyDefinition.MyModifiers)
			{
				found->MyDefinition.MyModifiers->front().MyValue = _kit.MyAttackSpeed * std::max(0.0, unit.MySkill.MyTimeLeft) / std::max(0.01, unit.MyVulpisDuration);
				Recalculate(unit);
			}
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			(void)RemoveBuff(_unit, "skill:vulpis_aspd");
			if (_event.MySkillReason != SkillReason::DEATH && unit.MyAlive && unit.MyVulpisKill)
			{
				StatusFlags flags; flags.set(static_cast<std::size_t>(CombatStatus::CAMOUFLAGE));
				(void)AddBuff(_unit, {.MyKey = "vulpis:camou", .MySource = _unit, .MyFlags = flags, .MyStatus = CombatStatus::CAMOUFLAGE});
			}
		}
	}

	void BattleCore::VulpisTick(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<VulpisKit>(*unit.MyDefinition.MyOperatorKit);
		const auto has = [&](std::string_view _key) { return std::ranges::any_of(unit.MyBuffs, [&](const auto& _buff) { return _buff.MyDefinition.MyKey == _key; }); };
		if (unit.MyAlive)
		{
			if (std::isgreater(kit.MyDpBonus, 0) && unit.MyOwner != NoPlayer && !std::ranges::any_of(_MyVulpises, [&](UnitId _other)
				{ return _other < _unit && Unit(_other).MyAlive && Unit(_other).MyOwner == unit.MyOwner; }))
				(void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _MyInput.MyDpPerSecond * BattleClock::StepSeconds * kit.MyDpBonus);
			const bool regen = std::isgreaterequal(Time() - unit.MyLastHitAt, kit.MyQuietTime) && std::isgreater(kit.MyRegenRatio, 0);
			const bool present = has("talent:vulpis_regen");
			if (regen && !present) (void)AddBuff(_unit, {.MyKey = "talent:vulpis_regen", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALTH_REGEN_RATIO, .MyValue = kit.MyRegenRatio}}});
			else if (!regen && present) (void)RemoveBuff(_unit, "talent:vulpis_regen");
		}
		if (!std::isgreater(kit.MyBlockingAttack, 0) && !std::isgreater(kit.MyBlockingDefense, 0)) return;
		const bool blocking = unit.MyAlive && !unit.MyBlocking.empty(), present = has("trait:vulpis_block");
		if (blocking && !present) (void)AddBuff(_unit, {.MyKey = "trait:vulpis_block", .MyModifiers = std::vector<AttributeChange>{
			{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyBlockingAttack}, {.MyAttribute = Attribute::DEFENSE_PERCENT, .MyValue = kit.MyBlockingDefense}}});
		else if (!blocking && present) (void)RemoveBuff(_unit, "trait:vulpis_block");
	}
}
