#include "battle_core.hpp"
#include <stronghold/simulation/effects.hpp>

namespace Stronghold
{
	namespace
	{
		constexpr double OperationCooldown = 3;
	}

	SkillBase::SkillBase(Battle& _battle, UnitId _unit) noexcept : _MyBattle(_battle), _MyUnit(_unit) {}

	BattleCore& SkillBase::Core() const noexcept
	{
		return *_MyBattle._MyView;
	}

	const SkillDefinition& SkillBase::Definition() const
	{
		return _MyBattle.Unit(_MyUnit).MyDefinition.MySkill;
	}

	const SkillState& SkillBase::State() const
	{
		return _MyBattle.Unit(_MyUnit).MySkill;
	}

	const std::bitset<FieldTiles>& SkillBase::Range() const
	{
		return _MyBattle.RuleRange(_MyUnit);
	}

	bool SkillBase::TriggerSatisfied() const
	{
		return Core().SkillCondition(Core().Unit(_MyUnit));
	}

	void SkillBase::SetTrigger(SkillTrigger _trigger, std::vector<RangeOffset> _range)
	{
		_MyBattle.SetSkillTrigger(_MyUnit, _trigger, std::move(_range));
	}

	bool SkillBase::CanStart(bool _free, SkillReason _reason) const
	{
		const auto& unit = _MyBattle.Unit(_MyUnit);
		const auto& definition = Definition();
		return _MyBattle.Started() && !_MyBattle.Finished() && unit.MyAlive && definition.MyKind != SkillKind::NONE &&
			definition.MyKind != SkillKind::PASSIVE && (_free || State().MyCharges > 0) &&
			!(State().MyActive && IsTimedSkill(definition.MyKind)) && CanActivate(_reason);
	}

	double SkillBase::SpCost() const
	{
		auto& core = Core();
		const auto& unit = core.Unit(_MyUnit);
		const auto& definition = unit.MyDefinition.MySkill;
		if (definition.MyKind == SkillKind::NONE || definition.MyKind == SkillKind::PASSIVE) return 0;
		const auto cost = std::floor(definition.MySpCost * unit.MySkill.MySpCostMultiplier + unit.MyStats.MySpCostFlat);
		return std::isfinite(cost) ? std::max(0.0, cost) : definition.MySpCost;
	}

	double SkillBase::SpTotal() const
	{
		auto& core = Core();
		const auto& unit = core.Unit(_MyUnit);
		const auto& definition = unit.MyDefinition.MySkill;
		if (definition.MyKind == SkillKind::NONE || definition.MyKind == SkillKind::PASSIVE) return 0;
		const auto& skill = unit.MySkill;
		return skill.MyCharges >= definition.MyMaxCharges ? definition.MyMaxCharges * SpCost()
			: skill.MyCharges * SpCost() + skill.MySp;
	}

