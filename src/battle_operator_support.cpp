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

	bool Battle::OperatorCanAttack(const CombatUnit& _unit)
	{
		if (const auto* billro = _unit.MyDefinition.MyOperatorKit ? std::get_if<BillroKit>(_unit.MyDefinition.MyOperatorKit) : nullptr; billro && billro->MySkill == BillroSkillKind::GUARD && _unit.MySkill.MyActive && _unit.MyBillroCharged) return false;
		const auto* rules = _unit.MyDefinition.MyOperatorKit;
		if (const auto* aglina = rules ? std::get_if<AglinaKit>(rules) : nullptr; aglina && aglina->MySkill != AglinaSkillKind::CHARGE && !_unit.MySkill.MyActive) return false;
		if (const auto* swire = rules ? std::get_if<Swire2Kit>(rules) : nullptr; swire && swire->MySkill == Swire2SkillKind::BOMB && Swire2ThrowDue(_unit.MyId, *swire)) return false;
		if (rules && std::holds_alternative<GrabdsKit>(*rules) && _unit.MySkill.MyActive && std::isless(Time(), _unit.MyGrabdsQuietUntil)) return false;
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

	void Battle::OperatorAfterHit(UnitId _source, UnitId _target, const AttackProfile& _profile, WorldPoint _point, double _dealt)
	{
		const auto& source = Unit(_source); const auto* rules = source.MyDefinition.MyOperatorKit;
		if (!rules) return;
		if (const auto* lemuen = std::get_if<LemuenKit>(rules); lemuen && _target && Unit(_target).MyAlive && !source.MySkill.MyActive && std::isgreater(lemuen->MySurvivorSp, 0)) (void)GainSp(_source, lemuen->MySurvivorSp, SpReason::TRAIT);
		if (const auto* gvial = std::get_if<GvialKit>(rules); gvial && gvial->MySkill == GvialSkillKind::HEAL && source.MySkill.MyActive && source.MyAlive && std::isgreater(_dealt, 0))
			(void)Heal(_source, _source, _dealt * gvial->MyLifeSteal, {.MySelf = true});
		if (const auto* ines = std::get_if<InesKit>(rules); ines && ines->MySkill == InesSkillKind::STEALTH && source.MySkill.MyActive) InesHit(_source, _target, *ines);
		if (const auto* qiubai = std::get_if<QiubaiKit>(rules); qiubai && _target && Unit(_target).MyAlive) QiubaiHit(_source, _target, *qiubai);
		if (const auto* f12yin = std::get_if<F12yinKit>(rules); f12yin && source.MyF12yinCritical && _target && Unit(_target).MyAlive && std::islessgreater(f12yin->MyWeaken, 0))
			(void)AddBuff(_target, {.MyKey = "f12yin:punch", .MyDuration = f12yin->MyWeakenDuration,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = f12yin->MyWeaken}}, .MyStatus = CombatStatus::WEAKEN});
		if (_profile.MyOperatorSkillHit)
		{
			if (const auto* cello = std::get_if<CelloKit>(rules); cello && _target && Unit(_target).MyAlive && std::isgreater(Unit(_target).MyHealth, 0) && std::isgreater(cello->MySkillElement, 0))
				(void)DealElement(_source, _target, {.MyElement = Element::APOPTOSIS, .MyAmount = source.MyStats.MyAttack * cello->MySkillElement, .MyTags = static_cast<DamageTags>(DamageTag::SKILL)});
			if (const auto* halo = std::get_if<Halo2Kit>(rules); halo && _target) Halo2Hit(_source, _target, *halo);
			if (const auto* siege = std::get_if<Siege2Kit>(rules); siege && siege->MySkill == Siege2SkillKind::REFORGE) Siege2Burst(_source, *siege);
			if (const auto* pepe = std::get_if<PepeKit>(rules); pepe && _target && Unit(_target).MyAlive && std::isgreater(pepe->MyMainStun, 0)) (void)ApplyStatus(_target, CombatStatus::STUN, pepe->MyMainStun, _source);
			if (const auto* qiubai = std::get_if<QiubaiKit>(rules); qiubai && _target && Unit(_target).MyAlive)
			{
				(void)ApplyStatus(_target, CombatStatus::BIND, qiubai->MySkillBind, _source);
				Schedule({.MyAt = Time() + qiubai->MySkillBind, .MyKind = ScheduledKind::QIUBAI_BURST, .MySource = _source, .MyTarget = _target, .MyVersion = source.MyDeploySequence});
			}
			if (const auto* nymph = std::get_if<NymphKit>(rules); nymph && _target && Unit(_target).MyAlive) (void)ApplyStatus(_target, CombatStatus::FEAR, nymph->MyFear, _source);
			if (const auto* horn = std::get_if<HornKit>(rules); horn && horn->MySkill == HornSkillKind::FLARE)
			{
				auto& unit = _MyUnits[Index(_source)];
				if (unit.MyHornFlare) { unit.MyHornFlare = false; unit.MyFlares.push_back({.MyPoint = _point, .MyUntil = Time() + horn->MyFlareDuration}); }
				return;
			}
			if (const auto* surtr = std::get_if<SurtrKit>(rules); surtr && surtr->MySkill == SurtrSkillKind::BLADE)
			{
				if (_target && Unit(_target).MySide == UnitSide::ENEMY && !Unit(_target).MyAlive) (void)GainSp(_source, SpCost(_source), SpReason::SKILL);
				return;
			}
			if (const auto* titi = std::get_if<TitiKit>(rules))
			{
				if (_target && Unit(_target).MyAlive && !Unit(_target).MyStatuses.Has(CombatStatus::SLEEP) &&
					(titi->MySkill != TitiSkillKind::ERODE || std::isless(_MyRandom.Next(), titi->MySleepChance))) (void)ApplyStatus(_target, CombatStatus::SLEEP, titi->MySleep, _source);
				return;
			}
			if (const auto* ines = std::get_if<InesKit>(rules))
			{
				InesHit(_source, _target, *ines);
				return;
			}
			if (const auto* rmixer = std::get_if<RmixerKit>(rules); rmixer && rmixer->MySkill == RmixerSkillKind::RELOAD)
			{
				ReloadNation(_source, "laterano", rmixer->MyReload, 1.5);
				return;
			}
			if (const auto* vulpis = std::get_if<VulpisKit>(rules); vulpis && vulpis->MySkill == VulpisSkillKind::PUNISH)
			{
				VulpisPunish(_source, _target, *vulpis);
				return;
			}
			if (const auto* archet = std::get_if<ArchetKit>(rules); archet && archet->MySkill == ArchetSkillKind::SCATTER)
			{
				ArchetScatter(_source, _target, _point, *archet);
				return;
			}
			if (const auto* blemsh = std::get_if<BlemshKit>(rules))
			{
				if (blemsh->MySkill == BlemshSkillKind::HEAL) BlemshHeal(_source, *blemsh, false);
				else if (_target && Unit(_target).MyAlive && std::isgreater(blemsh->MyAdditionScale, 0))
					(void)DealDamage(_source, _target, {.MyAmount = source.MyStats.MyAttack * blemsh->MyAdditionScale, .MyType = DamageType::ARTS,
						.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
				return;
			}
		}
		if (!_target || !Unit(_target).MyAlive) return;
		if (const auto* kit = std::get_if<IndigoKit>(rules))
		{
			if (Unit(_target).MySide != UnitSide::ENEMY) return;
			const auto probability = kit->MyProbability * (source.MySkill.MyActive ? kit->MySkillProbabilityScale : 1);
			// 原 rng.chance 即使概率为 1 也消耗随机数；整次命中（含蓄能）只判断一次。
			if (probability > 0 && _MyRandom.Next() < std::min(1.0, probability))
				(void)ApplyStatus(_target, CombatStatus::BIND, kit->MyBindDuration, _source);
		}
		else if (const auto* grabds = std::get_if<GrabdsKit>(rules); grabds && Unit(_target).MySide == UnitSide::ENEMY)
		{
			const bool beast = std::ranges::contains(Unit(_target).MyDefinition.MyEnemyTags, "infection");
			(void)ApplyStatus(_target, CombatStatus::SLUGGISH, grabds->MySluggish + (beast ? grabds->MyBeastSluggish : 0), _source);
		}
		else if (const auto* whitew = std::get_if<WhitewKit>(rules); whitew && std::isgreater(whitew->MyAdditionScale, 0) && Unit(_target).MySide == UnitSide::ENEMY)
			OperatorAddition(_source, _target, whitew->MyAdditionScale, DamageTag::MODULE | DamageTag::ADDITION);
		else if (const auto* ayer = std::get_if<AyerKit>(rules))
			OperatorAddition(_source, _target, ayer->MyAdditionScale, static_cast<DamageTags>(DamageTag::MODULE), false);
		else if (_profile.MyOperatorSkillHit)
		{
			if (const auto* horn = std::get_if<HornKit>(rules); horn && horn->MySkill == HornSkillKind::FLARE)
			{
				auto& unit = _MyUnits[Index(_source)];
				if (unit.MyHornFlare) { unit.MyHornFlare = false; unit.MyFlares.push_back({.MyPoint = _point, .MyUntil = Time() + horn->MyFlareDuration}); }
				return;
			}
			if (const auto* surtr = std::get_if<SurtrKit>(rules); surtr && surtr->MySkill == SurtrSkillKind::BLADE)
			{
				if (_target && Unit(_target).MySide == UnitSide::ENEMY && !Unit(_target).MyAlive) (void)GainSp(_source, SpCost(_source), SpReason::SKILL);
				return;
			}
			if (const auto* titi = std::get_if<TitiKit>(rules))
			{
				if (_target && Unit(_target).MyAlive && !Unit(_target).MyStatuses.Has(CombatStatus::SLEEP) &&
					(titi->MySkill != TitiSkillKind::ERODE || std::isless(_MyRandom.Next(), titi->MySleepChance))) (void)ApplyStatus(_target, CombatStatus::SLEEP, titi->MySleep, _source);
				return;
			}
			if (const auto* wildmn = std::get_if<WildmnKit>(rules); wildmn && Unit(_target).MySide == UnitSide::ENEMY)
			{
				const auto forward = RotateOffset({.MyColumn = 1}, source.MyFacing);
				(void)Push(_target, wildmn->MyForce, {.MyFrom = source.MyPosition,
					.MyDirection = {.MyX = static_cast<double>(forward.MyColumn), .MyY = static_cast<double>(forward.MyRow)}});
			}
			else if (const auto* kjera = std::get_if<KjeraKit>(rules); kjera && std::isgreater(kjera->MyCold, 0) && std::isless(_MyRandom.Next(), kjera->MyColdProbability))
				(void)ApplyStatus(_target, CombatStatus::COLD, kjera->MyCold, _source);
			else if (const auto* shotst = std::get_if<ShotstKit>(rules)) ShotstShred(_source, _target, *shotst);
			else if (const auto* gvial = std::get_if<GvialKit>(rules); gvial && gvial->MyHiddenVariant && Unit(_target).MySide == UnitSide::ENEMY && !Unit(_target).MyBlockedBy)
				(void)PullToFront(_target, _source, gvial->MyForce);
			else if (const auto* mudrok = std::get_if<MudrokKit>(rules); mudrok && Unit(_target).MyAlive && std::isless(_MyRandom.Next(), mudrok->MyProbability))
				(void)ApplyStatus(_target, CombatStatus::STUN, mudrok->MyStun, _source);
			else if (const auto* glady = std::get_if<GladyKit>(rules); glady && glady->MySkill == GladySkillKind::RIP) GladyPull(_source, _target, *glady);
			else if (const auto* mostma = std::get_if<MostmaKit>(rules); mostma && Unit(_target).MySide == UnitSide::ENEMY)
				(void)Push(_target, mostma->MyForce, {.MyFrom = source.MyPosition});
			else if (const auto* mint = std::get_if<MintKit>(rules); mint && Unit(_target).MySide == UnitSide::ENEMY)
				(void)Push(_target, mint->MyForce, {.MyFrom = source.MyPosition, .MyFromFacing = source.MyFacing, .MyInward = true});
			else if (const auto* liskam = std::get_if<LiskamKit>(rules); liskam && std::isless(_MyRandom.Next(), liskam->MyProbability))
				(void)ApplyStatus(_target, CombatStatus::STUN, liskam->MyStun, _source);
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

	void Battle::OperatorEnemiesInGrid(UnitId _source, std::span<const RangeOffset> _grid, std::vector<UnitId>& _targets, bool _sort)
	{
		const auto& source = Unit(_source); const auto point = RulePosition(source);
		const int row = static_cast<int>(std::floor(point.MyY + 0.5)), column = static_cast<int>(std::floor(point.MyX + 0.5));
		const AttackProfile filter{.MyCanHitFlying = true};
		_targets.clear(); _targets.reserve(_MyEnemyIds.size());
		if (!_sort)
		{
			for (const auto id : _MyEnemyIds) if (TargetableEnemy(Unit(id), filter) && OperatorInGrid(_source, id, _grid)) _targets.push_back(id);
			return;
		}
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
