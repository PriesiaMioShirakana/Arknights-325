#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::Excu2Skill(UnitId _unit, const Excu2Kit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			unit.MyExcuSpent = 0;
			if (_kit.MySkill == Excu2SkillKind::VERDICT)
			{
				unit.MyVerdictTargets.clear();
				unit.MyReaperHeal = unit.MyDefinition.MyProfession.MySelfHeal * _kit.MyHealScale;
			}
			const auto count = std::ranges::count_if(_MyAllyIds, [&](UnitId _id)
			{
				const auto& ally = Unit(_id); const auto& identity = ally.MyDefinition.MyIdentity;
				return ally.MyAlive && ally.MyKind == UnitKind::OPERATOR && (identity.MyNationId == "laterano" || std::ranges::contains(identity.MyBonds, "lateranoShip"));
			});
			(void)AddSkillAmmo(_unit, std::min(_kit.MyFactionCap, static_cast<double>(count)) * _kit.MyFactionAmmo);
		}
		else if (_event.MyKind == ContentEventKind::AMMO_USED)
		{
			++unit.MyExcuSpent;
			if (_kit.MySkill == Excu2SkillKind::VERDICT && unit.MySkill.MyActive)
				(void)AddBuff(_unit, {.MyKey = "excu2:verdict", .MyMaxStacks = _kit.MyMaxStacks, .MyRefresh = BuffRefresh::STACK,
					.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _kit.MyAttackPerAmmo}}});
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING && _kit.MySkill == Excu2SkillKind::VERDICT)
		{
			unit.MyReaperHeal.reset();
			auto& scratch = AcquireAttackScratch();
			scratch.MyTargets.assign(unit.MyVerdictTargets.begin(), unit.MyVerdictTargets.end());
			unit.MyVerdictTargets.clear();
			for (const auto id : scratch.MyTargets)
				if (unit.MyAlive && Unit(id).MyAlive) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyFinalScale,
					.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
			--_MyAttackDepth;
			(void)RemoveBuff(_unit, "excu2:verdict");
		}
		else if (_event.MyKind == ContentEventKind::SKILL_END) unit.MyExcuSpent = 0;
		else if (_event.MyKind == ContentEventKind::ATTACK && unit.MyAlive && !unit.MyExtraAttack)
		{
			const auto chance = _kit.MyExtraChance + (unit.MySkill.MyActive ? _kit.MyChancePerAmmo * unit.MyExcuSpent : 0);
			if (std::isgreater(chance, 0) && std::isless(_MyRandom.Next(), std::min(1.0, chance)))
				Schedule({.MyAt = Time(), .MyKind = ScheduledKind::EXCU2_ATTACK, .MySource = _unit, .MyVersion = unit.MyDeploySequence});
		}
	}

	void Battle::Excu2Dodge(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MyTarget || !_event.MySource || !_event.MyDamage.MyIsAttack) return;
		const auto& unit = Unit(_event.MyTarget); const auto& enemy = Unit(_event.MySource);
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<Excu2Kit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || kit->MySkill != Excu2SkillKind::GUNFIGHT || !unit.MySkill.MyActive || unit.MyOperatorHooksReleased || enemy.MySide != UnitSide::ENEMY) return;
		const auto& attack = EffectiveAttack(enemy); const auto radius = enemy.MyDefinition.MyAttack.MyEnemyRange;
		if (attack.MyRanged && std::isgreater(radius, 0) && !(enemy.MyBlockedBy == unit.MyId && std::isless(radius, 1))) return;
		if (!std::isgreater(kit->MyDodgeChance, 0) || !std::isless(_MyRandom.Next(), kit->MyDodgeChance)) return;
		_event.MyCancel = true;
		ContentEvent dodge{.MyKind = ContentEventKind::DODGED, .MySource = enemy.MyId, .MyTarget = unit.MyId, .MyDamage = _event.MyDamage};
		NotifyContent(dodge);
		(void)AddSkillAmmo(unit.MyId, kit->MyRefill);
	}

	void Battle::Excu2ExtraAttack(UnitId _unit, std::uint64_t _deployment)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (!unit.MyAlive || unit.MyRemoved || unit.MyOperatorHooksReleased || unit.MyDeploySequence != _deployment ||
			unit.MyStatuses.Has(CombatStatus::STUN) || unit.MyStatuses.Has(CombatStatus::DISARM)) return;
		unit.MyExtraAttack = true;
		try { (void)ForceAttack(_unit, {}, true); }
		catch (...) { unit.MyExtraAttack = false; throw; }
		unit.MyExtraAttack = false;
	}
}
