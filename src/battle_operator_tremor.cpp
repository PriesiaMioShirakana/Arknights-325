#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		constexpr std::array AbnormalStatuses{CombatStatus::STUN, CombatStatus::FREEZE, CombatStatus::COLD, CombatStatus::SLEEP, CombatStatus::SILENCE,
			CombatStatus::FEAR, CombatStatus::ATTRACT, CombatStatus::TREMBLE, CombatStatus::BIND, CombatStatus::LEVITATE, CombatStatus::DISARM,
			CombatStatus::PALSY, CombatStatus::SLUGGISH, CombatStatus::SLOW};

		struct TremorScratchGuard
		{
			std::size_t& MyDepth;

			~TremorScratchGuard() { --MyDepth; }
		};
	}

	bool Battle::AbnormalStatus(CombatStatus _status)
	{
		return std::ranges::contains(AbnormalStatuses, _status);
	}

	bool Battle::HasAbnormal(UnitId _unit) const
	{
		const auto& unit = Unit(_unit);
		return std::ranges::any_of(AbnormalStatuses, [&](CombatStatus _status) { return std::isgreater(unit.MyStatuses.MyRemaining[static_cast<std::size_t>(_status)], 0); }) ||
			std::ranges::any_of(unit.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyStatus && std::ranges::contains(AbnormalStatuses, *_buff.MyDefinition.MyStatus); });
	}

	unsigned Battle::CleanseAbnormal(UnitId _unit)
	{
		unsigned count = 0;
		for (const auto status : AbnormalStatuses)
		{
			const auto& unit = Unit(_unit);
			if (std::isgreater(unit.MyStatuses.MyRemaining[static_cast<std::size_t>(status)], 0) || std::ranges::any_of(unit.MyBuffs, [&](const auto& _buff) { return _buff.MyDefinition.MyStatus == status; }))
			{ (void)RemoveStatus(_unit, status); ++count; }
		}
		return count;
	}

	void Battle::PepeCleanse(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased || unit.MySkill.MyActive || !unit.MySkill.MyCharges || !HasAbnormal(_unit)) return;
		(void)CleanseAbnormal(_unit); (void)ActivateSkill(_unit, false, SkillReason::ABNORMAL);
	}

	void Battle::PepeBeforeAttack(CombatUnit& _unit, const PepeKit& _kit, std::vector<UnitId>& _targets)
	{
		auto& scratch = AcquireAttackScratch(); const TremorScratchGuard guard{.MyDepth = _MyAttackDepth};
		const auto& profile = EffectiveAttack(_unit);
		if (_kit.MySkill == PepeSkillKind::CHAOS && _unit.MySkill.MyActive)
		{
			for (const auto id : _MyEnemyIds) if (TargetableEnemy(Unit(id), profile) && InRuleRange(_unit.MyId, id)) scratch.MyTargets.push_back(id);
			for (const auto id : _unit.MyBlocking)
				if (Unit(id).MyBlockedBy == _unit.MyId && TargetableEnemy(Unit(id), profile) && !std::ranges::contains(scratch.MyTargets, id)) scratch.MyTargets.push_back(id);
			if (!scratch.MyTargets.empty()) _targets.assign(1, scratch.MyTargets[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MyTargets.size()))]);
		}
		if (!std::isgreater(_kit.MyCrowdScale, 1)) return;
		_unit.MyPepeBoost = false;
		if (_targets.empty()) return;
		FoesInRadius(Unit(_targets.front()).MyPosition, profile.MySplashRadius, scratch.MyTargets);
		_unit.MyPepeBoost = std::isgreaterequal(static_cast<double>(scratch.MyTargets.size()), _kit.MyCrowdCount);
	}

	void Battle::PepeSkill(UnitId _unit, const PepeKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyDamage.MyIsAttack && unit.MyPepeBoost) _event.MyDamage.MyMultiplier *= _kit.MyCrowdScale;
		if (_event.MyKind == ContentEventKind::DAMAGED && unit.MySkill.MyActive && _event.MyDamage.MyIsSplash && _event.MyTarget && Unit(_event.MyTarget).MyAlive && std::isgreater(_kit.MyStun, 0)) (void)ApplyStatus(_event.MyTarget, CombatStatus::STUN, _kit.MyStun, _unit);
		if (_event.MyKind == ContentEventKind::BEFORE_KILL && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY && unit.MySkill.MyActive) ++unit.MyPepeKills;
		if (_kit.MySkill == PepeSkillKind::STAMP) return;
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			unit.MyPepeKills = 0;
			if (_kit.MySkill == PepeSkillKind::CHAOS)
			{
				if (unit.MyPepeRage) (void)AddBuff(_unit, {.MyKey = "pepe:rage", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = _kit.MyRageSpeed * unit.MyPepeRage}}});
				unit.MyPepeRage = std::min(_kit.MyMaxRage, unit.MyPepeRage + 1);
			}
			else
			{
				unit.MyPepeStacks = 0; unit.MyPepeRadius = unit.MyDefinition.MyAttack.MySplashRadius;
				if (unit.MyDefinition.MySkill.MyAttack) unit.MyDefinition.MySkill.MyAttack->MySplashRadius = unit.MyPepeRadius;
			}
		}
		else if (_event.MyKind == ContentEventKind::ATTACK && _kit.MySkill == PepeSkillKind::TREMOR && unit.MySkill.MyActive && unit.MyPepeStacks < _kit.MyMaxStacks)
		{
			++unit.MyPepeStacks;
			(void)AddBuff(_unit, {.MyKey = "pepe:tremor", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _kit.MyStackAttack * unit.MyPepeStacks}}});
			if (unit.MyDefinition.MySkill.MyAttack) unit.MyDefinition.MySkill.MyAttack->MySplashRadius = unit.MyPepeRadius + _kit.MyRadiusGrowth * unit.MyPepeStacks;
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			(void)RemoveBuff(_unit, _kit.MySkill == PepeSkillKind::CHAOS ? "pepe:rage" : "pepe:tremor");
			if (_kit.MySkill == PepeSkillKind::TREMOR && unit.MyDefinition.MySkill.MyAttack) unit.MyDefinition.MySkill.MyAttack->MySplashRadius = unit.MyPepeRadius;
			const auto gain = std::min(_kit.MyMaxSp, unit.MyPepeKills * _kit.MyKillSp); unit.MyPepeKills = 0;
			if (unit.MyAlive && std::isgreater(gain, 0)) (void)GainSp(_unit, gain, SpReason::TALENT);
		}
	}
}
