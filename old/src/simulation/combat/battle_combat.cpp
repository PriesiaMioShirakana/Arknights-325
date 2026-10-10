#include <algorithm>
#include <limits>
#include "battle_core.hpp"
#include <tuple>

namespace Stronghold
{
	bool BattleCore::InRange(const CombatUnit& _attacker, const CombatUnit& _target) const
	{
		if (_attacker.MySkill.MyActive) return InRuleRange(_attacker.MyId, _target.MyId);
		return BodyInRange(_target, _attacker.MyRangeMask);
	}

	std::span<const UnitId> BattleCore::SelectTargets(std::size_t _limit)
	{
		const auto key = [](const TargetCandidate& _candidate)
		{
			return std::tie(_candidate.MyBlockedPriority, _candidate.MyPriority, _candidate.MyTaunt, _candidate.MyDistance, _candidate.MySequence);
		};
		const auto count = std::min(_limit, _MyTargetCandidates.size());
		std::ranges::partial_sort(
			_MyTargetCandidates,
			_MyTargetCandidates.begin() + static_cast<std::ptrdiff_t>(count),
			[&](const TargetCandidate& _a, const TargetCandidate& _b) { return key(_a) < key(_b); }
		);
		_MyTargets.clear();
		for (std::size_t i = 0; i < count; ++i)
			_MyTargets.emplace_back(_MyTargetCandidates[i].MyId);
		return _MyTargets;
	}

