#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct ShadowScratchGuard
		{
			std::size_t& MyDepth;

			~ShadowScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::InesEarly(const ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::DEATH || !_event.MyUnit || _event.MyRemovalReason == RemovalReason::EXPIRED) return;
		auto& unit = _MyUnits[Index(_event.MyUnit)];
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<InesKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || unit.MyOperatorHooksReleased) return;
		unit.MyInesSentry = unit.MyBaseTriggerMask;
		if (kit->MySkill == InesSkillKind::RECALL) unit.MyInesSentryAt = RulePosition(unit);
	}

	void BattleCore::InesClearSpeed(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		auto& scratch = AcquireAttackScratch(); const ShadowScratchGuard guard{.MyDepth = _MyAttackDepth};
		scratch.MyTargets.assign(unit.MyInesSpeedVictims.begin(), unit.MyInesSpeedVictims.end());
		for (const auto id : scratch.MyTargets) (void)RemoveBuff(id, unit.MyInesSpeedKey);
		unit.MyInesSpeedVictims.clear(); unit.MyInesSpeed = 0;
		(void)RemoveBuff(_unit, "ines:aspdGain");
	}

	void BattleCore::InesRefreshAttack(UnitId _unit, const InesKit& _kit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		std::erase_if(unit.MyInesAttackVictims, [&](UnitId _id) { return !Unit(_id).MyAlive; });
		const auto gain = std::min(_kit.MyMaxAttack, _kit.MyStealAttack * static_cast<double>(unit.MyInesAttackVictims.size()));
		if (!unit.MyAlive) return;
		if (std::isgreater(gain, 0)) (void)AddBuff(_unit, {.MyKey = "ines:atkGain",
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_FLAT, .MyValue = gain}}});
		else (void)RemoveBuff(_unit, "ines:atkGain");
	}

	void BattleCore::InesObserve(const ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::DEATH || !_event.MyUnit) return;
		for (std::size_t i = 0, count = _MyIneses.size(); i < count; ++i)
		{
			auto& unit = _MyUnits[Index(_MyIneses[i])];
			if (unit.MyOperatorHooksReleased) continue;
			const auto& kit = std::get<InesKit>(*unit.MyDefinition.MyOperatorKit);
			if (Unit(_event.MyUnit).MySide == UnitSide::ENEMY && std::ranges::contains(unit.MyInesAttackVictims, _event.MyUnit)) InesRefreshAttack(unit.MyId, kit);
			if (_event.MyUnit != unit.MyId) continue;
			auto& scratch = AcquireAttackScratch(); const ShadowScratchGuard guard{.MyDepth = _MyAttackDepth};
			scratch.MyTargets.assign(unit.MyInesAttackVictims.begin(), unit.MyInesAttackVictims.end());
			for (const auto id : scratch.MyTargets) (void)RemoveBuff(id, unit.MyInesAttackKey);
			unit.MyInesAttackVictims.clear(); unit.MyInesWoven.clear();
			if (std::islessgreater(kit.MyFirstRedeploy, 0) && !unit.MyInesRetreated && std::isfinite(unit.MyRespawnAt))
			{
				unit.MyInesRetreated = true;
				unit.MyRespawnAt = unit.MyRemovedAt + (unit.MyRespawnAt - unit.MyRemovedAt) * std::max(0.0, 1 + kit.MyFirstRedeploy);
			}
		}
	}

	void BattleCore::InesSentry(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (unit.MyOperatorHooksReleased) return;
		auto mask = unit.MyInesSentry;
		if (unit.MyAlive) for (const auto key : RuleRangeKeys(unit)) mask.set(static_cast<std::size_t>(key));
		if (mask.none()) return;
		const auto& kit = std::get<InesKit>(*unit.MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch(); const ShadowScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (!enemy.MyAlive || enemy.MyHidden) continue;
			const auto tiles = BodyTiles(enemy); bool selected = false;
			for (int row = tiles.MyFirstRow; row <= tiles.MyLastRow && !selected; ++row)
				for (int column = tiles.MyFirstColumn; column <= tiles.MyLastColumn; ++column)
					if (row >= 0 && row < FieldRows && column >= 0 && column < FieldColumns && mask.test(static_cast<std::size_t>(FieldGrid::Key(row, column)))) { selected = true; break; }
			if (selected) scratch.MyTargets.push_back(id);
		}
		StatusFlags flags; flags.set(static_cast<std::size_t>(CombatStatus::REVEAL));
		for (const auto id : scratch.MyTargets) (void)AddBuff(id, {.MyKey = unit.MyInesSentryKey, .MyDuration = 0.25,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::MOVE_MULTIPLIER, .MyValue = kit.MyMoveMultiplier}}, .MyFlags = flags});
	}

	void BattleCore::InesRecall(UnitId _unit, const InesKit& _kit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (unit.MyInesSentry.none() || !unit.MyInesSentryAt) return;
		const auto start = *unit.MyInesSentryAt, end = RulePosition(unit);
		unit.MyInesSentry.reset(); unit.MyInesSentryAt.reset();
		const auto dx = end.MyX - start.MyX, dy = end.MyY - start.MyY, length = dx * dx + dy * dy;
		auto& scratch = AcquireAttackScratch(); const ShadowScratchGuard guard{.MyDepth = _MyAttackDepth};
		const AttackProfile filter{.MyCanHitFlying = true};
		const auto projection = [&](UnitId _id)
		{
			const auto& enemy = Unit(_id);
			const auto t = std::isgreater(length, 1e-9) ? std::clamp(((enemy.MyPosition.MyX - start.MyX) * dx + (enemy.MyPosition.MyY - start.MyY) * dy) / length, 0.0, 1.0) : 0;
			return std::pair(t, BodyDistance(enemy, {.MyX = start.MyX + t * dx, .MyY = start.MyY + t * dy}));
		};
		for (const auto id : _MyEnemyIds)
			if (TargetableEnemy(Unit(id), filter) && std::islessequal(projection(id).second, _kit.MyRecallRadius + 1e-9)) scratch.MyTargets.push_back(id);
		std::ranges::sort(scratch.MyTargets, [&](UnitId _a, UnitId _b)
		{
			const auto a = projection(_a), b = projection(_b);
			if (std::islessgreater(a.first, b.first)) return std::isless(a.first, b.first);
			if (std::islessgreater(a.second, b.second)) return std::isless(a.second, b.second);
			return _a < _b;
		});
		if (scratch.MyTargets.size() > _kit.MyRecallTargets) scratch.MyTargets.resize(_kit.MyRecallTargets);
		const auto amount = unit.MyStats.MyAttack * _kit.MyRecallScale;
		for (const auto id : scratch.MyTargets) (void)DealDamage(_unit, id, {.MyAmount = amount, .MyType = DamageType::PHYSICAL,
			.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
	}

	void BattleCore::InesSkill(UnitId _unit, const InesKit& _kit, const ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY && _kit.MySkill == InesSkillKind::RECALL)
		{
			if (unit.MyInesPlaced) (void)ActivateSkill(_unit, false, SkillReason::DEPLOY);
			else { unit.MyInesPlaced = true; Schedule({.MyAt = Time(), .MyKind = ScheduledKind::INES_FIRST_RETREAT, .MySource = _unit}); }
		}
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			if (_kit.MySkill == InesSkillKind::STEALTH) InesClearSpeed(_unit);
			else if (_kit.MySkill == InesSkillKind::RECALL)
			{
				(void)AddBuff(_unit, {.MyKey = "ines:s3", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _kit.MyAttack}}});
				InesRecall(_unit, _kit);
			}
		}
		if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			if (_kit.MySkill == InesSkillKind::STEALTH) InesClearSpeed(_unit);
			else if (_kit.MySkill == InesSkillKind::RECALL) (void)RemoveBuff(_unit, "ines:s3");
		}
		if (_event.MyKind == ContentEventKind::ATTACK && _kit.MySkill == InesSkillKind::STEALTH && unit.MySkill.MyActive && unit.MyOwner != NoPlayer)
			(void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyDp);
		if (_event.MyKind != ContentEventKind::DAMAGED || !_event.MyTarget || Unit(_event.MyTarget).MySide != UnitSide::ENEMY || _event.MyElement) return;
		if (unit.MyAlive && !std::ranges::contains(unit.MyInesWoven, _event.MyTarget))
		{
			unit.MyInesWoven.push_back(_event.MyTarget);
			if (Unit(_event.MyTarget).MyAlive)
			{
				(void)ApplyStatus(_event.MyTarget, CombatStatus::BIND, _kit.MyBind, _unit);
				(void)AddBuff(_event.MyTarget, {.MyKey = unit.MyInesAttackKey, .MySource = _unit,
					.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_FLAT, .MyValue = -_kit.MyStealAttack}}});
				unit.MyInesAttackVictims.push_back(_event.MyTarget); InesRefreshAttack(_unit, _kit);
			}
		}
		if (_kit.MySkill == InesSkillKind::RECALL && std::isgreater(_event.MyAmount, 0) && unit.MySkill.MyActive && std::isgreater(unit.MySkill.MyTimeLeft, 0) && unit.MyOwner != NoPlayer)
			(void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyDp);
	}

	void BattleCore::InesHit(UnitId _unit, UnitId _target, const InesKit& _kit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_kit.MySkill == InesSkillKind::BLEED)
		{
			if (unit.MyOwner != NoPlayer) (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyDp);
			if (!_target || !Unit(_target).MyAlive || Unit(_target).MySide != UnitSide::ENEMY) return;
			(void)AddBuff(_target, {.MyKey = "ines:bleed:" + std::to_string(_unit), .MySource = _unit, .MyDuration = _kit.MyBleedDuration + 1e-6,
				.MyRefresh = BuffRefresh::EXTEND, .MyInterval = 1, .MyTickEffects = {{.MyAmount = _kit.MyBleedScale, .MyDamageType = DamageType::ARTS,
					.MyTags = DamageTag::SKILL | DamageTag::DOT, .MySourceAttackScale = true, .MyIsSkill = true}}});
			return;
		}
		if (_kit.MySkill != InesSkillKind::STEALTH || !_target || !Unit(_target).MyAlive || Unit(_target).MySide != UnitSide::ENEMY) return;
		const auto room = _kit.MyMaxSpeed - unit.MyInesSpeed;
		if (!std::isgreater(room, 0)) return;
		const auto gain = std::min(room, _kit.MyStealSpeed); unit.MyInesSpeed += gain;
		double previous = 0;
		const auto& buffs = Unit(_target).MyBuffs;
		const auto found = std::ranges::find(buffs, unit.MyInesSpeedKey, [](const CombatBuff& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
		if (found != buffs.end() && found->MyDefinition.MyModifiers)
			for (const auto& modifier : *found->MyDefinition.MyModifiers) if (modifier.MyAttribute == Attribute::ATTACK_SPEED) previous = modifier.MyValue;
		(void)AddBuff(_target, {.MyKey = unit.MyInesSpeedKey, .MySource = _unit,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = previous - gain}}});
		if (!std::ranges::contains(unit.MyInesSpeedVictims, _target)) unit.MyInesSpeedVictims.push_back(_target);
		(void)AddBuff(_unit, {.MyKey = "ines:aspdGain", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = unit.MyInesSpeed}}});
	}
}
