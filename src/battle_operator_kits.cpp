#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::InstallOperatorKit(CombatUnit& _unit)
	{
		const auto* kit = _unit.MyDefinition.MyOperatorKit;
		if (!kit) return;
		_MyOperatorBeforeAttack |= std::holds_alternative<VignaKit>(*kit) || std::holds_alternative<IndigoKit>(*kit);
		if (const auto* prove = std::get_if<ProveKit>(kit); prove && prove->MyHunt) _MyOperatorBeforeAttack = true;
		if (std::holds_alternative<VendlaKit>(*kit)) _MyVendlas.push_back({.MyUnit = _unit.MyId});
		if (std::holds_alternative<TexasKit>(*kit)) _MyTexasUnits.push_back(_unit.MyId);
		if (std::holds_alternative<EstellKit>(*kit)) _MyEstells.push_back(_unit.MyId);
		if (std::holds_alternative<UtageKit>(*kit)) _MyUtages.push_back(_unit.MyId);
		if (const auto* tinman = std::get_if<TinmanKit>(kit); tinman && tinman->MyWitherScale > 1) _MyTinmanWither = true;
		if (const auto* podego = std::get_if<PodegoKit>(kit))
		{
			if (podego->MyHealing) _MyPodegos.push_back(_unit.MyId);
			if (podego->MyAuraAttack != 0) Schedule({.MyAt = Time(), .MyKind = ScheduledKind::PODEGO_AURA, .MySource = _unit.MyId, .MyInterval = 0.5});
		}
		if (const auto* udflow = std::get_if<UdflowKit>(kit); udflow && udflow->MyReveal)
			Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::OPERATOR_REVEAL, .MySource = _unit.MyId, .MyInterval = 0.2});
	}

	bool Battle::OperatorBeforeAttack(CombatUnit& _unit, std::vector<UnitId>& _targets)
	{
		if (!_MyOperatorBeforeAttack) return true;
		if (const auto* kit = _unit.MyDefinition.MyOperatorKit)
		{
			if (const auto* vigna = std::get_if<VignaKit>(kit))
			{
				const auto chance = _unit.MySkill.MyActive ? vigna->MySkillProbability : vigna->MyProbability;
				if (chance > 0 && (chance >= 1 || _MyRandom.Next() < chance))
					(void)AddBuff(_unit.MyId, {.MyKey = "vigna:proc", .MyDuration = 0.1,
						.MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, vigna->MyAttack}}});
			}
			else if (const auto* prove = std::get_if<ProveKit>(kit); prove && prove->MyHunt && _unit.MySkill.MyActive)
			{
				const auto huntable = [&](UnitId id) { return Unit(id).MyHealth / Unit(id).MyStats.MyMaxHealth <= 0.8 + 1e-9; };
				if (!std::ranges::all_of(_targets, huntable))
				{
					const auto count = static_cast<unsigned>(_targets.size()); _targets.clear();
					for (const auto id : _MyEnemyIds)
						if (TargetableEnemy(Unit(id), EffectiveAttack(_unit)) && InRuleRange(_unit.MyId, id) && huntable(id)) _targets.push_back(id);
					SortOperatorTargets(_unit.MyId, _targets, count);
				}
			}
			else if (std::holds_alternative<IndigoKit>(*kit) &&
				std::ranges::any_of(_targets, [&](UnitId id) { return Unit(id).MyStatuses.Has(CombatStatus::BIND); }))
			{
				const auto count = static_cast<unsigned>(std::max<std::size_t>(1, _targets.size())); _targets.clear();
				const auto& profile = EffectiveAttack(_unit);
				for (const auto id : _MyEnemyIds)
				{
					const auto& enemy = Unit(id);
					if (!enemy.MyStatuses.Has(CombatStatus::BIND) && TargetableEnemy(enemy, profile) &&
						(enemy.MyBlockedBy == _unit.MyId || InRange(_unit, enemy))) _targets.push_back(id);
				}
				SortOperatorTargets(_unit.MyId, _targets, count, &profile);
			}
		}
		// 原 beforeAttack 总线在处理器执行后过滤目标；没有处理器时保留原强制攻击语义。
		std::erase_if(_targets, [&](UnitId id) { return !Unit(id).MyAlive; });
		return _unit.MyAlive && !_targets.empty();
	}

	void Battle::GrantInsiderAmmo(UnitId _unit, std::uint64_t _deployment)
	{
		const auto& source = Unit(_unit);
		if (!source.MyAlive || source.MyDeploySequence != _deployment) return;
		const auto& kit = std::get<InsiderKit>(*source.MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch();
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (id != _unit && ally.MyKind == UnitKind::OPERATOR && ally.MyOwner == source.MyOwner && ally.MyAlive &&
				ally.MyDefinition.MySkill.MyKind == SkillKind::AMMO && std::ranges::contains(ally.MyDefinition.MyIdentity.MyBonds, "lateranoShip")) scratch.MyTargets.push_back(id);
		}
		const auto target = scratch.MyTargets.empty() ? UnitId{} : scratch.MyTargets[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MyTargets.size()))];
		--_MyAttackDepth;
		if (!target) return;
		const auto found = std::ranges::find_if(_MyInsiderGrants, [&](const auto& g) { return g.MySource == _unit && g.MyTarget == target; });
		const InsiderAmmoGrant grant{_unit, target, _deployment, kit.MyAllyAmmo};
		if (found == _MyInsiderGrants.end()) _MyInsiderGrants.push_back(grant); else *found = grant;
	}

	void Battle::OperatorReveal(UnitId _unit)
	{
		if (!Unit(_unit).MyAlive) return;
		for (std::size_t i = 0; i < _MyEnemyIds.size(); ++i)
		{
			const auto id = _MyEnemyIds[i];
			const auto& enemy = Unit(id);
			if (enemy.MyAlive && !enemy.MyHidden && enemy.MyStatuses.Has(CombatStatus::STEALTH) && InRuleRange(_unit, id))
				(void)ApplyStatus(id, CombatStatus::REVEAL, 0.3, _unit);
		}
	}

	void Battle::NotifyOperatorKits(ContentEvent& _event)
	{
		NotifyVendlas(_event);
		NotifyOperatorObservers(_event);
		if (_event.MyKind == ContentEventKind::BATTLE_START)
		{
			for (const auto id : _MyTexasUnits)
			{
				const auto& unit = Unit(id); const auto& texas = std::get<TexasKit>(*unit.MyDefinition.MyOperatorKit);
				if (texas.MyInitialDp > 0) (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, texas.MyInitialDp);
			}
			return;
		}
		if (_event.MyKind == ContentEventKind::SKILL_START && _event.MyUnit && Unit(_event.MyUnit).MyDefinition.MySkill.MyKind == SkillKind::AMMO)
		{
			double extra = 0;
			std::erase_if(_MyInsiderGrants, [&](const auto& grant)
			{
				if (grant.MyTarget != _event.MyUnit) return false;
				const auto& source = Unit(grant.MySource);
				if (!source.MyAlive || source.MyDeploySequence != grant.MyDeployment) return true;
				extra += grant.MyAmount; return false;
			});
			if (extra > 0) (void)AddSkillAmmo(_event.MyUnit, extra);
		}
		if (_event.MyKind != ContentEventKind::DEPLOY && _event.MyKind != ContentEventKind::SKILL_START && _event.MyKind != ContentEventKind::SKILL_ENDING &&
			_event.MyKind != ContentEventKind::SKILL_TICK && _event.MyKind != ContentEventKind::BEFORE_HEAL && _event.MyKind != ContentEventKind::SP_GAIN &&
			_event.MyKind != ContentEventKind::BEFORE_DAMAGE && _event.MyKind != ContentEventKind::DAMAGED && _event.MyKind != ContentEventKind::ATTACK) return;
		const auto id = _event.MySource ? _event.MySource : _event.MyUnit;
		if (!id) return;
		const auto& unit = Unit(id); const auto* rules = unit.MyDefinition.MyOperatorKit;
		if (!rules) return;
		if (const auto* insider = std::get_if<InsiderKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::DEPLOY && insider->MyAllyAmmo > 0)
				Schedule({.MyAt = Time() + insider->MyDelay, .MyKind = ScheduledKind::INSIDER_AMMO, .MySource = id, .MyVersion = unit.MyDeploySequence});
			if (_event.MyKind == ContentEventKind::SKILL_START && unit.MyAlive && unit.MyDefinition.MySkill.MyKind == SkillKind::AMMO &&
				Time() - unit.MyDeployedAt + 1e-9 >= insider->MyDelay && insider->MySelfAmmo > 0) (void)AddSkillAmmo(id, insider->MySelfAmmo);
		}
		else if (const auto* leizi = std::get_if<LeiziKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyDamage.MyIsAttack && _event.MyTarget &&
				Unit(_event.MyTarget).MySide == UnitSide::ENEMY && !Unit(_event.MyTarget).MyBlockedBy) _event.MyDamage.MyAmount *= leizi->MyUnblockedScale;
		}
		else if (const auto* udflow = std::get_if<UdflowKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyDamage.MyIsAttack && _event.MyTarget && udflow->MyDuration > 0 && udflow->MyDamage > 0)
			{
				const auto& target = Unit(_event.MyTarget);
				if (target.MySide != UnitSide::ENEMY || !target.MyAlive) return;
				const auto amount = std::ranges::contains(target.MyDefinition.MyEnemyTags, "seamonster") ? udflow->MySeaDamage : udflow->MyDamage;
				(void)AddBuff(target.MyId, {.MyKey = "udflow:dot:" + std::to_string(id), .MySource = id, .MyDuration = udflow->MyDuration,
					.MyRefresh = BuffRefresh::EXTEND, .MyInterval = udflow->MyInterval,
					.MyTickEffects = {{.MyAmount = amount, .MyDamageType = DamageType::ARTS, .MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::DOT)}}});
			}
		}
		else if (const auto* vigna = std::get_if<VignaKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::ATTACK) (void)RemoveBuff(id, "vigna:proc");
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyDamage.MyIsAttack && _event.MyTarget)
			{
				const auto& target = Unit(_event.MyTarget);
				if (target.MySide == UnitSide::ENEMY && target.MyHealth / target.MyStats.MyMaxHealth < vigna->MyHealthThreshold) _event.MyDamage.MyAmount *= vigna->MyLowHealthScale;
			}
		}
		else if (const auto* prove = std::get_if<ProveKit>(rules))
		{
			if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MyDamage.MyIsAttack || !_event.MyTarget) return;
			const auto& target = Unit(_event.MyTarget); if (target.MySide != UnitSide::ENEMY) return;
			if (prove->MyHealthDrop > 0 && prove->MyScalePerDrop > 0)
				_event.MyDamage.MyAmount *= 1 + std::clamp(1 - target.MyHealth / target.MyStats.MyMaxHealth, 0.0, 1.0) / prove->MyHealthDrop * prove->MyScalePerDrop;
			if (_event.MyDamage.MyIsSplash || HasTag(_event.MyDamage.MyTags, DamageTag::CHAIN)) return;
			const auto origin = RulePosition(unit); const auto forward = RotateOffset({0, 1}, unit.MyFacing);
			const bool front = BodyOnTile(target, static_cast<int>(std::floor(origin.MyY + 0.5)) + forward.MyRow,
				static_cast<int>(std::floor(origin.MyX + 0.5)) + forward.MyColumn);
			const auto chance = front ? prove->MyFrontProbability : prove->MyProbability;
			if (chance > 0 && (chance >= 1 || _MyRandom.Next() < chance)) _event.MyDamage.MyAmount *= prove->MyCriticalScale;
		}
		else if (const auto* texas = std::get_if<TexasKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::SKILL_START) TexasSkill(id, *texas);
		}
		else if (const auto* caper = std::get_if<CaperKit>(rules))
		{
			if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MyDamage.MyIsAttack || !_event.MyTarget || Unit(_event.MyTarget).MySide != UnitSide::ENEMY) return;
			const auto origin = RulePosition(unit);
			if (BodyTileReach(Unit(_event.MyTarget), static_cast<int>(std::floor(origin.MyY + 0.5)), static_cast<int>(std::floor(origin.MyX + 0.5))) <= 1) _event.MyDamage.MyAmount *= caper->MyNearScale;
			if (!_event.MyDamage.MyIsSplash && caper->MyProbability > 0 && (caper->MyProbability >= 1 || _MyRandom.Next() < caper->MyProbability)) _event.MyDamage.MyAmount *= caper->MyCriticalScale;
		}
		else if (const auto* sunbr = std::get_if<SunbrKit>(rules))
		{
			SunbrSkill(id, *sunbr, _event.MyKind);
			if (_event.MyKind == ContentEventKind::BEFORE_HEAL && unit.MySkill.MyActive && !_event.MyHealOptions.MyRegen && !_event.MyHealOptions.MySelf && _event.MyTarget &&
				Unit(_event.MyTarget).MyHealth / Unit(_event.MyTarget).MyStats.MyMaxHealth < sunbr->MyHealthThreshold) _event.MyAmount *= sunbr->MyHealingScale;
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyDamage.MyIsAttack && !_event.MyDamage.MyIsSplash &&
				!HasTag(_event.MyDamage.MyTags, DamageTag::CHAIN) && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY &&
				sunbr->MyProbability > 0 && (sunbr->MyProbability >= 1 || _MyRandom.Next() < sunbr->MyProbability))
			{
				_event.MyDamage.MyAmount *= sunbr->MyCriticalScale; _event.MyDamage.MySunbrProc = true;
			}
			if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyDamage.MySunbrProc && _event.MyTarget && Unit(_event.MyTarget).MyAlive)
				(void)ApplyStatus(_event.MyTarget, CombatStatus::STUN, sunbr->MyStun, id);
		}
		else if (const auto* podego = std::get_if<PodegoKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::SKILL_START && !podego->MyHealing) PodegoStartZone(id, *podego);
			if (_event.MyKind == ContentEventKind::SP_GAIN && _event.MySpReason == SpReason::TIME && podego->MySpPerSecond > 0)
			{
				const AttackProfile filter{.MyCanHitFlying = true};
				if (std::ranges::any_of(_MyEnemyIds, [&](UnitId enemy) { return TargetableEnemy(Unit(enemy), filter) && InRuleRange(id, enemy); }))
					_event.MyAmount += podego->MySpPerSecond * BattleClock::StepSeconds;
			}
		}
		else if (const auto* utage = std::get_if<UtageKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && utage->MyBreach && unit.MySkill.MyActive &&
				_event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY && _event.MyDamage.MyIsAttack &&
				_event.MyDamage.MyType == DamageType::PHYSICAL) _event.MyDamage.MyType = DamageType::ARTS;
			if (_event.MyKind == ContentEventKind::SKILL_START)
			{
				if (utage->MyRest) ReleaseBlocked(_MyUnits[Index(id)]);
				else if (utage->MyBreach)
				{
					const auto loss = unit.MyHealth * utage->MyHealthLoss;
					if (loss > 0 && unit.MyHealth - loss >= 1) (void)LoseHealth(id, id, loss);
				}
			}
		}
		else if (const auto* indigo = std::get_if<IndigoKit>(rules); indigo && indigo->MyMaze)
		{
			if (_event.MyKind == ContentEventKind::SKILL_START) _MyUnits[Index(id)].MyIndigoAccumulator = 0;
			if (_event.MyKind == ContentEventKind::SKILL_TICK) IndigoTick(id, *indigo, _event.MyDelta);
		}
		else if (const auto* tinman = std::get_if<TinmanKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::SKILL_START && tinman->MyZone) TinmanStartZone(id, *tinman);
			if (_event.MyKind == ContentEventKind::SP_GAIN && _event.MySpReason == SpReason::TIME && unit.MyTinmanZones > 0 && tinman->MySpPerSecond > 0)
				_event.MyAmount += tinman->MySpPerSecond * BattleClock::StepSeconds;
		}
		else if (const auto* pithst = std::get_if<PithstKit>(rules))
		{
			if (!(pithst->MyElementRatio > 0) || _event.MyKind != ContentEventKind::DAMAGED || !_event.MyTarget || !(_event.MyAmount > 0) || _event.MyElement ||
				_event.MyDamage.MyType == DamageType::ELEMENTAL || HasTag(_event.MyDamage.MyTags, DamageTag::BURST)) return;
			const auto& target = Unit(_event.MyTarget);
			if (target.MySide != UnitSide::ENEMY || !(target.MyHealth > 0)) return;
			const bool elite = target.MySpawnTag == EnemySpawnTag::BOSS || target.MyDefinition.MyElite || target.MyDefinition.MyLeader;
			const auto amount = unit.MyStats.MyAttack * (elite ? pithst->MyEliteElementRatio : pithst->MyElementRatio);
			if (!(amount > 0)) return;
			for (const auto element : {Element::NEURAL, Element::BURN, Element::APOPTOSIS})
				if (Unit(target.MyId).MyHealth > 0) (void)DealElement(id, target.MyId, {.MyElement = element, .MyAmount = amount, .MyTags = static_cast<DamageTags>(DamageTag::TALENT)});
		}
	}
}