	std::span<const UnitId> BattleCore::AllyTargets(CombatUnit& _unit)
	{
		_MyTargetCandidates.clear();
		const auto& profile = EffectiveAttack(_unit);
		// 炮手的阻挡目标只取一个，忽略通常的额外目标；近战形态仍保留其溅射。
		if (!profile.MyHealing && profile.MyFortress)
		{
			_unit.MyProfession.MyFortressMelee = !_unit.MyBlocking.empty();
			if (_unit.MyProfession.MyFortressMelee)
			{
				_MyTargets.clear();
				for (const auto id : _unit.MyBlocking)
					if (TargetableEnemy(Unit(id), profile)) { _MyTargets.emplace_back(id); break; }
				return _MyTargets;
			}
		}
		if (profile.MyHealing)
		{
			for (std::size_t i = 0; i < _MyAllyIds.size(); ++i)
			{
				const auto& ally = _MyUnits[Index(_MyAllyIds[i])];
				// 原 injuredAlliesInKeys 只在选目标时检查 noHeal；healFree 留给治疗管线，isolated 留给技能自己的选择器。
				if (ally.MyAlive && ally.MyKind != UnitKind::DEVICE && !ally.MyHidden &&
					(ally.MyId == _unit.MyId || (!ally.MyStatuses.Has(CombatStatus::NO_HEAL) && !ally.MyDefinition.MyAttack.MyNoHeal)) && (std::isless(ally.MyHealth, ally.MyStats.MyMaxHealth - 1e-6) ||
						(std::isgreater(profile.MyElementHealRatio, 0) && !ally.MyStatuses.Has(CombatStatus::BURST_LOCK) && std::ranges::any_of(ally.MyElements.MyGauges, [](double _gauge) { return std::isgreater(_gauge, 0); }))) &&
					std::islessequal(ally.MyHealth / ally.MyStats.MyMaxHealth, profile.MyHealHpAtMost + 1e-9) && InRange(_unit, ally))
					_MyTargetCandidates.emplace_back(ally.MyId, 0, ally.MyHealth / ally.MyStats.MyMaxHealth, 0.0, 0.0, ally.MyDeploySequence);
			}
		}
		else
		{
			for (std::size_t i = 0; i < _MyEnemyIds.size(); ++i)
			{
				const auto& enemy = _MyUnits[Index(_MyEnemyIds[i])];
				if (!TargetableEnemy(enemy, profile) || (enemy.MyBlockedBy != _unit.MyId && !
					InRange(_unit, enemy)))
					continue;
				double priority = 0;
				switch (profile.MyPriority)
				{
				case TargetPriority::HIGH_DEFENSE: priority = -enemy.MyStats.MyDefense; break;
				case TargetPriority::RANGED: priority = enemy.MyDefinition.MyAttack.MyRanged && std::isgreater(enemy.MyDefinition.MyAttack.MyEnemyRange, 0) ? 0 : 1; break;
				case TargetPriority::LOWEST_HEALTH_RATIO: priority = enemy.MyHealth / enemy.MyStats.MyMaxHealth; break;
				case TargetPriority::HIGHEST_ATTACK: priority = -enemy.MyStats.MyAttack; break;
				case TargetPriority::BOSS: priority = enemy.MySpawnTag == EnemySpawnTag::BOSS ? 0 : 1; break;
				case TargetPriority::ELITE: priority = enemy.MySpawnTag == EnemySpawnTag::BOSS || enemy.MyDefinition.MyLeader || enemy.MyDefinition.MyElite ? 0 : 1; break;
				case TargetPriority::NOT_BURST: priority = enemy.MyStatuses.Has(CombatStatus::BURST_LOCK) ? 1 : 0; break;
				case TargetPriority::GROUND: priority = enemy.Flying() ? 1 : 0; break;
				case TargetPriority::HEAVIEST: priority = -enemy.MyStats.MyMass; break;
				case TargetPriority::FLYING:
					priority = enemy.Flying() ? 0 : 1;
					break;
				case TargetPriority::LOW_DEFENSE:
					priority = enemy.MyStats.MyDefense;
					break;
				case TargetPriority::LOWEST_HEALTH:
					priority = enemy.MyHealth;
					break;
				case TargetPriority::HIGHEST_HEALTH:
					priority = -enemy.MyHealth;
					break;
				case TargetPriority::NEAREST:
					priority = BodyDistance(enemy, _unit.MySkill.MyActive ? RulePosition(_unit) : _unit.MyPosition);
					break;
				case TargetPriority::FARTHEST:
					priority = -BodyDistance(enemy, _unit.MySkill.MyActive ? RulePosition(_unit) : _unit.MyPosition);
					break;
				default:
					break;
				}
				_MyTargetCandidates.emplace_back(
					enemy.MyId,
					enemy.MyBlockedBy == _unit.MyId ? 0 : 1,
					priority,
					-enemy.MyStats.MyTaunt,
					RemainingDistance(enemy.MyId),
					enemy.MySpawnSequence
				);
			}
		}
		const auto baseTargets = profile.MyHitAllBlocked && !profile.MyHealing ? static_cast<double>(std::max(1, _unit.MyStats.MyBlockCount)) : static_cast<double>(profile.MyMaxTargets);
		const auto extraTargets = profile.MyHealing ? std::max(0.0, std::floor(_unit.MyStats.MyExtraTargets)) : std::floor(_unit.MyStats.MyExtraTargets);
		const auto requested = std::max(1.0, baseTargets + extraTargets);
		// 先限于实际候选数量，再转换整数，避免极端加成产生越界的浮点转整数。
		const auto count = std::min(static_cast<double>(_MyTargetCandidates.size()), requested);
		return SelectTargets(profile.MyAllInRange ? _MyTargetCandidates.size() : static_cast<std::size_t>(count));
	}

