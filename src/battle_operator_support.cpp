#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		struct OperatorScratchGuard
		{
			std::size_t& MyDepth;

			~OperatorScratchGuard() { --MyDepth; }
		};
	}

	bool Battle::OperatorCanAttack(const CombatUnit& _unit) const
	{
		const auto* rules = _unit.MyDefinition.MyOperatorKit;
		if (rules && std::holds_alternative<IndigoKit>(*rules))
			return std::ranges::any_of(_MyEnemyIds, [&](UnitId id)
			{
				const auto& enemy = Unit(id);
				return !enemy.MyStatuses.Has(CombatStatus::BIND) && TargetableEnemy(enemy, _unit.MyDefinition.MyAttack) &&
					(enemy.MyBlockedBy == _unit.MyId || InRange(_unit, enemy));
			});
		const auto* prove = rules ? std::get_if<ProveKit>(rules) : nullptr;
		if (!prove || !prove->MyHunt || !_unit.MySkill.MyActive) return true;
		return std::ranges::any_of(_MyEnemyIds, [&](UnitId id)
		{
			const auto& enemy = Unit(id);
			return TargetableEnemy(enemy, EffectiveAttack(_unit)) && InRuleRange(_unit.MyId, id) && enemy.MyHealth / enemy.MyStats.MyMaxHealth <= 0.8 + 1e-9;
		});
	}

	void Battle::OperatorAfterHit(UnitId _source, UnitId _target)
	{
		const auto& source = Unit(_source); const auto* rules = source.MyDefinition.MyOperatorKit;
		if (!_target || !rules || !Unit(_target).MyAlive || Unit(_target).MySide != UnitSide::ENEMY) return;
		if (const auto* kit = std::get_if<IndigoKit>(rules))
		{
			const auto probability = kit->MyProbability * (source.MySkill.MyActive ? kit->MySkillProbabilityScale : 1);
			// 原 rng.chance 即使概率为 1 也消耗随机数；整次命中（含蓄能）只判断一次。
			if (probability > 0 && _MyRandom.Next() < std::min(1.0, probability))
				(void)ApplyStatus(_target, CombatStatus::BIND, kit->MyBindDuration, _source);
		}
	}

	void Battle::IndigoTick(UnitId _unit, const IndigoKit& _kit, double _delta)
	{
		if (!(_kit.MyDamageScale > 0) || !(_kit.MyInterval > 0)) return;
		auto& unit = _MyUnits[Index(_unit)]; unit.MyIndigoAccumulator += _delta;
		if (unit.MyIndigoAccumulator < _kit.MyInterval - 1e-9) return;
		auto& scratch = AcquireAttackScratch(); const OperatorScratchGuard guard{_MyAttackDepth};
		while (unit.MyIndigoAccumulator >= _kit.MyInterval - 1e-9)
		{
			unit.MyIndigoAccumulator -= _kit.MyInterval;
			GenericEnemies(_unit, scratch.MyTargets, 0, false);
			for (const auto id : scratch.MyTargets)
				if (Unit(id).MyStatuses.Has(CombatStatus::BIND))
					(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyDamageScale, .MyType = DamageType::ARTS,
						.MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::DOT), .MyIsSkill = true});
		}
	}

	void Battle::OperatorEnemiesInGrid(UnitId _source, std::span<const RangeOffset> _grid, std::vector<UnitId>& _targets)
	{
		const auto& source = Unit(_source); const auto point = RulePosition(source);
		const int row = static_cast<int>(std::floor(point.MyY + 0.5)), column = static_cast<int>(std::floor(point.MyX + 0.5));
		const AttackProfile filter{.MyCanHitFlying = true};
		_targets.clear(); _targets.reserve(_MyEnemyIds.size());
		for (const auto offset : _grid)
		{
			const auto local = RotateOffset(offset, source.MyFacing);
			const auto r = row + local.MyRow, c = column + local.MyColumn;
			if (r < 0 || r >= FieldRows || c < 0 || c >= FieldColumns) continue;
			for (const auto id : _MyEnemyIds)
				if (TargetableEnemy(Unit(id), filter) && BodyOnTile(Unit(id), r, c) && !std::ranges::contains(_targets, id)) _targets.push_back(id);
		}
		SortOperatorTargets(_source, _targets);
	}

	UnitId Battle::HighestHealthAllyInRange(UnitId _source) const
	{
		UnitId best = 0;
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (!ally.MyAlive || ally.MyHidden || ally.MyKind == UnitKind::DEVICE || (id != _source && ally.MyStatuses.Has(CombatStatus::ISOLATED))) continue;
			const auto point = RulePosition(ally);
			const int row = static_cast<int>(std::floor(point.MyY + 0.5)), column = static_cast<int>(std::floor(point.MyX + 0.5));
			if (row < 0 || row >= FieldRows || column < 0 || column >= FieldColumns ||
				!RuleRange(_source).test(static_cast<std::size_t>(FieldGrid::Key(row, column)))) continue;
			if (!best || ally.MyStats.MyMaxHealth > Unit(best).MyStats.MyMaxHealth ||
				(ally.MyStats.MyMaxHealth == Unit(best).MyStats.MyMaxHealth && ally.MyDeploySequence < Unit(best).MyDeploySequence)) best = id;
		}
		return best;
	}

	void Battle::NotifyVendlas(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::SKILL_START && _event.MyKind != ContentEventKind::SKILL_ENDING &&
			_event.MyKind != ContentEventKind::BEFORE_HEAL && _event.MyKind != ContentEventKind::DAMAGED) return;
		const auto count = _MyVendlas.size();
		for (std::size_t i = 0; i < count; ++i)
		{
			auto& state = _MyVendlas[i]; const auto& source = Unit(state.MyUnit);
			const auto& kit = std::get<VendlaKit>(*source.MyDefinition.MyOperatorKit);
			if (_event.MyKind == ContentEventKind::SKILL_START && _event.MyUnit == state.MyUnit && kit.MyProtection)
			{
				state.MyProtege = HighestHealthAllyInRange(state.MyUnit);
				if (state.MyProtege) (void)AddBuff(state.MyProtege, {.MyKey = "vendla:taunt", .MySource = state.MyUnit,
					.MyDuration = source.MyDefinition.MySkill.MyDuration + 0.1, .MyModifiers = std::vector<AttributeChange>{{Attribute::TAUNT, kit.MyTaunt}}});
			}
			else if (_event.MyKind == ContentEventKind::SKILL_ENDING && _event.MyUnit == state.MyUnit && kit.MyProtection)
			{
				if (state.MyProtege) (void)RemoveBuff(state.MyProtege, "vendla:taunt");
				state.MyProtege = 0;
			}
			else if (_event.MyKind == ContentEventKind::BEFORE_HEAL && kit.MyHealingScale != 1 && source.MyAlive && !_event.MyHealOptions.MyRegen)
			{
				if (state.MyCachedAt != Time()) { state.MyCachedAt = Time(); state.MyHealingTarget = HighestHealthAllyInRange(state.MyUnit); }
				if (_event.MyTarget == state.MyHealingTarget) _event.MyAmount *= kit.MyHealingScale;
			}
			else if (_event.MyKind == ContentEventKind::DAMAGED && source.MyAlive && !source.MyHidden && !source.MyStatuses.Has(CombatStatus::STUN) && source.MySkill.MyActive &&
				state.MyProtege && _event.MyTarget == state.MyProtege && Unit(state.MyProtege).MyAlive && _event.MySource &&
				Unit(_event.MySource).MySide == UnitSide::ENEMY && Unit(_event.MySource).MyAlive && !_event.MyDamage.MySourceless && !_event.MyElement &&
				!HasTag(_event.MyDamage.MyTags, DamageTag::HP_LOSS) && !HasTag(_event.MyDamage.MyTags, DamageTag::COUNTER) && !HasTag(_event.MyDamage.MyTags, DamageTag::REFLECT))
				(void)DealDamage(state.MyUnit, _event.MySource, {.MyAmount = source.MyStats.MyAttack * kit.MyCounterScale, .MyType = DamageType::ARTS, .MyCanDodge = false,
					.MyTags = static_cast<DamageTags>(DamageTag::COUNTER), .MyTraitAlly = state.MyProtege, .MyIsSkill = true});
		}
	}

	void Battle::TexasSkill(UnitId _unit, const TexasKit& _kit)
	{
		const auto& source = Unit(_unit);
		(void)AddDp(_MyPlayers[source.MyOwner].MyPlayerId, _kit.MyDp);
		if (!_kit.MySwordRain) return;
		auto& scratch = AcquireAttackScratch(); const OperatorScratchGuard guard{_MyAttackDepth};
		if (!_kit.MyRange.empty()) OperatorEnemiesInGrid(_unit, _kit.MyRange, scratch.MyTargets);
		else
		{
			const AttackProfile filter{.MyCanHitFlying = true, .MyHitSleep = true};
			for (const auto id : _MyEnemyIds) if (TargetableEnemy(Unit(id), filter) && BodyDistance(Unit(id), RulePosition(source)) <= 1.5 + 1e-9) scratch.MyTargets.push_back(id);
		}
		for (const auto id : scratch.MyTargets)
		{
			for (unsigned hit = 0; hit < 2 && Unit(id).MyAlive; ++hit)
				(void)DealDamage(_unit, id, {.MyAmount = source.MyStats.MyAttack * _kit.MyScale, .MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
			if (Unit(id).MyAlive) (void)ApplyStatus(id, CombatStatus::STUN, _kit.MyStun, _unit);
		}
	}

	void Battle::SunbrSkill(UnitId _unit, const SunbrKit& _kit, ContentEventKind _event)
	{
		if (!_kit.MyCooking) return;
		if (_event == ContentEventKind::SKILL_START)
		{
			StatusFlags flags; flags.set(static_cast<std::size_t>(CombatStatus::DISARM));
			(void)AddBuff(_unit, {.MyKey = "sunbr:cook", .MyDuration = _kit.MyCookingSeconds,
				.MyModifiers = std::vector<AttributeChange>{{Attribute::DEFENSE_PERCENT, _kit.MyCookingDefense}}, .MyFlags = flags});
		}
		else if (_event == ContentEventKind::SKILL_TICK)
		{
			const auto& buffs = Unit(_unit).MyBuffs;
			if (std::ranges::none_of(buffs, [](const auto& buff) { return buff.MyDefinition.MyKey == "sunbr:cook" || buff.MyDefinition.MyKey == "sunbr:serve"; }))
				(void)AddBuff(_unit, {.MyKey = "sunbr:serve", .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, _kit.MyServingAttack}}});
		}
		else if (_event == ContentEventKind::SKILL_ENDING)
		{
			(void)RemoveBuff(_unit, "sunbr:cook"); (void)RemoveBuff(_unit, "sunbr:serve");
		}
	}
}
