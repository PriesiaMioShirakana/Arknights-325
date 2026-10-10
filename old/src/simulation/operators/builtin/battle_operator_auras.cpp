#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct OperatorAuraScratchGuard
		{
			std::size_t& MyDepth;

			~OperatorAuraScratchGuard() { --MyDepth; }
		};
	}

	bool BattleCore::OperatorInGrid(UnitId _source, UnitId _target, std::span<const RangeOffset> _grid) const
	{
		const auto& source = Unit(_source); const auto origin = RulePosition(source);
		const int row = static_cast<int>(std::floor(origin.MyY + 0.5)), column = static_cast<int>(std::floor(origin.MyX + 0.5));
		for (const auto offset : _grid)
		{
			const auto local = RotateOffset(offset, source.MyFacing); const int r = row + local.MyRow, c = column + local.MyColumn;
			if (r < 0 || r >= FieldRows || c < 0 || c >= FieldColumns) continue;
			const auto& target = Unit(_target);
			if (target.MySide == UnitSide::ENEMY) { if (BodyOnTile(target, r, c)) return true; }
			else
			{
				const auto point = RulePosition(target);
				if (static_cast<int>(std::floor(point.MyY + 0.5)) == r && static_cast<int>(std::floor(point.MyX + 0.5)) == c) return true;
			}
		}
		return false;
	}

	void BattleCore::NotifyOperatorObservers(ContentEvent& _event)
	{
		DiyOperatorObserve(_event);
		StandinObserve(_event);
		Whitw2Honor(_event);
		MlyssObserve(_event);
		if (_event.MyKind == ContentEventKind::BEFORE_KILL && _event.MyTarget) HainiKill(_event.MyTarget);
		if (_event.MyKind == ContentEventKind::ATTACK && _event.MySource) BlemshAttackSp(_event.MySource);
		NotifyWolfCombat(_event);
		DuskKill(_event);
		Svash2Observe(_event);
		NymphObserve(_event);
		MlynarObserve(_event);
		LemuenObserve(_event);
		LumenObserve(_event);
		BlkkgtStatus(_event);
		YuObserve(_event);
		Sbell2Observe(_event);
		Siege2Observe(_event);
		Halo2Observe(_event);
		Agoat2Observe(_event);
		CelloElement(_event);
		Reed2Kill(_event);
		Skadi2Observe(_event);
		if (_event.MyKind != ContentEventKind::SKILL_START) Angel2Observe(_event);
		if (_event.MyKind != ContentEventKind::FATAL) TitiObserve(_event);
		GuardDamage(_event);
		Excu2Dodge(_event);
		Blaze2Observe(_event);
		UlpiaHurt(_event);
		EtlchiObserve(_event);
		GvialObserve(_event);
		if (_event.MyKind == ContentEventKind::DEATH || _event.MyKind == ContentEventKind::BEFORE_KILL) BldskDeath(_event);
		if (_event.MyKind == ContentEventKind::DEATH) Texas2Death(_event);
		if ((_event.MyKind == ContentEventKind::DODGED && _event.MyTarget) || (_event.MyKind == ContentEventKind::DEATH && _event.MyUnit))
		{
			auto& target = _MyUnits[Index(_event.MyKind == ContentEventKind::DEATH ? _event.MyUnit : _event.MyTarget)];
			if (target.MyDefinition.MyOperatorKit && std::holds_alternative<FlamtlKit>(*target.MyDefinition.MyOperatorKit) && !target.MyOperatorHooksReleased)
			{
				target.MyRiposte = _event.MyKind == ContentEventKind::DODGED;
				if (_event.MyKind == ContentEventKind::DEATH) target.MyRiposteNow = false;
			}
		}
		if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyTarget && std::isgreater(_event.MyAmount, 0) && !HasTag(_event.MyDamage.MyTags, DamageTag::HP_LOSS))
		{
			auto& target = _MyUnits[Index(_event.MyTarget)];
			if (target.MyDefinition.MyOperatorKit && std::holds_alternative<FartthKit>(*target.MyDefinition.MyOperatorKit) && !target.MyOperatorHooksReleased) target.MyFartthHurtAt = Time();
		}
		InesObserve(_event);
		AromaObserve(_event);
		if (_event.MyKind == ContentEventKind::DAMAGED) ReckprObserve(_event);
		GladyTide(_event);
		CetsyrProtect(_event);
		if (_event.MyKind == ContentEventKind::DAMAGED) RosesaObserve(_event);
		if (_event.MyKind == ContentEventKind::DEPLOY && _event.MyUnit) PrecisionDeploySp(_event.MyUnit);
		if (_event.MyKind == ContentEventKind::DEATH && _event.MyUnit && Unit(_event.MyUnit).MySide == UnitSide::ENEMY)
			for (const auto id : _MyLockedFunnelUsers)
				if (!Unit(id).MyOperatorHooksReleased) std::erase_if(_MyUnits[Index(id)].MyDroneQueues, [&](const DroneDamageQueue& _queue) { return _queue.MyTarget == _event.MyUnit; });
		Swire2Fatal(_event);
		if (_event.MyKind != ContentEventKind::FATAL) RmixerObserve(_event);
		PhilaeObserve(_event);
		if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyTarget)
		{
			const auto* rules = Unit(_event.MyTarget).MyDefinition.MyOperatorKit;
			if (const auto* kit = rules ? std::get_if<BubbleKit>(rules) : nullptr) BubbleDamaged(_event, *kit);
		}
		if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyTarget && !_event.MyElement && !_event.MyDamage.MyNoSp && std::isgreater(_event.MyAmount, 0))
		{
			const auto* rules = Unit(_event.MyTarget).MyDefinition.MyOperatorKit;
			if (const auto* kit = rules ? std::get_if<LiskamKit>(rules) : nullptr) LiskamDamaged(_event.MyTarget, *kit);
		}
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget)
		{
			const auto* rules = Unit(_event.MyTarget).MyDefinition.MyOperatorKit;
			if (const auto* kit = rules ? std::get_if<UtageKit>(rules) : nullptr) UtageProtect(_event.MyTarget, *kit);
			if (const auto* branch = rules ? std::get_if<BranchKit>(rules) : nullptr;
				branch && !Unit(_event.MyTarget).MyOperatorHooksReleased && _event.MySource && Unit(_event.MySource).MyBlockedBy == _event.MyTarget)
				_event.MyDamage.MyMultiplier *= branch->MyBlockedReduction;
		}
		if (_MyTinmanWither && _event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY &&
			(HasTag(_event.MyDamage.MyTags, DamageTag::DOT) || HasTag(_event.MyDamage.MyTags, DamageTag::NECROSIS) || HasTag(_event.MyDamage.MyTags, DamageTag::APOPTOSIS)))
		{
			const auto& buffs = Unit(_event.MyTarget).MyBuffs;
			const auto found = std::ranges::find(buffs, "tinman:wither", [](const auto& buff) -> const auto& { return buff.MyDefinition.MyKey; });
			if (found != buffs.end() && found->MyDefinition.MyStrength) _event.MyDamage.MyAmount *= found->MyDefinition.MyStrength->MyValue;
		}
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && _event.MyDamage.MyType == DamageType::PHYSICAL)
		{
			const auto& target = Unit(_event.MyTarget); const auto* kit = target.MyDefinition.MyOperatorKit;
			if (const auto* estell = kit ? std::get_if<EstellKit>(kit) : nullptr; estell && target.MyHealth / target.MyStats.MyMaxHealth > estell->MyHealthThreshold)
				_event.MyDamage.MyMultiplier *= 1 - estell->MyPhysicalReduction;
		}
		if (_event.MyKind == ContentEventKind::DEATH && _event.MyRemovalReason == RemovalReason::KILLED && Unit(_event.MyUnit).MySide == UnitSide::ENEMY)
		{
			for (std::size_t i = 0, count = _MyEstells.size(); i < count; ++i)
			{
				const auto& unit = Unit(_MyEstells[i]); if (!unit.MyAlive) continue;
				const auto& kit = std::get<EstellKit>(*unit.MyDefinition.MyOperatorKit); const auto origin = RulePosition(unit);
				const bool near = kit.MyHasRange ? OperatorInGrid(unit.MyId, _event.MyUnit, kit.MyRange) :
					BodyTileReach(Unit(_event.MyUnit), static_cast<int>(std::floor(origin.MyY + 0.5)), static_cast<int>(std::floor(origin.MyX + 0.5))) <= 1;
				if (near) (void)Heal(unit.MyId, unit.MyId, unit.MyStats.MyMaxHealth * kit.MyHealRatio, {.MySelf = true});
			}
		}
		if (_event.MyKind != ContentEventKind::TICK) return;
		for (std::size_t i = 0, count = _MySiege2s.size(); i < count; ++i) Siege2Range(_MySiege2s[i]);
		for (std::size_t i = 0, count = _MySbell2s.size(); i < count; ++i) Sbell2Tick(_MySbell2s[i], _event.MyDelta);
		for (std::size_t i = 0, count = _MyBlkkgts.size(); i < count; ++i) BlkkgtTick(_MyBlkkgts[i]);
		for (std::size_t i = 0, count = _MyLumens.size(); i < count; ++i) LumenTick(_MyLumens[i]);
		for (std::size_t i = 0, count = _MyPepes.size(); i < count; ++i) PepeCleanse(_MyPepes[i]);
		for (std::size_t i = 0, count = _MyLemuens.size(); i < count; ++i) LemuenRange(_MyLemuens[i]);
		for (std::size_t i = 0, count = _MyDemknis.size(); i < count; ++i) DemkniAuto(_MyDemknis[i]);
		for (std::size_t i = 0, count = _MyBlockingDefenders.size(); i < count; ++i)
			UpdateBlockingDefense(_MyBlockingDefenders[i], "trait:snakek_block", std::get<BlockingDefenseKit>(*Unit(_MyBlockingDefenders[i]).MyDefinition.MyOperatorKit).MyDefense);
		for (std::size_t i = 0, count = _MyVulpises.size(); i < count; ++i) VulpisTick(_MyVulpises[i]);
		for (std::size_t i = 0, count = _MyVigils.size(); i < count; ++i) VigilTick(_MyVigils[i]);
		for (std::size_t i = 0, count = _MySnhunts.size(); i < count; ++i) SnhuntTick(_MySnhunts[i]);
		for (std::size_t i = 0, count = _MySwire2s.size(); i < count; ++i) Swire2Tick(_MySwire2s[i]);
		for (std::size_t i = 0, count = _MySlchans.size(); i < count; ++i) SlchanTick(_MySlchans[i]);
		for (std::size_t i = 0, count = _MyBubbles.size(); i < count; ++i) BubbleTick(_MyBubbles[i]);
		for (std::size_t i = 0, count = _MyRockrs.size(); i < count; ++i) RockrTick(_MyRockrs[i]);
		for (std::size_t i = 0, count = _MyKazemas.size(); i < count; ++i) KazemaTick(_MyKazemas[i]);
		for (std::size_t i = 0, count = _MyAkkords.size(); i < count; ++i) AkkordTick(_MyAkkords[i]);
		for (std::size_t i = 0, count = _MySilents.size(); i < count; ++i)
		{
			const auto& unit = Unit(_MySilents[i]);
			if (!unit.MyAlive || unit.MyOperatorHooksReleased) continue;
			const auto& kit = std::get<SilentKit>(*unit.MyDefinition.MyOperatorKit);
			if (SkillSummonStock(unit.MyId, kit.MyToken) < kit.MyStockCap) continue;
			StatusFlags flags; flags.set(static_cast<std::size_t>(CombatStatus::NO_SP));
			(void)AddBuff(unit.MyId, {.MyKey = "silent:stockFull", .MyDuration = 2 * BattleClock::StepSeconds, .MyRefresh = BuffRefresh::EXTEND, .MyFlags = flags});
		}
		for (std::size_t i = 0, count = _MyUtages.size(); i < count; ++i) UtageTick(_MyUtages[i]);
		for (std::size_t i = 0, count = _MyPodegos.size(); i < count; ++i)
		{
			const auto& unit = Unit(_MyPodegos[i]);
			if (!CanAutoSkill(unit) || unit.MySkill.MyActive) continue;
			for (const auto id : _MyAllyIds)
			{
				const auto& ally = Unit(id);
				if (!ally.MyAlive || ally.MyHidden || ally.MyKind == UnitKind::DEVICE || !(ally.MyHealth < ally.MyStats.MyMaxHealth - 1e-6) ||
					(id != unit.MyId && (ally.MyStatuses.Has(CombatStatus::NO_HEAL) || ally.MyDefinition.MyAttack.MyNoHeal))) continue;
				const auto point = RulePosition(ally); const int r = static_cast<int>(std::floor(point.MyY + 0.5)), c = static_cast<int>(std::floor(point.MyX + 0.5));
				if (r < 0 || r >= FieldRows || c < 0 || c >= FieldColumns || !unit.MyBaseTriggerMask.test(static_cast<std::size_t>(FieldGrid::Key(r, c)))) continue;
				(void)ActivateSkill(unit.MyId, false, SkillReason::TRIGGER); break;
			}
		}
	}

	void BattleCore::UtageProtect(UnitId _unit, const UtageKit& _kit)
	{
		const auto& unit = Unit(_unit);
		if (unit.MyAlive && _kit.MyProtection > 0 && unit.MyHealth / unit.MyStats.MyMaxHealth < _kit.MyProtectThreshold)
			(void)ApplyStrongest(_unit, "protect", 1.5 * BattleClock::StepSeconds,
				{.MyValue = _kit.MyProtection, .MyAttribute = Attribute::PHYSICAL_TAKEN_MULTIPLIER, .MyScale = -1, .MyOffset = 1,
					.MySecondAttribute = Attribute::ARTS_TAKEN_MULTIPLIER}, _unit);
	}

	void BattleCore::UtageTick(UnitId _unit)
	{
		const auto& unit = Unit(_unit); if (!unit.MyAlive) return;
		const auto& kit = std::get<UtageKit>(*unit.MyDefinition.MyOperatorKit);
		if (kit.MyMaxAttackSpeed > 0 && kit.MyMinHealthRatio < 1)
		{
			const auto value = kit.MyMaxAttackSpeed * std::clamp((1 - unit.MyHealth / unit.MyStats.MyMaxHealth) / (1 - kit.MyMinHealthRatio), 0.0, 1.0);
			const auto found = std::ranges::find(unit.MyBuffs, "utage:serious", [](const auto& buff) -> const auto& { return buff.MyDefinition.MyKey; });
			const bool existing = found != unit.MyBuffs.end();
			const auto previous = existing && found->MyDefinition.MyStrength ? found->MyDefinition.MyStrength->MyValue : 0;
			if (!existing || std::abs(previous - value) >= 0.5)
			{
				if (value < 0.5) { if (existing) (void)RemoveBuff(_unit, "utage:serious"); }
				else (void)AddBuff(_unit, {.MyKey = "utage:serious", .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_SPEED, value}},
					.MyStrength = BuffStrength{.MyValue = value, .MyAttribute = Attribute::ATTACK_SPEED}});
			}
		}
		UtageProtect(_unit, kit);
	}

	std::size_t BattleCore::InstallOperatorAura(UnitId _unit, const OperatorAuraDefinition& _definition)
	{
		auto key = std::string(_definition.MyKey);
		if (_definition.MyStacking == OperatorAuraStacking::PER_SOURCE) key += ":" + std::to_string(_unit);
		const auto handle = _MyOperatorAuras.size();
		_MyOperatorAuras.emplace_back(OperatorAuraRuntime{.MySource = _unit, .MyDefinition = _definition, .MyBuffKey = std::move(key)});
		if (_definition.MyDropOutside) _MyOperatorAuras.back().MyCurrent.reserve(_definition.MyEnemies ? _MyEnemyIds.size() : _MyAllyIds.size());
		if (std::isgreater(_definition.MyInterval, 0)) Schedule({.MyAt = Time() + _definition.MyInitialDelay, .MyKind = ScheduledKind::OPERATOR_AURA,
			.MySource = _unit, .MyHandle = handle, .MyInterval = _definition.MyInterval});
		return handle;
	}

	void BattleCore::RefreshOperatorAura(std::size_t _handle)
	{
		auto& aura = _MyOperatorAuras[_handle];
		const auto& source = Unit(aura.MySource);
		const auto& definition = aura.MyDefinition;
		if (source.MyOperatorHooksReleased || (!source.MyAlive && !definition.MyDropOutside && !definition.MyCarriedSource)) return;
		const bool active = (source.MyAlive || definition.MyCarriedSource) && (!definition.MySkillInactive || !source.MySkill.MyActive) && (!definition.MySkillActive || source.MySkill.MyActive);
		auto value = (definition.MyValueFrom == OperatorAuraValue::SOURCE_ATTACK ? source.MyStats.MyAttack * definition.MyValue : definition.MyValue) *
			(source.MySkill.MyActive ? definition.MySkillScale : 1) * aura.MyScale + aura.MyOffset;
		if (definition.MyMinimum) value = std::max(value, *definition.MyMinimum);
		const auto range = source.MySkill.MyActive && !definition.MySkillRange.empty() ? definition.MySkillRange : definition.MyRange;
		auto& scratch = AcquireAttackScratch(); const OperatorAuraScratchGuard guard{.MyDepth = _MyAttackDepth};
		if (active) EffectExecutor::Select(_MyView, {.MySource = source.MyId, .MyOwner = source.MyOwner},
			{.MyKind = definition.MyEnemies ? SelectorKind::ENEMIES : definition.MyOwnerOnly ? SelectorKind::OWN_UNITS : SelectorKind::ALLIES,
				.MyIncludeHidden = !(definition.MyDropOutside || definition.MySkipHidden), .MyTargetable = false}, scratch.MySelections);
		if (!active) scratch.MySelections.clear();
		for (const auto& selected : scratch.MySelections)
		{
			const auto id = selected.MyUnit;
			const auto& ally = Unit(id);
			if (!ally.MyAlive || ally.MyKind == UnitKind::DEVICE || (!definition.MyEnemies && id != source.MyId && !definition.MyIgnoreIsolation && ally.MyStatuses.Has(CombatStatus::ISOLATED))) continue;
			if (definition.MyOwnerOnly && ally.MyOwner != source.MyOwner) continue;
			if (!definition.MyNation.empty() && ally.MyDefinition.MyIdentity.MyNationId != definition.MyNation) continue;
			if (definition.MySkipSelf && id == source.MyId) continue;
			if ((!definition.MyCharacters.empty() || !definition.MyGroup.empty()) && !std::ranges::contains(definition.MyCharacters, ally.MyDefinition.MyIdentity.MyCharacterId) &&
				(definition.MyGroup.empty() || ally.MyDefinition.MyIdentity.MyGroupId != definition.MyGroup)) continue;
			if (!definition.MyRequiredBuff.empty() && !std::ranges::any_of(ally.MyBuffs, [&](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == definition.MyRequiredBuff; })) continue;
			if (definition.MyProfession && (ally.MyKind != UnitKind::OPERATOR || ally.MyDefinition.MyOperatorProfession != *definition.MyProfession)) continue;
			if (definition.MyCostLimit && !std::islessequal(ally.MyDefinition.MyOriginalDeploymentCost.value_or(ally.MyDefinition.MyStats.MyDeploymentCost), *definition.MyCostLimit)) continue;
			if ((definition.MyDropOutside || definition.MySkipHidden) && ally.MyHidden) continue;
			if (definition.MyHealthRatioBelow && !std::isless(ally.MyHealth / ally.MyStats.MyMaxHealth, *definition.MyHealthRatioBelow)) continue;
			if (definition.MyOperatorsOnly && ally.MyKind != UnitKind::OPERATOR) continue;
			if (definition.MyMeleeOnly && !ally.MyDefinition.MyIdentity.MyMeleePosition) continue;
			if (definition.MyNormalEnemies && (ally.MyDefinition.MyElite || ally.MyDefinition.MyLeader || ally.MySpawnTag == EnemySpawnTag::BOSS)) continue;
			if (definition.MyAttackRange && !InRuleRange(source.MyId, id)) continue;
			if (!range.empty() && !OperatorInGrid(source.MyId, id, range)) continue;
			scratch.MyTargets.push_back(id);
		}
		if (definition.MyMinimumPartners && static_cast<std::size_t>(std::ranges::count_if(scratch.MyTargets,
			[&](UnitId _id) { return _id != source.MyId; })) < definition.MyMinimumPartners) scratch.MyTargets.clear();
		for (const auto id : scratch.MyTargets)
		{
			const auto& ally = Unit(id);
			const auto strengthValue = definition.MyStrengthValue.value_or(value);
			std::optional<BuffStrength> strength;
			if (definition.MyStacking == OperatorAuraStacking::STRONGEST || definition.MyStacking == OperatorAuraStacking::STRONGEST_LIVING || definition.MyStacking == OperatorAuraStacking::HIGHEST_PRESENT)
			{
				const auto found = std::ranges::find(ally.MyBuffs, aura.MyBuffKey, [](const auto& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
				if (definition.MyStacking == OperatorAuraStacking::HIGHEST_PRESENT && found != ally.MyBuffs.end() && found->MyDefinition.MyModifiers)
				{
					const auto higher = std::ranges::find(*found->MyDefinition.MyModifiers, Attribute::SP_RECOVERY_FLAT, &AttributeChange::MyAttribute);
					if (higher != found->MyDefinition.MyModifiers->end() && std::isgreater(higher->MyValue, value + 1e-9)) continue;
				}
				if (definition.MyStacking != OperatorAuraStacking::HIGHEST_PRESENT && found != ally.MyBuffs.end() && found->MyDefinition.MySource != source.MyId && found->MyDefinition.MyStrength &&
					std::isgreater(found->MyDefinition.MyStrength->MyValue, strengthValue) &&
					(!definition.MyStrengthRequiresLivingSource || (found->MyDefinition.MySource && Unit(found->MyDefinition.MySource).MyAlive)) && (definition.MyStacking == OperatorAuraStacking::STRONGEST_LIVING
						? found->MyDefinition.MySource && Unit(found->MyDefinition.MySource).MyAlive : std::isgreater(found->MyRemaining, 0.05))) continue;
				strength = BuffStrength{.MyValue = strengthValue, .MyAttribute = definition.MyAttribute};
			}
			std::vector<AttributeChange> modifiers;
			modifiers.reserve(1 + definition.MyModifiers.size());
			modifiers.push_back({.MyAttribute = definition.MyAttribute, .MyValue = value + (ally.MyGround ? definition.MyGroundExtra.value_or(0) : 0)});
			modifiers.insert(modifiers.end(), definition.MyModifiers.begin(), definition.MyModifiers.end());
			(void)AddBuff(id, {.MyKey = aura.MyBuffKey, .MySource = definition.MyNoSource ? 0 : source.MyId, .MyDuration = definition.MyDuration,
				.MyRefresh = definition.MyDropOutside ? BuffRefresh::EXTEND : definition.MyRefresh,
				.MyModifiers = std::move(modifiers), .MyStrength = strength, .MyStatus = definition.MyStatus});
		}
		if (definition.MyDropOutside)
		{
			scratch.MySeen.assign(aura.MyCurrent.begin(), aura.MyCurrent.end());
			for (const auto id : scratch.MySeen)
			{
				if (std::ranges::contains(scratch.MyTargets, id)) continue;
				const auto& buffs = Unit(id).MyBuffs;
				const auto found = std::ranges::find(buffs, aura.MyBuffKey, [](const auto& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
				if (found != buffs.end() && found->MyDefinition.MySource == source.MyId) (void)RemoveBuff(id, found->MyId);
			}
			aura.MyCurrent.assign(scratch.MyTargets.begin(), scratch.MyTargets.end());
		}
	}

	void BattleCore::OperatorEnemyTimeSp(ContentEvent& _event, double _perSecond)
	{
		if (_event.MyKind != ContentEventKind::SP_GAIN || _event.MySpReason != SpReason::TIME || !std::isgreater(_perSecond, 0)) return;
		const auto source = _event.MySource ? _event.MySource : _event.MyUnit;
		const AttackProfile filter{.MyCanHitFlying = true};
		if (std::ranges::any_of(_MyEnemyIds, [&](UnitId _enemy) { return TargetableEnemy(Unit(_enemy), filter) && InRuleRange(source, _enemy); }))
			_event.MyAmount += _perSecond * BattleClock::StepSeconds;
	}

	void BattleCore::PodegoStartZone(UnitId _unit, const PodegoKit& _kit)
	{
		const auto& source = Unit(_unit);
		auto& scratch = AcquireAttackScratch(); const OperatorAuraScratchGuard guard{_MyAttackDepth};
		GenericEnemies(_unit, scratch.MyTargets, 1);
		const auto origin = RulePosition(source); const auto forward = RotateOffset({0, 1}, source.MyFacing);
		const auto point = scratch.MyTargets.empty() ? WorldPoint{origin.MyX + forward.MyColumn, origin.MyY + forward.MyRow} : Unit(scratch.MyTargets.front()).MyPosition;
		// 位置和施放时攻击力随定时动作保存，来源退场不取消区域，也不重新取攻击力。
		Schedule({.MyAt = Time(), .MyKind = ScheduledKind::PODEGO_ZONE, .MySource = _unit, .MyInterval = 1, .MyPoint = point,
			.MyAmount = source.MyStats.MyAttack * _kit.MyZoneScale, .MyRemaining = static_cast<unsigned>(std::max(1.0, std::floor(_kit.MyZoneDuration + 0.5)))});
	}

	void BattleCore::PodegoZonePulse(UnitId _unit, WorldPoint _point, double _damage)
	{
		// 原区域在一次脉冲开始时采集敌人；回调新生成的敌人从下一次脉冲参与。
		auto& scratch = AcquireAttackScratch(); const OperatorAuraScratchGuard guard{_MyAttackDepth};
		const AttackProfile filter{.MyCanHitFlying = true, .MyHitSleep = true};
		for (const auto id : _MyEnemyIds)
			if (TargetableEnemy(Unit(id), filter) && BodyDistance(Unit(id), _point) <= 0.9 + 1e-9) scratch.MyTargets.push_back(id);
		for (const auto id : scratch.MyTargets)
		{
			(void)ApplyStatus(id, CombatStatus::SLUGGISH, 1.05, _unit); (void)ApplyStatus(id, CombatStatus::SILENCE, 1.05, _unit);
			(void)DealDamage(_unit, id, {.MyAmount = _damage, .MyType = DamageType::ARTS, .MyCanDodge = false, .MyTags = DamageTag::DOT | DamageTag::ZONE, .MyIsSkill = true});
		}
	}
}
