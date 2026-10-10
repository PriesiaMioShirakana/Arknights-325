#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct FirewallScratchGuard
		{
			std::size_t& MyDepth;

			~FirewallScratchGuard() { --MyDepth; }
		};
	}

	bool BattleCore::TeleportEnemy(UnitId _enemy, WorldPoint _point)
	{
		auto& enemy = _MyUnits[Index(_enemy)];
		if (!enemy.MyAlive || enemy.MySide != UnitSide::ENEMY || enemy.Flying() || enemy.MyStatuses.Has(CombatStatus::SELF_BOUND)) return false;
		const int row = static_cast<int>(std::floor(_point.MyY + 0.5)), column = static_cast<int>(std::floor(_point.MyX + 0.5));
		const int oldRow = static_cast<int>(std::floor(enemy.MyPosition.MyY + 0.5)), oldColumn = static_cast<int>(std::floor(enemy.MyPosition.MyX + 0.5));
		if (!FieldGrid::InBounds(row, column) || !FieldGrid::InBounds(oldRow, oldColumn)) return false;
		if (_MyGrid && (!_MyGrid->GroundPassable(row, column) || _MyGrid->Flow(row, column).MyDistance[static_cast<std::size_t>(FieldGrid::Key(oldRow, oldColumn))] < 0)) return false;
		enemy.MyPosition = {.MyX = static_cast<double>(column), .MyY = static_cast<double>(row)};
		ReleaseBlock(enemy); enemy.MyPath.clear(); enemy.MyPathPoint = 0; enemy.MyPlannedRoute = std::numeric_limits<std::size_t>::max();
		return true;
	}

	bool BattleCore::YuCrosses(const CombatUnit& _unit, UnitId _source, UnitId _target) const
	{
		if (!_unit.MyYuWall) return false;
		const auto source = RulePosition(Unit(_source)), target = RulePosition(Unit(_target));
		return std::isless((_unit.MyYuVertical ? source.MyX - *_unit.MyYuWall : source.MyY - *_unit.MyYuWall) *
			(_unit.MyYuVertical ? target.MyX - *_unit.MyYuWall : target.MyY - *_unit.MyYuWall), 0);
	}

	void BattleCore::YuPulse(UnitId _unit, unsigned _kind)
	{
		const auto& unit = Unit(_unit); const auto& kit = std::get<YuKit>(*unit.MyDefinition.MyOperatorKit);
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) return;
		if (_kind == 0)
		{
			if (!unit.MyBlocking.empty() && std::isgreater(kit.MyProtection, 0)) (void)ApplyStrongest(_unit, "protect", 0.2,
				{.MyValue = kit.MyProtection, .MyAttribute = Attribute::PHYSICAL_TAKEN_MULTIPLIER, .MyScale = -1, .MyOffset = 1, .MySecondAttribute = Attribute::ARTS_TAKEN_MULTIPLIER}, _unit);
			return;
		}
		auto& scratch = AcquireAttackScratch(); const FirewallScratchGuard guard{.MyDepth = _MyAttackDepth};
		if (_kind == 1)
		{
			scratch.MyTargets.assign(unit.MyBlocking.begin(), unit.MyBlocking.end());
			for (const auto id : scratch.MyTargets)
			{
				if (!Unit(id).MyAlive) continue;
				if (std::isgreater(kit.MyDamageScale, 0)) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * kit.MyDamageScale, .MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::TALENT)});
				if (std::isgreater(kit.MyElementRatio, 0)) (void)DealElement(_unit, id, {.MyElement = Element::BURN, .MyAmount = unit.MyStats.MyAttack * kit.MyElementRatio, .MyTags = static_cast<DamageTags>(DamageTag::TALENT)});
			}
			return;
		}
		for (const auto id : _MyAllyIds) if (Unit(id).MyOwner == unit.MyOwner && Unit(id).MyAlive && Unit(id).MyKind == UnitKind::OPERATOR) scratch.MyTargets.push_back(id);
		if (std::isless(static_cast<double>(scratch.MyTargets.size()), kit.MyOperatorCount)) return;
		if (!kit.MyDefault || !unit.MySkill.MyActive) scratch.MyTargets.assign(1, _unit);
		for (const auto id : scratch.MyTargets)
		{
			if (std::isgreater(kit.MyHealthRatio, 0)) (void)Heal(_unit, id, Unit(id).MyStats.MyMaxHealth * kit.MyHealthRatio * kit.MyHealInterval, {.MySelf = true, .MySilent = true});
			if (std::isgreater(kit.MyElementHealRatio, 0)) (void)ReduceElement(id, Unit(id).MyStats.MyMaxHealth * kit.MyElementHealRatio * kit.MyHealInterval);
		}
	}

	void BattleCore::YuSkill(UnitId _unit, const YuKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_kit.MySkill == YuSkillKind::WALL)
		{
			if (_event.MyKind == ContentEventKind::SKILL_START)
			{
				const auto point = RulePosition(unit); const auto forward = RotateOffset({.MyColumn = 1}, unit.MyFacing);
				unit.MyYuVertical = forward.MyRow == 0; unit.MyYuWall = std::floor((unit.MyYuVertical ? point.MyX : point.MyY) + 0.5);
			}
			if (_event.MyKind == ContentEventKind::SKILL_ENDING) unit.MyYuWall.reset();
		}
		if (_kit.MySkill != YuSkillKind::GUEST || _event.MyKind != ContentEventKind::SKILL_START) return;
		auto& scratch = AcquireAttackScratch(); const FirewallScratchGuard guard{.MyDepth = _MyAttackDepth};
		OperatorEnemiesInGrid(_unit, _kit.MyRange, scratch.MyTargets, false);
		for (const auto id : scratch.MyTargets) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MySkillScale, .MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
		for (const auto id : scratch.MyTargets) (void)TeleportEnemy(id, RulePosition(unit));
	}

	void BattleCore::YuObserve(ContentEvent& _event)
	{
		if ((_event.MyKind != ContentEventKind::ELEMENT_HIT && _event.MyKind != ContentEventKind::DAMAGED && _event.MyKind != ContentEventKind::BEFORE_DAMAGE) || !_event.MySource || !_event.MyTarget) return;
		const auto& source = Unit(_event.MySource); const auto& target = Unit(_event.MyTarget);
		for (const auto id : _MyYus)
		{
			const auto& unit = Unit(id); const auto& kit = std::get<YuKit>(*unit.MyDefinition.MyOperatorKit);
			if (unit.MyOperatorHooksReleased) continue;
			const bool live = unit.MyAlive && !unit.MyHidden;
			if (_event.MyKind == ContentEventKind::ELEMENT_HIT)
			{
				if (id == source.MyId && live && !unit.MyBlocking.empty() && std::isgreater(kit.MyElementMultiplier, 1)) _event.MyElementHit.MyMultiplier *= kit.MyElementMultiplier;
				continue;
			}
			if (_event.MyKind == ContentEventKind::DAMAGED)
			{
				if (kit.MySkill == YuSkillKind::HOST && target.MyId == id && unit.MySkill.MyActive && source.MySide == UnitSide::ENEMY && source.MyAlive && !_event.MyElement &&
					!_event.MyDamage.MySourceless && !HasTag(_event.MyDamage.MyTags, DamageTag::HP_LOSS) && !HasTag(_event.MyDamage.MyTags, DamageTag::COUNTER) && !HasTag(_event.MyDamage.MyTags, DamageTag::REFLECT) && std::isgreater(kit.MyCounterRatio, 0))
					(void)DealElement(id, source.MyId, {.MyElement = Element::BURN, .MyAmount = unit.MyStats.MyAttack * kit.MyCounterRatio, .MyTags = static_cast<DamageTags>(DamageTag::SKILL)});
				if (live && std::isgreater(kit.MyWallBurn, 0) && _event.MyDamage.MyType == DamageType::ARTS && std::isgreater(_event.MyAmount, 0) && source.MySide == UnitSide::ALLY && source.MyId != id &&
					target.MySide == UnitSide::ENEMY && target.MyAlive && YuCrosses(unit, source.MyId, target.MyId))
					(void)DealElement(id, target.MyId, {.MyElement = Element::BURN, .MyAmount = unit.MyStats.MyAttack * kit.MyWallBurn, .MyTags = DamageTag::SKILL | DamageTag::FIREWALL});
				continue;
			}
			if (live && std::isgreater(kit.MyWallChance, 0) && source.MySide == UnitSide::ENEMY && target.MySide == UnitSide::ALLY && _event.MyDamage.MyIsAttack &&
				std::isgreater(source.MyDefinition.MyAttack.MyEnemyRange, 0) && source.MyBlockedBy != target.MyId && YuCrosses(unit, source.MyId, target.MyId) && std::isless(_MyRandom.Next(), kit.MyWallChance)) _event.MyCancel = true;
			if (!std::isgreater(kit.MyArtsMultiplier, 1) || _event.MyDamage.MyType != DamageType::ARTS || target.MySide != UnitSide::ENEMY ||
				!std::ranges::any_of(target.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "burnBurst"; }) ||
				(source.MyId != id && !(source.MyKind == UnitKind::OPERATOR && source.MyOwner == unit.MyOwner && live && kit.MyDefault && unit.MySkill.MyActive))) continue;
			const auto count = std::ranges::count_if(_MyAllyIds, [&](UnitId _id) { const auto& ally = Unit(_id); return ally.MyOwner == unit.MyOwner && ally.MyAlive && ally.MyKind == UnitKind::OPERATOR; });
			if (std::isgreaterequal(static_cast<double>(count), kit.MyModuleCount)) _event.MyDamage.MyMultiplier *= kit.MyArtsMultiplier;
		}
	}
}