	std::span<const UnitId> BattleCore::EnemyTargets(const CombatUnit& _unit)
	{
		_MyTargetCandidates.clear();
		const auto& profile = EffectiveAttack(_unit);
		const auto radius = profile.MyRanged ? profile.MyEnemyRange : 0;
		const auto reach = radius + 0.25;
		for (std::size_t i = 0; i < _MyAllyIds.size(); ++i)
		{
			const auto& ally = _MyUnits[Index(_MyAllyIds[i])];
			if (!ally.MyAlive || ally.MyHidden || (ally.MyKind == UnitKind::DEVICE && ally.MyId != _unit.MyBlockedBy) || ally.MyStatuses.Has(CombatStatus::UNTARGETABLE) ||
				ally.MyStatuses.Has(CombatStatus::SLEEP) ||
				(ally.MyStatuses.Has(CombatStatus::LIFTOFF) && !_unit.Flying()) ||
				((ally.MyStatuses.Has(CombatStatus::STEALTH) || ally.MyStatuses.Has(CombatStatus::CAMOUFLAGE)) && ally.MyId != _unit.MyBlockedBy))
				continue;
			const auto dx = ally.MyPosition.MyX - _unit.MyPosition.MyX;
			const auto dy = ally.MyPosition.MyY - _unit.MyPosition.MyY;
			if (ally.MyId == _unit.MyBlockedBy || (radius > 0 && dx * dx + dy * dy <= reach * reach + 1e-9))
				_MyTargetCandidates.emplace_back(
					ally.MyId,
					ally.MyId == _unit.MyBlockedBy ? 0 : 1,
					0.0,
					-ally.MyStats.MyTaunt,
					0.0,
					std::numeric_limits<std::uint64_t>::max() - ally.MyAggroSequence
				);
		}
		const auto baseTargets = profile.MyHitAllBlocked && !profile.MyHealing ? static_cast<double>(std::max(1, _unit.MyStats.MyBlockCount)) : static_cast<double>(profile.MyMaxTargets);
		const auto extraTargets = profile.MyHealing ? std::max(0.0, std::floor(_unit.MyStats.MyExtraTargets)) : std::floor(_unit.MyStats.MyExtraTargets);
		const auto requested = std::max(1.0, baseTargets + extraTargets);
		// 先限于实际候选数量，再转换整数，避免极端加成产生越界的浮点转整数。
		const auto count = std::min(static_cast<double>(_MyTargetCandidates.size()), requested);
		return SelectTargets(profile.MyAllInRange ? _MyTargetCandidates.size() : static_cast<std::size_t>(count));
	}

	void BattleCore::UpdateAlly(CombatUnit& _unit)
	{
		TickSkill(_unit);
		// 阻挡数降低时先释放最后建立的阻挡；眩晕、缴械和攻击冷却均不能延迟容量约束。
		std::int64_t blockedWeight = 0;
		for (const auto id : _unit.MyBlocking) blockedWeight += Unit(id).MyDefinition.MyBlockWeight;
		while (!_unit.MyBlocking.empty() && blockedWeight > _unit.MyStats.MyBlockCount)
		{
			auto& enemy = _MyUnits[Index(_unit.MyBlocking.back())];
			blockedWeight -= enemy.MyDefinition.MyBlockWeight;
			ReleaseBlock(enemy);
		}
		if (Finished() || !_unit.MyAlive || _unit.MyStatuses.Has(CombatStatus::STUN))
			return;
		if (_unit.MyAttackCooldown > 0)
			_unit.MyAttackCooldown = std::max(0.0, _unit.MyAttackCooldown - BattleClock::StepSeconds);
		if (_unit.MyAttackCooldown > 0 || AttackDisabled(_unit) || _unit.MyStatuses.Has(CombatStatus::DISARM))
			return;
		if (!ProfessionCanAttack(_unit) || !OperatorCanAttack(_unit)) { StoreEnergy(_unit); return; }
		auto targets = AllyTargets(_unit);
		_unit.MyHadAttackTarget = !targets.empty();
		if (targets.empty() && !UsesInitialPosition(_unit)) { StoreEnergy(_unit); return; }
		if (SkillAboutToAttack(_unit))
		{
			if (Finished() || !_unit.MyAlive || AttackDisabled(_unit)) return;
			targets = AllyTargets(_unit);
			if (targets.empty()) { StoreEnergy(_unit); return; }
		}
		if (targets.empty()) { StoreEnergy(_unit); return; }
		Attack(_unit, targets);
		_unit.MyAttackCooldown = _unit.MyStats.AttackInterval();
	}

