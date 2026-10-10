#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct RecastScratchGuard
		{
			std::size_t& MyDepth;

			~RecastScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::SetOperatorModifiers(UnitId _unit, std::string_view _key, bool _on, std::span<const AttributeChange> _modifiers)
	{
		if (!_on) { (void)RemoveBuff(_unit, _key); return; }
		const auto& buffs = Unit(_unit).MyBuffs;
		const auto found = std::ranges::find(buffs, _key, [](const CombatBuff& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
		if (found != buffs.end() && found->MyDefinition.MyModifiers && std::ranges::equal(*found->MyDefinition.MyModifiers, _modifiers,
			[](const AttributeChange& _a, const AttributeChange& _b) { return _a.MyAttribute == _b.MyAttribute && !std::islessgreater(_a.MyValue, _b.MyValue); })) return;
		(void)AddBuff(_unit, {.MyKey = std::string(_key), .MyModifiers = std::vector<AttributeChange>(_modifiers.begin(), _modifiers.end())});
	}

	void BattleCore::ApplyResistanceCut(UnitId _unit, UnitId _target, std::string_view _key, double _duration, double _value)
	{
		if (!Unit(_target).MyAlive || !std::islessgreater(_value, 0)) return;
		const bool fractional = std::isless(std::abs(_value), 1);
		(void)ApplyStrongest(_target, std::string(_key), _duration, {.MyValue = _value,
			.MyAttribute = fractional ? Attribute::RESISTANCE_MULTIPLIER : Attribute::RESISTANCE_FLAT, .MyOffset = fractional ? 1.0 : 0.0}, _unit);
	}

	bool BattleCore::LonelyOperator(UnitId _unit, bool _diagonal) const
	{
		const auto origin = RulePosition(Unit(_unit));
		const auto row = static_cast<int>(std::floor(origin.MyY + 0.5)), column = static_cast<int>(std::floor(origin.MyX + 0.5));
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (id == _unit || !ally.MyAlive || ally.MyKind == UnitKind::DEVICE) continue;
			const auto point = RulePosition(ally);
			const auto r = static_cast<int>(std::floor(point.MyY + 0.5)), c = static_cast<int>(std::floor(point.MyX + 0.5));
			if (r < 0 || r >= FieldRows || c < 0 || c >= FieldColumns) continue;
			const auto dr = std::abs(r - row), dc = std::abs(c - column);
			if (dr <= 1 && dc <= 1 && dr + dc > 0 && (_diagonal || dr + dc == 1)) return false;
		}
		return true;
	}

	void BattleCore::OperatorModuleTick(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyOperatorHooksReleased || !unit.MyDefinition.MyOperatorKit) return;
		const auto* rules = unit.MyDefinition.MyOperatorKit;
		if (const auto* texas = std::get_if<Texas2Kit>(rules)) SetOperatorAttribute(_unit, "texas2:module", LonelyOperator(_unit, false), Attribute::ATTACK_PERCENT, texas->MyLonelyAttack);
		else if (const auto* hsguma = std::get_if<HsgumaKit>(rules)) SetOperatorAttribute(_unit, "hsguma:module", !unit.MyBlocking.empty(), Attribute::DEFENSE_PERCENT, hsguma->MyBlockingDefense);
		else if (const auto* blaze = std::get_if<Blaze2Kit>(rules)) BurstSpAura(_unit, "blaze2:module", blaze->MyBurstSp);
		else if (const auto* pasngr = std::get_if<PasngrKit>(rules); pasngr && !unit.MyHidden)
		{
			constexpr std::array<RangeOffset, 4> Neighbors{{{.MyRow = 1}, {.MyRow = -1}, {.MyColumn = 1}, {.MyColumn = -1}}};
			if (!std::ranges::any_of(_MyEnemyIds, [&](UnitId _id) { return Unit(_id).MyAlive && !Unit(_id).MyHidden && OperatorInGrid(_unit, _id, Neighbors); }))
				(void)AddBuff(_unit, {.MyKey = "pasngr:lone", .MyDuration = 0.4, .MyModifiers = std::vector<AttributeChange>{
					{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = pasngr->MyLonelyAttack}, {.MyAttribute = Attribute::SP_RECOVERY_FLAT, .MyValue = pasngr->MyLonelySp}}});
		}
		else if (const auto* qiubai = std::get_if<QiubaiKit>(rules); qiubai && !unit.MyHidden)
		{
			const AttackProfile filter{.MyCanHitFlying = true};
			const auto count = std::ranges::count_if(_MyEnemyIds, [&](UnitId _id) { return TargetableEnemy(Unit(_id), filter) && InRuleRange(_unit, _id); });
			if (std::isgreaterequal(static_cast<double>(count), qiubai->MyCrowdCount)) (void)AddBuff(_unit, {.MyKey = "qiubai:crowd", .MyDuration = 0.4,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = qiubai->MyCrowdSpeed}}});
		}
		else if (const auto* mlynar = std::get_if<MlynarKit>(rules))
		{
			auto& scratch = AcquireAttackScratch();
			FoesInRadius(RulePosition(unit), 1.5, scratch.MyTargets);
			_MyUnits[Index(_unit)].MyMlynarNear = static_cast<unsigned>(scratch.MyTargets.size());
			--_MyAttackDepth;
			if (std::isgreater(mlynar->MyDamageReduction, 0) && std::isgreaterequal(static_cast<double>(unit.MyMlynarNear), mlynar->MyNearCount))
				(void)AddBuff(_unit, {.MyKey = "mlynar:ranger", .MyDuration = 0.3, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::DAMAGE_TAKEN_MULTIPLIER, .MyValue = 1 - mlynar->MyDamageReduction}}});
		}
		else if (const auto* f12yin = std::get_if<F12yinKit>(rules))
		{
			if (std::isgreater(unit.MyHealth / unit.MyStats.MyMaxHealth, 0.5)) (void)AddBuff(_unit, {.MyKey = "f12yin:module", .MyDuration = 0.3,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = f12yin->MyHealthySpeed}}});
		}
		else if (const auto* nymph = std::get_if<NymphKit>(rules)) BurstSpAura(_unit, "nymph:module", nymph->MyBurstSp);
		else if (const auto* aglina = std::get_if<AglinaKit>(rules))
		{
			const AttackProfile filter{.MyCanHitFlying = true};
			if (std::ranges::any_of(_MyEnemyIds, [&](UnitId _id) { return TargetableEnemy(Unit(_id), filter) && InRuleRange(_unit, _id); }))
				(void)AddBuff(_unit, {.MyKey = "aglina:module", .MyDuration = 0.5, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::SP_RECOVERY_FLAT, .MyValue = aglina->MyEnemySp}}});
		}
		else if (const auto* ghost = std::get_if<Ghost2Kit>(rules))
		{
			if (unit.MyProfession.MyDoll)
			{
				if (std::islessgreater(ghost->MyDollAttack, 0)) (void)AddBuff(_unit, {.MyKey = "ghost2:module", .MyDuration = 0.3, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = ghost->MyDollAttack}}});
				if (std::islessgreater(ghost->MyDollHealth, 0)) (void)AddBuff(_unit, {.MyKey = "ghost2:moduleY", .MyDuration = 0.3, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALTH_PERCENT, .MyValue = ghost->MyDollHealth}}});
			}
		}
		else if (const auto* horn = std::get_if<HornKit>(rules))
		{
			if (unit.MyBlocking.empty()) (void)AddBuff(_unit, {.MyKey = "horn:moduleY", .MyDuration = 0.3, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = horn->MyUnblockedSpeed}}});
		}
		else if (const auto* surtr = std::get_if<SurtrKit>(rules))
		{
			if (std::islessgreater(surtr->MyUnblockedSpeed, 0))
			{
				if (unit.MyBlocking.empty()) (void)AddBuff(_unit, {.MyKey = "surtr:module", .MyDuration = 0.3, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = surtr->MyUnblockedSpeed}}});
				else (void)RemoveBuff(_unit, "surtr:module");
			}
			if (std::isgreater(surtr->MyArtsFragile, 0))
			{
				auto& scratch = AcquireAttackScratch(); const RecastScratchGuard guard{.MyDepth = _MyAttackDepth};
				scratch.MyTargets.assign(unit.MyBlocking.begin(), unit.MyBlocking.end());
				for (const auto id : scratch.MyTargets) if (Unit(id).MyAlive) (void)ApplyStatus(id, CombatStatus::ARTS_FRAGILE, {.MyDuration = 0.3, .MySource = _unit, .MyValue = surtr->MyArtsFragile});
			}
		}
		else if (const auto* etlchi = std::get_if<EtlchiKit>(rules)) CrowdAttackSpeed(_unit, "etlchi:module", etlchi->MyCrowdSpeed, etlchi->MyCrowdCount);
		else if (const auto* excu = std::get_if<Excu2Kit>(rules)) CrowdAttackSpeed(_unit, "excu2:module", excu->MyCrowdSpeed, excu->MyCrowdCount);
		else if (const auto* cetsyr = std::get_if<CetsyrKit>(rules))
		{
			const auto count = std::ranges::count_if(_MyAllyIds, [&](UnitId _id)
			{
				const auto& ally = Unit(_id);
				return _id != _unit && ally.MyAlive && (cetsyr->MyOrbitMotes || ally.MyOwner == unit.MyOwner) && ally.MyKind == UnitKind::OPERATOR && BodyInRange(ally, unit.MyBaseTriggerMask, RulePosition(ally));
			});
			if (cetsyr->MyOrbitMotes)
			{
				if (std::isgreaterequal(static_cast<double>(count), cetsyr->MyModuleCount))
					(void)AddBuff(_unit, {.MyKey = "cetsyr:module", .MyDuration = 0.5, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = cetsyr->MyModuleAttack}}});
			}
			else SetOperatorAttribute(_unit, "cetsyr:module", std::isgreaterequal(static_cast<double>(count), cetsyr->MyModuleCount), Attribute::ATTACK_PERCENT, cetsyr->MyModuleAttack);
		}
		else if (const auto* gvial = std::get_if<GvialKit>(rules))
		{
			const auto extra = static_cast<double>(unit.MyBlocking.empty() ? 0 : unit.MyBlocking.size() - 1);
			const std::array mods{AttributeChange{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = gvial->MyAttack + gvial->MyAttackPerBlock * extra},
				AttributeChange{.MyAttribute = Attribute::DEFENSE_PERCENT, .MyValue = gvial->MyDefense + gvial->MyDefensePerBlock * extra}};
			SetOperatorModifiers(_unit, gvial->MyHiddenVariant ? "gvial2:axe" : "gvial:t1", true, mods);
		}
		else if (const auto* billro = std::get_if<BillroKit>(rules)) SetOperatorAttribute(_unit, "billro:t2", unit.MySkill.MyCharges >= 1 && !unit.MySkill.MyActive, Attribute::SP_RECOVERY_FLAT, billro->MySpRecovery);
		else if (const auto* flamtl = std::get_if<FlamtlKit>(rules)) SetOperatorModifiers(_unit, "flamtl:module", !unit.MyBlocking.empty(), flamtl->MyBlockingModifiers);
		else if (const auto* fartth = std::get_if<FartthKit>(rules)) SetOperatorAttribute(_unit, "fartth:focus", std::isgreaterequal(Time() - unit.MyFartthHurtAt, fartth->MyQuietTime - 1e-9), Attribute::ATTACK_PERCENT, fartth->MyAttack);
		else if (const auto* mudrok = std::get_if<MudrokKit>(rules)) SetOperatorModifiers(_unit, "mudrok:module", LonelyOperator(_unit, true), mudrok->MyLonelyModifiers);
	}

	void BattleCore::Texas2Start(UnitId _unit, const Texas2Kit& _kit, SkillReason _reason)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_reason == SkillReason::KILL && unit.MyTexas2Recast) { unit.MyTexas2Recast = false; return; }
		unit.MyTexas2Casting = true;
		Texas2Cast(_unit, _kit);
		unit.MyTexas2Casting = false;
	}

	void BattleCore::Texas2FinishStart(UnitId _unit)
	{
		if (!Unit(_unit).MyTexas2Recast || !Unit(_unit).MyAlive) return;
		EndSkill(_unit, SkillReason::RECAST);
		(void)ActivateSkill(_unit, true, SkillReason::KILL);
	}

	void BattleCore::Texas2Cast(UnitId _unit, const Texas2Kit& _kit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || _kit.MySkill == Texas2SkillKind::DRIZZLE) return;
		auto& scratch = AcquireAttackScratch(); const RecastScratchGuard guard{.MyDepth = _MyAttackDepth};
		OperatorEnemiesInGrid(_unit, _kit.MyRange, scratch.MyTargets, false);
		for (const auto id : scratch.MyTargets)
		{
			const auto hits = _kit.MySkill == Texas2SkillKind::RAIN ? 2 : 1;
			for (int i = 0; i < hits && Unit(id).MyAlive; ++i)
				(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyBurstScale, .MyType = DamageType::ARTS,
					.MyTags = DamageTag::SKILL | DamageTag::BURST, .MyIsSkill = true});
			if (!Unit(id).MyAlive) continue;
			if (_kit.MySkill == Texas2SkillKind::STORM) ApplyResistanceCut(_unit, id, "texas2:resDown", _kit.MyDebuffDuration, _kit.MyResistance);
			else (void)ApplyStatus(id, CombatStatus::STUN, _kit.MyBurstStun, _unit);
		}
		if (_kit.MySkill != Texas2SkillKind::RAIN) return;
		const auto version = ++_MyUnits[Index(_unit)].MyTexas2RainVersion;
		Schedule({.MyAt = Time() + _kit.MyInterval - BattleClock::StepSeconds, .MyKind = ScheduledKind::TEXAS2_RAIN,
			.MySource = _unit, .MyInterval = _kit.MyInterval, .MyVersion = version});
	}

	void BattleCore::Texas2Rain(UnitId _unit)
	{
		const auto& unit = Unit(_unit); const auto& kit = std::get<Texas2Kit>(*unit.MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch(); const RecastScratchGuard guard{.MyDepth = _MyAttackDepth};
		OperatorEnemiesInGrid(_unit, kit.MyRange, scratch.MyTargets);
		const AttackProfile priority{};
		SortOperatorTargets(_unit, scratch.MyTargets, kit.MyTargets, &priority);
		for (const auto id : scratch.MyTargets)
		{
			(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * kit.MyScale, .MyType = DamageType::ARTS,
				.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
			if (Unit(id).MyAlive) (void)ApplyStatus(id, CombatStatus::STUN, kit.MyStun, _unit);
		}
	}

	void BattleCore::Texas2Death(const ContentEvent& _event)
	{
		if (!_event.MyUnit) return;
		auto& unit = _MyUnits[Index(_event.MyUnit)];
		if (unit.MyOperatorHooksReleased || !unit.MyDefinition.MyOperatorKit || !std::holds_alternative<Texas2Kit>(*unit.MyDefinition.MyOperatorKit)) return;
		unit.MyTexas2Killed = unit.MyTexas2Casting = unit.MyTexas2Recast = false;
	}

	void BattleCore::Texas2Skill(UnitId _unit, const Texas2Kit& _kit, const ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY && !unit.MyTexas2Killed)
			(void)AddBuff(_unit, {.MyKey = "texas2:swordplay", .MyModifiers = std::vector<AttributeChange>{
				{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = _kit.MyAttackSpeed}, {.MyAttribute = Attribute::DAMAGE_TAKEN_MULTIPLIER, .MyValue = 1 - _kit.MyDamageReduction}}});
		else if (_event.MyKind == ContentEventKind::BEFORE_KILL && _event.MyTarget && unit.MyAlive && !unit.MyTexas2Killed && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
		{
			unit.MyTexas2Killed = true;
			(void)RemoveBuff(_unit, "texas2:swordplay");
			(void)Heal(_unit, _unit, unit.MyStats.MyMaxHealth * _kit.MyHealRatio, {.MySelf = true});
			if (unit.MyTexas2Casting) { unit.MyTexas2Recast = true; Texas2Cast(_unit, _kit); }
			else { EndSkill(_unit, SkillReason::RECAST); (void)ActivateSkill(_unit, true, SkillReason::KILL); }
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			if (_event.MySkillReason != SkillReason::RECAST || !unit.MyTexas2Recast) ++unit.MyTexas2RainVersion;
		}
		else if (_event.MyKind == ContentEventKind::DAMAGED && _kit.MySkill == Texas2SkillKind::DRIZZLE && unit.MySkill.MyActive &&
			_event.MyDamage.MyIsAttack && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY && Unit(_event.MyTarget).MyAlive)
		{
			(void)ApplyStatus(_event.MyTarget, CombatStatus::SILENCE, _kit.MySilence, _unit);
			(void)AddBuff(_event.MyTarget, {.MyKey = unit.MyTexas2DotKey, .MySource = _unit, .MyDuration = _kit.MyDotDuration + 1e-6,
				.MyRefresh = BuffRefresh::EXTEND, .MyInterval = _kit.MyDotInterval,
				.MyTickEffects = {{.MyKind = BuffEffectKind::DAMAGE, .MyAmount = _kit.MyDotDamage, .MyDamageType = DamageType::ARTS,
					.MyTags = DamageTag::SKILL | DamageTag::DOT, .MyIsSkill = true}}});
		}
	}
}
