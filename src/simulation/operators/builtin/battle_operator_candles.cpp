#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct CandleScratchGuard
		{
			std::size_t& MyDepth;

			~CandleScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::CrowdAttackSpeed(UnitId _unit, std::string_view _key, double _speed, double _count)
	{
		const auto& unit = Unit(_unit); const auto& profile = EffectiveAttack(unit);
		const auto count = std::ranges::count_if(_MyEnemyIds, [&](UnitId _id) { return TargetableEnemy(Unit(_id), profile) && InRuleRange(_unit, _id); });
		if (std::isgreaterequal(static_cast<double>(count), _count))
			(void)AddBuff(_unit, {.MyKey = std::string(_key), .MyDuration = 0.3, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = _speed}}});
	}

	void BattleCore::RedirectCandles(UnitId _unit, std::vector<UnitId>& _targets)
	{
		if (Unit(_unit).MySide != UnitSide::ALLY || _MyEtlchis.empty()) return;
		for (const auto owner : _MyEtlchis)
		{
			if (owner == _unit || Unit(owner).MyOperatorHooksReleased || !std::ranges::any_of(_targets, [&](UnitId _id) { return Unit(_id).MyCandleOwner == owner; })) continue;
			auto& scratch = AcquireAttackScratch(); const CandleScratchGuard guard{.MyDepth = _MyAttackDepth};
			for (const auto id : _targets)
			{
				const auto& target = Unit(id);
				if (target.MyCandleOwner != owner)
				{ if (!std::ranges::contains(scratch.MyTargets, id)) scratch.MyTargets.push_back(id); continue; }
				const auto original = target.MyCandleOriginal;
				if (original && Unit(original).MyAlive && !std::ranges::contains(scratch.MyTargets, original) && !std::ranges::contains(_targets, original)) scratch.MyTargets.push_back(original);
			}
			_targets.assign(scratch.MyTargets.begin(), scratch.MyTargets.end());
		}
	}

	void BattleCore::EtlchiCandles(UnitId _unit, const EtlchiKit& _kit)
	{
		auto& unit = _MyUnits[Index(_unit)]; unit.MyCandles.clear();
		auto& scratch = AcquireAttackScratch(); const CandleScratchGuard guard{.MyDepth = _MyAttackDepth};
		const AttackProfile filter{.MyCanHitFlying = false};
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (TargetableEnemy(enemy, filter) && InRuleRange(_unit, id) && !enemy.Flying() && !enemy.MyCandleOwner && !enemy.MyDefinition.MyLeader && enemy.MySpawnTag != EnemySpawnTag::BOSS) scratch.MyTargets.push_back(id);
		}
		std::ranges::sort(scratch.MyTargets, [&](UnitId _a, UnitId _b)
		{
			const auto& a = Unit(_a); const auto& b = Unit(_b);
			return std::islessgreater(a.MyHealth, b.MyHealth) ? std::isgreater(a.MyHealth, b.MyHealth) : a.MySpawnSequence < b.MySpawnSequence;
		});
		if (scratch.MyTargets.size() > _kit.MyCandles) scratch.MyTargets.resize(_kit.MyCandles);
		for (const auto id : scratch.MyTargets)
		{
			const auto& enemy = Unit(id); const auto point = enemy.MyPosition;
			CombatDefinition definition{.MyId = std::string(_kit.MyCandleId), .MyStats = {.MyMaxHealth = std::max(1.0, enemy.MyHealth * _kit.MyCandleHealth),
				.MyDefense = enemy.MyStats.MyDefense * _kit.MyCandleDefense, .MyResistance = enemy.MyStats.MyResistance * _kit.MyCandleResistance,
				.MyMoveSpeed = 0, .MyMass = enemy.MyDefinition.MyStats.MyMass}, .MyAttack = {.MyDisabled = true}};
			const auto spawned = SpawnEnemy({.MyOwnerId = _MyPlayers[enemy.MyOwner].MyPlayerId, .MyDefinition = std::move(definition),
				.MyRoute = {.MyStart = point, .MyEnd = {.MyX = std::floor(point.MyX + 0.5), .MyY = std::floor(point.MyY + 0.5)}}, .MyLifeCost = 0, .MyCounted = false});
			if (!spawned) continue;
			auto& candle = _MyUnits[Index(spawned)]; candle.MyCandleOwner = _unit; candle.MyCandleOriginal = id; candle.MyNoLeak = true;
			StatusFlags flags; flags.set(static_cast<std::size_t>(CombatStatus::UNBLOCKABLE)); flags.set(static_cast<std::size_t>(CombatStatus::NO_MOVE));
			(void)AddBuff(spawned, {.MyKey = "etlchi:candle", .MyFlags = flags});
			unit.MyCandles.push_back(spawned);
		}
	}

	void BattleCore::EtlchiObserve(ContentEvent& _event, bool _late)
	{
		if (!_late && _event.MyKind == ContentEventKind::DEATH && _event.MyUnit && Unit(_event.MyUnit).MySide == UnitSide::ENEMY && !Unit(_event.MyUnit).MyCandleOwner)
		{
			for (std::size_t n = 0, count = _MyEtlchis.size(); n < count; ++n)
			{
				const auto& owner = Unit(_MyEtlchis[n]); if (owner.MyOperatorHooksReleased) continue;
				auto& scratch = AcquireAttackScratch(); const CandleScratchGuard guard{.MyDepth = _MyAttackDepth};
				scratch.MyTargets.assign(owner.MyCandles.begin(), owner.MyCandles.end());
				for (const auto id : scratch.MyTargets)
					if (Unit(id).MyAlive && Unit(id).MyCandleOriginal == _event.MyUnit) Kill(_MyUnits[Index(id)], 0);
			}
		}
		if (!_event.MyTarget) return;
		auto& target = _MyUnits[Index(_event.MyTarget)];
		if (!_late && _event.MyKind == ContentEventKind::DAMAGED && target.MyAlive && !target.MyOperatorHooksReleased && !target.MyEtlchiReborn && std::isgreater(target.MyHealth, 0))
		{
			const auto* kit = target.MyDefinition.MyOperatorKit ? std::get_if<EtlchiKit>(target.MyDefinition.MyOperatorKit) : nullptr;
			if (kit && std::isless(target.MyHealth / target.MyStats.MyMaxHealth, kit->MyHealthThreshold))
			{
				target.MyEtlchiReborn = true;
				(void)Heal(target.MyId, target.MyId, target.MyStats.MyMaxHealth * kit->MyHealRatio, {.MySelf = true});
				if (std::isgreater(kit->MyReduction, 0)) (void)AddBuff(target.MyId, {.MyKey = "etlchi:reborn",
					.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::PHYSICAL_TAKEN_MULTIPLIER, .MyValue = 1 - kit->MyReduction}}});
			}
		}
		if (!target.MyCandleOwner || Unit(target.MyCandleOwner).MyOperatorHooksReleased) return;
		const auto owner = target.MyCandleOwner;
		if (!_late && (_event.MyKind == ContentEventKind::ELEMENT_HIT || _event.MyKind == ContentEventKind::BEFORE_STATUS) && _event.MySource != owner) _event.MyCancel = true;
		if (!_late && _event.MyKind == ContentEventKind::DAMAGED && !_event.MyElement && std::isgreater(_event.MyAmount, 0) && target.MyCandleOriginal && Unit(target.MyCandleOriginal).MyAlive)
			(void)LoseHealth(owner, target.MyCandleOriginal, _event.MyAmount);
		if (!_late || _event.MyKind != ContentEventKind::BEFORE_DAMAGE) return;
		if (_event.MySource != owner) { _event.MyCancel = true; return; }
		auto& damage = _event.MyDamage;
		if (damage.MyType != DamageType::PHYSICAL && damage.MyType != DamageType::ARTS) return;
		const auto& source = Unit(owner).MyStats;
		const auto mitigated = Mitigate(damage.MyAmount, damage.MyType, {.MyDefense = target.MyStats.MyDefense, .MyResistance = target.MyStats.MyResistance,
			.MyDefenseIgnorePercent = source.MyDefenseIgnorePercent + damage.MyDefenseIgnorePercent, .MyDefenseIgnoreFlat = source.MyDefenseIgnoreFlat + damage.MyDefenseIgnoreFlat,
			.MyResistanceIgnorePercent = source.MyResistanceIgnorePercent + damage.MyResistanceIgnorePercent, .MyResistanceIgnoreFlat = source.MyResistanceIgnoreFlat + damage.MyResistanceIgnoreFlat});
		if (std::isless(mitigated, source.MyAttack * 0.35)) { damage.MyType = DamageType::TRUE_DAMAGE; damage.MyAmount = source.MyAttack * 0.35; damage.MyCanDodge = false; }
	}

	void BattleCore::EtlchiSkill(UnitId _unit, const EtlchiKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY) { unit.MyStolenHealth = 0; unit.MyEtlchiReborn = false; }
		if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyDamage.MyIsAttack && !_event.MyElement && _event.MyTarget && unit.MyAlive)
		{
			auto& target = _MyUnits[Index(_event.MyTarget)];
			if (target.MySide != UnitSide::ENEMY || !target.MyAlive || target.MyCandleOwner) return;
			if (std::isgreater(_kit.MyDot, 0)) (void)AddBuff(target.MyId, {.MyKey = "etlchi:dot:" + std::to_string(_unit), .MySource = _unit, .MyDuration = _kit.MyDotDuration,
				.MyRefresh = BuffRefresh::EXTEND, .MyInterval = _kit.MyDotInterval,
				.MyTickEffects = {{.MyAmount = _kit.MyDot, .MyDamageType = DamageType::ARTS, .MyTags = DamageTag::TALENT | DamageTag::DOT}}});
			const auto amount = std::min({_kit.MySteal, _kit.MyStealCap - unit.MyStolenHealth, std::floor(target.MyStats.MyMaxHealth - 1)});
			if (std::isgreater(amount, 0) && !(target.MyDefinition.MySharedBoss && _MyInput.MySharedBoss))
			{
				unit.MyStolenHealth += amount;
				const auto key = "etlchi:steal:" + std::to_string(_unit);
				const auto found = std::ranges::find(target.MyBuffs, key, [](const CombatBuff& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
				const auto total = amount + (found != target.MyBuffs.end() && found->MyDefinition.MyStrength ? found->MyDefinition.MyStrength->MyValue : 0);
				const auto health = target.MyHealth;
				(void)AddBuff(target.MyId, {.MyKey = key, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALTH_FLAT, .MyValue = -total}}, .MyStrength = BuffStrength{.MyValue = total}});
				if (target.MyAlive) target.MyHealth = std::min(health, target.MyStats.MyMaxHealth);
				(void)AddBuff(_unit, {.MyKey = "etlchi:gain", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALTH_FLAT, .MyValue = unit.MyStolenHealth}}});
			}
		}
		if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			unit.MySickles.clear();
			auto& scratch = AcquireAttackScratch(); const CandleScratchGuard guard{.MyDepth = _MyAttackDepth};
			scratch.MyTargets.assign(unit.MyCandles.begin(), unit.MyCandles.end());
			for (const auto id : scratch.MyTargets) if (Unit(id).MyAlive) Kill(_MyUnits[Index(id)], 0);
			unit.MyCandles.clear();
		}
		else if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			if (_kit.MySkill == EtlchiSkillKind::CANDLE) { EtlchiCandles(_unit, _kit); return; }
			if (_kit.MySkill != EtlchiSkillKind::SICKLE) return;
			auto& scratch = AcquireAttackScratch(); const CandleScratchGuard guard{.MyDepth = _MyAttackDepth};
			UnitId pick = 0; std::size_t most = 0; double nearest = std::numeric_limits<double>::infinity();
			for (const auto id : _MyAllyIds)
			{
				const auto& ally = Unit(id);
				if (id == _unit || !ally.MyAlive || ally.MyKind == UnitKind::DEVICE || !ally.MyGround || !std::isgreater(ally.MyHealth, 0) || ally.MyStatuses.Has(CombatStatus::ISOLATED)) continue;
				FoesInRadius(RulePosition(ally), 1.5, scratch.MyTargets);
				const auto count = scratch.MyTargets.size(); const auto distance = Distance(RulePosition(ally), RulePosition(unit));
				if (!pick || count > most || (count == most && (std::isless(distance, nearest) || (!std::islessgreater(distance, nearest) && id < pick))))
				{ pick = id; most = count; nearest = distance; }
			}
			unit.MySickles.assign(1, _unit); if (pick) unit.MySickles.push_back(pick); unit.MySickleAccumulator = 0;
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK && _kit.MySkill == EtlchiSkillKind::SICKLE)
		{
			unit.MySickleAccumulator += _event.MyDelta;
			auto& scratch = AcquireAttackScratch(); const CandleScratchGuard guard{.MyDepth = _MyAttackDepth};
			while (std::isgreaterequal(unit.MySickleAccumulator, _kit.MySickleInterval - 1e-9))
			{
				unit.MySickleAccumulator -= _kit.MySickleInterval;
				for (const auto id : unit.MySickles)
				{
					const auto& carrier = Unit(id); if (!carrier.MyAlive) continue;
					const bool air = carrier.MyStatuses.Has(CombatStatus::LIFTOFF);
					FoesInRadius(RulePosition(carrier), 1.5, scratch.MyTargets);
					for (const auto enemy : scratch.MyTargets)
						if (air || !Unit(enemy).Flying()) (void)DealDamage(_unit, enemy, {.MyAmount = unit.MyStats.MyAttack * _kit.MySickleScale,
							.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
				}
			}
		}
	}
}
