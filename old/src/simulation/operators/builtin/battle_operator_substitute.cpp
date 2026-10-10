#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::Ghost2Team(UnitId _unit)
	{
		const auto& unit = Unit(_unit); const auto& kit = std::get<Ghost2Kit>(*unit.MyDefinition.MyOperatorKit);
		if (unit.MyOperatorHooksReleased || !std::islessgreater(kit.MyTeamHealth, 0)) return;
		for (const auto id : _MyAllyIds)
			if (Unit(id).MyOwner == unit.MyOwner && Unit(id).MyKind == UnitKind::OPERATOR && IsAbyssal(id))
				(void)AddBuff(id, {.MyKey = "ghost2:abyss", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALTH_PERCENT, .MyValue = kit.MyTeamHealth}},
					.MyPersistent = true, .MyAllowDead = true});
	}

	void BattleCore::Ghost2Pulse(UnitId _unit, bool _damage)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyOperatorHooksReleased || !unit.MyProfession.MyDoll || unit.MyProfession.MyDollSwitching) return;
		const auto& kit = std::get<Ghost2Kit>(*unit.MyDefinition.MyOperatorKit);
		if (_damage ? !std::isgreater(kit.MyDamageScale, 0) : !std::islessgreater(kit.MySlow, 0)) return;
		auto& scratch = AcquireAttackScratch();
		struct Guard
		{
			std::size_t& MyDepth;

			~Guard() { --MyDepth; }
		};
		const Guard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyEnemyIds)
			if (Unit(id).MyAlive && !Unit(id).MyHidden && !Unit(id).Flying() && OperatorInGrid(_unit, id, kit.MyAround)) scratch.MyTargets.push_back(id);
		for (const auto id : scratch.MyTargets)
		{
			if (_damage) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * kit.MyDamageScale, .MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::TALENT)});
			else (void)AddBuff(id, {.MyKey = "ghost2:embrace", .MyDuration = 0.5,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::MOVE_MULTIPLIER, .MyValue = std::max(0.0, 1 + kit.MySlow)}}});
		}
	}

	void BattleCore::Ghost2EachHit(UnitId _source, UnitId _target)
	{
		const auto& source = Unit(_source); const auto& target = Unit(_target);
		if (target.MySide != UnitSide::ENEMY) return;
		const auto& kit = std::get<Ghost2Kit>(*source.MyDefinition.MyOperatorKit);
		if (kit.MySkill != Ghost2SkillKind::WEIGHT) return;
		if (std::ranges::contains(source.MyGhostHeavy, _target))
		{
			if (target.MyAlive) (void)DealDamage(_source, _target, {.MyAmount = source.MyStats.MyAttack * kit.MyExtraScale,
				.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
		}
		else if (source.MyAlive) (void)LoseHealth(_source, _source, source.MyStats.MyMaxHealth * kit.MyHealthLoss);
	}

	void BattleCore::Ghost2Skill(UnitId _unit, const Ghost2Kit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::SKILL_ENDING && _kit.MySkill == Ghost2SkillKind::DESIRE &&
			_event.MySkillReason != SkillReason::DEATH && _event.MySkillReason != SkillReason::SUBSTITUTE && unit.MyAlive && !unit.MyProfession.MyDoll)
			(void)EnterDoll(_unit);
		if (_event.MyKind != ContentEventKind::SKILL_START || _kit.MySkill != Ghost2SkillKind::SHARE) return;
		UnitId best = 0; double ratio = std::numeric_limits<double>::infinity();
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (id == _unit || !ally.MyAlive || ally.MyHidden || ally.MyKind != UnitKind::OPERATOR || !std::isgreater(ally.MyHealth, 0) || !OperatorInGrid(_unit, id, _kit.MyShareRange)) continue;
			const auto value = ally.MyHealth / ally.MyStats.MyMaxHealth;
			if (std::isless(value, ratio) || (!std::islessgreater(value, ratio) && id < best)) { best = id; ratio = value; }
		}
		if (!best) return;
		auto& ally = _MyUnits[Index(best)]; const auto mine = unit.MyHealth / unit.MyStats.MyMaxHealth;
		unit.MyHealth = std::max(1.0, std::min(unit.MyStats.MyMaxHealth, unit.MyStats.MyMaxHealth * ratio));
		ally.MyHealth = std::max(1.0, std::min(ally.MyStats.MyMaxHealth, ally.MyStats.MyMaxHealth * mine));
	}
}
