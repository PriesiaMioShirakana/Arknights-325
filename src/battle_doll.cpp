#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::StartDollSwitch(CombatUnit& _unit)
	{
		if (_unit.MySkill.MyActive && _unit.MyDefinition.MySkill.MyKind != SkillKind::PASSIVE)
			EndSkill(_unit.MyId, SkillReason::SUBSTITUTE);
		// 原版“清除 Buff”仅清状态及当前技能；普通属性增益、外部治疗与护盾不在此处驱散。
		std::vector<std::uint64_t> statusBuffs; statusBuffs.reserve(_unit.MyBuffs.size());
		for (const auto& buff : _unit.MyBuffs) if (buff.MyDefinition.MyStatus) statusBuffs.push_back(buff.MyId);
		for (const auto id : statusBuffs) (void)RemoveBuff(_unit.MyId, id);
		for (std::size_t i = 0; i < static_cast<std::size_t>(CombatStatus::COUNT); ++i)
			(void)RemoveStatus(_unit.MyId, static_cast<CombatStatus>(i));
		_unit.MyProfession.MyDollSwitching = true;
		StatusFlags flags;
		for (const auto status : {CombatStatus::INVULNERABLE, CombatStatus::NO_SP, CombatStatus::NO_HEAL,
			CombatStatus::HEAL_FREE, CombatStatus::ISOLATED, CombatStatus::DISARM}) flags.set(static_cast<std::size_t>(status));
		(void)AddBuff(_unit.MyId, BuffDefinition{.MyKey = "trait:dollSwitching", .MyDuration = 1,
			.MyFlags = flags, .MyBuiltin = BuiltinBuff::DOLL_SWITCH});
	}

	bool Battle::EnterDoll(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		const auto& definition = unit.MyDefinition.MyProfession;
		if (!_MyStarted || Finished() || !unit.MyAlive || definition.MyKind != ProfessionTrait::DOLLKEEPER || unit.MyProfession.MyDoll) return false;
		unit.MyProfession.MyDoll = true;
		StartDollSwitch(unit);
		StatusFlags flags;
		flags.set(static_cast<std::size_t>(CombatStatus::NO_SP));
		if (definition.MyDollNoAttack)
		{
			flags.set(static_cast<std::size_t>(CombatStatus::DISARM));
			flags.set(static_cast<std::size_t>(CombatStatus::SILENCE));
		}
		(void)AddBuff(_unit, BuffDefinition{.MyKey = "trait:substitute", .MyDuration = 1 + definition.MyDollDuration,
			.MyModifiers = std::vector{AttributeChange{.MyAttribute = Attribute::BLOCK_COUNT, .MyValue = -99},
				AttributeChange{.MyAttribute = Attribute::HEALTH_MULTIPLIER, .MyValue = definition.MyDollHealthMultiplier}},
			.MyFlags = flags, .MyBuiltin = BuiltinBuff::DOLL_FORM});
		ReleaseBlocked(unit);
		unit.MyHealth = unit.MyStats.MyMaxHealth;
		ContentEvent event{.MyKind = ContentEventKind::DOLL_SWAP, .MyUnit = _unit, .MyDoll = true};
		NotifyContent(event);
		return true;
	}

	void Battle::EndProfessionBuff(CombatUnit& _unit, BuiltinBuff _kind, bool _expired)
	{
		if (_kind == BuiltinBuff::DOLL_SWITCH)
		{
			_unit.MyProfession.MyDollSwitching = false;
			// 到期写回满血不算治疗；显式驱散只结束动画，不产生满血效果。
			if (_expired && _unit.MyAlive) _unit.MyHealth = _unit.MyStats.MyMaxHealth;
		}
		else if (_kind == BuiltinBuff::DOLL_FORM && _unit.MyProfession.MyDoll)
		{
			_unit.MyProfession.MyDoll = false;
			if (!_unit.MyAlive) return;
			StartDollSwitch(_unit);
			_unit.MyHealth = _unit.MyStats.MyMaxHealth;
			ContentEvent event{.MyKind = ContentEventKind::DOLL_SWAP, .MyUnit = _unit.MyId};
			NotifyContent(event);
		}
	}
}
