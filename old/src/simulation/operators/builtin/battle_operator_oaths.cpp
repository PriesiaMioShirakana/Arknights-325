#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct OathScratchGuard
		{
			std::size_t& MyDepth;

			~OathScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::Siege2Range(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto& kit = std::get<Siege2Kit>(*unit.MyDefinition.MyOperatorKit);
		if (!kit.MyDefault || !unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased || !unit.MySkill.MyActive) return;
		auto& keys = unit.MyNextExtraRangeKeys; keys.clear();
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (!ally.MyAlive || ally.MyHidden || !OperatorInGrid(_unit, id, kit.MyTalentRange)) continue;
			for (const auto enemyId : ally.MyBlocking)
			{
				const auto& enemy = Unit(enemyId); if (!enemy.MyAlive) continue;
				const auto row = static_cast<int>(std::floor(enemy.MyPosition.MyY + 0.5)), column = static_cast<int>(std::floor(enemy.MyPosition.MyX + 0.5));
				if (FieldGrid::InBounds(row, column)) keys.push_back(FieldGrid::Key(row, column));
			}
		}
		SetExtraRange(_unit, keys);
	}

	void BattleCore::Siege2Pulse(UnitId _unit)
	{
		const auto& unit = Unit(_unit); const auto& kit = std::get<Siege2Kit>(*unit.MyDefinition.MyOperatorKit);
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) return;
		auto& scratch = AcquireAttackScratch(); const OathScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (ally.MyAlive && !ally.MyHidden && ally.MyKind != UnitKind::DEVICE && (id == _unit || !ally.MyStatuses.Has(CombatStatus::ISOLATED)) && OperatorInGrid(_unit, id, kit.MyTalentRange)) scratch.MyTargets.push_back(id);
		}
		const auto others = static_cast<unsigned>(std::ranges::count_if(scratch.MyTargets, [&](UnitId _id) { return _id != _unit; }));
		if (kit.MySkill == Siege2SkillKind::HOMELAND && others >= kit.MySpCount && std::isgreater(kit.MySpRecovery, 0))
			(void)AddBuff(_unit, {.MyKey = "siege2:homeland", .MyDuration = 0.4, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::SP_RECOVERY_FLAT, .MyValue = kit.MySpRecovery}}});
		if (std::isgreater(kit.MyFragile, 0))
		{
			for (const auto id : _MyEnemyIds)
			{
				const auto& enemy = Unit(id); if (!enemy.MyAlive || !enemy.MyBlockedBy) continue;
				const auto& blocker = Unit(enemy.MyBlockedBy);
				if (blocker.MyId == _unit || (blocker.MyKind == UnitKind::TOKEN && blocker.MyOwnerUnit == _unit)) scratch.MySeen.push_back(id);
			}
			for (const auto id : scratch.MySeen) (void)ApplyStatus(id, CombatStatus::FRAGILE, {.MyDuration = 0.4, .MySource = _unit, .MyValue = kit.MyFragile});
		}
		if (!std::ranges::contains(scratch.MyTargets, _unit)) scratch.MyTargets.push_back(_unit);
		if (std::isgreater(kit.MyReduction, 0))
			for (const auto id : scratch.MyTargets) (void)AddBuff(id, {.MyKey = "siege2:sigh", .MyDuration = 0.4, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::PHYSICAL_TAKEN_MULTIPLIER, .MyValue = 1 - kit.MyReduction}}});
		if (others && std::islessgreater(kit.MyAttackPerAlly, 0)) (void)AddBuff(_unit, {.MyKey = "siege2:kings", .MyDuration = 0.4,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyAttackPerAlly * others}}});
		if (unit.MyBlocking.empty() && std::islessgreater(kit.MyFreeSpeed, 0)) (void)AddBuff(_unit, {.MyKey = "siege2:free", .MyDuration = 0.4,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = kit.MyFreeSpeed}}});
	}

	void BattleCore::Siege2Burst(UnitId _unit, const Siege2Kit& _kit)
	{
		const auto& unit = Unit(_unit);
		auto& scratch = AcquireAttackScratch(); const OathScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyEnemyIds)
			if (Unit(id).MyAlive && !Unit(id).MyHidden && !Unit(id).Flying() && OperatorInGrid(_unit, id, _kit.MySkillRange)) scratch.MyTargets.push_back(id);
		for (const auto id : scratch.MyTargets) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyBurstScale, .MyType = DamageType::TRUE_DAMAGE, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
	}

	void BattleCore::Siege2Observe(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MySource || !_event.MyTarget || Unit(_event.MyTarget).MySide != UnitSide::ENEMY || !Unit(_event.MyTarget).MyStatuses.Has(CombatStatus::TREMBLE)) return;
		const auto& source = Unit(_event.MySource);
		const auto id = source.MyKind == UnitKind::TOKEN ? source.MyOwnerUnit : source.MyId;
		if (!id) return;
		const auto& owner = Unit(id); const auto* kit = owner.MyDefinition.MyOperatorKit ? std::get_if<Siege2Kit>(owner.MyDefinition.MyOperatorKit) : nullptr;
		if (kit && !owner.MyOperatorHooksReleased && std::isgreater(kit->MyTrembleScale, 1)) _event.MyDamage.MyMultiplier *= kit->MyTrembleScale;
	}

	void BattleCore::Siege2Skill(UnitId _unit, const Siege2Kit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY && !std::ranges::contains(unit.MySiegeSeen, _event.MyTarget))
		{
			const auto& target = Unit(_event.MyTarget);
			const auto duration = target.MyDefinition.MyElite || target.MyDefinition.MyLeader || target.MySpawnTag == EnemySpawnTag::BOSS ? _kit.MyEliteTremble : _kit.MyTremble;
			if (std::isgreater(duration, 0)) { unit.MySiegeSeen.push_back(target.MyId); if (target.MyAlive) (void)ApplyStatus(target.MyId, CombatStatus::TREMBLE, duration, _unit); }
		}
		if (_kit.MySkill != Siege2SkillKind::NAME) return;
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			auto& scratch = AcquireAttackScratch(); const OathScratchGuard guard{.MyDepth = _MyAttackDepth};
			unit.MyGoldenLions.clear(); const auto* body = FindTokenTemplate(_unit, _kit.MyLion); if (!body) return;
			FreeSummonTiles(_unit, _kit.MyTalentRange, scratch.MyBuffIds, false);
			for (const auto key : scratch.MyBuffIds)
			{
				auto definition = *body; if (!definition.MyTokenKit) definition.MyAttack.MyDamageType = DamageType::TRUE_DAMAGE;
				const auto lion = SpawnToken({.MyDefinition = std::move(definition), .MyPosition = {.MyX = static_cast<double>(key % FieldColumns), .MyY = static_cast<double>(key / FieldColumns)}, .MyOwnerUnit = _unit, .MyDuration = unit.MySkill.MyTimeLeft});
				if (lion) unit.MyGoldenLions.push_back(lion);
			}
		}
		if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			auto lions = std::move(unit.MyGoldenLions); unit.MyGoldenLions.reserve(8);
			for (const auto id : lions) if (Unit(id).MyAlive) (void)Retreat(id, true, RemovalReason::EXPIRED);
			SetExtraRange(_unit, {});
		}
	}
}
