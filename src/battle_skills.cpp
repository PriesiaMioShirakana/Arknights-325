#include <algorithm>
#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		constexpr double OperationCooldown = 3;

		bool Contains(const std::bitset<399>& _mask, WorldPoint _point) noexcept
		{
			const auto row = static_cast<int>(std::floor(_point.MyY + 0.5));
			const auto column = static_cast<int>(std::floor(_point.MyX + 0.5));
			return row >= 0 && row < 19 && column >= 0 && column < 21 && _mask[static_cast<std::size_t>(row * 21 + column)];
		}
	}

	const AttackProfile& Battle::EffectiveAttack(const CombatUnit& _unit) const noexcept
	{
		const auto& definition = _unit.MyDefinition.MySkill;
		const auto& skill = _unit.MySkill;
		if (skill.MyActive && definition.MyAttack &&
			(IsTimedSkill(definition.MyKind) || definition.MyKind == SkillKind::PASSIVE || skill.MyPending))
			return *definition.MyAttack;
		return _unit.MyDefinition.MyAttack;
	}

	bool Battle::AttackDisabled(const CombatUnit& _unit) const noexcept
	{
		const auto& profile = EffectiveAttack(_unit);
		return profile.MyDisabled || (profile.MyOnlyDuringSkill && !_unit.MySkill.MyActive);
	}

	void Battle::SetSkillTrigger(UnitId _unit, SkillTrigger _rule, std::vector<RangeOffset> _grid)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (static_cast<unsigned>(_rule) > static_cast<unsigned>(SkillTrigger::NEVER)) throw std::invalid_argument("invalid skill trigger");
		for (const auto offset : _grid)
			if (offset.MyRow < -100 || offset.MyRow > 100 || offset.MyColumn < -100 || offset.MyColumn > 100)
				throw std::invalid_argument("invalid skill trigger grid");
		if (Finished()) return;
		unit.MyDefinition.MySkill.MyTrigger = _rule;
		unit.MyDefinition.MySkill.MyTriggerRange = std::move(_grid);
		RefreshRange(unit);
	}

	std::uint64_t Battle::AddSkillTriggerRange(UnitId _unit, SkillTriggerArea _area)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_area.MySourceUnit && Unit(_area.MySourceUnit).MySide != UnitSide::ALLY)
			throw std::invalid_argument("trigger range source must be an ally");
		if (Finished()) return 0;
		const auto id = ++_MyTriggerRangeSequence;
		unit.MySkill.MyExtraRanges.emplace_back(SkillTriggerRange{.MyId = id, .MyArea = _area});
		return id;
	}

	bool Battle::RemoveSkillTriggerRange(UnitId _unit, std::uint64_t _range)
	{
		auto& ranges = _MyUnits[Index(_unit)].MySkill.MyExtraRanges;
		return std::erase_if(ranges, [&](const SkillTriggerRange& _entry) { return _entry.MyId == _range; }) != 0;
	}

	bool Battle::UpdateSkillTriggerRange(UnitId _unit, std::uint64_t _range, SkillTriggerArea _area)
	{
		auto& ranges = _MyUnits[Index(_unit)].MySkill.MyExtraRanges;
		if (_area.MySourceUnit && Unit(_area.MySourceUnit).MySide != UnitSide::ALLY)
			throw std::invalid_argument("trigger range source must be an ally");
		if (Finished()) return false;
		const auto found = std::ranges::find(ranges, _range, &SkillTriggerRange::MyId);
		if (found == ranges.end()) return false;
		found->MyArea = _area;
		return true;
	}

	bool Battle::ExtraSkillTriggerSatisfied(const CombatUnit& _unit) const
	{
		for (const auto& range : _unit.MySkill.MyExtraRanges)
		{
			const auto& area = range.MyArea;
			if (area.MySourceUnit)
			{
				const auto& source = Unit(area.MySourceUnit);
				if (!source.MyAlive || source.MyHidden) continue;
			}
			const auto& mask = area.MySourceUnit ? RuleRange(area.MySourceUnit) : area.MyMask;
			const AttackProfile profile{.MyCanHitFlying = area.MyCanHitFlying, .MyHitSleep = area.MyHitSleep, .MyGroundOnly = area.MyGroundOnly};
			for (const auto id : _MyEnemyIds)
				if (const auto& target = Unit(id); TargetableEnemy(target, profile) && BodyInRange(target, mask)) return true;
		}
		return false;
	}

	double Battle::SpCost(UnitId _unit) const
	{
		const auto& unit = Unit(_unit);
		const auto& definition = unit.MyDefinition.MySkill;
		if (definition.MyKind == SkillKind::NONE || definition.MyKind == SkillKind::PASSIVE) return 0;
		const auto cost = std::floor(definition.MySpCost * unit.MySkill.MySpCostMultiplier + unit.MyStats.MySpCostFlat);
		return std::isfinite(cost) ? std::max(0.0, cost) : definition.MySpCost;
	}

	double Battle::SpTotal(UnitId _unit) const
	{
		const auto& unit = Unit(_unit);
		const auto& definition = unit.MyDefinition.MySkill;
		if (definition.MyKind == SkillKind::NONE || definition.MyKind == SkillKind::PASSIVE) return 0;
		const auto& skill = unit.MySkill;
		return skill.MyCharges >= definition.MyMaxCharges ? definition.MyMaxCharges * SpCost(_unit)
			: skill.MyCharges * SpCost(_unit) + skill.MySp;
	}

	void Battle::NormalizeSp(CombatUnit& _unit)
	{
		const auto& definition = _unit.MyDefinition.MySkill;
		if (definition.MyKind == SkillKind::NONE || definition.MyKind == SkillKind::PASSIVE) return;
		auto& skill = _unit.MySkill;
		const auto cost = SpCost(_unit.MyId);
		if (!std::isgreater(cost, 0))
		{
			if (std::isgreater(skill.MySp, 0)) { skill.MySp = 0; skill.MyCharges = definition.MyMaxCharges; }
			return;
		}
		// 用商一次转换多层充能，不随授予的 SP 数量循环。满充能时 SP 栏显示完整一格。
		const auto count = std::min(std::floor(skill.MySp / cost), static_cast<double>(definition.MyMaxCharges - skill.MyCharges));
		if (std::isgreater(count, 0))
		{
			skill.MyCharges += static_cast<unsigned>(count);
			skill.MySp = skill.MyCharges >= definition.MyMaxCharges ? cost : skill.MySp - count * cost;
		}
		skill.MySp = std::min(skill.MySp, cost);
	}

	double Battle::GainSp(UnitId _unit, double _amount, SpReason _reason, bool _silent)
	{
		auto& unit = _MyUnits[Index(_unit)];
		const auto& definition = unit.MyDefinition.MySkill;
		auto& skill = unit.MySkill;
		if (!std::isfinite(_amount) || !std::isgreater(_amount, 0) || !_MyStarted || Finished() ||
			definition.MyKind == SkillKind::NONE || definition.MyKind == SkillKind::PASSIVE) return 0;
		if (_reason != SpReason::INITIAL && ((skill.MyActive && IsTimedSkill(definition.MyKind)) || unit.MyStatuses.Has(CombatStatus::NO_SP))) return 0;
		auto cost = SpCost(_unit);
		if (skill.MyCharges >= definition.MyMaxCharges && std::isgreaterequal(skill.MySp, cost)) return 0;
		if (!_silent)
		{
			ContentEvent event{.MyKind = ContentEventKind::SP_GAIN, .MyUnit = _unit, .MyAmount = _amount, .MySpReason = _reason};
			NotifyContent(event);
			_amount = event.MyAmount;
			if (event.MyCancel || Finished() || !std::isfinite(_amount) || !std::isgreater(_amount, 0)) return 0;
			cost = SpCost(_unit); // 回调可改变费用，不能使用广播前的缓存值。
			if (skill.MyCharges >= definition.MyMaxCharges && std::isgreaterequal(skill.MySp, cost)) return 0;
		}
		if (!std::isgreater(cost, 0)) return 0; // 零费用技能只在部署时获得充能，不无限回复。
		skill.MySp += std::min(_amount, cost * definition.MyMaxCharges);
		NormalizeSp(unit);
		return _amount;
	}

	void Battle::SetSpTotal(UnitId _unit, double _total)
	{
		auto& unit = _MyUnits[Index(_unit)];
		const auto kind = unit.MyDefinition.MySkill.MyKind;
		if (!_MyStarted || Finished() || !std::isfinite(_total) || kind == SkillKind::NONE ||
			kind == SkillKind::PASSIVE || (unit.MySkill.MyActive && IsTimedSkill(kind))) return;
		unit.MySkill.MySp = 0;
		unit.MySkill.MyCharges = 0;
		GainSp(_unit, std::max(0.0, _total), SpReason::INITIAL, true);
		if (!std::isgreater(SpCost(_unit), 0)) unit.MySkill.MyCharges = unit.MyDefinition.MySkill.MyMaxCharges;
	}

	void Battle::SetSpCostMultiplier(UnitId _unit, double _multiplier)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (!_MyStarted || Finished()) return;
		unit.MySkill.MySpCostMultiplier = std::isfinite(_multiplier) && std::isgreaterequal(_multiplier, 0) ? _multiplier : 1;
		NormalizeSp(unit);
	}

	void Battle::ResetSkill(CombatUnit& _unit, bool _initial, std::optional<double> _carrySp)
	{
		auto& skill = _unit.MySkill;
		const auto& definition = _unit.MyDefinition.MySkill;
		// 激活总次数和费用倍率在复活时保留；一次施放的计时／弹药／待发攻击不可继承。
		skill.MyActive = skill.MyPending = false;
		skill.MySp = skill.MyTimeLeft = skill.MyAmmoLeft = skill.MyAmmoMax = 0;
		skill.MyCharges = 0;
		skill.MyBuff = 0;
		skill.MyOperationReadyAt = -std::numeric_limits<double>::infinity();
		if (definition.MyKind == SkillKind::NONE)
		{
			if (_initial) skill.MyOperationReadyAt = Time() + OperationCooldown;
			return;
		}
		if (definition.MyKind == SkillKind::PASSIVE)
		{
			skill.MyActive = true;
			ApplySkillModifiers(_unit);
			ContentEvent event{.MyKind = ContentEventKind::SKILL_START, .MyUnit = _unit.MyId, .MySkillReason = SkillReason::PASSIVE};
			NotifyContent(event);
		}
		else
		{
			SetSpTotal(_unit.MyId, _carrySp.value_or(definition.MyInitialSp));
			if (definition.MyActivateOnDeploy) ActivateSkill(_unit.MyId, false, SkillReason::DEPLOY);
		}
		if (_initial) skill.MyOperationReadyAt = Time() + OperationCooldown;
	}

	void Battle::ApplySkillModifiers(CombatUnit& _unit)
	{
		const auto& definition = _unit.MyDefinition.MySkill;
		if (!definition.MyModifiers.empty() || definition.MyFlags.any())
			_unit.MySkill.MyBuff = AddBuff(_unit.MyId, BuffDefinition{
				.MyKey = "skill:" + std::to_string(_unit.MyId),
				.MySource = _unit.MyId,
				.MyModifiers = definition.MyModifiers,
				.MyFlags = definition.MyFlags});
		if (!definition.MyRange.empty() || definition.MyRangeExtend != 0 || definition.MyNoRangeExtend) RefreshRange(_unit);
	}

	bool Battle::ActivateSkill(UnitId _unit, bool _free, SkillReason _reason)
	{
		auto& unit = _MyUnits[Index(_unit)];
		const auto& definition = unit.MyDefinition.MySkill;
		auto& skill = unit.MySkill;
		if (!_MyStarted || Finished() || !unit.MyAlive || definition.MyKind == SkillKind::NONE || definition.MyKind == SkillKind::PASSIVE ||
			(!_free && skill.MyCharges == 0) || (skill.MyActive && IsTimedSkill(definition.MyKind))) return false;
		if (!_free)
		{
			const bool full = skill.MyCharges == definition.MyMaxCharges;
			--skill.MyCharges;
			if (full || definition.MyMaxCharges == 1) skill.MySp = 0;
		}
		++skill.MyActivations;
		skill.MyLastStart = Time();
		if (definition.MyManual) skill.MyOperationReadyAt = Time() + OperationCooldown;
		skill.MyActive = true;
		skill.MyPending = !IsTimedSkill(definition.MyKind) && definition.MyAttack.has_value();
		if (IsTimedSkill(definition.MyKind))
		{
			skill.MyTimeLeft = definition.MyKind == SkillKind::DURATION ? std::max(0.01, definition.MyDuration)
				: definition.MyKind == SkillKind::AMMO && std::isgreater(definition.MyDuration, 0) ? definition.MyDuration
				: std::numeric_limits<double>::infinity();
			skill.MyAmmoLeft = definition.MyKind == SkillKind::AMMO ? std::max(1.0, definition.MyAmmo) : 0;
			skill.MyAmmoMax = skill.MyAmmoLeft;
		}
		ApplySkillModifiers(unit);
		ContentEvent event{.MyKind = ContentEventKind::SKILL_START, .MyUnit = _unit, .MySkillReason = _reason};
		NotifyContent(event);
		if (skill.MyActive) skill.MyAmmoMax = std::max(skill.MyAmmoMax, skill.MyAmmoLeft);
		if (!IsTimedSkill(definition.MyKind) && !skill.MyPending) EndSkill(_unit, SkillReason::INSTANT);
		return true;
	}

	void Battle::EndSkill(UnitId _unit, SkillReason _reason)
	{
		auto& unit = _MyUnits[Index(_unit)];
		auto& skill = unit.MySkill;
		if (!skill.MyActive || (unit.MyDefinition.MySkill.MyKind == SkillKind::PASSIVE && _reason != SkillReason::DEATH)) return;
		skill.MyActive = skill.MyPending = false;
		skill.MyTimeLeft = skill.MyAmmoLeft = skill.MyAmmoMax = 0;
		const auto activation = skill.MyActivations;
		const auto buff = skill.MyBuff;
		// 收尾伤害要读到技能的属性和范围；先关活动状态，再调用收尾，最后移除修正。
		ContentEvent event{.MyKind = ContentEventKind::SKILL_ENDING, .MyUnit = _unit, .MySkillReason = _reason};
		NotifyContent(event);
		if (skill.MyActive || activation != skill.MyActivations) return;
		if (buff) (void)RemoveBuff(_unit, buff);
		skill.MyBuff = 0;
		const auto& definition = unit.MyDefinition.MySkill;
		if (!definition.MyRange.empty() || definition.MyRangeExtend != 0 || definition.MyNoRangeExtend) RefreshRange(unit);
		event.MyKind = ContentEventKind::SKILL_END;
		NotifyContent(event);
	}

	void Battle::AddSkillAmmo(UnitId _unit, double _amount)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (Finished() || !unit.MySkill.MyActive || unit.MyDefinition.MySkill.MyKind != SkillKind::AMMO || !std::isfinite(_amount)) return;
		const auto next = unit.MySkill.MyAmmoLeft + _amount;
		if (!std::isfinite(next)) return;
		unit.MySkill.MyAmmoLeft = next;
		unit.MySkill.MyAmmoMax = std::max(unit.MySkill.MyAmmoMax, next);
	}

	void Battle::ExtendSkill(UnitId _unit, double _seconds)
	{
		auto& skill = _MyUnits[Index(_unit)].MySkill;
		if (!Finished() && skill.MyActive && std::isfinite(_seconds) && std::isfinite(skill.MyTimeLeft + _seconds))
			skill.MyTimeLeft += _seconds;
	}

	void Battle::AddSkillCharge(UnitId _unit, int _amount)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (!_MyStarted || Finished()) return;
		const auto maximum = unit.MyDefinition.MySkill.MyMaxCharges;
		unit.MySkill.MyCharges = static_cast<unsigned>(std::clamp<std::int64_t>(static_cast<std::int64_t>(unit.MySkill.MyCharges) + _amount, 0, maximum));
		if (unit.MySkill.MyCharges == maximum) unit.MySkill.MySp = SpCost(_unit);
	}

	bool Battle::CanAutoSkill(const CombatUnit& _unit) const noexcept
	{
		const auto& definition = _unit.MyDefinition.MySkill;
		const auto& skill = _unit.MySkill;
		return _unit.MyAlive && !_unit.MyHidden && !_unit.MyStatuses.Has(CombatStatus::STUN) &&
			!_unit.MyStatuses.Has(CombatStatus::SILENCE) && definition.MyKind != SkillKind::NONE && definition.MyKind != SkillKind::PASSIVE &&
			skill.MyCharges > 0 && !skill.MyPending && !(skill.MyActive && IsTimedSkill(definition.MyKind)) &&
			(!definition.MyManual || !std::isless(Time(), skill.MyOperationReadyAt - 1e-9));
	}

	bool Battle::SkillCondition(const CombatUnit& _unit, bool _allyOnly) const
	{
		const auto& definition = _unit.MyDefinition.MySkill;
		const auto rule = definition.MyTrigger;
		if (!_allyOnly && rule == SkillTrigger::SP_FULL) return true;
		const bool custom = !definition.MyTriggerRange.empty() && (_allyOnly || rule == SkillTrigger::ACTIVE_RANGE ||
			rule == SkillTrigger::SKILL_RANGE || rule == SkillTrigger::CUSTOM_RANGE);
		const auto& mask = custom ? _unit.MySkill.MyTriggerMask : _unit.MyBaseTriggerMask;
		const bool global = !_allyOnly && rule == SkillTrigger::GLOBAL;
		const bool healing = _allyOnly || (rule == SkillTrigger::SKILL_RANGE && definition.MyTriggerAllies) ||
			(!(custom && (rule == SkillTrigger::CUSTOM_RANGE || rule == SkillTrigger::SKILL_RANGE)) && definition.MyHealSkill);
		const auto& profile = _unit.MyDefinition.MyAttack;
		for (const auto& target : _MyUnits)
		{
			if (!target.MyAlive || target.MyHidden) continue;
			if (healing)
			{
				if (target.MySide != UnitSide::ALLY ||
					(target.MyId != _unit.MyId && (target.MyStatuses.Has(CombatStatus::NO_HEAL) || target.MyDefinition.MyAttack.MyNoHeal)) ||
					!std::isless(target.MyHealth, target.MyStats.MyMaxHealth - 1e-6)) continue;
				if ((_allyOnly || definition.MyTriggerAllies) && std::isgreater(target.MyHealth / target.MyStats.MyMaxHealth, definition.MyTriggerHpAtMost + 1e-9)) continue;
				if (global || Contains(mask, RulePosition(target))) return true;
			}
			else if (target.MySide == UnitSide::ENEMY)
			{
				// 技能范围策略故意忽略不可选中／隐匿／飞行限制；其它策略保留选择器约束。
				const bool any = custom && rule == SkillTrigger::SKILL_RANGE;
				const bool allFlying = global || (custom && rule == SkillTrigger::CUSTOM_RANGE);
				if (!any && (target.MyStatuses.Has(CombatStatus::UNTARGETABLE) ||
					(target.MyStatuses.Has(CombatStatus::SLEEP) && (allFlying || !profile.MyHitSleep)) ||
					EnemyStealthed(target) ||
					(!allFlying && target.Flying() && (!profile.MyCanHitFlying || profile.MyGroundOnly)))) continue;
				const bool blocked = !global && !(custom && (rule == SkillTrigger::CUSTOM_RANGE || rule == SkillTrigger::SKILL_RANGE)) && target.MyBlockedBy == _unit.MyId;
				if (global || blocked || BodyInRange(target, mask)) return true;
			}
		}
		const bool defaultLike = rule == SkillTrigger::DEFAULT || rule == SkillTrigger::SEARCH || rule == SkillTrigger::ACTIVE_RANGE ||
			(!custom && (rule == SkillTrigger::CUSTOM_RANGE || rule == SkillTrigger::SKILL_RANGE));
		return !_allyOnly && !definition.MyHealSkill && defaultLike && ExtraSkillTriggerSatisfied(_unit);
	}

	void Battle::TickSkill(CombatUnit& _unit)
	{
		const auto& definition = _unit.MyDefinition.MySkill;
		if (definition.MyKind == SkillKind::NONE) return;
		auto& skill = _unit.MySkill;
		if (std::isgreater(skill.MySp, SpCost(_unit.MyId))) NormalizeSp(_unit);
		if (skill.MyActive)
		{
			ContentEvent event{.MyKind = ContentEventKind::SKILL_TICK, .MyUnit = _unit.MyId, .MyDelta = BattleClock::StepSeconds};
			NotifyContent(event);
			if (Finished() || !_unit.MyAlive) return;
			if (definition.MyKind == SkillKind::DURATION || (definition.MyKind == SkillKind::AMMO && std::isfinite(skill.MyTimeLeft)))
			{
				skill.MyTimeLeft -= BattleClock::StepSeconds;
				if (std::islessequal(skill.MyTimeLeft, 1e-9)) EndSkill(_unit.MyId, SkillReason::DURATION);
			}
		}
		// 先推进技能再检查能否行动：眩晕不停止自然技力；阻回和运行中的持续技能会停止。
		if (definition.MySpType == SpType::TIME && !_unit.MyHidden)
			GainSp(_unit.MyId, _unit.MyStats.MySpRecovery * BattleClock::StepSeconds, SpReason::TIME);
		if (Finished() || !_unit.MyAlive || _unit.MyStatuses.Has(CombatStatus::STUN)) return;
		if (skill.MyPending && definition.MyTriggerAllies && definition.MyTrigger != SkillTrigger::SKILL_RANGE && !SkillCondition(_unit, true))
		{
			EndSkill(_unit.MyId, SkillReason::WITHDRAWN);
			AddSkillCharge(_unit.MyId);
		}
		if (!CanAutoSkill(_unit)) return;
		const auto rule = definition.MyTrigger;
		if (rule == SkillTrigger::NEVER || rule == SkillTrigger::TAKE_DAMAGE) return;
		if (rule == SkillTrigger::DEFAULT && !AttackDisabled(_unit))
		{
			if (!definition.MyHealSkill && ExtraSkillTriggerSatisfied(_unit)) ActivateSkill(_unit.MyId, false, SkillReason::TRIGGER);
			return;
		}
		if (SkillCondition(_unit)) ActivateSkill(_unit.MyId, false, SkillReason::TRIGGER);
	}

	bool Battle::SkillAboutToAttack(CombatUnit& _unit)
	{
		const auto& definition = _unit.MyDefinition.MySkill;
		if (definition.MyTrigger != SkillTrigger::DEFAULT || !CanAutoSkill(_unit) || !SkillCondition(_unit) ||
			(definition.MyTriggerAllies && !SkillCondition(_unit, true))) return false;
		return ActivateSkill(_unit.MyId, false, SkillReason::TRIGGER);
	}

	void Battle::SkillDamaged(CombatUnit& _unit)
	{
		const auto& definition = _unit.MyDefinition.MySkill;
		if (definition.MySpType == SpType::HURT) GainSp(_unit.MyId, 1, SpReason::HURT);
		if (definition.MyTrigger == SkillTrigger::TAKE_DAMAGE && CanAutoSkill(_unit))
			ActivateSkill(_unit.MyId, false, SkillReason::TRIGGER);
	}

	void Battle::SkillAttackPerformed(CombatUnit& _unit, bool _usedOverride, bool _noAmmo, std::span<const UnitId> _targets)
	{
		const auto& definition = _unit.MyDefinition.MySkill;
		auto& skill = _unit.MySkill;
		const bool skillAttack = skill.MyActive && (IsTimedSkill(definition.MyKind) || (_usedOverride && skill.MyPending));
		ContentEvent event{.MyKind = ContentEventKind::ATTACK, .MyUnit = _unit.MyId, .MySource = _unit.MyId, .MyNoAmmo = _noAmmo, .MyTargetCount = _targets.size(), .MyTargets = _targets};
		NotifyContent(event);
		if (Finished() || !_unit.MyAlive) return;
		if (skill.MyActive && definition.MyKind == SkillKind::AMMO)
		{
			if (!event.MyNoAmmo)
			{
				--skill.MyAmmoLeft;
				ContentEvent ammo{.MyKind = ContentEventKind::AMMO_USED, .MyUnit = _unit.MyId, .MyAmount = skill.MyAmmoLeft};
				NotifyContent(ammo);
				if (std::islessequal(skill.MyAmmoLeft, 0)) EndSkill(_unit.MyId, SkillReason::AMMO);
			}
		}
		else if (_usedOverride && skill.MyPending && !IsTimedSkill(definition.MyKind)) EndSkill(_unit.MyId, SkillReason::INSTANT);
		if (definition.MySpType == SpType::ATTACK && !skillAttack) GainSp(_unit.MyId, 1, SpReason::ATTACK);
	}
}