	void SkillBase::NormalizeSp()
	{
		auto& core = Core();
		auto& unit = core._MyUnits[core.Index(_MyUnit)];
		const auto& definition = unit.MyDefinition.MySkill;
		if (definition.MyKind == SkillKind::NONE || definition.MyKind == SkillKind::PASSIVE) return;
		auto& skill = unit.MySkill;
		const auto cost = SpCost();
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

	double SkillBase::GainSp(double _amount, SpReason _reason, bool _silent)
	{
		auto& core = Core();
		auto& unit = core._MyUnits[core.Index(_MyUnit)];
		const auto& definition = unit.MyDefinition.MySkill;
		auto& skill = unit.MySkill;
		if (!std::isfinite(_amount) || !std::isgreater(_amount, 0) || !core._MyStarted || core.Finished() ||
			definition.MyKind == SkillKind::NONE || definition.MyKind == SkillKind::PASSIVE) return 0;
		if (_reason != SpReason::INITIAL && ((skill.MyActive && IsTimedSkill(definition.MyKind)) || unit.MyStatuses.Has(CombatStatus::NO_SP))) return 0;
		auto cost = SpCost();
		if (skill.MyCharges >= definition.MyMaxCharges && std::isgreaterequal(skill.MySp, cost)) return 0;
		if (!_silent)
		{
			ContentEvent event{.MyKind = ContentEventKind::SP_GAIN, .MyUnit = _MyUnit, .MyAmount = _amount, .MySpReason = _reason};
			core.NotifyContent(event);
			_amount = event.MyAmount;
			if (event.MyCancel || core.Finished() || !std::isfinite(_amount) || !std::isgreater(_amount, 0)) return 0;
			cost = SpCost(); // 回调可改变费用，不能使用广播前的缓存值。
			if (skill.MyCharges >= definition.MyMaxCharges && std::isgreaterequal(skill.MySp, cost)) return 0;
		}
		if (!std::isgreater(cost, 0)) return 0; // 零费用技能只在部署时获得充能，不无限回复。
		skill.MySp += std::min(_amount, cost * definition.MyMaxCharges);
		NormalizeSp();
		return _amount;
	}

	void SkillBase::SetSpTotal(double _total)
	{
		auto& core = Core();
		auto& unit = core._MyUnits[core.Index(_MyUnit)];
		const auto kind = unit.MyDefinition.MySkill.MyKind;
		if (!core._MyStarted || core.Finished() || !std::isfinite(_total) || kind == SkillKind::NONE ||
			kind == SkillKind::PASSIVE || (unit.MySkill.MyActive && IsTimedSkill(kind))) return;
		unit.MySkill.MySp = 0;
		unit.MySkill.MyCharges = 0;
		GainSp(std::max(0.0, _total), SpReason::INITIAL, true);
		if (!std::isgreater(SpCost(), 0)) unit.MySkill.MyCharges = unit.MyDefinition.MySkill.MyMaxCharges;
	}

	void SkillBase::SetSpCostMultiplier(double _multiplier)
	{
		auto& core = Core();
		auto& unit = core._MyUnits[core.Index(_MyUnit)];
		if (!core._MyStarted || core.Finished()) return;
		unit.MySkill.MySpCostMultiplier = std::isfinite(_multiplier) && std::isgreaterequal(_multiplier, 0) ? _multiplier : 1;
		NormalizeSp();
	}

	void SkillBase::Reset(bool _initial, std::optional<double> _carrySp)
	{
		auto& core = Core();
		auto& unit = core._MyUnits[core.Index(_MyUnit)];
		auto& skill = unit.MySkill;
		const auto& definition = unit.MyDefinition.MySkill;
		// 激活总次数和费用倍率在复活时保留；一次施放的计时／弹药／待发攻击不可继承。
		skill.MyActive = skill.MyPending = false;
		skill.MySp = skill.MyTimeLeft = skill.MyAmmoLeft = skill.MyAmmoMax = 0;
		skill.MyCharges = 0;
		skill.MyBuff = 0;
		skill.MyOperationReadyAt = -std::numeric_limits<double>::infinity();
		if (definition.MyKind == SkillKind::NONE)
		{
			if (_initial) skill.MyOperationReadyAt = core.Time() + OperationCooldown;
			return;
		}
		if (definition.MyKind == SkillKind::PASSIVE)
		{
			skill.MyActive = true;
			core.ApplySkillModifiers(unit);
			OnStart(SkillReason::PASSIVE);
			if (definition.MyStartEffects) EffectExecutor::Execute(_MyBattle, {.MySource = _MyUnit, .MyOwner = unit.MyOwner}, *definition.MyStartEffects);
			ContentEvent event{.MyKind = ContentEventKind::SKILL_START, .MyUnit = unit.MyId, .MySkillReason = SkillReason::PASSIVE};
			core.NotifyContent(event);
		}
		else
		{
			SetSpTotal(_carrySp.value_or(definition.MyInitialSp));
			if (definition.MyActivateOnDeploy) Start(false, SkillReason::DEPLOY);
		}
		if (_initial) skill.MyOperationReadyAt = core.Time() + OperationCooldown;
	}

	bool SkillBase::Start(bool _free, SkillReason _reason)
	{
		auto& core = Core();
		auto& unit = core._MyUnits[core.Index(_MyUnit)];
		const auto& definition = unit.MyDefinition.MySkill;
		auto& skill = unit.MySkill;
		if (!CanStart(_free, _reason)) return false;
		if (!_free)
		{
			const bool full = skill.MyCharges == definition.MyMaxCharges;
			--skill.MyCharges;
			if (full || definition.MyMaxCharges == 1) skill.MySp = 0;
		}
		++skill.MyActivations;
		skill.MyLastStart = core.Time();
		if (definition.MyManual) skill.MyOperationReadyAt = core.Time() + OperationCooldown;
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
		BeforeStart(_reason);
		if (!unit.MyAlive || core.Finished()) return true;
		core.ApplySkillModifiers(unit);
		OnStart(_reason);
		if (definition.MyStartEffects) EffectExecutor::Execute(_MyBattle, {.MySource = _MyUnit, .MyOwner = unit.MyOwner}, *definition.MyStartEffects);
		ContentEvent event{.MyKind = ContentEventKind::SKILL_START, .MyUnit = _MyUnit, .MySkillReason = _reason};
		core.NotifyContent(event);
		AfterStart(_reason);
		if (skill.MyActive) skill.MyAmmoMax = std::max(skill.MyAmmoMax, skill.MyAmmoLeft);
		if (!IsTimedSkill(definition.MyKind) && !skill.MyPending) End(SkillReason::INSTANT);
		return true;
	}

	void SkillBase::End(SkillReason _reason)
	{
		auto& core = Core();
		auto& unit = core._MyUnits[core.Index(_MyUnit)];
		auto& skill = unit.MySkill;
		if (!skill.MyActive || (unit.MyDefinition.MySkill.MyKind == SkillKind::PASSIVE && _reason != SkillReason::DEATH)) return;
		skill.MyActive = skill.MyPending = false;
		skill.MyTimeLeft = skill.MyAmmoLeft = skill.MyAmmoMax = 0;
		const auto activation = skill.MyActivations;
		const auto buff = skill.MyBuff;
		// 收尾伤害要读到技能的属性和范围；先关活动状态，再调用收尾，最后移除修正。
		ContentEvent event{.MyKind = ContentEventKind::SKILL_ENDING, .MyUnit = _MyUnit, .MySkillReason = _reason};
		OnEnding(_reason);
		if (Definition().MyEndingEffects) EffectExecutor::Execute(_MyBattle, {.MySource = _MyUnit, .MyOwner = unit.MyOwner}, *Definition().MyEndingEffects);
		core.NotifyContent(event);
		if (skill.MyActive || activation != skill.MyActivations) return;
		if (buff) (void)core.RemoveBuff(_MyUnit, buff);
		skill.MyBuff = 0;
		const auto& definition = unit.MyDefinition.MySkill;
		if (!definition.MyRange.empty() || definition.MyRangeExtend != 0 || definition.MyNoRangeExtend) core.RefreshRange(unit);
		event.MyKind = ContentEventKind::SKILL_END;
		core.NotifyContent(event);
		OnEnd(_reason);
		if (definition.MyEndEffects) EffectExecutor::Execute(_MyBattle, {.MySource = _MyUnit, .MyOwner = unit.MyOwner}, *definition.MyEndEffects);
	}

	void SkillBase::AddAmmo(double _amount)
	{
		auto& core = Core();
		auto& unit = core._MyUnits[core.Index(_MyUnit)];
		if (core.Finished() || !unit.MySkill.MyActive || unit.MyDefinition.MySkill.MyKind != SkillKind::AMMO || !std::isfinite(_amount)) return;
		const auto next = unit.MySkill.MyAmmoLeft + _amount;
		if (!std::isfinite(next)) return;
		unit.MySkill.MyAmmoLeft = next;
		unit.MySkill.MyAmmoMax = std::max(unit.MySkill.MyAmmoMax, next);
	}

	void SkillBase::Extend(double _seconds)
	{
		auto& core = Core();
		auto& skill = core._MyUnits[core.Index(_MyUnit)].MySkill;
		if (!core.Finished() && skill.MyActive && std::isfinite(_seconds) && std::isfinite(skill.MyTimeLeft + _seconds))
			skill.MyTimeLeft += _seconds;
	}

	void SkillBase::AddCharge(int _amount)
	{
		auto& core = Core();
		auto& unit = core._MyUnits[core.Index(_MyUnit)];
		if (!core._MyStarted || core.Finished()) return;
		const auto maximum = unit.MyDefinition.MySkill.MyMaxCharges;
		unit.MySkill.MyCharges = static_cast<unsigned>(std::clamp<std::int64_t>(static_cast<std::int64_t>(unit.MySkill.MyCharges) + _amount, 0, maximum));
		if (unit.MySkill.MyCharges == maximum) unit.MySkill.MySp = SpCost();
	}

	void SkillBase::Tick()
	{
		auto& core = Core();
		auto& unit = core._MyUnits[core.Index(_MyUnit)];
		const auto& definition = unit.MyDefinition.MySkill;
		if (definition.MyKind == SkillKind::NONE) return;
		auto& skill = unit.MySkill;
		if (std::isgreater(skill.MySp, SpCost())) NormalizeSp();
		if (skill.MyActive)
		{
			ContentEvent event{.MyKind = ContentEventKind::SKILL_TICK, .MyUnit = unit.MyId, .MyDelta = BattleClock::StepSeconds};
			if (definition.MyCustom.MyKind != ComponentKind::BUILTIN) OnTick(BattleClock::StepSeconds);
			else core._MyComponents[core.Index(_MyUnit)].MySkill.TickBuiltin(BattleClock::StepSeconds);
			if (definition.MyTickEffects) EffectExecutor::Execute(_MyBattle, {.MySource = _MyUnit, .MyOwner = unit.MyOwner, .MyDelta = BattleClock::StepSeconds}, *definition.MyTickEffects);
			core.NotifyContent(event);
			if (core.Finished() || !unit.MyAlive) return;
			if (definition.MyKind == SkillKind::DURATION || (definition.MyKind == SkillKind::AMMO && std::isfinite(skill.MyTimeLeft)))
			{
				skill.MyTimeLeft -= BattleClock::StepSeconds;
				if (std::islessequal(skill.MyTimeLeft, 1e-9)) End(SkillReason::DURATION);
			}
		}
		// 先推进技能再检查能否行动：眩晕不停止自然技力；阻回和运行中的持续技能会停止。
		if (definition.MySpType == SpType::TIME && !unit.MyHidden)
			GainSp(unit.MyStats.MySpRecovery * BattleClock::StepSeconds, SpReason::TIME);
		if (core.Finished() || !unit.MyAlive || unit.MyStatuses.Has(CombatStatus::STUN)) return;
		if (skill.MyPending && definition.MyTriggerAllies && definition.MyTrigger != SkillTrigger::SKILL_RANGE && !core.SkillCondition(unit, true))
		{
			End(SkillReason::WITHDRAWN);
			AddCharge();
		}
		if (!core.CanAutoSkill(unit)) return;
		const auto rule = definition.MyTrigger;
		if (rule == SkillTrigger::NEVER || rule == SkillTrigger::TAKE_DAMAGE) return;
		if (rule == SkillTrigger::DEFAULT && !core.AttackDisabled(unit))
		{
			if (!definition.MyHealSkill && core.ExtraSkillTriggerSatisfied(unit)) Start(false, SkillReason::TRIGGER);
			return;
		}
		if (core.SkillCondition(unit)) Start(false, SkillReason::TRIGGER);
	}

	bool SkillBase::CanActivate(SkillReason) const { return true; }

	void SkillBase::BeforeStart(SkillReason) {}

	void SkillBase::OnStart(SkillReason) {}

	void SkillBase::AfterStart(SkillReason) {}

	void SkillBase::OnEnding(SkillReason) {}

	void SkillBase::OnEnd(SkillReason) {}

	void SkillBase::OnTick(double) {}
}
