#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct RequiemScratchGuard
		{
			std::size_t& MyDepth;

			~RequiemScratchGuard() { --MyDepth; }
		};

		double CelloBoost(const CombatUnit& _unit)
		{
			return std::isgreater(_unit.MyCelloBoost, 0) ? _unit.MyCelloBoost : 1;
		}
	}

	void BattleCore::CelloPick(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)]; unit.MyCelloPartner = 0; unit.MyCelloPick = 0.5;
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (id == _unit || !ally.MyAlive || ally.MyHidden || ally.MyKind != UnitKind::OPERATOR || ally.MyStatuses.Has(CombatStatus::ISOLATED) || !InRuleRange(_unit, id)) continue;
			if (!unit.MyCelloPartner || std::isgreater(ally.MyStats.MyAttack, Unit(unit.MyCelloPartner).MyStats.MyAttack + 1e-9)) unit.MyCelloPartner = id;
		}
	}

	void BattleCore::CelloSkill(UnitId _unit, const CelloKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_kit.MySkill == CelloSkillKind::REQUIEM)
		{
			if (_event.MyKind == ContentEventKind::SKILL_START) CelloPick(_unit);
			if (_event.MyKind == ContentEventKind::SKILL_ENDING) unit.MyCelloPartner = 0;
			if (_event.MyKind == ContentEventKind::SKILL_TICK)
			{
				unit.MyCelloPick -= _event.MyDelta;
				if (!std::isgreater(unit.MyCelloPick, 0) || !unit.MyCelloPartner || !Unit(unit.MyCelloPartner).MyAlive || Unit(unit.MyCelloPartner).MyHidden) CelloPick(_unit);
			}
			return;
		}
		if (_kit.MySkill != CelloSkillKind::TANGO) return;
		if (_event.MyKind == ContentEventKind::SKILL_START) { unit.MyCelloBoost = _kit.MyTalentBoost; unit.MyCelloAccumulator = std::numeric_limits<double>::infinity(); }
		if (_event.MyKind == ContentEventKind::SKILL_ENDING) unit.MyCelloBoost = 1;
		if (_event.MyKind != ContentEventKind::SKILL_TICK) return;
		unit.MyCelloAccumulator += _event.MyDelta;
		if (std::isless(unit.MyCelloAccumulator, 0.5)) return;
		unit.MyCelloAccumulator = 0;
		std::array<UnitId, 3> highest{}; std::array<double, 3> values{};
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (id == _unit || !ally.MyAlive || ally.MyHidden || ally.MyKind != UnitKind::OPERATOR || ally.MyStatuses.Has(CombatStatus::ISOLATED) || !InRuleRange(_unit, id)) continue;
			const std::array base{ally.MyDefinition.MyStats.MyMaxHealth, ally.MyDefinition.MyStats.MyAttack, ally.MyDefinition.MyStats.MyDefense};
			for (std::size_t i = 0; i < highest.size(); ++i) if (!highest[i] || std::isgreater(base[i], values[i] + 1e-9)) { highest[i] = id; values[i] = base[i]; }
		}
		constexpr std::array keys{"cello:tango:hp", "cello:tango:atk", "cello:tango:def"};
		constexpr std::array attributes{Attribute::HEALTH_PERCENT, Attribute::ATTACK_PERCENT, Attribute::DEFENSE_PERCENT};
		const std::array amounts{_kit.MyGrantHealth, _kit.MyGrantAttack, _kit.MyGrantDefense};
		for (std::size_t i = 0; i < highest.size(); ++i) if (highest[i]) (void)AddBuff(highest[i], {.MyKey = keys[i], .MySource = _unit, .MyDuration = 0.75,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = attributes[i], .MyValue = amounts[i]}}});
	}

	void BattleCore::CelloElement(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::ELEMENT_HIT || !_event.MyTarget || Unit(_event.MyTarget).MySide != UnitSide::ENEMY) return;
		if (_event.MyElementHit.MyElement == Element::APOPTOSIS)
		{
			double best = 1;
			for (const auto id : _MyCellos)
			{
				const auto& unit = Unit(id); const auto& kit = std::get<CelloKit>(*unit.MyDefinition.MyOperatorKit);
				if (!std::isgreater(kit.MyAmplification, 1) || !unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased || (!kit.MyFieldWide && !InRuleRange(id, _event.MyTarget))) continue;
				best = std::max(best, 1 + (kit.MyAmplification - 1) * CelloBoost(unit));
			}
			_event.MyElementHit.MyMultiplier *= best;
		}
		if (!_event.MySource) return;
		const auto& source = Unit(_event.MySource); const auto* kit = source.MyDefinition.MyOperatorKit ? std::get_if<CelloKit>(source.MyDefinition.MyOperatorKit) : nullptr;
		const auto& target = Unit(_event.MyTarget);
		if (kit && !source.MyOperatorHooksReleased && std::isgreater(kit->MyEliteScale, 1) && (target.MyDefinition.MyElite || target.MyDefinition.MyLeader || target.MySpawnTag == EnemySpawnTag::BOSS)) _event.MyElementHit.MyMultiplier *= kit->MyEliteScale;
	}

	void BattleCore::CelloHit(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || _event.MyCancel || !_event.MySource || !_event.MyTarget || _event.MyDamage.MyType == DamageType::ELEMENTAL ||
			!std::isgreater(_event.MyDamage.MyAmount, 0) || Unit(_event.MyTarget).MySide != UnitSide::ENEMY) return;
		for (const auto id : _MyCellos)
		{
			const auto& unit = Unit(id); const auto& kit = std::get<CelloKit>(*unit.MyDefinition.MyOperatorKit);
			if (kit.MySkill != CelloSkillKind::REQUIEM || !unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased || !unit.MySkill.MyActive || !std::isgreater(kit.MySkillElement, 0) ||
				(_event.MySource != id && _event.MySource != unit.MyCelloPartner) || !Unit(_event.MyTarget).MyAlive || !std::isgreater(Unit(_event.MyTarget).MyHealth, 0)) continue;
			(void)DealElement(id, _event.MyTarget, {.MyElement = Element::APOPTOSIS, .MyAmount = unit.MyStats.MyAttack * kit.MySkillElement, .MyTags = static_cast<DamageTags>(DamageTag::SKILL)});
		}
	}

	void BattleCore::CelloPulse(UnitId _unit, unsigned _kind)
	{
		const auto& unit = Unit(_unit); const auto& kit = std::get<CelloKit>(*unit.MyDefinition.MyOperatorKit);
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) return;
		auto& scratch = AcquireAttackScratch(); const RequiemScratchGuard guard{.MyDepth = _MyAttackDepth};
		const AttackProfile filter{.MyCanHitFlying = true};
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (_kind == 2 ? (enemy.MyAlive && !enemy.MyHidden && std::ranges::any_of(enemy.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "apoptosisBurst"; })) :
				(TargetableEnemy(enemy, filter) && InRuleRange(_unit, id))) scratch.MyTargets.push_back(id);
		}
		for (const auto id : scratch.MyTargets)
		{
			if (_kind == 0)
			{
				if (std::isgreater(kit.MyElementRatio, 0) && std::isgreater(Unit(id).MyHealth, 0)) (void)DealElement(_unit, id, {.MyElement = Element::APOPTOSIS, .MyAmount = unit.MyStats.MyAttack * kit.MyElementRatio, .MyTags = static_cast<DamageTags>(DamageTag::TALENT)});
				if (std::isgreater(kit.MySluggish, 0) && Unit(id).MyAlive) (void)ApplyStatus(id, CombatStatus::SLUGGISH, kit.MySluggish, _unit);
			}
			else if (_kind == 1)
			{
				if (std::isgreater(kit.MyFragile, 0) && std::ranges::any_of(Unit(id).MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "apoptosisBurst"; }))
					(void)ApplyStatus(id, CombatStatus::ELEMENTAL_FRAGILE, {.MyDuration = 0.4, .MySource = _unit, .MyValue = kit.MyFragile * CelloBoost(unit)});
				if (std::isgreater(kit.MyModuleFragile, 0)) (void)ApplyStatus(id, CombatStatus::ELEMENTAL_FRAGILE, {.MyDuration = 0.4, .MySource = _unit, .MyValue = kit.MyModuleFragile});
			}
			else (void)DealDamage(_unit, id, {.MyAmount = kit.MyDot * CelloBoost(unit), .MyType = DamageType::ELEMENTAL, .MyCanDodge = false, .MyTags = DamageTag::TALENT | DamageTag::ELEMENT_DAMAGE});
		}
	}
}