	bool BattleCore::EnemyAttack(CombatUnit& _unit, double _previousCooldown)
	{
		const auto& definition = _unit.MyDefinition;
		const auto& profile = definition.MyAttack;
		if (profile.MyDisabled || !std::isgreater(_unit.MyStats.MyAttack, 0) || _unit.MyStatuses.Has(CombatStatus::DISARM) || _unit.MyStatuses.Has(CombatStatus::FEAR) || (_unit.MyBlockedBy && _unit.MyStatuses.Has(CombatStatus::TREMBLE)))
		{
			_unit.MySwing = false;
			return false;
		}
		if (profile.MyHealing)
		{
			// 敌方治疗没有攻击前摇或停步，且不会选择自身、隐藏单位或共享血池首领。
			// 禁疗不参与筛选：可能选中禁疗者而治疗失败，仍消费一次攻击间隔，与原版一致。
			if (std::isgreater(_unit.MyAttackCooldown, 0)) return false;
			UnitId best = 0;
			double lowestRatio = 1;
			const auto radius = std::max(1.0, profile.MyEnemyRange);
			for (const auto id : _MyEnemyIds)
			{
				const auto& target = Unit(id);
				if (id == _unit.MyId || !target.MyAlive || target.MyHidden || target.MyDefinition.MySharedBoss ||
					std::isgreaterequal(target.MyHealth, target.MyStats.MyMaxHealth)) continue;
				const auto distance = BodyDistance(target, _unit.MyPosition);
				if (std::isgreater(distance * distance, radius * radius + 1e-9)) continue;
				const auto ratio = target.MyHealth / target.MyStats.MyMaxHealth;
				if (!best || std::isless(ratio, lowestRatio)) { best = id; lowestRatio = ratio; }
			}
			if (best)
			{
				_unit.MyLastAttackAt = Time();
				++_unit.MyTotals.MyAttacks;
				Emit(BattleEventKind::ATTACKED, _unit.MyId, best);
				(void)Heal(_unit.MyId, best, _unit.MyStats.MyAttack);
				_unit.MyAttackCooldown = _unit.MyStats.AttackInterval();
			}
			return false;
		}
		const auto interval = _unit.MyStats.AttackInterval();
		const auto duration = profile.MyAnimationDuration;
		const auto clipSpeed = duration > interval ? duration / interval : 1;
		const auto hit = profile.MyAnimationHit.value_or(duration / 2);
		const auto wind = hit / clipSpeed;
		if (_unit.MyAttackCooldown > wind + 1e-9)
		{
			_unit.MySwing = false;
			return false;
		}
		const auto targets = EnemyTargets(_unit);
		if (targets.empty())
		{
			_unit.MySwing = false;
			return false;
		}
		if (!_unit.MySwing)
		{
			_unit.MySwing = true;
			if (_previousCooldown < wind)
				_unit.MyAttackCooldown = wind;
		}
		if (_unit.MyAttackCooldown > 0)
			return !_unit.MyBlockedBy && profile.MyRanged && profile.MyEnemyRange > 0 && !profile.MyAttackWhileMoving;
		_unit.MySwing = false;
		if (_unit.MyStatuses.Has(CombatStatus::PALSY))
		{
			auto& stacks = _unit.MyStatuses.MyValues[static_cast<std::size_t>(CombatStatus::PALSY)];
			if (std::islessequal(--stacks, 0)) (void)RemoveStatus(_unit.MyId, CombatStatus::PALSY);
			_unit.MyAttackCooldown = interval;
			if (!_unit.MyBlockedBy && profile.MyRanged && !profile.MyAttackWhileMoving) _unit.MyAttackStandUntil = Time() + 0.35;
			return false;
		}
		Attack(_unit, targets);
		_unit.MyAttackCooldown = interval;
		const auto rest = profile.MyAttackWhileMoving ? 0 : duration > 0 ? (duration - hit) / clipSpeed : 0.35;
		_unit.MyAttackStandUntil = Time() + rest;
		return false;
	}


