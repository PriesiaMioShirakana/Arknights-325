#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		const DiyOperatorKit* DiyRules(const CombatUnit& _unit)
		{
			return _unit.MyDefinition.MyOperatorKit ? std::get_if<DiyOperatorKit>(_unit.MyDefinition.MyOperatorKit) : nullptr;
		}

		bool DiyUp(const CombatUnit& _unit)
		{
			return _unit.MyAlive && !_unit.MyHidden && !_unit.MyOperatorHooksReleased;
		}

		bool HasDiyBuff(const CombatUnit& _unit, std::string_view _key)
		{
			return std::ranges::any_of(_unit.MyBuffs, [&](const auto& _buff) { return _buff.MyDefinition.MyKey == _key; });
		}

		struct DiyScratchGuard
		{
			std::size_t& MyDepth;

			~DiyScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::InstallDiyOperator(UnitId _unit, const DiyOperatorKit& _kit)
	{
		_MyDiyOperators.push_back(_unit);
		InstallDiyTeam(_unit, _kit);
		for (const auto& aura : _kit.MyAuras)
		{
			const auto handle = InstallOperatorAura(_unit, {.MyKey = aura.MyKey, .MyAttribute = aura.MyAttribute,
				.MyValue = aura.MyValue, .MyProfession = aura.MyProfession, .MyInterval = aura.MyInterval, .MyInitialDelay = aura.MyInterval,
				.MyDuration = aura.MyDuration, .MyOperatorsOnly = aura.MyProfession.has_value(), .MySkillActive = aura.MySkillActive,
				.MyAttackRange = aura.MyAttackRange, .MySkipHidden = true, .MyModifiers = aura.MyModifiers,
				.MyGroundExtra = aura.MyGroundExtra, .MyStrengthValue = aura.MyStrength.value_or(aura.MyValue + aura.MyGroundExtra),
				.MyStrengthRequiresLivingSource = _kit.MyKind == DiyOperatorKind::SHINING || _kit.MyKind == DiyOperatorKind::CGBIRD, .MyCarriedSource = aura.MyCarriedSource});
			RefreshOperatorAura(handle);
		}
		if (_kit.MyKind == DiyOperatorKind::ZUMAMA)
		{
			DiyOperatorTick(_unit, true);
			Schedule({.MyAt = Time() + 0.1, .MyKind = ScheduledKind::DIY_OPERATOR_PULSE, .MySource = _unit, .MyInterval = 0.1});
		}
		if (_kit.MyKind == DiyOperatorKind::SHWAZ && std::isgreater(_kit.MyCrossfireAttack, 0))
		{
			const auto& source = Unit(_unit);
			const auto sniper = [&](UnitId _id)
			{
				const auto& ally = Unit(_id);
				return ally.MyKind == UnitKind::OPERATOR && ally.MyDefinition.MyOperatorProfession == OperatorProfession::SNIPER;
			};
			if (_kit.MySquadCrossfire)
			{
				if (std::ranges::any_of(_MyAllyIds, [&](UnitId _id) { return _id != _unit && Unit(_id).MyOwner == source.MyOwner && sniper(_id); }))
					for (const auto id : _MyAllyIds)
					{
						const auto& ally = Unit(id);
						if (ally.MyOwner != source.MyOwner || !sniper(id)) continue;
						const auto current = std::ranges::find(ally.MyBuffs, std::string_view("talent:shwaz:crossfire"), [](const auto& _buff) { return std::string_view(_buff.MyDefinition.MyKey); });
						if (current != ally.MyBuffs.end() && current->MyDefinition.MySource != _unit && current->MyDefinition.MyStrength &&
							std::isgreater(current->MyDefinition.MyStrength->MyValue, _kit.MyCrossfireAttack) && std::isgreater(current->MyRemaining, 0.05)) continue;
						(void)AddBuff(id, {.MyKey = "talent:shwaz:crossfire", .MySource = _unit,
							.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _kit.MyCrossfireAttack}},
							.MyPersistent = true, .MyAllowDead = true, .MyStrength = BuffStrength{.MyValue = _kit.MyCrossfireAttack, .MyAttribute = Attribute::ATTACK_PERCENT}});
					}
			}
			else (void)InstallOperatorAura(_unit, {.MyKey = "talent:shwaz:crossfire", .MyValue = _kit.MyCrossfireAttack,
				.MyProfession = OperatorProfession::SNIPER, .MyInterval = 0.5, .MyInitialDelay = 0.5, .MyDuration = 0.6,
				.MyOperatorsOnly = true, .MySkipHidden = true, .MyMinimumPartners = 1});
		}
		if (_kit.MyKind == DiyOperatorKind::GDGLOW)
		{
			auto& unit = _MyUnits[Index(_unit)];
			unit.MyFlyingDrones.reserve(_kit.MyDrones); unit.MyDiyDroneStacks.reserve(_kit.MyDrones);
		}
	}

	bool BattleCore::DiyInFrontLine(UnitId _unit, UnitId _target) const
	{
		const auto& source = Unit(_unit); const auto point = RulePosition(source);
		const auto row = static_cast<int>(std::floor(point.MyY + 0.5)), column = static_cast<int>(std::floor(point.MyX + 0.5));
		const auto forward = RotateOffset({.MyColumn = 1}, source.MyFacing);
		for (int r = row, c = column; r >= 0 && r < FieldRows && c >= 0 && c < FieldColumns; r += forward.MyRow, c += forward.MyColumn)
			if (BodyOnTile(Unit(_target), r, c)) return true;
		return false;
	}

	void BattleCore::DiyOperatorDroneTick(UnitId _unit, double _delta)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto& kit = *DiyRules(unit);
		const auto activation = unit.MySkill.MyActivations;
		const auto valid = [&]() { return unit.MyAlive && unit.MySkill.MyActive && unit.MySkill.MyActivations == activation && !Finished(); };
		for (std::size_t i = 0; i < unit.MyFlyingDrones.size() && valid(); ++i)
		{
			auto drone = unit.MyFlyingDrones[i]; const auto profile = EffectiveAttack(unit);
			drone.MyCooldown = std::max(0.0, drone.MyCooldown - _delta);
			if (drone.MyTarget && !TargetableEnemy(Unit(drone.MyTarget), profile)) drone.MyTarget = 0;
			if (!drone.MyTarget)
			{
				const auto targets = AllyTargets(unit);
				if (targets.empty()) { unit.MyFlyingDrones[i] = drone; continue; }
				drone.MyTarget = targets.front(); drone.MyRampTarget = 0;
			}
			if (std::isgreater(drone.MyCooldown, 1e-9)) { unit.MyFlyingDrones[i] = drone; continue; }
			drone.MyCooldown = unit.MyStats.AttackInterval();
			const auto& funnel = unit.MyDefinition.MyProfession;
			drone.MyRamp = drone.MyRampTarget == drone.MyTarget ? std::min(funnel.MyFunnelMax, drone.MyRamp + funnel.MyFunnelDelta) : funnel.MyFunnelInitial;
			drone.MyRampTarget = drone.MyTarget;
			unit.MyFlyingDrones[i] = drone;
			const auto target = drone.MyTarget;
			(void)DealDamage(_unit, target, {.MyAmount = unit.MyStats.MyAttack * unit.MyStats.MyAttackScaleMultiplier * drone.MyRamp,
				.MyType = DamageType::ARTS, .MyTags = DamageTag::SKILL | DamageTag::DRONE, .MyIsSkill = true});
			if (std::isgreater(kit.MySluggish, 0) && Unit(target).MyAlive) (void)ApplyStatus(target, CombatStatus::SLUGGISH, kit.MySluggish, _unit);
			if (!valid() || i >= unit.MyFlyingDrones.size()) return;
			if (!std::isgreater(kit.MyProbability, 0) || !std::isgreater(kit.MyBlastScale, 0)) continue;
			const auto stack = unit.MyDiyDroneStacks[i];
			const auto probability = std::min(1.0, kit.MyProbability * stack);
			if (!(stack > kit.MyMaxStacks || std::isless(_MyRandom.Next(), probability)))
			{
				unit.MyDiyDroneStacks[i] = std::min(kit.MyMaxStacks + 1, stack + 1); continue;
			}
			unit.MyDiyDroneStacks[i] = 1; unit.MyFlyingDrones[i].MyTarget = 0;
			auto& scratch = AcquireAttackScratch(); const DiyScratchGuard guard{.MyDepth = _MyAttackDepth};
			FoesInRadius(Unit(target).MyPosition, 1.1, scratch.MyTargets, true);
			for (const auto enemy : scratch.MyTargets)
			{
				if (!Unit(enemy).MyAlive) continue;
				(void)DealDamage(_unit, enemy, {.MyAmount = unit.MyStats.MyAttack * kit.MyBlastScale, .MyType = DamageType::ARTS,
					.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
				if (std::isgreater(kit.MySluggish, 0) && Unit(enemy).MyAlive) (void)ApplyStatus(enemy, CombatStatus::SLUGGISH, kit.MySluggish, _unit);
			}
		}
	}

	void BattleCore::DiyOperatorTick(UnitId _unit, bool _periodic)
	{
		const auto& unit = Unit(_unit); const auto& kit = *DiyRules(unit);
		const bool alive = DiyUp(unit), blocking = !unit.MyBlocking.empty();
		if (!_periodic)
		{
			if (!kit.MyBlockingModifiers.empty()) SetOperatorModifiers(_unit, "diy:block", alive && blocking, kit.MyBlockingModifiers);
			if (kit.MyKind == DiyOperatorKind::ZUMAMA)
			{
				SetOperatorAttribute(_unit, "talent:zumama:fight", alive && blocking, Attribute::SP_RECOVERY_FLAT, kit.MyBlockingSp);
				if (kit.MyUnblockedSpMultiplier) SetOperatorAttribute(_unit, "trait:zumama:duel", alive && !blocking, Attribute::SP_RECOVERY_MULTIPLIER, *kit.MyUnblockedSpMultiplier);
				else if (alive && !blocking && !HasDiyBuff(unit, "trait:zumama:duel"))
				{
					StatusFlags flags; flags.set(static_cast<std::size_t>(CombatStatus::NO_SP));
					(void)AddBuff(_unit, {.MyKey = "trait:zumama:duel", .MyFlags = flags});
				}
				else if (!alive || blocking) (void)RemoveBuff(_unit, "trait:zumama:duel");
			}
			if (kit.MyKind == DiyOperatorKind::HELAGE)
			{
				SetOperatorAttribute(_unit, "talent:helage:regen", alive && !blocking, Attribute::HEALTH_REGEN, kit.MyRegen);
				SetOperatorAttribute(_unit, "talent:helage:regenLow", alive && blocking && std::isless(unit.MyHealth / unit.MyStats.MyMaxHealth, kit.MyRegenHealthRatio), Attribute::HEALTH_REGEN, kit.MyLowHealthRegen);
				if (std::isgreater(kit.MyBerserkSpeed, 0) && std::isless(kit.MyBerserkHealthRatio, 1))
				{
					const auto value = alive ? kit.MyBerserkSpeed * std::clamp((1 - unit.MyHealth / unit.MyStats.MyMaxHealth) / (1 - kit.MyBerserkHealthRatio), 0.0, 1.0) : 0;
					const auto current = std::ranges::find(unit.MyBuffs, std::string_view("talent:helage:berserk"), [](const auto& _buff) { return std::string_view(_buff.MyDefinition.MyKey); });
					const auto previous = current != unit.MyBuffs.end() && current->MyDefinition.MyStrength ? current->MyDefinition.MyStrength->MyValue : 0;
					if (current == unit.MyBuffs.end() || std::isgreaterequal(std::abs(value - previous), 0.05))
					{
						if (std::isless(value, 0.05)) (void)RemoveBuff(_unit, "talent:helage:berserk");
						else (void)AddBuff(_unit, {.MyKey = "talent:helage:berserk",
							.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = value}}, .MyStrength = BuffStrength{.MyValue = value, .MyAttribute = Attribute::ATTACK_SPEED}});
					}
				}
				if (alive && std::isgreater(kit.MyProtection, 0) && std::isless(unit.MyHealth / unit.MyStats.MyMaxHealth, kit.MyProtectHealthRatio))
					(void)ApplyStrongest(_unit, "protect", 1.5 * BattleClock::StepSeconds, {.MyValue = kit.MyProtection,
						.MyAttribute = Attribute::PHYSICAL_TAKEN_MULTIPLIER, .MyScale = -1, .MyOffset = 1, .MySecondAttribute = Attribute::ARTS_TAKEN_MULTIPLIER}, _unit);
			}
		}
		else if (kit.MyKind == DiyOperatorKind::ZUMAMA)
		{
			const bool high = alive && std::isgreater(unit.MyHealth / unit.MyStats.MyMaxHealth, kit.MyProtectHealthRatio + 1e-9);
			SetOperatorAttribute(_unit, "talent:zumama:valor", high && std::islessgreater(kit.MyHighHealthScale, 1), Attribute::ATTACK_SCALE_MULTIPLIER, kit.MyHighHealthScale);
			if (alive && !high && std::isgreater(kit.MyProtection, 0)) (void)ApplyStrongest(_unit, "protect", 0.15,
				{.MyValue = kit.MyProtection, .MyAttribute = Attribute::PHYSICAL_TAKEN_MULTIPLIER, .MyScale = -1, .MyOffset = 1,
					.MySecondAttribute = Attribute::ARTS_TAKEN_MULTIPLIER}, _unit);
		}
	}

	void BattleCore::GrantSharedBarrier(UnitId _source, UnitId _target, std::string_view _key, std::string_view _statKey,
		double _shield, double _duration, std::span<const AttributeChange> _modifiers, unsigned _typeMask)
	{
		if (!Unit(_target).MyAlive || !std::isgreater(_shield, 0) || !std::isgreater(_duration, 0)) return;
		(void)AddBuff(_target, {.MyKey = std::string(_key), .MySource = _source, .MyDuration = _duration, .MyMaxStacks = 99,
			.MyRefresh = BuffRefresh::INDEPENDENT, .MyShield = {.MyHealth = _shield, .MyTypeMask = _typeMask},
			.MyBuiltin = BuiltinBuff::SHARED_BARRIER, .MyBarrierStatKey = _statKey});
		(void)AddBuff(_target, {.MyKey = std::string(_statKey), .MySource = _source, .MyDuration = _duration, .MyRefresh = BuffRefresh::EXTEND,
			.MyModifiers = std::vector<AttributeChange>(_modifiers.begin(), _modifiers.end())});
	}

	void BattleCore::SyncSharedBarrier(UnitId _target, std::string_view _statKey)
	{
		if (_statKey.empty()) return;
		if (!std::ranges::any_of(Unit(_target).MyBuffs, [&](const auto& _buff) { return _buff.MyDefinition.MyBuiltin == BuiltinBuff::SHARED_BARRIER && _buff.MyDefinition.MyBarrierStatKey == _statKey; }))
			(void)RemoveBuff(_target, _statKey);
	}

	void BattleCore::DiyOperatorHealHit(UnitId _unit, UnitId _target, const AttackProfile& _profile)
	{
		const auto& source = Unit(_unit); const auto* kit = DiyRules(source);
		if (!kit || kit->MySkill != 2 || !_profile.MySkillDamage || (kit->MyKind != DiyOperatorKind::SHINING && kit->MyKind != DiyOperatorKind::CGBIRD)) return;
		const bool shining = kit->MyKind == DiyOperatorKind::SHINING;
		GrantSharedBarrier(_unit, _target, shining ? "shining:barrier" : "cgbird:barrier", shining ? "shining:barrierDef" : "cgbird:barrierRes",
			source.MyStats.MyAttack * kit->MyBarrierScale, kit->MyBarrierDuration, kit->MyBarrierModifiers, shining ? 15U : 2U);
	}

	void BattleCore::DiyOperatorSkillHit(UnitId _unit, UnitId _target, const AttackProfile& _profile)
	{
		const auto& source = Unit(_unit); const auto* kit = DiyRules(source);
		if (!kit || !((kit->MyKind == DiyOperatorKind::SIEGE && kit->MySkill == 3) || (kit->MyKind == DiyOperatorKind::PALLAS && kit->MySkill == 2)) || !_profile.MySkillDamage || !_target || !Unit(_target).MyAlive) return;
		if (std::isless(_MyRandom.Next(), kit->MyStunChance))
			(void)ApplyStatus(_target, CombatStatus::STUN, kit->MyStunDuration, _unit);
	}

	void BattleCore::DiyOperatorBeforeStart(UnitId _unit)
	{
		const auto& unit = Unit(_unit); const auto* kit = DiyRules(unit);
		if (!kit || kit->MyKind != DiyOperatorKind::LESSNG || kit->MySkill != 3 || !HasAbnormal(_unit)) return;
		(void)DealDamage(_unit, _unit, {.MyAmount = kit->MyOathSelfDamage, .MyType = DamageType::ARTS, .MyCanDodge = false,
			.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
		if (Unit(_unit).MyAlive) (void)CleanseAbnormal(_unit);
	}

	void BattleCore::CgbirdPhantomDeploy(UnitId _unit)
	{
		const auto& token = Unit(_unit);
		if (!token.MyOwnerUnit) return;
		const auto* owner = DiyRules(Unit(token.MyOwnerUnit));
		if (!owner || owner->MyKind != DiyOperatorKind::CGBIRD || token.MyDefinition.MyId != owner->MyToken) return;
		const auto& kit = *token.MyDefinition.MyTokenKit;
		if (std::isgreater(kit.MyDodgeProbability, 0)) (void)AddBuff(_unit, {.MyKey = "cgbird:phantomDodge",
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::PHYSICAL_DODGE, .MyValue = kit.MyDodgeProbability}}});
		if (std::isgreater(kit.MyHealthLossRatio, 0)) (void)AddBuff(_unit, {.MyKey = "cgbird:phantomDrain", .MyInterval = 1, .MyNotifyTick = true});
	}

	void BattleCore::CgbirdPhantomObserve(ContentEvent& _event)
	{
		if (!_event.MyUnit || (_event.MyKind != ContentEventKind::BUFF_TICK && _event.MyKind != ContentEventKind::DEATH)) return;
		auto& token = _MyUnits[Index(_event.MyUnit)];
		if (!token.MyDefinition.MyTokenKit || token.MyDefinition.MyTokenKit->MyKind != TokenKitKind::CGBIRD_PHANTOM || !token.MyOwnerUnit) return;
		const auto* owner = DiyRules(Unit(token.MyOwnerUnit));
		if (!owner || owner->MyKind != DiyOperatorKind::CGBIRD || token.MyDefinition.MyId != owner->MyToken) return;
		if (_event.MyKind == ContentEventKind::BUFF_TICK)
		{
			const auto buff = std::ranges::find(token.MyBuffs, _event.MyBuff, &CombatBuff::MyId);
			if (buff != token.MyBuffs.end() && buff->MyDefinition.MyKey == "cgbird:phantomDrain")
				(void)LoseHealth(0, token.MyId, token.MyStats.MyMaxHealth * token.MyDefinition.MyTokenKit->MyHealthLossRatio, true);
		}
		else if (_event.MyRemovalReason == RemovalReason::KILLED && !Finished())
		{
			token.MyRemoved = false;
			token.MyRespawnAt = Time() + std::max(0.0, token.MyDefinition.MyStats.MyRedeploySeconds);
			Schedule({.MyAt = Time() + 0.5, .MyKind = ScheduledKind::CGBIRD_RESPAWN, .MySource = token.MyOwnerUnit,
				.MyTarget = token.MyId, .MyVersion = token.MyDeploySequence});
		}
	}

	void BattleCore::CgbirdPhantomRespawn(UnitId _owner, UnitId _token, std::uint64_t _deployment)
	{
		const auto& token = Unit(_token); auto& owner = _MyUnits[Index(_owner)];
		if (token.MyAlive || token.MyRemoved || token.MyDeploySequence != _deployment || owner.MyOperatorHooksReleased || Finished()) return;
		if (!std::isgreater(token.MyRespawnAt, Time() + 1e-9) && owner.MyDiyTokenStock > 0 && Redeploy(_token, false))
		{
			--owner.MyDiyTokenStock; return;
		}
		Schedule({.MyAt = Time() + 0.5, .MyKind = ScheduledKind::CGBIRD_RESPAWN, .MySource = _owner, .MyTarget = _token, .MyVersion = _deployment});
	}

	void BattleCore::DiyOperatorObserve(ContentEvent& _event, bool _late)
	{
		if (_late)
		{
			if (_event.MyKind != ContentEventKind::FATAL || !_event.MyUnit || _event.MyPrevented) return;
			auto& unit = _MyUnits[Index(_event.MyUnit)]; const auto* kit = DiyRules(unit);
			if (!kit || unit.MyOperatorHooksReleased || !std::isgreater(kit->MyReviveHealthRatio, 0) || unit.MyDiyReviveDeployment == unit.MyDeploySequence) return;
			if (kit->MyKind == DiyOperatorKind::LESSNG && HoldsUndying(unit.MyId)) return;
			unit.MyDiyReviveDeployment = unit.MyDeploySequence; _event.MyPrevented = true;
			if (!kit->MyReviveModifiers.empty()) (void)AddBuff(unit.MyId, {.MyKey = "trait:lessng:reborn", .MyModifiers = std::vector<AttributeChange>(kit->MyReviveModifiers.begin(), kit->MyReviveModifiers.end())});
			unit.MyHealth = std::max(1.0, unit.MyStats.MyMaxHealth * kit->MyReviveHealthRatio);
			return;
		}
		DiyTeamObserve(_event);
		if (_event.MyKind == ContentEventKind::TICK || _event.MyKind == ContentEventKind::BATTLE_START)
			for (std::size_t i = 0, count = _MyDiyOperators.size(); i < count; ++i) if (!Unit(_MyDiyOperators[i]).MyOperatorHooksReleased) DiyOperatorTick(_MyDiyOperators[i], false);
		if (_event.MyKind == ContentEventKind::BEFORE_STATUS && _event.MyTarget && AbnormalStatus(_event.MyStatus) && HasDiyBuff(Unit(_event.MyTarget), "skill:lessng:oath")) _event.MyCancel = true;
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget)
		{
			const auto& target = Unit(_event.MyTarget); const auto* kit = DiyRules(target);
			if (kit && !target.MyOperatorHooksReleased)
			{
				if (kit->MyKind == DiyOperatorKind::HELAGE && DiyUp(target) && std::isgreater(kit->MyProtection, 0) &&
					std::isless(target.MyHealth / target.MyStats.MyMaxHealth, kit->MyProtectHealthRatio))
					(void)ApplyStrongest(target.MyId, "protect", 1.5 * BattleClock::StepSeconds, {.MyValue = kit->MyProtection,
						.MyAttribute = Attribute::PHYSICAL_TAKEN_MULTIPLIER, .MyScale = -1, .MyOffset = 1, .MySecondAttribute = Attribute::ARTS_TAKEN_MULTIPLIER}, target.MyId);
				if (kit->MyKind == DiyOperatorKind::LESSNG && DiyUp(target))
				{
					const bool blocked = std::ranges::any_of(target.MyBlocking, [&](UnitId _id) { return Unit(_id).MyAlive && Unit(_id).MyBlockedBy == target.MyId; });
					const bool fromBlocked = _event.MySource && Unit(_event.MySource).MySide == UnitSide::ENEMY && Unit(_event.MySource).MyBlockedBy == target.MyId;
					if (blocked && !fromBlocked && (_event.MyDamage.MyType == DamageType::PHYSICAL || _event.MyDamage.MyType == DamageType::ARTS))
						_event.MyDamage.MyMultiplier *= std::max(0.0, 1 - kit->MyProtection * (HasDiyBuff(target, "skill:lessng:duel") ? kit->MyDuelScale : 1));
					if (std::islessgreater(kit->MyPainAttack, 0)) (void)AddBuff(target.MyId, {.MyKey = "talent:lessng:pain", .MyDuration = kit->MyPainDuration,
						.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit->MyPainAttack}}});
				}
			}
		}
		if (_event.MyKind == ContentEventKind::DEATH && _event.MyUnit && Unit(_event.MyUnit).MySide == UnitSide::ENEMY && _event.MyRemovalReason == RemovalReason::KILLED)
			for (std::size_t i = 0, count = _MyDiyOperators.size(); i < count; ++i)
			{
				const auto sourceId = _MyDiyOperators[i]; const auto& source = Unit(sourceId); const auto& kit = *DiyRules(source);
				if (kit.MyKind != DiyOperatorKind::SIEGE || !DiyUp(source) || !std::isgreater(kit.MyKillSp, 0) || !OperatorInGrid(sourceId, _event.MyUnit, kit.MyKillSpRange)) continue;
				if (!(source.MySkill.MyActive && IsTimedSkill(source.MyDefinition.MySkill.MyKind))) (void)GainSp(sourceId, kit.MyKillSp, SpReason::INITIAL);
				if (!std::isgreater(kit.MyOtherKillSp, 0)) continue;
				auto& scratch = AcquireAttackScratch(); const DiyScratchGuard guard{.MyDepth = _MyAttackDepth};
				for (const auto ally : _MyAllyIds) if (ally != sourceId && Unit(ally).MyAlive && !Unit(ally).MyHidden && Unit(ally).MyKind == UnitKind::OPERATOR && Unit(ally).MyDefinition.MyOperatorProfession == OperatorProfession::PIONEER) scratch.MyTargets.push_back(ally);
				if (!scratch.MyTargets.empty()) (void)GainSp(scratch.MyTargets[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MyTargets.size()))], kit.MyOtherKillSp);
			}
		const auto id = _event.MySource ? _event.MySource : _event.MyUnit;
		if (!id) return;
		auto& unit = _MyUnits[Index(id)]; const auto* kit = DiyRules(unit);
		if (!kit || unit.MyOperatorHooksReleased) return;
		if (_event.MyKind == ContentEventKind::DEPLOY && _event.MyUnit == id)
		{
			DiyOperatorTick(id, false);
			if (kit->MyKind == DiyOperatorKind::ZUMAMA) DiyOperatorTick(id, true);
			if (kit->MyKind == DiyOperatorKind::CGBIRD) unit.MyDiyTokenStock = std::min(3U, unit.MyDiyTokenStock + kit->MyTokenStock);
			if (kit->MyKind == DiyOperatorKind::GDGLOW) unit.MyDiyDroneStacks.assign(kit->MyDrones, 1);
		}
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && kit->MyKind == DiyOperatorKind::SHWAZ &&
			Unit(_event.MyTarget).MySide == UnitSide::ENEMY && _event.MyDamage.MyIsAttack && _event.MyDamage.MyType != DamageType::ELEMENTAL)
		{
			auto& damage = _event.MyDamage;
			if (!damage.MyIsSplash)
			{
				if (!damage.MyAttackId || unit.MyDiyRollId != damage.MyAttackId)
				{
					unit.MyDiyRollId = damage.MyAttackId;
					const auto chance = damage.MyIsSkill ? kit->MySkillProbability : kit->MyProbability;
					unit.MyDiyProc = std::isgreater(chance, 0) && std::isless(_MyRandom.Next(), chance);
				}
				if (unit.MyDiyProc) { damage.MyAmount *= kit->MyCriticalScale; damage.MyDiyPierce = true; }
			}
			if (std::isgreater(kit->MyFrontScale, 0) && std::islessgreater(kit->MyFrontScale, 1) && DiyInFrontLine(id, _event.MyTarget))
			{
				damage.MyAmount *= kit->MyFrontScale; damage.MyCanDodge = false;
			}
		}
		if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyTarget && kit->MyKind == DiyOperatorKind::SHWAZ &&
			_event.MyDamage.MyDiyPierce && std::isgreater(kit->MyDefenseCut, 0) && Unit(_event.MyTarget).MySide == UnitSide::ENEMY && std::isgreater(Unit(_event.MyTarget).MyHealth, 0))
			(void)ApplyStatus(_event.MyTarget, CombatStatus::DEFENSE_DOWN, {.MyDuration = kit->MyDefenseCutDuration, .MySource = id, .MyValue = kit->MyDefenseCut});
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && kit->MyKind == DiyOperatorKind::LESSNG && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
		{
			if (Unit(_event.MyTarget).MyBlockedBy == id) _event.MyDamage.MyDefenseIgnorePercent += kit->MyBlockedDefenseIgnore;
			if (_event.MyDamage.MyIsAttack && Unit(_event.MyTarget).MyBlockedBy) _event.MyDamage.MyAmount *= kit->MyBlockedScale * (HasDiyBuff(unit, "skill:lessng:oath") ? kit->MyOathBlockedScale : 1);
		}
		if (_event.MyKind == ContentEventKind::BEFORE_HEAL && _event.MyTarget && !_event.MyHealOptions.MyRegen && kit->MyKind == DiyOperatorKind::SHINING)
		{
			const auto& target = Unit(_event.MyTarget);
			if (kit->MyHealGround ? target.MyGround : std::islessequal(target.MyHealth / target.MyStats.MyMaxHealth, kit->MyHealHealthRatio + 1e-9)) _event.MyAmount *= kit->MyHealScale;
		}
		if (_event.MyKind == ContentEventKind::SKILL_START && _event.MyUnit == id)
		{
			if (kit->MyKind == DiyOperatorKind::GDGLOW) unit.MyFlyingDrones.assign(kit->MyDrones, FlyingDrone{});
			if (kit->MyKind == DiyOperatorKind::SIEGE && kit->MySkill < 3) (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, kit->MyDp);
			if (kit->MyKind == DiyOperatorKind::LESSNG)
			{
				if (kit->MySkill == 2 && std::isgreater(kit->MyDuelDuration, 0)) (void)AddBuff(id, {.MyKey = "skill:lessng:duel", .MyDuration = kit->MyDuelDuration,
					.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit->MyDuelAttack}}});
				if (kit->MySkill == 3 && unit.MyAlive) (void)AddBuff(id, {.MyKey = "skill:lessng:oath",
					.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALTH_PERCENT, .MyValue = kit->MyOathHealth}}});
			}
		}
		if (_event.MyKind == ContentEventKind::SKILL_TICK && _event.MyUnit == id && kit->MyKind == DiyOperatorKind::GDGLOW) DiyOperatorDroneTick(id, _event.MyDelta);
		if (_event.MyKind == ContentEventKind::SKILL_TICK && _event.MyUnit == id && kit->MyKind == DiyOperatorKind::ZUMAMA && kit->MySkill == 2)
		{
			auto& scratch = AcquireAttackScratch(); const DiyScratchGuard guard{.MyDepth = _MyAttackDepth};
			scratch.MyTargets.assign(unit.MyBlocking.begin(), unit.MyBlocking.end());
			for (const auto enemy : scratch.MyTargets) if (Unit(enemy).MyAlive && Unit(enemy).MyBlockedBy == id) (void)ApplyStatus(enemy, CombatStatus::STUN, {.MyDuration = 0.1, .MySource = id, .MyResistApplied = true});
		}
		if (_event.MyKind == ContentEventKind::SKILL_ENDING && _event.MyUnit == id)
		{
			if (kit->MyKind == DiyOperatorKind::GDGLOW) unit.MyFlyingDrones.clear();
			if (kit->MyKind == DiyOperatorKind::ZUMAMA && kit->MySkill == 3 && DiyUp(unit) && std::isgreater(kit->MyEndStun, 0)) (void)ApplyStatus(id, CombatStatus::STUN, kit->MyEndStun, id);
			if (kit->MyKind == DiyOperatorKind::LESSNG && kit->MySkill == 3) (void)RemoveBuff(id, "skill:lessng:oath");
		}
	}
}
