#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::PreferUnblocked(CombatUnit& _unit, std::vector<UnitId>& _targets)
	{
		if (_targets.empty() || !Unit(_targets.front()).MyBlockedBy) return;
		auto& scratch = AcquireAttackScratch();
		for (const auto id : _MyEnemyIds)
			if (TargetableEnemy(Unit(id), _unit.MyDefinition.MyAttack) && InRuleRange(_unit.MyId, id)) scratch.MyTargets.push_back(id);
		SortOperatorTargets(_unit.MyId, scratch.MyTargets, static_cast<unsigned>(scratch.MyTargets.size()));
		const auto found = std::ranges::find_if(scratch.MyTargets, [&](UnitId _id) { return !Unit(_id).MyBlockedBy; });
		if (found != scratch.MyTargets.end())
		{
			const auto id = *found;
			_targets.erase(_targets.begin()); std::erase(_targets, id); _targets.insert(_targets.begin(), id);
		}
		--_MyAttackDepth;
	}

	void BattleCore::DuskSummon(UnitId _unit, const DuskKit& _kit, UnitId _target)
	{
		const auto point = Unit(_target).MyPosition;
		const auto row = static_cast<int>(std::floor(point.MyY + 0.5)), column = static_cast<int>(std::floor(point.MyX + 0.5));
		const WorldPoint destination{.MyX = static_cast<double>(column), .MyY = static_cast<double>(row)};
		const auto freeGround = [&]
		{
			return FieldGrid::InBounds(row, column) && !ReservedTile(destination) && (!_MyGrid || (_MyGrid->InRect(row, column) && _MyGrid->CanStand(row, column) &&
				!_MyGrid->Obstacle(row, column, ObstacleKind::BLOCK) && !_MyGrid->Obstacle(row, column, ObstacleKind::CRATE)));
		};
		for (const auto id : _MyAllyIds)
		{
			auto& token = _MyUnits[Index(id)];
			if (!token.MyAlive || token.MyKind != UnitKind::TOKEN || token.MyOwnerUnit != _unit || token.MyDefinition.MyId != _kit.MyToken) continue;
			if ((std::islessgreater(token.MyPosition.MyX, destination.MyX) || std::islessgreater(token.MyPosition.MyY, destination.MyY)) &&
				(!freeGround() || !Relocate(id, destination))) return;
			token.MyDuskUntil = Time() + _kit.MyTokenDuration;
			return;
		}
		if (!freeGround()) return;
		const auto* body = FindTokenTemplate(_unit, _kit.MyToken);
		if (!body) return;
		const auto id = SpawnToken({.MyDefinition = *body, .MyPosition = destination, .MyOwnerUnit = _unit});
		if (!id) return;
		auto& token = _MyUnits[Index(id)]; token.MyDuskUntil = Time() + _kit.MyTokenDuration;
		Schedule({.MyAt = token.MyDuskUntil, .MyKind = ScheduledKind::DUSK_EXPIRE, .MyTarget = id, .MyVersion = token.MyDeploySequence});
	}

	void BattleCore::DuskKill(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_KILL || !_event.MySource || !_event.MyTarget || Unit(_event.MyTarget).MySide != UnitSide::ENEMY) return;
		const auto& killer = Unit(_event.MySource);
		const auto id = killer.MyKind == UnitKind::TOKEN ? killer.MyOwnerUnit : killer.MyId;
		if (!id) return;
		const auto& owner = Unit(id);
		const auto* kit = owner.MyDefinition.MyOperatorKit ? std::get_if<DuskKit>(owner.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || !owner.MyAlive || owner.MyOperatorHooksReleased) return;
		(void)AddBuff(id, {.MyKey = "dusk:realm", .MyMaxStacks = kit->MyMaxStacks, .MyRefresh = BuffRefresh::STACK,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit->MyKillAttack}}});
	}

	void BattleCore::DuskSkill(UnitId _unit, const DuskKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY) unit.MyDuskSummoned = false;
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _kit.MySkill == DuskSkillKind::INK && unit.MySkill.MyActive &&
			_event.MyDamage.MyType == DamageType::ARTS && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
		{
			const auto& target = Unit(_event.MyTarget);
			if (std::isless(target.MyHealth / target.MyStats.MyMaxHealth, _kit.MyHealthThreshold)) _event.MyDamage.MyMultiplier *= _kit.MyLowHealthScale;
		}
		if (_event.MyKind != ContentEventKind::ATTACK) return;
		const auto target = std::ranges::find_if(_event.MyTargets, [&](UnitId _id) { return Unit(_id).MySide == UnitSide::ENEMY; });
		if (target == _event.MyTargets.end()) return;
		if (!unit.MyDuskSummoned) { unit.MyDuskSummoned = true; DuskSummon(_unit, _kit, *target); }
		if (_kit.MySkill == DuskSkillKind::FREEHAND && unit.MySkill.MyActive) DuskSummon(_unit, _kit, *target);
	}
}
