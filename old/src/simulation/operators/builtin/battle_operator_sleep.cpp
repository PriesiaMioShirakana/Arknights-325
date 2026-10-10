#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct SleepScratchGuard
		{
			std::size_t& MyDepth;

			~SleepScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::FoesInRadius(WorldPoint _origin, double _radius, std::vector<UnitId>& _targets, bool _center) const
	{
		_targets.clear(); _targets.reserve(_MyEnemyIds.size());
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (!enemy.MyAlive || enemy.MyHidden || enemy.MyStatuses.Has(CombatStatus::UNTARGETABLE) || EnemyStealthed(enemy)) continue;
			const auto distance = _center ? Distance(_origin, enemy.MyPosition) : BodyDistance(enemy, _origin);
			if (std::islessequal(distance * distance, _radius * _radius + 1e-9)) _targets.push_back(id);
		}
	}

	void BattleCore::TitiWard(UnitId _unit, UnitId _target, bool _fatal)
	{
		auto& target = _MyUnits[Index(_target)];
		const bool entered = !target.MyStatuses.Has(CombatStatus::SLEEP);
		StatusFlags flags; flags.set(static_cast<std::size_t>(CombatStatus::SLEEP));
		(void)AddBuff(_target, {.MyKey = _fatal ? "titi:allySleep" : "titi:ward", .MySource = _unit, .MyFlags = flags,
			.MyInterval = _fatal ? 0.2 : 0, .MyNotifyTick = _fatal, .MyStatus = CombatStatus::SLEEP});
		if (!_fatal) ReleaseBlocked(target);
		ContentEvent status{.MyKind = ContentEventKind::STATUS_APPLIED, .MySource = _unit, .MyTarget = _target,
			.MyAmount = std::numeric_limits<double>::infinity(), .MyStatus = CombatStatus::SLEEP,
			.MyApplication = {.MySource = _unit}, .MyStatusEntered = entered};
		NotifyContent(status);
	}

	void BattleCore::TitiSleepOthers(UnitId _unit, UnitId _from, const TitiKit& _kit)
	{
		auto& scratch = AcquireAttackScratch(); const SleepScratchGuard guard{.MyDepth = _MyAttackDepth};
		const auto origin = Unit(_from).MyPosition;
		FoesInRadius(origin, _kit.MyRadius, scratch.MyTargets);
		std::erase_if(scratch.MyTargets, [&](UnitId _id) { return _id == _from || Unit(_id).MyStatuses.Has(CombatStatus::SLEEP) || Unit(_id).MyCandleOwner; });
		std::ranges::sort(scratch.MyTargets, [&](UnitId _a, UnitId _b)
		{
			const auto& a = Unit(_a); const auto& b = Unit(_b);
			if (std::islessgreater(a.MyStats.MyTaunt, b.MyStats.MyTaunt)) return std::isgreater(a.MyStats.MyTaunt, b.MyStats.MyTaunt);
			const auto da = Distance(a.MyPosition, origin), db = Distance(b.MyPosition, origin);
			return std::islessgreater(da, db) ? std::isless(da, db) : a.MySpawnSequence < b.MySpawnSequence;
		});
		unsigned count = 0;
		for (const auto id : scratch.MyTargets)
		{
			if (count >= _kit.MyChainTargets) break;
			if (ApplyStatus(id, CombatStatus::SLEEP, _kit.MyChainSleep, _unit)) ++count;
		}
	}

	void BattleCore::TitiObserve(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::BUFF_TICK && _event.MyUnit)
		{
			const auto& target = Unit(_event.MyUnit);
			const auto found = std::ranges::find(target.MyBuffs, _event.MyBuff, &CombatBuff::MyId);
			if (found != target.MyBuffs.end() && found->MyDefinition.MyKey == "titi:allySleep" && std::isgreaterequal(target.MyHealth, target.MyStats.MyMaxHealth - 1e-6))
				(void)RemoveBuff(target.MyId, "titi:allySleep");
			return;
		}
		if (_event.MyKind != ContentEventKind::STATUS_APPLIED && _event.MyKind != ContentEventKind::TICK && _event.MyKind != ContentEventKind::BEFORE_KILL && _event.MyKind != ContentEventKind::FATAL) return;
		for (std::size_t n = 0, count = _MyTitis.size(); n < count; ++n)
		{
			auto& unit = _MyUnits[Index(_MyTitis[n])]; const auto& kit = std::get<TitiKit>(*unit.MyDefinition.MyOperatorKit);
			if (unit.MyOperatorHooksReleased || kit.MySkill != TitiSkillKind::BLOOM) continue;
			if (_event.MyKind == ContentEventKind::STATUS_APPLIED && _event.MyStatus == CombatStatus::SLEEP && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
			{
				if (std::ranges::find(unit.MySleepStarts, _event.MyTarget, &TimedTargetMark::MyTarget) == unit.MySleepStarts.end())
					unit.MySleepStarts.push_back({.MyTarget = _event.MyTarget, .MyTime = Time()});
			}
			else if (_event.MyKind == ContentEventKind::TICK)
			{
				for (std::size_t i = 0; i < unit.MySleepStarts.size();)
				{
					const auto mark = unit.MySleepStarts[i]; const auto& target = Unit(mark.MyTarget);
					if (target.MyAlive && target.MyStatuses.Has(CombatStatus::SLEEP)) { ++i; continue; }
					unit.MySleepStarts.erase(unit.MySleepStarts.begin() + static_cast<std::ptrdiff_t>(i));
					if (!target.MyAlive || !unit.MyAlive || !unit.MySkill.MyActive) continue;
					const auto scale = kit.MyMinScale + (kit.MyMaxScale - kit.MyMinScale) * std::min(1.0, (Time() - mark.MyTime) / std::max(0.1, kit.MyChainSleep));
					(void)DealDamage(unit.MyId, mark.MyTarget, {.MyAmount = unit.MyStats.MyAttack * scale, .MyType = DamageType::ARTS,
						.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
					TitiSleepOthers(unit.MyId, mark.MyTarget, kit);
				}
			}
			else if (_event.MyKind == ContentEventKind::BEFORE_KILL && _event.MyUnit && Unit(_event.MyUnit).MySide == UnitSide::ENEMY)
			{
				const auto erased = std::erase_if(unit.MySleepStarts, [&](const TimedTargetMark& _mark) { return _mark.MyTarget == _event.MyUnit; });
				if (erased && unit.MyAlive && unit.MySkill.MyActive) TitiSleepOthers(unit.MyId, _event.MyUnit, kit);
			}
			else if (_event.MyKind == ContentEventKind::FATAL && !_event.MyPrevented && _event.MyUnit && _event.MyUnit != unit.MyId)
			{
				const auto& target = Unit(_event.MyUnit);
				if (target.MySide != UnitSide::ALLY) continue;
				const auto found = std::ranges::find(target.MyBuffs, "titi:allySleep", [](const CombatBuff& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
				if (found != target.MyBuffs.end()) { if (found->MyDefinition.MySource == unit.MyId) _event.MyPrevented = true; continue; }
				if (target.MyKind != UnitKind::OPERATOR || !unit.MyAlive || !unit.MySkill.MyActive || !InRuleRange(unit.MyId, target.MyId)) continue;
				_event.MyPrevented = true; TitiWard(unit.MyId, target.MyId, true);
			}
		}
	}

	void BattleCore::TitiPulse(UnitId _unit, bool _dream)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<TitiKit>(*unit.MyDefinition.MyOperatorKit);
		if (_dream)
		{
			const auto amount = unit.MyStats.MyAttack * kit.MyDreamScale * (unit.MySkill.MyActive && kit.MySkill == TitiSkillKind::WARD ? kit.MyTalentScale : 1);
			for (std::size_t i = 0, count = _MyEnemyIds.size(); i < count; ++i)
			{
				const auto id = _MyEnemyIds[i]; const auto& enemy = Unit(id);
				if (enemy.MyAlive && !enemy.MyHidden && enemy.MyStatuses.Has(CombatStatus::SLEEP))
					(void)DealDamage(_unit, id, {.MyAmount = amount, .MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::TALENT)});
			}
		}
		else for (std::size_t i = 0, count = _MyAllyIds.size(); i < count; ++i)
		{
			const auto& ally = Unit(_MyAllyIds[i]); const auto& identity = ally.MyDefinition.MyIdentity;
			if (ally.MyAlive && ally.MyKind == UnitKind::OPERATOR && std::isgreater(ally.MyHealth / ally.MyStats.MyMaxHealth, kit.MyHealthThreshold) &&
				(identity.MyNationId == "sargon" || identity.MyNationId == "minos" || std::ranges::contains(identity.MyBonds, "sargonShip")))
				(void)AddBuff(ally.MyId, {.MyKey = "titi:vigor", .MyDuration = 0.5, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = kit.MyAuraSpeed}}});
		}
	}

	void BattleCore::TitiSkill(UnitId _unit, const TitiKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyTarget && _event.MyDamage.MyIsAttack && !_event.MySplash && !_event.MyElement)
		{
			const auto& enemy = Unit(_event.MyTarget);
			if (enemy.MyAlive && enemy.MySide == UnitSide::ENEMY && (enemy.MyBlockedBy || !enemy.MyMoving || enemy.MyStatuses.Has(CombatStatus::STUN) || enemy.MyStatuses.Has(CombatStatus::NO_MOVE) || !std::isgreater(enemy.MyStats.MyMoveSpeed, 0)))
				(void)DealDamage(_unit, enemy.MyId, {.MyAmount = unit.MyStats.MyAttack * _kit.MyStillScale * (unit.MySkill.MyActive && _kit.MySkill == TitiSkillKind::WARD ? _kit.MyTalentScale : 1),
					.MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::TALENT)});
		}
		if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			const auto key = _kit.MySkill == TitiSkillKind::WARD ? "titi:ward" : "titi:allySleep";
			const std::span<const UnitId> targets = _kit.MySkill == TitiSkillKind::WARD ? std::span<const UnitId>(unit.MySleepWards) : std::span<const UnitId>(_MyAllyIds);
			for (const auto id : targets)
				if (std::ranges::any_of(Unit(id).MyBuffs, [&](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == key && _buff.MyDefinition.MySource == _unit; })) (void)RemoveBuff(id, key);
			unit.MySleepWards.clear();
		}
		if (_kit.MySkill != TitiSkillKind::WARD) return;
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			auto& scratch = AcquireAttackScratch(); const SleepScratchGuard guard{.MyDepth = _MyAttackDepth};
			OperatorAlliesInRange(_unit, scratch.MyTargets);
			UnitId pick = 0; double lowest = std::numeric_limits<double>::infinity();
			for (const auto id : scratch.MyTargets)
			{
				const auto& ally = Unit(id);
				if (id == _unit || ally.MyKind != UnitKind::OPERATOR || !std::isgreater(ally.MyHealth, 0)) continue;
				const auto ratio = ally.MyHealth / ally.MyStats.MyMaxHealth;
				if (std::isless(ratio, lowest) || (!std::islessgreater(ratio, lowest) && id < pick)) { pick = id; lowest = ratio; }
			}
			unit.MySleepWards.assign(1, _unit); if (pick) unit.MySleepWards.push_back(pick);
			for (const auto id : unit.MySleepWards) TitiWard(_unit, id, false);
			unit.MyWardAccumulator = 0.25;
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK)
		{
			unit.MyWardAccumulator += _event.MyDelta;
			if (std::isless(unit.MyWardAccumulator, 0.25 - 1e-9)) return;
			unit.MyWardAccumulator = 0;
			auto& scratch = AcquireAttackScratch(); const SleepScratchGuard guard{.MyDepth = _MyAttackDepth};
			for (const auto id : unit.MySleepWards)
			{
				const auto& ward = Unit(id);
				if (!ward.MyAlive || !std::ranges::any_of(ward.MyBuffs, [](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == "titi:ward"; })) continue;
				FoesInRadius(RulePosition(ward), 1.5, scratch.MyTargets);
				for (const auto target : scratch.MyTargets)
				{
					const auto& enemy = Unit(target);
					const bool longer = std::isgreater(enemy.MyStatuses.MyRemaining[static_cast<std::size_t>(CombatStatus::SLEEP)], 0.5 + 1e-9) ||
						std::ranges::any_of(enemy.MyBuffs, [](const CombatBuff& _buff) { return _buff.MyDefinition.MyStatus == CombatStatus::SLEEP && std::isgreater(_buff.MyRemaining, 0.5 + 1e-9); });
					const bool reenter = !std::ranges::contains(scratch.MySeen, target) && !longer;
					scratch.MySeen.push_back(target);
					(void)ApplyStatus(target, CombatStatus::SLEEP, {.MyDuration = 0.5, .MySource = _unit, .MyReenter = reenter});
				}
			}
		}
	}
}
