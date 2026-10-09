#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::InstallEquipmentEffects(CombatUnit& _unit)
	{
		if (_unit.MyKind != UnitKind::OPERATOR) return;
		for (std::size_t i = 0; i < _unit.MyDefinition.MyEquipmentEffects.size(); ++i)
		{
			const auto index = _MyEquipment.size();
			_MyEquipment.emplace_back(EquipmentRuntime{.MyUnit = _unit.MyId, .MyEffect = i});
			InitializeEquipment(index);
		}
	}

	void Battle::InitializeEquipment(std::size_t _index)
	{
		auto& runtime = _MyEquipment[_index];
		const auto& _unit = Unit(runtime.MyUnit);
		const auto& effect = EquipmentDefinition(runtime);
		const auto& p = effect.MyParameters;
		const auto kind = p.MyKind;
		runtime.MyBuffKey = effect.MyKey + (kind == EquipmentEffectKind::FRONT_HEALTH ? ":front" : ":stacks");
		_MyEquipmentHitEffects |= kind == EquipmentEffectKind::DISTANCE_DAMAGE || kind == EquipmentEffectKind::BLOCK_DAMAGE ||
			kind == EquipmentEffectKind::DECAY_DAMAGE || kind == EquipmentEffectKind::WEAKNESS || kind == EquipmentEffectKind::FLAT_DAMAGE_REDUCTION;
		_MyEquipmentEnemyQueries |= kind == EquipmentEffectKind::EXTRA_BULLET || kind == EquipmentEffectKind::COLD_DAMAGE || kind == EquipmentEffectKind::KNIGHT_CREED;
		_MyEquipmentStatusEffects |= kind == EquipmentEffectKind::HEALTH_CONTROL_IMMUNITY;
		if ((kind == EquipmentEffectKind::SOLVENT || kind == EquipmentEffectKind::PARTNER_HEAL) && p.MyValue > 0)
			runtime.MyTickBuff = ApplyEquipmentBuff(runtime, _unit.MyId, BuffDefinition{.MyKey = effect.MyKey + (kind == EquipmentEffectKind::SOLVENT ? ":drain" : ":set"),
				.MyPersistent = true, .MyInterval = 1, .MyAllowDead = true, .MyNotifyTick = true});
		if (kind == EquipmentEffectKind::SIDE_ATTACK_SPEED && p.MyValue != 0)
		{
			runtime.MyBuffKey = effect.MyKey + ":side";
			Schedule(ScheduledAction{.MyAt = Time(), .MyKind = ScheduledKind::EQUIPMENT, .MySource = _unit.MyId,
				.MyHandle = _index, .MyInterval = 0.5});
		}
		if (kind == EquipmentEffectKind::COLD_DAMAGE || kind == EquipmentEffectKind::STEALTH_CHARGE)
		{
			const auto interval = kind == EquipmentEffectKind::COLD_DAMAGE ? std::max(0.1, p.MyInterval) : 0.25;
			runtime.MyBuffKey = effect.MyKey + ":stealth";
			Schedule(ScheduledAction{.MyAt = Time() + interval, .MyKind = ScheduledKind::EQUIPMENT, .MySource = _unit.MyId,
				.MyHandle = _index, .MyInterval = interval});
		}
		if (kind == EquipmentEffectKind::KNIGHT_CREED)
		{
			runtime.MyBuffKey = effect.MyKey + ":combo";
			Schedule(ScheduledAction{.MyAt = Time(), .MyKind = ScheduledKind::EQUIPMENT, .MySource = _unit.MyId,
				.MyHandle = _index, .MyInterval = 0.5});
		}
		if (kind == EquipmentEffectKind::GAINED_ATTACK_SPEED && EquipmentMember(_unit, "yanShip"))
		{
			const auto n = std::min<std::uint64_t>(_MyInput.MyPlayers[_unit.MyOwner].MyGainedChess, p.MyMaximum);
			if (n) (void)ApplyEquipmentBuff(runtime, _unit.MyId, BuffDefinition{.MyKey = effect.MyKey + ":yan",
				.MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_SPEED, p.MyValue * static_cast<double>(n)}}, .MyPersistent = true, .MyAllowDead = true});
		}
		if (kind >= EquipmentEffectKind::HAMMER_BURN && kind <= EquipmentEffectKind::STEAM_HEART) InstallHammer(runtime);
		if (kind == EquipmentEffectKind::FRONT_HEALTH && p.MyExtra != 0)
			(void)ApplyEquipmentBuff(runtime, _unit.MyId, BuffDefinition{.MyKey = effect.MyKey + ":hp", .MyModifiers = std::vector<AttributeChange>{{Attribute::HEALTH_PERCENT, p.MyExtra}},
				.MyPersistent = true, .MyAllowDead = true});
		if (_unit.MyAlive && kind == EquipmentEffectKind::FRONT_HEALTH && p.MyValue != 0) CheckEquipmentFront(runtime, false);
		if (_unit.MyAlive && kind == EquipmentEffectKind::SKILL_COMPASS && p.MyValue > 0) (void)GainSp(_unit.MyId, p.MyValue, SpReason::INITIAL);
	}

	void Battle::RetypeWeakness(DamageInfo& _damage, UnitId _source, UnitId _target, bool _schoolMultipliers) const
	{
		if (_damage.MyType != DamageType::PHYSICAL && _damage.MyType != DamageType::ARTS) return;
		const auto& source = Unit(_source).MyStats;
		const auto& target = Unit(_target).MyStats;
		const Mitigation mitigation{.MyDefense = target.MyDefense, .MyResistance = target.MyResistance,
			.MyDefenseIgnorePercent = source.MyDefenseIgnorePercent + _damage.MyDefenseIgnorePercent,
			.MyDefenseIgnoreFlat = source.MyDefenseIgnoreFlat + _damage.MyDefenseIgnoreFlat,
			.MyResistanceIgnorePercent = source.MyResistanceIgnorePercent + _damage.MyResistanceIgnorePercent,
			.MyResistanceIgnoreFlat = source.MyResistanceIgnoreFlat + _damage.MyResistanceIgnoreFlat};
		const auto physical = Mitigate(_damage.MyAmount, DamageType::PHYSICAL, mitigation) * (_schoolMultipliers ? source.MyPhysicalDealtMultiplier * target.MyPhysicalTakenMultiplier : 1);
		const auto arts = Mitigate(_damage.MyAmount, DamageType::ARTS, mitigation) * (_schoolMultipliers ? source.MyArtsDealtMultiplier * target.MyArtsTakenMultiplier : 1);
		if (arts > physical + 1e-9) _damage.MyType = DamageType::ARTS;
		else if (physical > arts + 1e-9) _damage.MyType = DamageType::PHYSICAL;
	}

	void Battle::NotifyEquipment(ContentEvent& _event, bool _early)
	{
		const auto count = _MyEquipment.size();
		for (std::size_t i = 0; i < count; ++i)
		{
			auto& runtime = _MyEquipment[i];
			const auto& unit = Unit(runtime.MyUnit);
			if (unit.MyRemoved) continue;
			if (!_early && runtime.MyHammer)
			{
				auto& hammer = _MyHammers[*runtime.MyHammer];
				if (hammer.MyReferences && hammer.MyGeneration == runtime.MyHammerGeneration) NotifyHammer(hammer, _event);
			}
			if (!EquipmentActive(runtime)) continue;
			const auto& p = EquipmentDefinition(runtime).MyParameters;
			if (_early != (p.MyKind == EquipmentEffectKind::REVIVE || p.MyKind == EquipmentEffectKind::WEAKNESS)) continue;
			const bool own = _event.MyUnit == unit.MyId, source = _event.MySource == unit.MyId, target = _event.MyTarget == unit.MyId;
			const auto chance = [&] { return p.MyProbability > 0 && (p.MyProbability >= 1 || _MyRandom.Next() < p.MyProbability); };
			const auto attackEnemies = [&] { return _event.MyKind == ContentEventKind::ATTACK && source; };
			switch (p.MyKind)
			{
			case EquipmentEffectKind::HAMMER_BURN:
			case EquipmentEffectKind::HAMMER_UNDYING:
			case EquipmentEffectKind::HAMMER_SPEED:
			case EquipmentEffectKind::HAMMER_TREMBLE:
			case EquipmentEffectKind::STEAM_HEART:
				break;
			case EquipmentEffectKind::SOLVENT:
				if (_event.MyKind == ContentEventKind::BUFF_TICK && own && _event.MyBuff == runtime.MyTickBuff && unit.MyAlive) EquipmentSolvent(runtime);
				break;
			case EquipmentEffectKind::ARCANE_SILENCE:
				if (!(p.MyDuration > 0)) break;
				if (attackEnemies()) for (const auto id : _event.MyTargets)
					if (Unit(id).MySide == UnitSide::ENEMY && Unit(id).MyAlive) (void)ApplyStatus(id, CombatStatus::SILENCE, p.MyDuration, unit.MyId);
				if (_event.MyKind == ContentEventKind::DAMAGED && _event.MySource && !source && !_event.MyElement && !_event.MyEquipmentSilenced && unit.MyAlive)
				{
					const auto& attacker = Unit(_event.MySource); const auto& victim = Unit(_event.MyTarget);
					if (attacker.MySide == UnitSide::ALLY && attacker.MyDefinition.MyId.starts_with("enemy_") && victim.MySide == UnitSide::ENEMY && victim.MyAlive)
					{
						_event.MyEquipmentSilenced = true;
						(void)ApplyStatus(victim.MyId, CombatStatus::SILENCE, p.MyDuration, attacker.MyId);
					}
				}
				break;
			case EquipmentEffectKind::TRENCH_COUNTER:
				if (_event.MyKind == ContentEventKind::DAMAGED && target && _event.MySource && unit.MyAlive && Time() >= runtime.MyReadyAt &&
					!HasTag(_event.MyDamage.MyTags, DamageTag::ITEM) && EquipmentMember(unit, "egirShip"))
				{
					const auto& attacker = Unit(_event.MySource);
					if (attacker.MySide != UnitSide::ENEMY || !attacker.MyAlive) break;
					runtime.MyReadyAt = Time() + p.MyInterval;
					const auto& partner = EquipmentDefinition(runtime).MyPartner;
					const auto hits = !partner.empty() && CarriesEquipment(unit, partner) ? 2U : 1U;
					for (unsigned hit = 0; hit < hits && attacker.MyAlive; ++hit)
						(void)DealDamage(unit.MyId, attacker.MyId, DamageInfo{.MyAmount = unit.MyStats.MyAttack * p.MyValue,
							.MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::ITEM)});
				}
				break;
			case EquipmentEffectKind::KNIGHT_CREED:
			case EquipmentEffectKind::PARTNER_HEAL:
			case EquipmentEffectKind::PARTNER_TRUE_DAMAGE:
			case EquipmentEffectKind::EXTRA_BULLET:
			case EquipmentEffectKind::SKILL_COMPASS:
			case EquipmentEffectKind::STEALTH_CHARGE:
				NotifyEquipmentSignature(runtime, _event);
				break;
			case EquipmentEffectKind::GAINED_ATTACK_SPEED:
			case EquipmentEffectKind::COLD_DAMAGE:
			case EquipmentEffectKind::SIDE_ATTACK_SPEED:
			case EquipmentEffectKind::FLAT_DAMAGE_REDUCTION:
				break; // 分别在公共定时队列和低优先级命中阶段处理。
			case EquipmentEffectKind::DISTANCE_DAMAGE:
				if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && source && Unit(_event.MyTarget).MySide == UnitSide::ENEMY &&
					Distance(unit.MyPosition, Unit(_event.MyTarget).MyPosition) >= p.MyThreshold - 1e-6) _event.MyDamage.MyMultiplier *= p.MyValue;
				break;
			case EquipmentEffectKind::SKILL_ATTACK_STACK:
				if (_event.MyKind == ContentEventKind::SKILL_START && own && runtime.MyUsed < p.MyMaximum && chance())
				{
					++runtime.MyUsed;
					(void)ApplyEquipmentBuff(runtime, unit.MyId, BuffDefinition{.MyKey = runtime.MyBuffKey,
						.MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, p.MyValue * runtime.MyUsed}}, .MyPersistent = true, .MyAllowDead = true});
				}
				break;
			case EquipmentEffectKind::DEPLOY_BOND_SP:
				if (_event.MyKind == ContentEventKind::DEPLOY && own && p.MyValue > 0)
				{
					unsigned n = 1;
					for (const auto id : _MyAllyIds)
					{
						const auto& other = Unit(id);
						if (id == unit.MyId || other.MyKind != UnitKind::OPERATOR || other.MyOwner != unit.MyOwner || other.MyRemoved ||
							(!_event.MyInitial && !other.MyAlive)) continue;
						if (std::ranges::any_of(other.MyDefinition.MyIdentity.MyBonds, [&](const std::string& _bond)
							{ return std::ranges::contains(unit.MyDefinition.MyIdentity.MyBonds, _bond); })) ++n;
					}
					(void)GainSp(unit.MyId, p.MyValue * n);
				}
				break;
			case EquipmentEffectKind::FRONT_HEALTH:
				if (_event.MyKind == ContentEventKind::DEPLOY && own && p.MyValue != 0) CheckEquipmentFront(runtime, _event.MyInitial);
				break;
			case EquipmentEffectKind::DEPLOY_STUN:
			case EquipmentEffectKind::ATTACK_PALSY:
			case EquipmentEffectKind::ATTACK_COLD:
				if (attackEnemies()) for (const auto id : _event.MyTargets)
				{
					const auto& victim = Unit(id);
					if (victim.MySide != UnitSide::ENEMY || !victim.MyAlive) continue;
					if (p.MyKind == EquipmentEffectKind::DEPLOY_STUN)
					{
						if (p.MyDuration > 0 && p.MyValue > 0 && Time() - unit.MyDeployedAt <= p.MyDuration + 1e-9)
							(void)ApplyStatus(id, CombatStatus::STUN, p.MyValue, unit.MyId);
					}
					else if (p.MyKind == EquipmentEffectKind::ATTACK_PALSY)
					{
						if (chance()) (void)ApplyStatus(id, CombatStatus::PALSY, StatusApplication{.MySource = unit.MyId, .MyValue = 1});
					}
					else if (p.MyDuration > 0 && chance()) (void)ApplyStatus(id, CombatStatus::COLD, p.MyDuration, unit.MyId);
				}
				break;
			case EquipmentEffectKind::ATTACK_SPEED_STACK:
				if (attackEnemies() && p.MyValue != 0) (void)ApplyEquipmentBuff(runtime, unit.MyId, BuffDefinition{.MyKey = runtime.MyBuffKey,
					.MyMaxStacks = p.MyMaximum, .MyRefresh = BuffRefresh::STACK, .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_SPEED, p.MyValue}},
					.MyPersistent = true, .MyAllowDead = true});
				break;
			case EquipmentEffectKind::FIRST_DAMAGE_STEALTH:
				if (_event.MyKind == ContentEventKind::DAMAGED && target && _event.MyAmount > 0 && unit.MyAlive && !runtime.MyUsed &&
					p.MyDuration > 0 && !HasTag(_event.MyDamage.MyTags, DamageTag::HP_LOSS))
				{
					++runtime.MyUsed; (void)ApplyStatus(unit.MyId, CombatStatus::STEALTH, p.MyDuration, unit.MyId);
				}
				break;
			case EquipmentEffectKind::BLOCK_DAMAGE:
				if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && target && !unit.MyBlocking.empty() && _event.MySource && !source &&
					!std::ranges::contains(unit.MyBlocking, _event.MySource)) _event.MyDamage.MyMultiplier *= p.MyValue;
				break;
			case EquipmentEffectKind::ATTACK_SELF_HEAL:
				if (attackEnemies() && p.MyValue > 0) for (const auto id : _event.MyTargets)
					if (Unit(id).MySide == UnitSide::ENEMY && Unit(id).MyAlive) (void)Heal(unit.MyId, unit.MyId, unit.MyStats.MyMaxHealth * p.MyValue, HealOptions{.MySelf = true});
				break;
			case EquipmentEffectKind::DECAY_DAMAGE:
				if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && source)
				{
					const auto time = Time() - unit.MyDeployedAt;
					const auto mul = time <= p.MyDuration + 1e-9 ? p.MyValue : p.MyExtra < 0 ?
						std::max(1.0, p.MyValue + p.MyExtra * std::ceil((time - p.MyDuration) / std::max(0.1, p.MyInterval) - 1e-9)) : 1;
					_event.MyDamage.MyMultiplier *= mul;
				}
				break;
			case EquipmentEffectKind::AMMO_REFILL:
				if (_event.MyKind == ContentEventKind::AMMO_USED && own && _event.MyAmount == 1)
				{
					if (runtime.MyDeployment != unit.MyDeploySequence) { runtime.MyDeployment = unit.MyDeploySequence; runtime.MyUsed = 0; }
					if (runtime.MyUsed < p.MyMaximum && chance()) { ++runtime.MyUsed; AddSkillAmmo(unit.MyId, std::max(1.0, std::ceil(p.MyValue * unit.MyDefinition.MySkill.MyAmmo - 1e-9))); }
				}
				break;
			case EquipmentEffectKind::HEALTH_CONTROL_IMMUNITY:
				if (_event.MyKind == ContentEventKind::BEFORE_STATUS && target && unit.MyHealth / unit.MyStats.MyMaxHealth > p.MyThreshold &&
					(_event.MyStatus == CombatStatus::STUN || _event.MyStatus == CombatStatus::FREEZE || _event.MyStatus == CombatStatus::SLEEP ||
					 _event.MyStatus == CombatStatus::LEVITATE || _event.MyStatus == CombatStatus::PALSY)) _event.MyCancel = true;
				break;
			case EquipmentEffectKind::HEAL_SHIELD:
				if (_event.MyKind == ContentEventKind::BEFORE_HEAL && source && _event.MyAmount > 0 && !_event.MyHealOptions.MyRegen && !_event.MyHealOptions.MySelf)
				{
					constexpr std::string_view key = "item:shield_drone";
					const auto& buffs = Unit(_event.MyTarget).MyBuffs;
					const auto buff = std::ranges::find(buffs, key, [](const CombatBuff& _buff) -> const std::string& { return _buff.MyDefinition.MyKey; });
					const auto hits = buff == buffs.end() ? 0 : buff->MyDefinition.MyShield.MyHits;
					if (hits < static_cast<int>(p.MyMaximum) && chance())
						(void)AddBuff(_event.MyTarget, BuffDefinition{.MyKey = std::string(key), .MyShield = Shield{.MyHits = hits + 1}});
				}
				break;
			case EquipmentEffectKind::REVIVE:
				if (_event.MyKind == ContentEventKind::DEATH && own && _event.MyRemovalReason == RemovalReason::KILLED &&
					runtime.MyUsed < p.MyMaximum && Redeploy(unit.MyId)) { ++runtime.MyUsed; _event.MyRevived = true; }
				break;
			case EquipmentEffectKind::WEAKNESS:
				if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && source) RetypeWeakness(_event.MyDamage, unit.MyId, _event.MyTarget);
				break;
			}
		}
	}
}
