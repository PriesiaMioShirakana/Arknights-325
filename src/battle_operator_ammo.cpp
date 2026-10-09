#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::ReloadNation(UnitId _unit, std::string_view _nation, double _ammo, std::optional<double> _radius)
	{
		if (!std::isgreater(_ammo, 0)) return;
		const auto& source = Unit(_unit); const auto origin = RulePosition(source);
		UnitId nearest = 0; double distance = std::numeric_limits<double>::infinity();
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (id == _unit || !ally.MyAlive || ally.MyOwner != source.MyOwner || ally.MyKind != UnitKind::OPERATOR ||
				ally.MyDefinition.MyIdentity.MyNationId != _nation || !ally.MySkill.MyActive || ally.MyDefinition.MySkill.MyKind != SkillKind::AMMO) continue;
			if (!_radius) { AddSkillAmmo(id, _ammo); continue; }
			if (ally.MyHidden) continue;
			const auto next = Distance(origin, RulePosition(ally));
			if (std::isgreater(next * next, *_radius * *_radius + 1e-9)) continue;
			if (!nearest || std::isless(next, distance) || (!std::islessgreater(next, distance) && id < nearest)) { nearest = id; distance = next; }
		}
		if (nearest) AddSkillAmmo(nearest, _ammo);
	}

	void Battle::RmixerShield(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<RmixerKit>(*unit.MyDefinition.MyOperatorKit);
		const auto since = std::max({unit.MyDeployedAt, unit.MyRmixerActiveAttackAt, unit.MyRmixerShieldLostAt});
		if (std::isless(Time() - since, kit.MyShieldInterval - 1e-9) ||
			std::ranges::any_of(unit.MyBuffs, [](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == "rmixer:shield"; })) return;
		(void)AddBuff(_unit, {.MyKey = "rmixer:shield", .MyShield = {.MyHealth = unit.MyStats.MyMaxHealth * kit.MyShieldRatio}, .MyBuiltin = BuiltinBuff::RMIXER_SHIELD});
	}

	void Battle::RmixerSkill(UnitId _unit, const RmixerKit& _kit, const ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY)
		{ unit.MyRmixerActiveAttackAt = unit.MyRmixerShieldLostAt = -std::numeric_limits<double>::infinity(); }
		else if (_event.MyKind == ContentEventKind::ATTACK && !unit.MyRmixerInCounter) unit.MyRmixerActiveAttackAt = Time();
		else if (_event.MyKind == ContentEventKind::SKILL_START && _kit.MySkill == RmixerSkillKind::COUNTER)
		{
			unit.MyRmixerCounterAt = -std::numeric_limits<double>::infinity();
			ReloadNation(_unit, "laterano", _kit.MyReload);
		}
		else if (_event.MyKind == ContentEventKind::DAMAGED && unit.MyAlive && _event.MyTarget &&
			Unit(_event.MyTarget).MySide == UnitSide::ENEMY && std::isgreater(_event.MyAmount, 0))
			(void)AddBuff(_unit, {.MyKey = "rmixer:t1", .MyDuration = _kit.MyStackDuration, .MyMaxStacks = _kit.MyStacks, .MyRefresh = BuffRefresh::STACK,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::DEFENSE_FLAT, .MyValue = _kit.MyDefense}, {.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = _kit.MyAttackSpeed}}});
	}

	void Battle::RmixerObserve(ContentEvent& _event)
	{
		const auto id = _event.MyKind == ContentEventKind::FATAL ? _event.MyUnit : _event.MyTarget;
		if (!id) return;
		auto& unit = _MyUnits[Index(id)];
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<RmixerKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || unit.MyOperatorHooksReleased || !unit.MySkill.MyActive) return;
		if (_event.MyKind == ContentEventKind::FATAL)
		{
			if (kit->MySkill != RmixerSkillKind::GUARD || _event.MyPrevented || !std::isgreater(unit.MySkill.MyAmmoLeft, 0) || HoldsUndying(id)) return;
			_event.MyPrevented = true;
			unit.MyHealth = _event.MyDamage.MySequence && unit.MyRmixerPreSequence == _event.MyDamage.MySequence ? std::max(1.0, std::min(unit.MyStats.MyMaxHealth, unit.MyRmixerPreHealth)) : 1;
			unit.MySkill.MyAmmoLeft -= std::min(kit->MyGuardCost, unit.MySkill.MyAmmoLeft);
			if (std::islessequal(unit.MySkill.MyAmmoLeft, 0)) EndSkill(id, SkillReason::AMMO);
			return;
		}
		if (_event.MyKind != ContentEventKind::DAMAGED || kit->MySkill != RmixerSkillKind::COUNTER || !unit.MyAlive || unit.MyHidden ||
			unit.MyStatuses.Has(CombatStatus::STUN) || unit.MyStatuses.Has(CombatStatus::DISARM) || _event.MyElement || _event.MyDamage.MyType == DamageType::ELEMENTAL ||
			HasTag(_event.MyDamage.MyTags, DamageTag::HP_LOSS) || std::isless(Time(), unit.MyRmixerCounterAt - 1e-9)) return;
		auto& scratch = AcquireAttackScratch();
		struct Guard
		{
			std::size_t& MyDepth;
			bool& MyInCounter;

			~Guard() { --MyDepth; MyInCounter = false; }
		};
		const Guard guard{.MyDepth = _MyAttackDepth, .MyInCounter = unit.MyRmixerInCounter};
		GenericEnemies(id, scratch.MyTargets, 0, true);
		if (_event.MySource)
		{
			const auto source = std::ranges::find(scratch.MyTargets, _event.MySource);
			if (source != scratch.MyTargets.end()) std::rotate(scratch.MyTargets.begin(), source, std::next(source));
		}
		if (scratch.MyTargets.size() > kit->MyCounterTargets) scratch.MyTargets.resize(kit->MyCounterTargets);
		unit.MyRmixerCounterAt = Time() + unit.MyStats.AttackInterval() * kit->MyCounterRatio;
		unit.MyRmixerInCounter = true;
		if (!scratch.MyTargets.empty()) (void)ForceAttack(id, scratch.MyTargets);
		else { ++unit.MyTotals.MyAttacks; SkillAttackPerformed(unit, true, false, {}); }
	}
}
