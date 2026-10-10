#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::MlynarTrait(UnitId _unit, const MlynarKit& _kit)
	{
		const auto& unit = Unit(_unit); const auto ramp = unit.MyProfession.MyRamp;
		const auto extra = unit.MyMlynarUp ? std::max(0.0, ramp * _kit.MyTraitScale + _kit.MyPerKill * unit.MyMlynarKills) - ramp : 0;
		SetOperatorAttribute(_unit, "mlynar:traitUp", std::islessgreater(extra, 0), Attribute::ATTACK_PERCENT, extra);
	}

	void BattleCore::MlynarObserve(ContentEvent& _event, bool _early)
	{
		if (_early)
		{
			const auto id = _event.MySource ? _event.MySource : _event.MyUnit;
			if (!id) return;
			auto& unit = _MyUnits[Index(id)]; const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<MlynarKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
			if (!kit || kit->MySkill == MlynarSkillKind::ANGER || unit.MyOperatorHooksReleased) return;
			if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyTarget && (_event.MyDamage.MyIsAttack || _event.MyDamage.MyMlynarOwn) && !std::ranges::contains(unit.MyMlynarHits, _event.MyTarget)) unit.MyMlynarHits.push_back(_event.MyTarget);
			if (_event.MyKind == ContentEventKind::SKILL_END && kit->MySkill == MlynarSkillKind::SORROW)
				unit.MyMlynarRamp = unit.MyMlynarKeep && _event.MySkillReason != SkillReason::DEATH ? std::optional(unit.MyProfession.MyRamp) : std::nullopt;
			return;
		}
		if (_event.MyKind != ContentEventKind::DAMAGED || !_event.MyTarget || !_event.MySource || _event.MyElement) return;
		const auto& source = Unit(_event.MySource); const auto& target = Unit(_event.MyTarget);
		const auto isKazimierz = [](const CombatUnit& _unit)
		{ return _unit.MyKind == UnitKind::OPERATOR && (_unit.MyDefinition.MyIdentity.MyNationId == "kazimierz" || std::ranges::contains(_unit.MyDefinition.MyIdentity.MyBonds, "kazimierzShip")); };
		for (std::size_t i = 0, count = _MyMlynars.size(); i < count; ++i)
		{
			const auto& unit = Unit(_MyMlynars[i]); if (!unit.MyAlive || unit.MyOperatorHooksReleased) continue;
			const auto& kit = std::get<MlynarKit>(*unit.MyDefinition.MyOperatorKit);
			if (std::isgreater(kit.MyReflectScale, 0) && target.MySide == UnitSide::ALLY && isKazimierz(target) && source.MySide == UnitSide::ENEMY && source.MyAlive &&
				!_event.MyDamage.MySourceless && !HasTag(_event.MyDamage.MyTags, DamageTag::HP_LOSS) && !HasTag(_event.MyDamage.MyTags, DamageTag::COUNTER) && !HasTag(_event.MyDamage.MyTags, DamageTag::REFLECT) && IsOperatorLeader(unit.MyId))
				(void)DealDamage(unit.MyId, source.MyId, {.MyAmount = unit.MyStats.MyAttack * kit.MyReflectScale, .MyType = DamageType::TRUE_DAMAGE,
					.MyCanDodge = false, .MyTags = DamageTag::TALENT | DamageTag::REFLECT});
			if (kit.MySkill != MlynarSkillKind::GLORY || !std::isgreater(kit.MyMarkScale, 0) || source.MySide != UnitSide::ALLY || !isKazimierz(source) || !_event.MyDamage.MyIsAttack ||
				target.MySide != UnitSide::ENEMY || !target.MyAlive || !unit.MySkill.MyActive || !unit.MyAlive || !InRuleRange(unit.MyId, target.MyId)) continue;
			(void)DealDamage(unit.MyId, target.MyId, {.MyAmount = unit.MyStats.MyAttack * kit.MyMarkScale, .MyType = DamageType::TRUE_DAMAGE,
				.MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true, .MyMlynarOwn = source.MyId == unit.MyId});
		}
	}

	void BattleCore::MlynarSkill(UnitId _unit, const MlynarKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			if (_kit.MySkill == MlynarSkillKind::SORROW) unit.MyMlynarKeep = false;
			if (_kit.MySkill == MlynarSkillKind::GLORY)
			{ unit.MyMlynarUp = true; unit.MyMlynarKills = unit.MyMlynarPending = 0; MlynarTrait(_unit, _kit); }
		}
		else if (_event.MyKind == ContentEventKind::BEFORE_KILL && unit.MySkill.MyActive && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY && std::ranges::contains(unit.MyMlynarHits, _event.MyTarget))
		{
			if (_kit.MySkill == MlynarSkillKind::SORROW) unit.MyMlynarKeep = true;
			else if (_kit.MySkill == MlynarSkillKind::GLORY) ++unit.MyMlynarPending;
		}
		else if (_event.MyKind == ContentEventKind::ATTACK)
		{
			unit.MyMlynarHits.clear();
			if (unit.MyMlynarPending && unit.MyMlynarUp)
			{ unit.MyMlynarKills += unit.MyMlynarPending; unit.MyMlynarPending = 0; MlynarTrait(_unit, _kit); }
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING && _kit.MySkill == MlynarSkillKind::GLORY)
		{ unit.MyMlynarUp = false; unit.MyMlynarPending = 0; (void)RemoveBuff(_unit, "mlynar:traitUp"); }
		else if (_event.MyKind == ContentEventKind::SKILL_END && _kit.MySkill == MlynarSkillKind::SORROW && unit.MyMlynarRamp)
		{
			unit.MyProfession.MyRamp = *unit.MyMlynarRamp; unit.MyMlynarRamp.reset();
			if (std::isgreater(unit.MyProfession.MyRamp, 0) && unit.MyAlive) (void)AddBuff(_unit, {.MyKey = "trait:libratorRamp",
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = unit.MyProfession.MyRamp}}});
		}
	}
}