	double BattleCore::DealDamage(UnitId _source, UnitId _target, double _amount, DamageType _type)
	{
		return DealDamage(_source, _target, DamageInfo{.MyAmount = _amount, .MyType = _type});
	}

	double BattleCore::DealDamage(UnitId _source, UnitId _target, const DamageInfo& _damage)
	{
		auto damage = _damage;
		damage.MySequence = ++_MyDamageSequence;
		if (!std::isfinite(damage.MyAmount) || std::isless(damage.MyAmount, 0) ||
			!std::isfinite(damage.MyMultiplier) || std::isless(damage.MyMultiplier, 0) ||
			static_cast<unsigned>(damage.MyType) > static_cast<unsigned>(DamageType::ELEMENTAL))
			throw std::invalid_argument("invalid battle damage");
		if (damage.MyTraitAlly) (void)Index(damage.MyTraitAlly);
		const auto targetIndex = Index(_target);
		if (_source) damage.MyHitSleep |= Unit(_source).MyDefinition.MyAttack.MyHitSleep;
		auto& target = _MyUnits[targetIndex];
		if (!_MyStarted || Finished() || !target.MyAlive || target.MyHidden || target.MyStatuses.Has(CombatStatus::INVULNERABLE) ||
			(target.MyStatuses.Has(CombatStatus::SLEEP) && !damage.MyHitSleep) ||
			(!damage.MySourceless && !damage.MyIgnoreSelect && _source && target.MySide == UnitSide::ALLY &&
				target.MyStatuses.Has(CombatStatus::LIFTOFF) && !Unit(_source).Flying())) return 0;
		if (!_MyContentInstances.empty() || _MyBandHitEffects || _MyEquipmentHitEffects || _MyBondHitEffects || _MyTinmanWither || !_MyGarrisons.empty() || !_MyWolves.empty() || !_MyRosesas.empty() || !_MyGladys.empty() || !_MyCetsyrs.empty() || !_MyEtlchis.empty() || !_MyLemuens.empty() || !_MyYus.empty() || !_MySiege2s.empty() || !_MyHalo2s.empty() || !_MyCellos.empty() || !_MySkadi2s.empty() || !_MyManifolds.empty() || !_MyMlysses.empty() || !_MyRaidians.empty() ||
			target.MyDefinition.MyOperatorKit || (_source && Unit(_source).MyDefinition.MyOperatorKit))
		{
			ContentEvent event{.MyKind = ContentEventKind::BEFORE_DAMAGE, .MySource = damage.MySourceless ? 0 : _source, .MyTarget = _target, .MyDamage = damage, .MyCredit = _source};
			NotifyContent(event);
			if (event.MyCancel || Finished() || !target.MyAlive) return 0;
			damage = event.MyDamage;
			if (!std::isfinite(damage.MyAmount) || std::isless(damage.MyAmount, 0) || !std::isfinite(damage.MyMultiplier) ||
				std::isless(damage.MyMultiplier, 0) || static_cast<unsigned>(damage.MyType) > static_cast<unsigned>(DamageType::ELEMENTAL)) return 0;
			if (target.MyStatuses.Has(CombatStatus::INVULNERABLE) || (target.MyStatuses.Has(CombatStatus::SLEEP) && !damage.MyHitSleep)) return 0;
		}
		const auto type = damage.MyType;
		const auto& stats = target.MyStats;
		if (damage.MyCanDodge && (type == DamageType::PHYSICAL || type == DamageType::ARTS))
		{
			const auto chance = type == DamageType::PHYSICAL ? stats.MyPhysicalDodge : stats.MyArtsDodge;
			if (std::isgreater(chance, 0) && std::isless(_MyRandom.Next(), chance))
			{
				ContentEvent event{.MyKind = ContentEventKind::DODGED, .MySource = _source, .MyTarget = _target, .MyDamage = damage};
				NotifyContent(event);
				return 0;
			}
		}
		static constexpr CombatStats Neutral;
		const auto& source = _source && !damage.MySourceless ? Unit(_source).MyStats : Neutral;
		auto amount = Mitigate(damage.MyAmount, type, Mitigation{
			.MyDefense = stats.MyDefense,
			.MyResistance = stats.MyResistance,
			.MyDefenseIgnorePercent = source.MyDefenseIgnorePercent + damage.MyDefenseIgnorePercent,
			.MyDefenseIgnoreFlat = source.MyDefenseIgnoreFlat + damage.MyDefenseIgnoreFlat,
			.MyResistanceIgnorePercent = source.MyResistanceIgnorePercent + damage.MyResistanceIgnorePercent,
			.MyResistanceIgnoreFlat = source.MyResistanceIgnoreFlat + damage.MyResistanceIgnoreFlat,
			.MyElementalResistance = stats.MyElementalResistance});
		auto multiplier = damage.MyMultiplier * (type == DamageType::ELEMENTAL ? 1 : stats.MyDamageTakenMultiplier);
		multiplier *= source.MyDamageDealtMultiplier * (type == DamageType::PHYSICAL ? source.MyPhysicalDealtMultiplier : type == DamageType::ARTS ? source.MyArtsDealtMultiplier : 1);
		multiplier *= type == DamageType::PHYSICAL ? stats.MyPhysicalTakenMultiplier : type == DamageType::ARTS ? stats.MyArtsTakenMultiplier : type == DamageType::ELEMENTAL ? stats.MyElementalTakenMultiplier : stats.MyTrueTakenMultiplier;
		amount *= multiplier;
		if (!std::isfinite(amount) || !std::isgreater(amount, 0)) amount = 0;
		const bool hitCount = target.MyStatuses.Has(CombatStatus::HIT_COUNT);
		const bool artsCount = target.MyStatuses.Has(CombatStatus::HIT_COUNT_ARTS);
		if (hitCount || artsCount) amount = artsCount && !hitCount && type == DamageType::PHYSICAL ? 0 : 1;
		if (LeaderHitCancelled(amount, _MyInput.MyBossBattle, target.MySpawnTag == EnemySpawnTag::BOSS)) return 0;
		amount = AbsorbShields(target, amount, type);
		if (std::isgreater(amount, 0) && !hitCount && !artsCount)
		{
			ContentEvent event{.MyKind = ContentEventKind::BEFORE_HEALTH_DAMAGE, .MySource = damage.MySourceless ? 0 : _source,
				.MyTarget = _target, .MyAmount = amount, .MyDamage = damage, .MyCredit = _source};
			NotifyContent(event);
			if (std::isfinite(event.MyAmount)) amount = std::clamp(event.MyAmount, 0.0, amount);
		}
		return ApplyHealthLoss(_source, _target, amount, !damage.MyNoSp, damage);
	}

