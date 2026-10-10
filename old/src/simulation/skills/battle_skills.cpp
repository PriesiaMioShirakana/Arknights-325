#include <algorithm>
#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		bool Contains(const std::bitset<399>& _mask, WorldPoint _point) noexcept
		{
			const auto row = static_cast<int>(std::floor(_point.MyY + 0.5));
			const auto column = static_cast<int>(std::floor(_point.MyX + 0.5));
			return row >= 0 && row < 19 && column >= 0 && column < 21 && _mask[static_cast<std::size_t>(row * 21 + column)];
		}
	}

	const AttackProfile& BattleCore::EffectiveAttack(const CombatUnit& _unit) const noexcept
	{
		const auto& definition = _unit.MyDefinition.MySkill;
		const auto& skill = _unit.MySkill;
		if (skill.MyActive && definition.MyAttack &&
			(IsTimedSkill(definition.MyKind) || definition.MyKind == SkillKind::PASSIVE || skill.MyPending))
			return *definition.MyAttack;
		return _unit.MyDefinition.MyAttack;
	}

	bool BattleCore::AttackDisabled(const CombatUnit& _unit) const noexcept
	{
		const auto& profile = EffectiveAttack(_unit);
		return profile.MyDisabled || (profile.MyOnlyDuringSkill && !_unit.MySkill.MyActive);
	}

	void BattleCore::SetSkillTrigger(UnitId _unit, SkillTrigger _rule, std::vector<RangeOffset> _grid)
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

	std::uint64_t BattleCore::AddSkillTriggerRange(UnitId _unit, SkillTriggerArea _area)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_area.MySourceUnit && Unit(_area.MySourceUnit).MySide != UnitSide::ALLY)
			throw std::invalid_argument("trigger range source must be an ally");
		if (Finished()) return 0;
		const auto id = ++_MyTriggerRangeSequence;
		unit.MySkill.MyExtraRanges.emplace_back(SkillTriggerRange{.MyId = id, .MyArea = _area});
		return id;
	}

	bool BattleCore::RemoveSkillTriggerRange(UnitId _unit, std::uint64_t _range)
	{
		auto& ranges = _MyUnits[Index(_unit)].MySkill.MyExtraRanges;
		return std::erase_if(ranges, [&](const SkillTriggerRange& _entry) { return _entry.MyId == _range; }) != 0;
	}

	bool BattleCore::UpdateSkillTriggerRange(UnitId _unit, std::uint64_t _range, SkillTriggerArea _area)
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

	bool BattleCore::ExtraSkillTriggerSatisfied(const CombatUnit& _unit) const
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

	double BattleCore::SpCost(UnitId _unit) const
	{
		return const_cast<BattleCore*>(this)->Skill(_unit).SpCost();
	}

	double BattleCore::SpTotal(UnitId _unit) const
	{
		return const_cast<BattleCore*>(this)->Skill(_unit).SpTotal();
	}

	void BattleCore::NormalizeSp(CombatUnit& _unit)
	{
		Skill(_unit.MyId).NormalizeSp();
	}

	double BattleCore::GainSp(UnitId _unit, double _amount, SpReason _reason, bool _silent)
	{
		return Skill(_unit).GainSp(_amount, _reason, _silent);
	}

	void BattleCore::SetSpTotal(UnitId _unit, double _total)
	{
		Skill(_unit).SetSpTotal(_total);
	}

	void BattleCore::SetSpCostMultiplier(UnitId _unit, double _multiplier)
	{
		Skill(_unit).SetSpCostMultiplier(_multiplier);
	}

	void BattleCore::ResetSkill(CombatUnit& _unit, bool _initial, std::optional<double> _carrySp)
	{
		Skill(_unit.MyId).Reset(_initial, _carrySp);
	}

	void BattleCore::ApplySkillModifiers(CombatUnit& _unit)
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

	bool BattleCore::ActivateSkill(UnitId _unit, bool _free, SkillReason _reason)
	{
		return Skill(_unit).Start(_free, _reason);
	}

	void BattleCore::EndSkill(UnitId _unit, SkillReason _reason)
	{
		Skill(_unit).End(_reason);
	}

	void BattleCore::AddSkillAmmo(UnitId _unit, double _amount)
	{
		Skill(_unit).AddAmmo(_amount);
	}

	void BattleCore::ExtendSkill(UnitId _unit, double _seconds)
	{
		Skill(_unit).Extend(_seconds);
	}

	void BattleCore::AddSkillCharge(UnitId _unit, int _amount)
	{
		Skill(_unit).AddCharge(_amount);
	}

	bool BattleCore::CanAutoSkill(const CombatUnit& _unit) const noexcept
	{
		const auto& definition = _unit.MyDefinition.MySkill;
		const auto& skill = _unit.MySkill;
		return _unit.MyAlive && !_unit.MyHidden && !_unit.MyStatuses.Has(CombatStatus::STUN) &&
			!_unit.MyStatuses.Has(CombatStatus::SILENCE) && definition.MyKind != SkillKind::NONE && definition.MyKind != SkillKind::PASSIVE &&
			skill.MyCharges > 0 && !skill.MyPending && !(skill.MyActive && IsTimedSkill(definition.MyKind)) &&
			(!definition.MyManual || !std::isless(Time(), skill.MyOperationReadyAt - 1e-9));
	}

	bool BattleCore::SkillCondition(const CombatUnit& _unit, bool _allyOnly) const
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

	void BattleCore::TickSkill(CombatUnit& _unit)
	{
		Skill(_unit.MyId).Tick();
	}

	bool BattleCore::SkillAboutToAttack(CombatUnit& _unit)
	{
		const auto& definition = _unit.MyDefinition.MySkill;
		if (definition.MyTrigger != SkillTrigger::DEFAULT || !CanAutoSkill(_unit) || !SkillCondition(_unit) ||
			(definition.MyTriggerAllies && !SkillCondition(_unit, true))) return false;
		return ActivateSkill(_unit.MyId, false, SkillReason::TRIGGER);
	}

	void BattleCore::SkillDamaged(CombatUnit& _unit)
	{
		const auto& definition = _unit.MyDefinition.MySkill;
		if (definition.MySpType == SpType::HURT) GainSp(_unit.MyId, 1, SpReason::HURT);
		if (definition.MyTrigger == SkillTrigger::TAKE_DAMAGE && CanAutoSkill(_unit))
			ActivateSkill(_unit.MyId, false, SkillReason::TRIGGER);
	}

	void BattleCore::SkillAttackPerformed(CombatUnit& _unit, bool _usedOverride, bool _noAmmo, std::span<const UnitId> _targets)
	{
		const auto& definition = _unit.MyDefinition.MySkill;
		auto& skill = _unit.MySkill;
		const bool skillAttack = skill.MyActive && (IsTimedSkill(definition.MyKind) || (_usedOverride && skill.MyPending));
		ContentEvent event{.MyKind = ContentEventKind::ATTACK, .MyUnit = _unit.MyId, .MySource = _unit.MyId, .MyNoAmmo = _noAmmo, .MyTargetCount = _targets.size(), .MySkillAttack = skillAttack, .MyTargets = _targets};
		NotifyContent(event);
		if (Finished() || !_unit.MyAlive) return;
		if (const auto* mizuki = _unit.MyDefinition.MyOperatorKit ? std::get_if<MizukiKit>(_unit.MyDefinition.MyOperatorKit) : nullptr;
			mizuki && mizuki->MySkill == MizukiSkillKind::MIRROR && skill.MyActive && std::ranges::count_if(_targets, [&](UnitId _id) { return Unit(_id).MySide == UnitSide::ENEMY; }) < 3)
			(void)LoseHealth(0, _unit.MyId, _unit.MyStats.MyMaxHealth * mizuki->MySelfLoss);
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
