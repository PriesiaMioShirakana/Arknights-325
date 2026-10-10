#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::InstallYanyou(CombatUnit& _unit)
	{
		if (!_unit.MyDefinition.MyYanyou) return;
		_unit.MyYanyouIndex = _MyYanyous.size();
		auto& state = _MyYanyous.emplace_back(); state.MyUnit = _unit.MyId;
		state.MyTargets.reserve(_MyInput.MySpawns.size());
	}

	void BattleCore::RefreshYanyouRange(CombatUnit& _unit, YanyouRuntime& _state)
	{
		if (_state.MyKeysAt && _state.MyRangeRevision == _unit.MyRangeRevision &&
			std::abs(_state.MyKeysAt->MyX - _unit.MyPosition.MyX) < 0.05 && std::abs(_state.MyKeysAt->MyY - _unit.MyPosition.MyY) < 0.05) return;
		const auto radius = _unit.MyDefinition.MyYanyou->MyRangeRadius;
		const auto point = _unit.MyPosition;
		const auto fill = [&](WorldPoint _origin, std::bitset<FieldTiles>& _mask, std::vector<int>& _keys)
		{
			_mask.reset(); _keys.clear(); _keys.reserve(FieldTiles);
			for (auto row = std::max(0, static_cast<int>(std::floor(_origin.MyY - radius))); row <= std::min(FieldRows - 1, static_cast<int>(std::ceil(_origin.MyY + radius))); ++row)
				for (auto col = std::max(0, static_cast<int>(std::floor(_origin.MyX - radius))); col <= std::min(FieldColumns - 1, static_cast<int>(std::ceil(_origin.MyX + radius))); ++col)
					if (std::hypot(col - _origin.MyX, row - _origin.MyY) <= radius + 1e-9)
					{
						const auto key = FieldGrid::Key(row, col);
						_mask.set(static_cast<std::size_t>(key)); _keys.push_back(key);
					}
		};
		fill(point, _unit.MyRangeMask, _unit.MyRangeKeys);
		if (UsesInitialPosition(_unit)) fill(_unit.MyHome, _unit.MyInitialRuleRangeMask, _unit.MyInitialRuleRangeKeys);
		_unit.MyBaseRangeMask = _unit.MyRangeMask;
		_unit.MyBaseTriggerMask = RuleRange(_unit);
		_state.MyKeysAt = point; _state.MyRangeRevision = _unit.MyRangeRevision;
	}

	UnitId BattleCore::YanyouLock(const YanyouRuntime& _state) const
	{
		return _state.MyLock && TargetableEnemy(Unit(_state.MyLock), AttackProfile{.MyCanHitFlying = true}) ? _state.MyLock : 0;
	}

	void BattleCore::YanyouRadius(YanyouRuntime& _state, WorldPoint _point, double _radius)
	{
		auto& out = _state.MyTargets; out.clear(); out.reserve(_MyEnemyIds.size());
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (!enemy.MyAlive || enemy.MyHidden || enemy.MyStatuses.Has(CombatStatus::UNTARGETABLE) || EnemyStealthed(enemy)) continue;
			const auto distance = BodyDistance(enemy, _point);
			if (distance * distance <= _radius * _radius + 1e-9) out.push_back(id);
		}
	}

	void BattleCore::NotifyYanyou(ContentEvent& _event)
	{
		if (_MyYanyous.empty()) return;
		if (_event.MyKind == ContentEventKind::ELEMENT_HIT && _event.MyTarget && Unit(_event.MyTarget).MyDefinition.MyYanyou)
		{ _event.MyCancel = true; return; }
		if (_event.MyKind == ContentEventKind::DAMAGED && _event.MySource && _event.MyTarget && !_event.MyElement &&
			_event.MyDamage.MyType != DamageType::ELEMENTAL && _event.MyAmount > 0)
		{
			const auto& source = Unit(_event.MySource); const auto& target = Unit(_event.MyTarget);
			if (source.MyDefinition.MyYanyou && (!source.MyRemoved || source.MyRemovedAt == Time()) && target.MySide == UnitSide::ENEMY && target.MyAlive && target.MyHealth > 0 && source.MyDefinition.MyYanyou->MyBurnRatio > 0)
				(void)DealElement(source.MyId, target.MyId, ElementHit{.MyElement = Element::BURN,
					.MyAmount = source.MyStats.MyAttack * source.MyDefinition.MyYanyou->MyBurnRatio});
		}
		if (_event.MyUnit && Unit(_event.MyUnit).MyYanyouIndex < _MyYanyous.size())
		{
			auto& unit = _MyUnits[Index(_event.MyUnit)]; auto& state = _MyYanyous[unit.MyYanyouIndex];
			const auto& kit = *unit.MyDefinition.MyYanyou;
			if (_event.MyKind == ContentEventKind::DEPLOY) { state.MyKeysAt.reset(); RefreshYanyouRange(unit, state); }
			else if (_event.MyKind == ContentEventKind::SKILL_START)
			{
				const auto targets = AllyTargets(unit); state.MyLock = targets.empty() ? 0 : targets.front(); state.MyFlameAccumulator = 1;
				if (!state.MyLock) EndSkill(unit.MyId);
			}
			else if (_event.MyKind == ContentEventKind::SKILL_ENDING) state.MyLock = 0;
			else if (_event.MyKind == ContentEventKind::SKILL_TICK)
			{
				if (!state.MyLock) return;
				const auto target = YanyouLock(state);
				if (unit.MyStatuses.Has(CombatStatus::SILENCE) || !target) { EndSkill(unit.MyId); return; }
				if (unit.MyHidden || unit.MyStatuses.Has(CombatStatus::STUN)) return;
				state.MyFlameAccumulator += _event.MyDelta;
				if (state.MyFlameAccumulator < 1 - 1e-9) return;
				state.MyFlameAccumulator -= 1;
				if (kit.MyFlameRadius > 0) YanyouRadius(state, Unit(target).MyPosition, kit.MyFlameRadius); else state.MyTargets.clear();
				if (!std::ranges::contains(state.MyTargets, target)) state.MyTargets.insert(state.MyTargets.begin(), target);
				for (const auto id : state.MyTargets) (void)DealDamage(unit.MyId, id, unit.MyStats.MyAttack * kit.MyFlameScale, DamageType::ARTS);
			}
		}
		if (_event.MyKind != ContentEventKind::TICK) return;
		for (std::size_t i = 0, count = _MyYanyous.size(); i < count; ++i)
		{
			auto& state = _MyYanyous[i]; auto& unit = _MyUnits[Index(state.MyUnit)];
			if (!unit.MyAlive || unit.MyRemoved) continue;
			const auto& kit = *unit.MyDefinition.MyYanyou;
			if (!unit.MyHidden && !unit.MyStatuses.Has(CombatStatus::STUN) && !unit.MyStatuses.Has(CombatStatus::NO_MOVE) && kit.MyMoveSpeed > 0)
			{
				auto target = YanyouLock(state);
				if (!target)
				{
					double bestTaunt = 0, bestDistance = 0;
					for (const auto id : _MyEnemyIds)
					{
						const auto& enemy = Unit(id);
						if (!TargetableEnemy(enemy, AttackProfile{.MyCanHitFlying = true})) continue;
						const auto taunt = enemy.MyStats.MyTaunt, distance = RemainingDistance(id);
						if (!target || taunt > bestTaunt || (taunt == bestTaunt && (distance < bestDistance - 1e-9 ||
							(std::abs(distance - bestDistance) <= 1e-9 && enemy.MySpawnSequence < Unit(target).MySpawnSequence))))
						{ target = id; bestTaunt = taunt; bestDistance = distance; }
					}
				}
				if (target)
				{
					const auto dx = Unit(target).MyPosition.MyX - unit.MyPosition.MyX;
					const auto dy = Unit(target).MyPosition.MyY + state.MyHoverOffset - unit.MyPosition.MyY;
					const auto distance = std::hypot(dx, dy);
					if (distance > 0.25 + 1e-6)
					{
						const auto step = std::min(distance - 0.25, kit.MyMoveSpeed * _event.MyDelta);
						const auto rect = _MyGrid ? _MyGrid->Rect() : FieldRect{};
						unit.MyPosition.MyX = std::clamp(unit.MyPosition.MyX + dx / distance * step, static_cast<double>(rect.MyFirstColumn), static_cast<double>(rect.MyLastColumn));
						unit.MyPosition.MyY = std::clamp(unit.MyPosition.MyY + dy / distance * step, static_cast<double>(rect.MyFirstRow), static_cast<double>(rect.MyLastRow));
					}
				}
			}
			RefreshYanyouRange(unit, state);
			state.MyAuraAccumulator += _event.MyDelta;
			if (kit.MyFragileMultiplier > 1 && state.MyAuraAccumulator >= 0.2 - 1e-9)
			{
				state.MyAuraAccumulator = 0; YanyouRadius(state, RulePosition(unit), 1.5);
				for (const auto id : state.MyTargets) (void)ApplyStatus(id, CombatStatus::ELEMENTAL_FRAGILE,
					StatusApplication{.MyDuration = 0.4, .MySource = unit.MyId, .MyValue = kit.MyFragileMultiplier - 1});
			}
		}
	}
}
