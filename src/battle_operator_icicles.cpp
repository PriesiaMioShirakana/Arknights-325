#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		struct IcicleScratchGuard
		{
			std::size_t& MyDepth;

			~IcicleScratchGuard() { --MyDepth; }
		};
	}

	void Battle::SntllaTalent(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<SntllaKit>(*unit.MyDefinition.MyOperatorKit);
		if (std::isless(Time() - unit.MyDeployedAt, kit.MyDelay - 1e-9) ||
			std::ranges::any_of(unit.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "sntlla:born"; })) return;
		(void)AddBuff(_unit, {.MyKey = "sntlla:born", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyAttack}}});
		if (std::isgreater(kit.MyResist, 0)) (void)ApplyStatus(_unit, CombatStatus::RESIST, {.MySource = _unit, .MyValue = kit.MyResist});
	}

	void Battle::SntllaSkill(UnitId _unit, const SntllaKit& _kit, ContentEvent& _event)
	{
		if (!_kit.MyIcicle) return;
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::SKILL_START) { unit.MyIcicleCooldown = 0; unit.MyIcicleRow = 0; }
		if (_event.MyKind != ContentEventKind::SKILL_TICK) return;
		unit.MyIcicleCooldown -= _event.MyDelta;
		if (std::isgreater(unit.MyIcicleCooldown, 0) || !unit.MyAlive || unit.MyHidden || unit.MyStatuses.Has(CombatStatus::STUN) || unit.MyStatuses.Has(CombatStatus::DISARM)) return;
		const AttackProfile filter{.MyCanHitFlying = true};
		if (!std::ranges::any_of(_MyEnemyIds, [&](UnitId _id) { return TargetableEnemy(Unit(_id), filter) && InRuleRange(_unit, _id); })) return;
		unit.MyIcicleCooldown = unit.MyStats.AttackInterval();
		constexpr std::array<int, 3> Rows{1, -1, 0};
		const auto origin = RulePosition(unit); const auto forward = RotateOffset({.MyColumn = 1}, unit.MyFacing);
		const auto row = static_cast<int>(std::floor(origin.MyY + 0.5)), column = static_cast<int>(std::floor(origin.MyX + 0.5));
		auto& scratch = AcquireAttackScratch(); const IcicleScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (unsigned n = 0; n < Rows.size(); ++n)
		{
			const auto index = (unit.MyIcicleRow + n) % Rows.size(); scratch.MyBuffIds.clear();
			for (const auto key : RuleRangeKeys(unit))
				if ((key / FieldColumns - row) * forward.MyColumn - (key % FieldColumns - column) * forward.MyRow == Rows[index]) scratch.MyBuffIds.push_back(static_cast<std::uint64_t>(key));
			if (scratch.MyBuffIds.empty()) continue;
			const auto key = scratch.MyBuffIds[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MyBuffIds.size()))];
			unit.MyIcicleRow = static_cast<unsigned>((index + 1) % Rows.size());
			unit.MyLastAttackAt = Time(); ++unit.MyTotals.MyAttacks;
			Schedule({.MyAt = Time() + 0.3, .MyKind = ScheduledKind::SNTLLA_IMPACT, .MySource = _unit,
				.MyPoint = {.MyX = static_cast<double>(key % FieldColumns), .MyY = static_cast<double>(key / FieldColumns)}});
			return;
		}
	}

	void Battle::SntllaImpact(UnitId _unit, WorldPoint _point)
	{
		const auto& unit = Unit(_unit); const auto& kit = std::get<SntllaKit>(*unit.MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch(); const IcicleScratchGuard guard{.MyDepth = _MyAttackDepth};
		FoesInRadius(_point, 1.5, scratch.MyTargets);
		for (const auto id : scratch.MyTargets)
		{
			(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * kit.MyScale, .MyType = DamageType::ARTS,
				.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsAttack = true, .MyIsSplash = true, .MyIsSkill = true});
			if (Unit(id).MyAlive && std::isgreater(kit.MyCold, 0)) (void)ApplyStatus(id, CombatStatus::COLD, kit.MyCold, _unit);
			if (Unit(id).MyAlive) scratch.MySeen.push_back(id);
		}
		if (scratch.MySeen.empty()) return;
		ContentEvent event{.MyKind = ContentEventKind::ATTACK, .MySource = _unit, .MySkillAttack = true, .MyTargets = scratch.MySeen};
		NotifyContent(event);
	}
}