	double BattleCore::Heal(UnitId _source, UnitId _target, double _amount, HealOptions _options)
	{
		if (!std::isfinite(_amount) || std::isless(_amount, 0) || std::isnan(_options.MyOverhealDuration) || std::isless(_options.MyOverhealDuration, 0))
			throw std::invalid_argument("invalid battle healing");
		const auto index = Index(_target);
		if (_source)
			(void)Index(_source);
		auto& target = _MyUnits[index];
		if (!_MyStarted || Finished() || !target.MyAlive || target.MyRemoved || target.MyDefinition.MySharedBoss)
			return 0;
		const bool self = _source == _target || _options.MySelf;
		if (!self && ((target.MyStatuses.Has(CombatStatus::NO_HEAL) && !_options.MyThrough) || target.MyDefinition.MyAttack.MyNoHeal)) return 0;
		if (target.MyStatuses.Has(CombatStatus::HEAL_FREE) && !_options.MyRegen && !_options.MyIgnoreHealFree && !_options.MyThrough) return 0;
		auto amount = _options.MyRegen ? _amount : _amount * (_source ? Unit(_source).MyStats.MyHealingDealtMultiplier : 1) * target.MyStats.MyHealingTakenMultiplier;
		if (!std::isfinite(amount) || !std::isgreater(amount, 0)) return 0;
		ContentEvent before{.MyKind = ContentEventKind::BEFORE_HEAL, .MySource = _source, .MyTarget = _target, .MyAmount = amount, .MyHealOptions = _options};
		NotifyContent(before);
		if ((!_options.MyRegen && before.MyCancel) || !target.MyAlive || Finished()) return 0;
		// 再生仍广播供监听，但监听器不能改变其固定回复量。
		if (!_options.MyRegen) amount = std::isfinite(before.MyAmount) ? std::max(0.0, before.MyAmount) : 0;
		const auto healed = std::max(0.0, std::min(amount, target.MyStats.MyMaxHealth - target.MyHealth));
		target.MyHealth = std::min(target.MyStats.MyMaxHealth, target.MyHealth + healed);
		if (_options.MyOverheal && std::isgreater(amount, healed))
		{
			const auto previous = std::ranges::find(target.MyBuffs, std::string_view("overheal"),
				[](const CombatBuff& _buff) -> std::string_view { return _buff.MyDefinition.MyKey; });
			const auto shield = std::min(target.MyStats.MyMaxHealth, (previous != target.MyBuffs.end() ? previous->MyDefinition.MyShield.MyHealth : 0) + (amount - healed));
			(void)AddBuff(_target, BuffDefinition{.MyKey = "overheal", .MyDuration = _options.MyOverhealDuration,
				.MyShield = Shield{.MyHealth = shield}});
		}
		if (_source)
		{
			auto& source = _MyUnits[Index(_source)];
			source.MyTotals.MyHealing += healed;
			if (source.MySide == UnitSide::ALLY)
				if (const auto owner = CreditOwner(source.MyOwner); owner != NoPlayer) _MyPlayers[owner].MyHealing += healed;
		}
		if (!_options.MySilent && std::isgreaterequal(healed, 0.5)) Emit(BattleEventKind::HEALED, _source, _target, healed);
		ContentEvent event{.MyKind = ContentEventKind::HEALED, .MySource = _source, .MyTarget = _target, .MyAmount = healed, .MyHealOptions = _options};
		NotifyContent(event);
		return healed;
	}

	void BattleCore::AddShield(UnitId _target, Shield _shield)
	{
		const auto index = Index(_target);
		if (!std::isfinite(_shield.MyHealth) || _shield.MyHealth < 0 || _shield.MyHits < 0 || _shield.MyTypeMask > 15)
			throw std::invalid_argument("invalid battle shield");
		if (_MyStarted && !Finished() && _MyUnits[index].MyAlive)
			_MyUnits[index].MyShields.emplace_back(_shield);
	}

	void BattleCore::Kill(CombatUnit& _unit, UnitId _source)
	{
		if (!_unit.MyAlive) return;
		_unit.MyHealth = 0;
		ContentEvent before{.MyKind = ContentEventKind::BEFORE_KILL, .MyUnit = _unit.MyId, .MySource = _source, .MyTarget = _unit.MyId};
		NotifyContent(before);
		if (!_unit.MyAlive) { _unit.MyHealth = 0; return; }
		if (std::isgreater(_unit.MyHealth, 0)) { _unit.MyHealth = std::min(_unit.MyHealth, _unit.MyStats.MyMaxHealth); return; }
		RemoveUnit(_unit, RemovalReason::KILLED, _source);
	}
}
