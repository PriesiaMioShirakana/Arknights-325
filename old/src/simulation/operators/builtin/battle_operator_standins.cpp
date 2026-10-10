#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		constexpr std::array<RangeOffset, 5> Cross{{{.MyRow = 0}, {.MyRow = 1}, {.MyRow = -1}, {.MyColumn = 1}, {.MyColumn = -1}}};
		constexpr std::array<RangeOffset, 4> Neighbors{{{.MyRow = 1}, {.MyRow = -1}, {.MyColumn = 1}, {.MyColumn = -1}}};

		const StandinKit* StandinRules(const CombatUnit& _unit)
		{
			return _unit.MyDefinition.MyOperatorKit ? std::get_if<StandinKit>(_unit.MyDefinition.MyOperatorKit) : nullptr;
		}

		bool StandinUp(const CombatUnit& _unit)
		{
			return _unit.MyAlive && !_unit.MyHidden && !_unit.MyOperatorHooksReleased;
		}

		struct StandinScratchGuard
		{
			std::size_t& MyDepth;

			~StandinScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::InstallStandin(UnitId _unit, const StandinKit& _kit)
	{
		_MyStandins.push_back(_unit);
		auto& unit = _MyUnits[Index(_unit)];
		unit.MyStandinMarks.reserve(_MyEnemyIds.size());
		unit.MyStandinPullLanded.reserve(_MyEnemyIds.size());
		if (_kit.MyKind == StandinKind::RAIDIAN) _MyRaidians.push_back(_unit);
		if (_kit.MyKind == StandinKind::MECHANIST)
			Schedule({.MyAt = Time() + 0.1, .MyKind = ScheduledKind::STANDIN_PULSE, .MySource = _unit, .MyInterval = 0.1});
		if (_kit.MyKind == StandinKind::PITH || _kit.MyKind == StandinKind::RAIDIAN)
		{
			StandinAura(_unit, _kit, true);
			const auto interval = _kit.MyKind == StandinKind::PITH ? 0.5 : 0.25;
			Schedule({.MyAt = Time() + interval, .MyKind = ScheduledKind::STANDIN_PULSE, .MySource = _unit, .MyInterval = interval});
			if (_kit.MyKind == StandinKind::PITH && _kit.MySkill == 2)
				Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::STANDIN_PULSE, .MySource = _unit, .MyInterval = 0.25});
		}
	}

	void BattleCore::StandinConditions(UnitId _unit, const StandinKit& _kit, bool _periodic)
	{
		const auto& unit = Unit(_unit);
		for (const auto& rule : _kit.MyConditions)
		{
			if (rule.MyPeriodic != _periodic) continue;
			bool wanted = false;
			if (StandinUp(unit)) switch (rule.MyCondition)
			{
			case OperatorBuffCondition::DEPLOY_ELAPSED:
				wanted = std::isgreaterequal(Time() - unit.MyDeployedAt, rule.MyThreshold - 1e-9); break;
			case OperatorBuffCondition::TARGETABLE_RANGE:
				wanted = static_cast<std::size_t>(std::ranges::count_if(_MyEnemyIds, [&](UnitId _id) { return TargetableEnemy(Unit(_id), EffectiveAttack(unit)) && InRuleRange(_unit, _id); })) >= rule.MyCount; break;
			case OperatorBuffCondition::EXACT_CROSS:
				wanted = static_cast<std::size_t>(std::ranges::count_if(_MyEnemyIds, [&](UnitId _id) { return Unit(_id).MyAlive && !Unit(_id).MyHidden && OperatorInGrid(_unit, _id, Cross); })) == rule.MyCount; break;
			case OperatorBuffCondition::LONELY: wanted = LonelyOperator(_unit, false); break;
			case OperatorBuffCondition::BLOCKING: wanted = !unit.MyBlocking.empty(); break;
			case OperatorBuffCondition::HEALTH_BELOW:
				wanted = std::isless(unit.MyHealth / unit.MyStats.MyMaxHealth, rule.MyThreshold - 1e-9); break;
			}
			SetOperatorModifiers(_unit, rule.MyKey, wanted, rule.MyModifiers);
		}
	}

	void BattleCore::StandinFeedback(UnitId _unit, const StandinKit& _kit)
	{
		const auto& unit = Unit(_unit);
		if (!StandinUp(unit) || !std::islessgreater(_kit.MyFeedbackSpeed, 0)) return;
		const bool enhanced = unit.MySkill.MyActive && _kit.MySkill == 2;
		for (std::size_t i = 0; i < unit.MyBlocking.size(); ++i)
		{
			const auto id = unit.MyBlocking[i]; const auto& enemy = Unit(id);
			if (!enemy.MyAlive || enemy.MyBlockedBy != _unit) continue;
			const auto has = [&](std::string_view _key) { return std::ranges::any_of(enemy.MyBuffs, [&](const auto& _buff) { return _buff.MyDefinition.MyKey == _key; }); };
			if (has("acfend:feedback2") || (!enhanced && has("acfend:feedback"))) continue;
			if (enhanced) (void)RemoveBuff(id, "acfend:feedback");
			(void)AddBuff(id, {.MyKey = enhanced ? "acfend:feedback2" : "acfend:feedback", .MySource = _unit,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = _kit.MyFeedbackSpeed * (enhanced ? _kit.MyFeedbackScale : 1)}},
				.MyInterval = 0.1, .MyBuiltin = BuiltinBuff::MECHANIST_FEEDBACK, .MyNotifyTick = true});
		}
	}

	void BattleCore::StandinAura(UnitId _unit, const StandinKit& _kit, bool _talent)
	{
		const auto& source = Unit(_unit);
		if (!StandinUp(source) || (!_talent && !source.MySkill.MyActive)) return;
		const bool pith = _kit.MyKind == StandinKind::PITH;
		const bool touch = _kit.MyKind == StandinKind::TOUCH;
		const auto attribute = _kit.MyKind == StandinKind::RAIDIAN ? Attribute::ATTACK_SPEED : Attribute::ATTACK_PERCENT;
		const auto value = _talent ? _kit.MyAuraValue * (_kit.MyKind == StandinKind::RAIDIAN && _kit.MySkill == 3 && source.MySkill.MyActive ? _kit.MySkillAuraScale : 1) : _kit.MySkillAuraValue;
		const std::string_view key = touch ? "acmedc:apocalypse" : pith ? (_talent ? "talent:accast" : "accast:s2") : "acsupo:t1";
		if (std::islessgreater(value, 0)) for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (!ally.MyAlive || ally.MyHidden || ally.MyKind != UnitKind::OPERATOR || (touch && ally.MyOwner != source.MyOwner)) continue;
			if (pith && ally.MyDefinition.MyOperatorProfession != OperatorProfession::CASTER) continue;
			if (touch && ally.MyDefinition.MyOperatorProfession != OperatorProfession::MEDIC) continue;
			if (_talent && !(pith && id == _unit) && !OperatorInGrid(_unit, id, Neighbors)) continue;
			const auto existing = std::ranges::find(ally.MyBuffs, key, [](const auto& _buff) { return std::string_view(_buff.MyDefinition.MyKey); });
			if (existing != ally.MyBuffs.end())
			{
				const auto strength = existing->MyDefinition.MyStrength ? existing->MyDefinition.MyStrength->MyValue : 0;
				if (touch && (existing->MyDefinition.MySource == _unit || std::isgreaterequal(strength, value))) continue;
				if (!touch && existing->MyDefinition.MySource != _unit && std::isgreater(strength, value) && std::isgreater(existing->MyRemaining, 0.05)) continue;
			}
			(void)AddBuff(id, {.MyKey = std::string(key), .MySource = _unit,
				.MyDuration = touch ? std::numeric_limits<double>::infinity() : pith && _talent ? 0.6 : 0.4,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = attribute, .MyValue = value}},
				.MyStrength = BuffStrength{.MyValue = value, .MyAttribute = attribute}});
		}
		if (_kit.MyKind == StandinKind::RAIDIAN && !_talent && _kit.MySkill == 3)
		{
			auto& scratch = AcquireAttackScratch(); const StandinScratchGuard guard{.MyDepth = _MyAttackDepth};
			GenericEnemies(_unit, scratch.MyTargets, 0, false);
			for (const auto id : scratch.MyTargets)
			{
				if (std::isgreater(_kit.MyFragile, 0)) (void)ApplyStatus(id, CombatStatus::FRAGILE, {.MyDuration = 0.4, .MySource = _unit, .MyValue = _kit.MyFragile});
				if (Unit(id).MyAlive && std::isgreater(_kit.MyWeakenMultiplier, 0) && std::isless(_kit.MyWeakenMultiplier, 1))
					(void)ApplyStatus(id, CombatStatus::WEAKEN, {.MyDuration = 0.4, .MySource = _unit, .MyValue = 1 - _kit.MyWeakenMultiplier, .MyStackAs = _kit.MyWeakenMultiplier});
			}
		}
	}

	void BattleCore::StandinPulse(UnitId _unit, double _interval)
	{
		const auto& unit = Unit(_unit); const auto& kit = *StandinRules(unit);
		if (unit.MyOperatorHooksReleased) return;
		if (kit.MyKind == StandinKind::MECHANIST) { StandinConditions(_unit, kit, true); StandinFeedback(_unit, kit); }
		else
		{
			if (kit.MyKind != StandinKind::PITH || std::isgreater(_interval, 0.3)) StandinAura(_unit, kit, true);
			if ((kit.MyKind == StandinKind::PITH && kit.MySkill == 2 && std::isless(_interval, 0.3)) || kit.MyKind == StandinKind::RAIDIAN) StandinAura(_unit, kit, false);
		}
	}

	void BattleCore::StandinBurst(UnitId _unit, const StandinKit& _kit, bool _pull, bool _slash)
	{
		const auto& source = Unit(_unit);
		auto& scratch = AcquireAttackScratch(); const StandinScratchGuard guard{.MyDepth = _MyAttackDepth};
		const bool stress = _kit.MyKind == StandinKind::MECHANIST;
		const auto& profile = source.MyDefinition.MyAttack;
		const AttackProfile filter{.MyCanHitFlying = _slash || stress || (!_pull && profile.MyCanHitFlying), .MyGroundOnly = _pull};
		if (stress) scratch.MyTargets.assign(source.MyBlocking.begin(), source.MyBlocking.end());
		else if (!_kit.MyBurstRange.empty())
		{
			for (const auto id : _MyEnemyIds) if (TargetableEnemy(Unit(id), filter) && OperatorInGrid(_unit, id, _kit.MyBurstRange)) scratch.MyTargets.push_back(id);
		}
		else
		{
			GenericEnemies(_unit, scratch.MyTargets, 0, false);
			std::erase_if(scratch.MyTargets, [&](UnitId _id) { return !TargetableEnemy(Unit(_id), profile); });
			if (_kit.MyKind == StandinKind::TULIP)
			{
				for (const auto id : source.MyBlocking) if (TargetableEnemy(Unit(id), profile) && !std::ranges::contains(scratch.MyTargets, id)) scratch.MyTargets.push_back(id);
				SortOperatorTargets(_unit, scratch.MyTargets, _kit.MyDpCount, &profile);
			}
		}
		const auto type = stress || _kit.MyKind == StandinKind::PITH ? DamageType::ARTS : DamageType::PHYSICAL;
		for (const auto id : scratch.MyTargets)
			if (Unit(id).MyAlive && !Finished()) (void)DealDamage(_unit, id, {.MyAmount = source.MyStats.MyAttack * _kit.MyBurstScale, .MyType = type,
				.MyDefenseIgnorePercent = _slash ? _kit.MyDefenseIgnore : 0, .MyTags = DamageTag::SKILL | (_slash ? DamageTag::SLASH : DamageTag::BURST), .MyIsSkill = true});
		if (!_pull) return;
		// A separate zero-damage hit must land before a pull is eligible. Applying it after
		// all hits preserves RNG order and avoids treating a dodge as a successful zero hit.
		auto& landed = _MyUnits[Index(_unit)].MyStandinPullLanded; landed.clear();
		for (const auto id : scratch.MyTargets) if (Unit(id).MyAlive && !Unit(id).MyBlockedBy)
			(void)DealDamage(_unit, id, {.MyType = DamageType::PHYSICAL, .MyTags = DamageTag::SKILL | DamageTag::MISERY_PULL, .MyIsSkill = true});
		for (const auto id : scratch.MyTargets) if (Unit(id).MyAlive && std::ranges::contains(landed, id))
		{
			(void)PullToFront(id, _unit, _kit.MyPullForce);
			if (Unit(id).MyAlive) (void)ApplyStatus(id, CombatStatus::SLUGGISH, _kit.MySluggish, _unit);
		}
		landed.clear();
	}

	void BattleCore::StandinBeforeAttack(CombatUnit& _unit, std::span<const UnitId> _targets)
	{
		const auto* kit = StandinRules(_unit);
		if (!kit || kit->MyKind != StandinKind::SHARP || kit->MySkill != 3 || !_unit.MySkill.MyActive || !std::isgreater(kit->MyStackAttack, 0)) return;
		const auto found = std::ranges::find_if(_targets, [&](UnitId _id) { return Unit(_id).MyAlive; });
		if (found == _targets.end()) return;
		if (_unit.MyStandinStackTarget != *found) { (void)RemoveBuff(_unit.MyId, "acguad:s3stack"); _unit.MyStandinStackTarget = *found; }
		(void)AddBuff(_unit.MyId, {.MyKey = "acguad:s3stack", .MyMaxStacks = kit->MyMaxStacks, .MyRefresh = BuffRefresh::STACK,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit->MyStackAttack}}});
	}

	void BattleCore::StandinEachHit(UnitId _unit, UnitId _target, const AttackProfile& _profile, bool _main)
	{
		auto& source = _MyUnits[Index(_unit)]; const auto* kit = StandinRules(source);
		if (!kit || kit->MyKind != StandinKind::STORMEYE || !_main) return;
		const auto found = std::ranges::find(source.MyStandinMarks, _target, &StandinAttackMark::MyTarget);
		if (found == source.MyStandinMarks.end() || found->MyAttack != _profile.MyAttackId || !found->MyExtra) return;
		found->MyExtra = false;
		if (Unit(_target).MyAlive) (void)DealDamage(_unit, _target, {.MyAmount = source.MyStats.MyAttack * kit->MyExtraDamageScale,
			.MyType = DamageType::PHYSICAL, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
	}

	void BattleCore::StandinHealAttack(UnitId _unit, UnitId _target, double _dealt)
	{
		const auto& source = Unit(_unit); const auto* kit = StandinRules(source);
		if (!kit || kit->MyKind != StandinKind::TOUCH || kit->MySkill != 1 || !source.MySkill.MyActive || !std::isgreater(_dealt, 0)) return;
		const auto chance = kit->MySkillProbability;
		if (std::isgreater(chance, 0) && std::isless(_MyRandom.Next(), chance)) (void)Heal(_unit, _target, _dealt);
	}

	void BattleCore::StandinSlash(UnitId _unit, std::uint64_t _deployment)
	{
		const auto& source = Unit(_unit);
		if (StandinUp(source) && source.MyDeploySequence == _deployment) StandinBurst(_unit, *StandinRules(source), false, true);
	}

	void BattleCore::StandinEarly(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::BUFF_TICK && _event.MyUnit)
		{
			const auto& unit = Unit(_event.MyUnit); const auto buff = std::ranges::find(unit.MyBuffs, _event.MyBuff, &CombatBuff::MyId);
			if (buff != unit.MyBuffs.end() && buff->MyDefinition.MyBuiltin == BuiltinBuff::MECHANIST_FEEDBACK && !unit.MyBlockedBy) (void)RemoveBuff(unit.MyId, _event.MyBuff);
		}
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MySource || !_event.MyDamage.MyIsAttack || _event.MyCancel || _MyRaidians.empty()) return;
		auto& source = _MyUnits[Index(_event.MySource)]; const auto type = _event.MyDamage.MyType;
		if (source.MySide != UnitSide::ENEMY || (type != DamageType::PHYSICAL && type != DamageType::ARTS)) return;
		const auto attack = _event.MyDamage.MyAttackId;
		if (!attack || source.MyRaidianAttack != attack)
		{
			double cut = 0; const AttackProfile filter{.MyCanHitFlying = true};
			for (const auto id : _MyRaidians)
			{
				const auto& raid = Unit(id); const auto& kit = *StandinRules(raid);
				if (StandinUp(raid) && TargetableEnemy(source, filter) && InRuleRange(id, source.MyId)) cut = std::max(cut, type == DamageType::ARTS ? kit.MyArtsMiss : kit.MyPhysicalMiss);
			}
			source.MyRaidianAttack = attack;
			source.MyRaidianMiss = std::isgreater(cut, 0) && std::isless(_MyRandom.Next(), std::min(1.0, cut));
		}
		if (source.MyRaidianMiss) _event.MyCancel = true;
	}

	void BattleCore::StandinObserve(ContentEvent& _event, bool _late)
	{
		if (_late)
		{
			if (_event.MyKind == ContentEventKind::FATAL && _event.MyUnit)
			{
				const auto& unit = Unit(_event.MyUnit); const auto* kit = StandinRules(unit);
				if (kit && kit->MyKind == StandinKind::SHARP && kit->MySkill == 3 && unit.MySkill.MyActive && !unit.MyOperatorHooksReleased) _event.MyPrevented = true;
			}
			return;
		}
		if (_event.MyKind == ContentEventKind::TICK || _event.MyKind == ContentEventKind::BATTLE_START)
			for (std::size_t i = 0, count = _MyStandins.size(); i < count; ++i) if (!Unit(_MyStandins[i]).MyOperatorHooksReleased)
				StandinConditions(_MyStandins[i], *StandinRules(Unit(_MyStandins[i])), false);
		if (_event.MyKind == ContentEventKind::BEFORE_STATUS && _event.MyTarget && (_event.MyStatus == CombatStatus::STUN || _event.MyStatus == CombatStatus::FREEZE))
		{
			const auto& target = Unit(_event.MyTarget);
			if (std::ranges::any_of(target.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "acpion:fragrance"; })) _event.MyCancel = true;
		}
		const auto id = _event.MySource ? _event.MySource : _event.MyUnit;
		if (!id) return;
		auto& unit = _MyUnits[Index(id)]; const auto* kit = StandinRules(unit);
		if (!kit || unit.MyOperatorHooksReleased) return;
		if (_event.MyKind == ContentEventKind::DEPLOY && _event.MyUnit == id)
		{
			StandinConditions(id, *kit, false);
			if (kit->MyKind == StandinKind::MISERY && kit->MySkill == 3 && !_event.MyMove && StandinUp(unit)) StandinBurst(id, *kit, true);
			if (kit->MyKind == StandinKind::TULIP)
			{
				unit.MyStandinCasts = 0;
				SetOperatorAttribute(id, "acpion:boundless", std::isgreater(kit->MySpPerSecond, 0), Attribute::SP_RECOVERY_FLAT, kit->MySpPerSecond);
			}
		}
		if (_event.MyKind == ContentEventKind::BEFORE_HEAL && _event.MyTarget && ! _event.MyHealOptions.MyRegen && kit->MyKind == StandinKind::TOUCH &&
			std::isless(Unit(_event.MyTarget).MyHealth / Unit(_event.MyTarget).MyStats.MyMaxHealth, kit->MyHealHealthRatio - 1e-9)) _event.MyAmount *= kit->MyHealScale;
		if (_event.MyKind == ContentEventKind::SP_GAIN && _event.MySpReason == SpReason::TIME)
		{
			bool wanted = kit->MyKind == StandinKind::STORMEYE && std::isgreaterequal(Time() - std::max(unit.MyLastAttackAt, unit.MyDeployedAt), kit->MyIdleDelay - 1e-9);
			if (kit->MyKind == StandinKind::RAIDIAN)
			{
				const AttackProfile filter{.MyCanHitFlying = true};
				wanted = std::ranges::any_of(_MyEnemyIds, [&](UnitId _enemy) { return TargetableEnemy(Unit(_enemy), filter) && InRuleRange(id, _enemy); });
			}
			if (wanted) _event.MyAmount += kit->MySpPerSecond * BattleClock::StepSeconds;
		}
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && _event.MyDamage.MyIsAttack && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
		{
			if (kit->MyKind == StandinKind::SHARP && Unit(_event.MyTarget).MyBlockedBy) _event.MyDamage.MyAmount *= kit->MyBlockedScale;
			if (kit->MyKind == StandinKind::STORMEYE && _event.MyDamage.MyType != DamageType::ELEMENTAL)
			{
				const auto chance = unit.MySkill.MyActive && kit->MySkill == 3 ? kit->MySkillProbability : kit->MyProbability;
				if (std::isgreater(kit->MyCriticalScale, 0) && std::islessgreater(kit->MyCriticalScale, 1) && std::isgreater(chance, 0) &&
					std::isless(_MyRandom.Next(), chance)) _event.MyDamage.MyAmount *= kit->MyCriticalScale;
				if (unit.MySkill.MyActive && kit->MySkill == 3 && !_event.MyDamage.MyIsSplash)
				{
					const auto attack = _event.MyDamage.MyAttackId;
					auto found = std::ranges::find(unit.MyStandinMarks, _event.MyTarget, &StandinAttackMark::MyTarget);
					if (found == unit.MyStandinMarks.end()) found = unit.MyStandinMarks.emplace(unit.MyStandinMarks.end(), StandinAttackMark{.MyTarget = _event.MyTarget});
					if (!attack || found->MyAttack != attack)
					{
						found->MyAttack = attack; found->MyExtra = std::isgreaterequal(Unit(_event.MyTarget).MyHealth / Unit(_event.MyTarget).MyStats.MyMaxHealth, kit->MyExtraHealthRatio - 1e-9);
					}
				}
			}
		}
		if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyTarget)
		{
			if (kit->MyKind == StandinKind::MISERY && HasTag(_event.MyDamage.MyTags, DamageTag::MISERY_PULL)) unit.MyStandinPullLanded.push_back(_event.MyTarget);
			if (kit->MyKind == StandinKind::SHARP_LORD && StandinUp(unit) && Unit(_event.MyTarget).MySide == UnitSide::ENEMY &&
				!HasTag(_event.MyDamage.MyTags, DamageTag::HP_LOSS) && std::isgreater(Unit(_event.MyTarget).MyHealth, 0) && std::isgreater(kit->MyModuleHealthLoss, 0))
				(void)LoseHealth(id, _event.MyTarget, unit.MyStats.MyAttack * kit->MyModuleHealthLoss);
		}
		if (_event.MyKind == ContentEventKind::DEATH && _event.MySource == id && _event.MyUnit && Unit(_event.MyUnit).MySide == UnitSide::ENEMY && kit->MyKind == StandinKind::TULIP && std::isgreater(kit->MyKillDp, 0) && _event.MyRemovalReason == RemovalReason::KILLED)
			(void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, kit->MyKillDp);
		if (_event.MyKind == ContentEventKind::SKILL_START && _event.MyUnit == id)
		{
			unit.MyStandinAccumulator = 0; unit.MyStandinDpCount = 0;
			if (kit->MyKind == StandinKind::SHARP) { unit.MyStandinStackTarget = 0; (void)RemoveBuff(id, "acguad:s3stack"); }
			if (kit->MyKind == StandinKind::MECHANIST && kit->MySkill == 2) StandinFeedback(id, *kit);
			if (kit->MyKind == StandinKind::PITH && kit->MySkill == 3) StandinBurst(id, *kit);
			if ((kit->MyKind == StandinKind::TOUCH || kit->MyKind == StandinKind::PITH) && kit->MySkill == 2) StandinAura(id, *kit, false);
			if (kit->MyKind == StandinKind::RAIDIAN && kit->MySkill == 3) StandinAura(id, *kit, false);
			if (kit->MyKind == StandinKind::TULIP)
			{
				if (++unit.MyStandinCasts >= kit->MyBoundlessCasts) (void)RemoveBuff(id, "acpion:boundless");
				if (kit->MySkill != 2) (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, kit->MyDp);
				if (kit->MySkill == 1) StandinBurst(id, *kit);
				if (kit->MySkill == 3)
				{
					StatusFlags flags; for (const auto status : {CombatStatus::INVULNERABLE, CombatStatus::NO_BLOCK, CombatStatus::DISARM}) flags.set(static_cast<std::size_t>(status));
					(void)AddBuff(id, {.MyKey = "acpion:fragrance", .MySource = id, .MyDuration = 0.4 + (kit->MySlashes - 1) / 6.0 + 0.233, .MyFlags = flags});
					ReleaseBlocked(unit);
					for (unsigned i = 0; i < kit->MySlashes; ++i) Schedule({.MyAt = Time() + 0.4 + i / 6.0, .MyKind = ScheduledKind::TULIP_SLASH, .MySource = id, .MyVersion = unit.MyDeploySequence});
				}
			}
		}
		if (_event.MyKind == ContentEventKind::SKILL_TICK && _event.MyUnit == id)
		{
			if (kit->MyKind == StandinKind::TOUCH && kit->MySkill == 2) StandinAura(id, *kit, false);
			const bool stress = kit->MyKind == StandinKind::MECHANIST && kit->MySkill == 3;
			const bool dp = kit->MyKind == StandinKind::TULIP && kit->MySkill == 2;
			if (stress || dp)
			{
				unit.MyStandinAccumulator += _event.MyDelta; const auto interval = stress ? 1.0 : kit->MyDpInterval;
				while (std::isgreaterequal(unit.MyStandinAccumulator, interval - (stress ? 1e-6 : 1e-9)) && unit.MyAlive && !Finished() && (!dp || unit.MyStandinDpCount < kit->MyDpCount))
				{
					unit.MyStandinAccumulator -= interval;
					if (stress) StandinBurst(id, *kit);
					else { ++unit.MyStandinDpCount; (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, kit->MyDp); }
				}
			}
		}
		if (_event.MyKind == ContentEventKind::SKILL_ENDING && _event.MyUnit == id)
		{
			if (kit->MyKind == StandinKind::SHARP) { unit.MyStandinStackTarget = 0; (void)RemoveBuff(id, "acguad:s3stack"); }
			if (kit->MyKind == StandinKind::MECHANIST && kit->MySkill == 2) for (const auto enemy : _MyEnemyIds)
			{
				const auto& buffs = Unit(enemy).MyBuffs;
				const auto buff = std::ranges::find(buffs, std::string_view("acfend:feedback2"), [](const auto& _buff) { return std::string_view(_buff.MyDefinition.MyKey); });
				if (buff != buffs.end() && buff->MyDefinition.MySource == id) (void)RemoveBuff(enemy, buff->MyId);
			}
			if (kit->MyKind == StandinKind::TOUCH && kit->MySkill == 2) for (const auto ally : _MyAllyIds)
			{
				const auto& buffs = Unit(ally).MyBuffs;
				const auto buff = std::ranges::find(buffs, std::string_view("acmedc:apocalypse"), [](const auto& _buff) { return std::string_view(_buff.MyDefinition.MyKey); });
				if (buff != buffs.end() && buff->MyDefinition.MySource == id) (void)RemoveBuff(ally, buff->MyId);
			}
		}
	}
}
