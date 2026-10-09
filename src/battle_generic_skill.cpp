#include <stronghold/simulation/battle.hpp>
#include <tuple>

namespace Stronghold
{
	namespace
	{
		struct GenericScratchGuard
		{
			std::size_t& MyDepth;

			~GenericScratchGuard() { --MyDepth; }
		};
	}

	void Battle::GenericEnemies(UnitId _source, std::vector<UnitId>& _targets, unsigned _limit, bool _sort)
	{
		const auto& source = Unit(_source);
		const AttackProfile filter{.MyCanHitFlying = true};
		_targets.clear(); _targets.reserve(_MyEnemyIds.size());
		for (const auto key : RuleRangeKeys(source))
			for (const auto id : _MyEnemyIds)
			{
				const auto& enemy = Unit(id);
				if (!TargetableEnemy(enemy, filter)) continue;
				const auto tiles = BodyTiles(enemy);
				if (key / FieldColumns >= tiles.MyFirstRow && key / FieldColumns <= tiles.MyLastRow &&
					key % FieldColumns >= tiles.MyFirstColumn && key % FieldColumns <= tiles.MyLastColumn && !std::ranges::contains(_targets, id)) _targets.push_back(id);
			}
		if (!_sort) return;
		SortOperatorTargets(_source, _targets, _limit);
	}

	void Battle::SortOperatorTargets(UnitId _source, std::vector<UnitId>& _targets, unsigned _limit, const AttackProfile* _profile)
	{
		const auto& source = Unit(_source);
		const auto priorityKind = _profile ? _profile->MyPriority : source.MyDefinition.MyAttack.MyPriority;
		const auto origin = _profile && !source.MySkill.MyActive ? source.MyPosition : RulePosition(source);
		_MyTargetCandidates.clear();
		for (const auto id : _targets)
		{
			const auto& enemy = Unit(id); double priority = 0;
			switch (priorityKind)
			{
			case TargetPriority::FLYING: priority = enemy.Flying() ? 0 : 1; break;
			case TargetPriority::LOW_DEFENSE: priority = enemy.MyStats.MyDefense; break;
			case TargetPriority::HIGH_DEFENSE: priority = -enemy.MyStats.MyDefense; break;
			case TargetPriority::LOWEST_HEALTH: priority = enemy.MyHealth; break;
			case TargetPriority::HIGHEST_HEALTH: priority = -enemy.MyHealth; break;
			case TargetPriority::NEAREST: priority = BodyDistance(enemy, origin); break;
			case TargetPriority::FARTHEST: priority = -BodyDistance(enemy, origin); break;
			case TargetPriority::RANGED: priority = enemy.MyDefinition.MyAttack.MyRanged && enemy.MyDefinition.MyAttack.MyEnemyRange > 0 ? 0 : 1; break;
			case TargetPriority::LOWEST_HEALTH_RATIO: priority = enemy.MyHealth / enemy.MyStats.MyMaxHealth; break;
			case TargetPriority::HIGHEST_ATTACK: priority = -enemy.MyStats.MyAttack; break;
			case TargetPriority::BOSS: priority = enemy.MySpawnTag == EnemySpawnTag::BOSS ? 0 : 1; break;
			case TargetPriority::ELITE: priority = enemy.MySpawnTag == EnemySpawnTag::BOSS || enemy.MyDefinition.MyLeader || enemy.MyDefinition.MyElite ? 0 : 1; break;
			case TargetPriority::NOT_BURST: priority = enemy.MyStatuses.Has(CombatStatus::BURST_LOCK) ? 1 : 0; break;
			case TargetPriority::GROUND: priority = enemy.Flying() ? 1 : 0; break;
			case TargetPriority::HEAVIEST: priority = -enemy.MyStats.MyMass; break;
			default: break;
			}
			_MyTargetCandidates.push_back({id, enemy.MyBlockedBy == _source ? 0 : 1, priority, -enemy.MyStats.MyTaunt, RemainingDistance(id), enemy.MySpawnSequence});
		}
		const auto selected = SelectTargets(_limit ? _limit : _targets.size());
		_targets.assign(selected.begin(), selected.end());
	}

	void Battle::GenericElement(UnitId _source, UnitId _target, double _dealt)
	{
		const auto& source = Unit(_source); const auto& effect = *source.MyDefinition.MyGenericSkill;
		if (!effect.MyElement || !_target || !Unit(_target).MyAlive) return;
		const auto amount = (effect.MyElementOfDamage ? _dealt : source.MyStats.MyAttack) * effect.MyElementRatio;
		if (amount > 0) (void)DealElement(_source, _target, {.MyElement = *effect.MyElement, .MyAmount = amount, .MyTags = static_cast<DamageTags>(DamageTag::SKILL)});
	}

