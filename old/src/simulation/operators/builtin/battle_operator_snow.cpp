#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		constexpr std::array<RangeOffset, 4> SnowNeighbors{{{.MyColumn = 1}, {.MyRow = 1}, {.MyColumn = -1}, {.MyRow = -1}}};
		constexpr std::array<RangeOffset, 8> SnowAround{{{.MyRow = 1, .MyColumn = -1}, {.MyRow = 1}, {.MyRow = 1, .MyColumn = 1},
			{.MyColumn = -1}, {.MyColumn = 1}, {.MyRow = -1, .MyColumn = -1}, {.MyRow = -1}, {.MyRow = -1, .MyColumn = 1}}};

		struct SnowScratchGuard
		{
			std::size_t& MyDepth;

			~SnowScratchGuard() { --MyDepth; }
		};
	}

	bool BattleCore::Sbell2FreezeTile(UnitId _unit, int _key)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto& kit = std::get<Sbell2Kit>(*unit.MyDefinition.MyOperatorKit);
		const int row = _key / FieldColumns, column = _key % FieldColumns;
		const WorldPoint point{.MyX = static_cast<double>(column), .MyY = static_cast<double>(row)};
		if (!_MyGrid || !_MyGrid->Tile(row, column).MyGoal || ReservedTile(point)) return false;
		for (const auto id : _MyAllyIds)
			if (Unit(id).MyKind == UnitKind::TOKEN && Unit(id).MyOwnerUnit == _unit && Unit(id).MyDefinition.MyId == kit.MyIceToken && Unit(id).MyAlive) return false;
		const auto* body = FindTokenTemplate(_unit, kit.MyIceToken);
		if (!body || !SpawnToken({.MyDefinition = *body, .MyPosition = point, .MyOwnerUnit = _unit})) return false;
		unit.MySnow[static_cast<std::size_t>(_key)] = 0;
		return true;
	}

	bool BattleCore::Sbell2AddSnow(UnitId _unit, int _key)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto& kit = std::get<Sbell2Kit>(*unit.MyDefinition.MyOperatorKit);
		if (_key < 0 || _key >= FieldTiles) return false;
		const int row = _key / FieldColumns, column = _key % FieldColumns;
		if (_MyGrid && (!_MyGrid->InRect(row, column) || !_MyGrid->GroundPassable(row, column, true))) return false;
		for (const auto id : _MyAllyIds)
		{
			const auto& token = Unit(id);
			if (token.MyAlive && token.MyKind == UnitKind::TOKEN && token.MyOwnerUnit == _unit && token.MyDefinition.MyId == kit.MyIceToken &&
				static_cast<int>(std::floor(token.MyPosition.MyX + 0.5)) == column && static_cast<int>(std::floor(token.MyPosition.MyY + 0.5)) == row) return false;
		}
		auto& layers = unit.MySnow[static_cast<std::size_t>(_key)];
		if (unit.MySnowWaves && layers >= kit.MyMaxSnow && Sbell2FreezeTile(_unit, _key)) return true;
		if (layers >= kit.MyMaxSnow)
		{
			if (!unit.MySnowWaves || !unit.MySnowSpreads) return false;
			int best = -1; unsigned thinnest = kit.MyMaxSnow;
			for (const auto offset : SnowNeighbors)
			{
				const auto r = row + offset.MyRow, c = column + offset.MyColumn;
				if (!FieldGrid::InBounds(r, c) || (_MyGrid && (!_MyGrid->InRect(r, c) || !_MyGrid->GroundPassable(r, c, true)))) continue;
				const auto k = FieldGrid::Key(r, c);
				if (unit.MySnow[static_cast<std::size_t>(k)] < thinnest) { thinnest = unit.MySnow[static_cast<std::size_t>(k)]; best = k; }
			}
			if (best < 0) return false;
			--unit.MySnowSpreads; return Sbell2AddSnow(_unit, best);
		}
		++layers;
		if (unit.MySnowWaves && layers >= kit.MyMaxSnow) (void)Sbell2FreezeTile(_unit, _key);
		return true;
	}

	void BattleCore::Sbell2Lay(UnitId _unit)
	{
		auto& scratch = AcquireAttackScratch(); const SnowScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto key : RuleRangeKeys(Unit(_unit))) scratch.MyBuffIds.push_back(static_cast<std::uint64_t>(key));
		for (const auto key : scratch.MyBuffIds) (void)Sbell2AddSnow(_unit, static_cast<int>(key));
	}

	void BattleCore::Sbell2Tick(UnitId _unit, double _delta)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto& kit = std::get<Sbell2Kit>(*unit.MyDefinition.MyOperatorKit);
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) return;
		unit.MySnowAccumulator += _delta;
		if (std::isgreaterequal(unit.MySnowAccumulator + 1e-9, unit.MySkill.MyActive ? kit.MySkillInterval : kit.MyInterval)) { unit.MySnowAccumulator = 0; Sbell2Lay(_unit); }
		if (!std::ranges::any_of(unit.MySnow, [](unsigned _layers) { return _layers != 0; })) return;
		if (unit.MySnowLast.size() < _MyUnits.size() + 1) unit.MySnowLast.resize(_MyUnits.size() + 1, -1);
		for (std::size_t i = 0; i < _MyEnemyIds.size(); ++i)
		{
			const auto id = _MyEnemyIds[i]; const auto& enemy = Unit(id);
			if (!enemy.MyAlive || enemy.MyHidden) continue;
			if (id >= unit.MySnowLast.size()) unit.MySnowLast.resize(_MyUnits.size() + 1, -1);
			const int row = static_cast<int>(std::floor(enemy.MyPosition.MyY + 0.5)), column = static_cast<int>(std::floor(enemy.MyPosition.MyX + 0.5));
			const int key = FieldGrid::InBounds(row, column) ? FieldGrid::Key(row, column) : -1;
			const auto previous = unit.MySnowLast[id];
			if (previous != key)
			{
				unit.MySnowLast[id] = key;
				const bool hadSnow = previous >= 0 && !enemy.Flying() && unit.MySnow[static_cast<std::size_t>(previous)];
				if (hadSnow) unit.MySnow[static_cast<std::size_t>(previous)] = 0;
				if (!enemy.Flying() && key >= 0 && unit.MySnow[static_cast<std::size_t>(key)] && std::isgreater(kit.MyEntryScale, 0))
					(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * kit.MyEntryScale, .MyType = DamageType::ARTS, .MyTags = DamageTag::TALENT | DamageTag::SNOW});
				if (unit.MySnowWaves && hadSnow && (key < 0 || !unit.MySnow[static_cast<std::size_t>(key)]) && enemy.MyAlive && std::isgreater(kit.MySnowCold, 0)) (void)ApplyStatus(id, CombatStatus::COLD, kit.MySnowCold, _unit);
			}
			if (key >= 0 && unit.MySnow[static_cast<std::size_t>(key)] && enemy.MyAlive && std::islessgreater(kit.MySlow, 0)) (void)AddBuff(id,
				{.MyKey = "sbell2:snow", .MySource = _unit, .MyDuration = 0.2, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::MOVE_MULTIPLIER, .MyValue = std::max(0.0, 1 + kit.MySlow * unit.MySnow[static_cast<std::size_t>(key)])}}});
		}
		if (!unit.MySnowWaves || !std::isgreater(kit.MySnowDamage, 0)) { unit.MySnowDamageAccumulator = 0; return; }
		unit.MySnowDamageAccumulator += _delta;
		if (std::isless(unit.MySnowDamageAccumulator + 1e-9, 1)) return;
		unit.MySnowDamageAccumulator -= 1;
		auto& scratch = AcquireAttackScratch(); const SnowScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (!enemy.MyAlive || enemy.MyHidden || enemy.Flying()) continue;
			const auto body = BodyTiles(enemy); bool snow = false;
			for (int r = body.MyFirstRow; r <= body.MyLastRow && !snow; ++r)
				for (int c = body.MyFirstColumn; c <= body.MyLastColumn; ++c)
					if (FieldGrid::InBounds(r, c) && unit.MySnow[static_cast<std::size_t>(FieldGrid::Key(r, c))]) { snow = true; break; }
			if (snow) scratch.MyTargets.push_back(id);
		}
		for (const auto id : scratch.MyTargets) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * kit.MySnowDamage, .MyType = DamageType::ARTS, .MyTags = DamageTag::SKILL | DamageTag::SNOW, .MyIsSkill = true});
	}

	void BattleCore::Sbell2Module(UnitId _unit)
	{
		const auto& unit = Unit(_unit); const auto& kit = std::get<Sbell2Kit>(*unit.MyDefinition.MyOperatorKit);
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) return;
		const AttackProfile filter{.MyCanHitFlying = true};
		const auto count = std::min<std::size_t>(kit.MyCrowdMax, std::ranges::count_if(_MyEnemyIds, [&](UnitId _id) { return TargetableEnemy(Unit(_id), filter) && InRuleRange(_unit, _id); }));
		if (count) (void)AddBuff(_unit, {.MyKey = "sbell2:heart", .MyDuration = 0.4, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::DAMAGE_DEALT_MULTIPLIER, .MyValue = 1 + kit.MyCrowdDamage * static_cast<double>(count)}}});
	}

	void BattleCore::Sbell2Observe(ContentEvent& _event, bool _fatal)
	{
		if (_fatal ? (_event.MyKind != ContentEventKind::FATAL || !_event.MyUnit || _event.MyPrevented) : (_event.MyKind != ContentEventKind::DAMAGED || !_event.MyTarget || _event.MyElement)) return;
		const auto id = _fatal ? _event.MyUnit : _event.MyTarget; auto& unit = _MyUnits[Index(id)];
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<Sbell2Kit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || unit.MyOperatorHooksReleased) return;
		if (!_fatal)
		{
			if (std::isgreater(kit->MyCounterCold, 0) && _event.MySource && Unit(_event.MySource).MySide == UnitSide::ENEMY && Unit(_event.MySource).MyAlive)
				(void)ApplyStatus(_event.MySource, CombatStatus::COLD, kit->MyCounterCold, id);
			return;
		}
		if (unit.MySnowBlessingUsed) return;
		_event.MyPrevented = true; unit.MySnowBlessingUsed = true; unit.MyHealth = std::max(1.0, unit.MyStats.MyMaxHealth * kit->MyReviveHealth);
		if (std::isgreater(kit->MySelfFreeze, 0)) (void)ApplyStatus(id, CombatStatus::FREEZE, kit->MySelfFreeze, id, true);
		if (!std::isgreater(kit->MyEnemyFreeze, 0)) return;
		auto& scratch = AcquireAttackScratch(); const SnowScratchGuard guard{.MyDepth = _MyAttackDepth};
		const AttackProfile filter{.MyCanHitFlying = true};
		for (const auto enemy : _MyEnemyIds) if (TargetableEnemy(Unit(enemy), filter) && InRuleRange(id, enemy)) scratch.MyTargets.push_back(enemy);
		for (const auto enemy : scratch.MyTargets) (void)ApplyStatus(enemy, CombatStatus::FREEZE, kit->MyEnemyFreeze, id);
	}

	void BattleCore::Sbell2Skill(UnitId _unit, const Sbell2Kit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY)
		{ std::ranges::fill(unit.MySnow, 0); unit.MySnowAccumulator = 0; unit.MySnowBlessingUsed = false; if (_kit.MyFirstSnow) Sbell2Lay(_unit); return; }
		if (_kit.MySkill == Sbell2SkillKind::WAVES)
		{
			if (_event.MyKind == ContentEventKind::SKILL_START) { unit.MySnowWaves = true; unit.MySnowSpreads = _kit.MySpreads; }
			if (_event.MyKind == ContentEventKind::SKILL_ENDING)
			{
				unit.MySnowWaves = false;
				for (std::size_t i = 0, count = _MyAllyIds.size(); i < count; ++i)
				{
					const auto& token = Unit(_MyAllyIds[i]);
					if (token.MyAlive && token.MyKind == UnitKind::TOKEN && token.MyOwnerUnit == _unit && token.MyDefinition.MyId == _kit.MyIceToken) (void)Retreat(token.MyId, true, RemovalReason::EXPIRED);
				}
			}
			return;
		}
		if (_event.MyKind != ContentEventKind::SKILL_START) return;
		auto& scratch = AcquireAttackScratch(); const SnowScratchGuard guard{.MyDepth = _MyAttackDepth};
		const AttackProfile filter{.MyCanHitFlying = true};
		for (const auto id : _MyEnemyIds) if (TargetableEnemy(Unit(id), filter) && InRuleRange(_unit, id)) scratch.MyTargets.push_back(id);
		const auto point = RulePosition(unit); const int row = static_cast<int>(std::floor(point.MyY + 0.5)), column = static_cast<int>(std::floor(point.MyX + 0.5));
		if (_kit.MySkill == Sbell2SkillKind::BREEZE)
		{
			const auto forward = RotateOffset({.MyColumn = 1}, unit.MyFacing);
			for (const auto id : scratch.MyTargets)
			{
				(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyBurstScale, .MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
				if (!Unit(id).MyAlive) continue;
				if (std::isgreater(_kit.MyCold, 0)) (void)ApplyStatus(id, CombatStatus::COLD, _kit.MyCold, _unit);
				if (!Unit(id).Flying()) (void)Push(id, _kit.MyForce, {.MyFrom = point, .MyDirection = WorldPoint{.MyX = static_cast<double>(forward.MyColumn), .MyY = static_cast<double>(forward.MyRow)}, .MyFixed = true});
			}
			for (unsigned distance = 1; distance <= _kit.MyForwardSnow; ++distance)
			{
				const auto r = row + forward.MyRow * static_cast<int>(distance), c = column + forward.MyColumn * static_cast<int>(distance);
				if (!FieldGrid::InBounds(r, c) || (_MyGrid && !_MyGrid->InRect(r, c))) break;
				(void)Sbell2AddSnow(_unit, FieldGrid::Key(r, c));
			}
			return;
		}
		if (!std::isgreater(_kit.MyAttract, 0)) return;
		for (const auto id : scratch.MyTargets)
		{
			const auto& enemy = Unit(id); if (enemy.Flying() || enemy.MySpawnTag == EnemySpawnTag::BOSS || !enemy.MyAlive) continue;
			std::optional<WorldPoint> goal; double best = std::numeric_limits<double>::infinity();
			for (const auto offset : SnowAround)
			{
				const auto r = row + offset.MyRow, c = column + offset.MyColumn;
				if (!FieldGrid::InBounds(r, c) || (_MyGrid && !_MyGrid->GroundPassable(r, c))) continue;
				const WorldPoint candidate{.MyX = static_cast<double>(c), .MyY = static_cast<double>(r)}; const auto distance = Distance(enemy.MyPosition, candidate);
				if (std::isless(distance, best - 1e-9)) { best = distance; goal = candidate; }
			}
			if (goal) (void)ApplyStatus(id, CombatStatus::ATTRACT, {.MyDuration = _kit.MyAttract, .MySource = _unit, .MyPoint = *goal});
		}
	}
}