	void Battle::GenericDebuff(UnitId _source, UnitId _target, double _duration)
	{
		const auto& effect = *Unit(_source).MyDefinition.MyGenericSkill;
		if (!_target || !Unit(_target).MyAlive || effect.MyDebuff.empty()) return;
		(void)AddBuff(_target, {.MyKey = std::string(effect.MyDebuffKey), .MySource = _source, .MyDuration = _duration,
			.MyRefresh = BuffRefresh::EXTEND, .MyModifiers = std::vector<AttributeChange>(effect.MyDebuff.begin(), effect.MyDebuff.end())});
	}

	void Battle::GenericHeal(UnitId _source)
	{
		const auto& source = Unit(_source); const auto& effect = *source.MyDefinition.MyGenericSkill;
		auto& scratch = AcquireAttackScratch(); const GenericScratchGuard guard{_MyAttackDepth};
		const auto eligible = [&](const CombatUnit& ally)
		{
			return ally.MyAlive && !ally.MyHidden && ally.MyKind != UnitKind::DEVICE && (!effect.MyHealOthersOnly || ally.MyId != _source) &&
				(ally.MyId == _source || (!ally.MyStatuses.Has(CombatStatus::NO_HEAL) && !ally.MyDefinition.MyAttack.MyNoHeal));
		};
		for (const auto id : _MyAllyIds)
			if (const auto& ally = Unit(id); eligible(ally) && ally.MyHealth < ally.MyStats.MyMaxHealth - 1e-6 && InRuleRange(_source, id)) scratch.MyTargets.push_back(id);
		std::ranges::sort(scratch.MyTargets, [&](UnitId a, UnitId b)
		{
			const auto& left = Unit(a); const auto& right = Unit(b);
			return std::tuple{left.MyHealth / left.MyStats.MyMaxHealth, left.MyDeploySequence} < std::tuple{right.MyHealth / right.MyStats.MyMaxHealth, right.MyDeploySequence};
		});
		if (effect.MyHealAll)
		{
			for (const auto id : scratch.MyTargets) (void)Heal(_source, id, source.MyStats.MyAttack * effect.MyHealAlly);
			return;
		}
		UnitId best = scratch.MyTargets.empty() ? 0 : scratch.MyTargets.front();
		if (!best)
			for (const auto id : _MyAllyIds)
			{
				const auto& ally = Unit(id);
				if (!eligible(ally) || ally.MyHealth >= ally.MyStats.MyMaxHealth || Distance(RulePosition(source), RulePosition(ally)) > 1.5 + 1e-9) continue;
				if (!best || ally.MyHealth / ally.MyStats.MyMaxHealth < Unit(best).MyHealth / Unit(best).MyStats.MyMaxHealth) best = id;
			}
		if (best) (void)Heal(_source, best, source.MyStats.MyAttack * effect.MyHealAlly);
	}

	void Battle::GenericHit(UnitId _source, UnitId _target, double _dealt)
	{
		const auto& source = Unit(_source);
		if (!source.MyDefinition.MyGenericSkill) return;
		const auto& effect = *source.MyDefinition.MyGenericSkill;
		if (effect.MyHealAlly > 0) GenericHeal(_source);
		if (_target && Unit(_target).MyAlive)
		{
			if (effect.MyHitElement) GenericElement(_source, _target, _dealt);
			if (!effect.MyHitStatuses.empty() && (!(effect.MyProbability > 0 && effect.MyProbability < 1) || _MyRandom.Next() < effect.MyProbability))
				for (const auto& status : effect.MyHitStatuses) (void)ApplyStatus(_target, status.MyStatus, status.MyDuration, _source);
			if (effect.MyForce && Unit(_target).MyAlive && Unit(_target).MySide == UnitSide::ENEMY)
			{
				// 位移的路径和落点仍使用实时身体坐标。
				if (effect.MyPull) (void)PullToFront(_target, _source, *effect.MyForce);
				else
				{
					const auto fwd = RotateOffset(RangeOffset{0, 1}, source.MyFacing);
					(void)Push(_target, *effect.MyForce, {.MyFrom = source.MyPosition,
						.MyDirection = effect.MyDirectional ? WorldPoint{static_cast<double>(fwd.MyColumn), static_cast<double>(fwd.MyRow)} : WorldPoint{},
						.MyFromFacing = source.MyFacing, .MyEffect = effect.MyEffectPush});
				}
			}
		}
		if (!effect.MyAura) GenericDebuff(_source, _target, effect.MyDebuffDuration);
	}

	void Battle::GenericBurst(UnitId _source, double _scale)
	{
		const auto& source = Unit(_source); const auto& effect = *source.MyDefinition.MyGenericSkill;
		auto& scratch = AcquireAttackScratch(); const GenericScratchGuard guard{_MyAttackDepth};
		GenericEnemies(_source, scratch.MyTargets);
		for (const auto id : scratch.MyTargets)
		{
			const auto dealt = DealDamage(_source, id, {.MyAmount = source.MyStats.MyAttack * _scale, .MyType = effect.MyBurstType.value_or(source.MyDefinition.MyAttack.MyDamageType),
				.MyTags = DamageTag::SKILL | DamageTag::BURST, .MyIsSkill = true});
			GenericElement(_source, id, dealt);
		}
	}

	void Battle::NotifyGenericSkill(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::SKILL_START && _event.MyKind != ContentEventKind::SKILL_ENDING &&
			_event.MyKind != ContentEventKind::SKILL_TICK && _event.MyKind != ContentEventKind::BUFF_TICK && _event.MyKind != ContentEventKind::DAMAGED) return;
		const auto id = _event.MyKind == ContentEventKind::DAMAGED ? _event.MyTarget : _event.MyUnit;
		if (!id) return;
		auto& unit = _MyUnits[Index(id)];
		if (!unit.MyDefinition.MyGenericSkill) return;
		const auto& effect = *unit.MyDefinition.MyGenericSkill;
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			if (effect.MyDp > 0) (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, effect.MyDp);
			const auto loss = unit.MyHealth * effect.MyLoseHp;
			if (loss > 0 && unit.MyHealth - loss >= 1) (void)LoseHealth(id, id, loss);
			if (effect.MyHealHp > 0) (void)Heal(id, id, unit.MyStats.MyMaxHealth * effect.MyHealHp, {.MySelf = true});
			if (effect.MyShield > 0)
			{
				const auto total = unit.MyStats.MyMaxHealth * effect.MyShield;
				unit.MyGenericShieldLoss = effect.MyShieldDecay ? total * 0.5 / effect.MyShieldDuration : 0;
				unit.MyGenericShieldBuff = AddBuff(id, {.MyKey = std::string(effect.MyShieldKey),
					.MyDuration = effect.MyShieldDuration > 0 ? effect.MyShieldDuration : std::numeric_limits<double>::infinity(),
					.MyInterval = effect.MyShieldDecay ? 0.5 : 0, .MyShield = {.MyHealth = total}, .MyNotifyTick = effect.MyShieldDecay});
			}
			if (effect.MyStartBurst > 0) GenericBurst(id, effect.MyStartBurst);
			if (!effect.MyStartStatuses.empty())
			{
				auto& scratch = AcquireAttackScratch(); const GenericScratchGuard guard{_MyAttackDepth};
				GenericEnemies(id, scratch.MyTargets, effect.MyStartTargets);
				for (const auto target : scratch.MyTargets) for (const auto& status : effect.MyStartStatuses) (void)ApplyStatus(target, status.MyStatus, status.MyDuration, id);
			}
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING && unit.MyAlive && _event.MySkillReason != SkillReason::DEATH)
		{
			if (effect.MyEndBurst > 0) GenericBurst(id, effect.MyEndBurst);
			if (effect.MySelfStun > 0) (void)ApplyStatus(id, CombatStatus::STUN, effect.MySelfStun, id);
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK && effect.MyAura)
		{
			auto& scratch = AcquireAttackScratch(); const GenericScratchGuard guard{_MyAttackDepth};
			GenericEnemies(id, scratch.MyTargets, 0, false);
			for (const auto target : scratch.MyTargets) GenericDebuff(id, target, 0.5);
		}
		else if (_event.MyKind == ContentEventKind::BUFF_TICK && _event.MyBuff == unit.MyGenericShieldBuff)
		{
			const auto found = std::ranges::find(unit.MyBuffs, _event.MyBuff, &CombatBuff::MyId);
			if (found != unit.MyBuffs.end()) found->MyDefinition.MyShield.MyHealth = std::max(0.0, found->MyDefinition.MyShield.MyHealth - unit.MyGenericShieldLoss);
		}
		else if (_event.MyKind == ContentEventKind::DAMAGED && effect.MyCounter && unit.MyAlive && unit.MySkill.MyActive &&
			Time() >= unit.MyGenericCounterReadyAt && _event.MySource && Unit(_event.MySource).MySide == UnitSide::ENEMY && _event.MyDamage.MyIsAttack)
		{
			unit.MyGenericCounterReadyAt = Time() + effect.MyCounterCooldown;
			const auto amount = (effect.MyCounterDefense ? unit.MyStats.MyDefense : unit.MyStats.MyAttack) * effect.MyCounterScale;
			auto& scratch = AcquireAttackScratch(); const GenericScratchGuard guard{_MyAttackDepth};
			if (effect.MyCounterAround)
			{
				const AttackProfile filter{.MyCanHitFlying = true, .MyHitSleep = true, .MyGroundOnly = effect.MyCounterGroundOnly};
				for (const auto enemy : _MyEnemyIds) if (TargetableEnemy(Unit(enemy), filter) && BodyDistance(Unit(enemy), RulePosition(unit)) <= 1.5 + 1e-9) scratch.MyTargets.push_back(enemy);
			}
			else if (Unit(_event.MySource).MyAlive) scratch.MyTargets.push_back(_event.MySource);
			for (const auto target : scratch.MyTargets)
			{
				const auto dealt = amount > 0 ? DealDamage(id, target, {.MyAmount = amount, .MyType = effect.MyCounterType, .MyCanDodge = false,
					.MyTags = static_cast<DamageTags>(DamageTag::COUNTER), .MyIsSkill = true}) : 0;
				GenericElement(id, target, dealt);
			}
		}
	}
}
