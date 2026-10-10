#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct AlchemyScratchGuard
		{
			std::size_t& MyDepth;

			~AlchemyScratchGuard() { --MyDepth; }
		};
	}

	unsigned BattleCore::StraightRoad(int _row, int _column)
	{
		if (!_MyStraightRoads)
		{
			_MyStraightRoads.emplace(); const auto rect = _MyGrid ? _MyGrid->Rect() : FieldRect{};
			const auto pass = [&](int _r, int _c)
			{ return _r >= rect.MyFirstRow && _r <= rect.MyLastRow && _c >= rect.MyFirstColumn && _c <= rect.MyLastColumn && (!_MyGrid || _MyGrid->GroundPassable(_r, _c, true)); };
			for (int r = rect.MyFirstRow; r <= rect.MyLastRow; ++r)
				for (int c = rect.MyFirstColumn; c <= rect.MyLastColumn; ++c)
				{
					if (!pass(r, c)) continue;
					unsigned horizontal = 1, vertical = 1;
					for (int k = c - 1; pass(r, k); --k) ++horizontal;
					for (int k = c + 1; pass(r, k); ++k) ++horizontal;
					for (int k = r - 1; pass(k, c); --k) ++vertical;
					for (int k = r + 1; pass(k, c); ++k) ++vertical;
					(*_MyStraightRoads)[static_cast<std::size_t>(r * FieldColumns + c)] = std::max(horizontal, vertical);
				}
		}
		return FieldGrid::InBounds(_row, _column) ? (*_MyStraightRoads)[static_cast<std::size_t>(_row * FieldColumns + _column)] : 0;
	}

	void BattleCore::Thorn2Vision(UnitId _unit)
	{
		const auto& unit = Unit(_unit); if (!unit.MyAlive || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<Thorn2Kit>(*unit.MyDefinition.MyOperatorKit);
		const auto grant = [&](UnitId _id, double _base, double _extra)
		{
			const auto point = RulePosition(Unit(_id));
			const auto length = StraightRoad(static_cast<int>(std::floor(point.MyY + 0.5)), static_cast<int>(std::floor(point.MyX + 0.5)));
			const auto speed = _base + (std::isgreaterequal(static_cast<double>(length), kit.MyRoadLength) ? _extra : 0);
			if (std::islessgreater(speed, 0)) (void)AddBuff(_id, {.MyKey = "thorn2:vision", .MyDuration = 0.5,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = speed}}});
		};
		for (std::size_t i = 0, count = _MyAllyIds.size(); i < count; ++i)
		{
			const auto id = _MyAllyIds[i]; const auto& ally = Unit(id);
			if (ally.MyAlive && ally.MyKind != UnitKind::DEVICE && (id == _unit || !ally.MyStatuses.Has(CombatStatus::ISOLATED))) grant(id, kit.MyAllySpeed, kit.MyAllyRoadSpeed);
		}
		for (std::size_t i = 0, count = _MyEnemyIds.size(); i < count; ++i)
			if (Unit(_MyEnemyIds[i]).MyAlive && !Unit(_MyEnemyIds[i]).MyHidden) grant(_MyEnemyIds[i], kit.MyEnemySpeed, kit.MyEnemyRoadSpeed);
	}

	void BattleCore::Thorn2Start(UnitId _unit, const Thorn2Kit& _kit)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto origin = RulePosition(unit);
		auto& scratch = AcquireAttackScratch(); const AlchemyScratchGuard guard{.MyDepth = _MyAttackDepth};
		OperatorAlliesInRange(_unit, scratch.MyTargets);
		const auto duration = _kit.MyDuration + (std::ranges::any_of(scratch.MyTargets, [&](UnitId _id) { return _id != _unit && Unit(_id).MyKind == UnitKind::OPERATOR; }) ? _kit.MyExtraDuration : 0);
		const auto addZone = [&](WorldPoint _point, WorldPoint _velocity, UnitId _anchor, double _accumulator)
		{
			auto& zone = unit.MyAlchemyZones.emplace_back(AlchemyZone{.MyPoint = _point, .MyVelocity = _velocity, .MyAnchor = _anchor, .MyAccumulator = _accumulator, .MyDuration = duration});
			if (_kit.MySkill == Thorn2SkillKind::SEA) zone.MyBurnTargets.reserve(_MyEnemyIds.size());
		};
		if (_kit.MySkill == Thorn2SkillKind::GUARD)
		{
			std::erase_if(scratch.MyTargets, [&](UnitId _id) { return !std::isgreater(Unit(_id).MyHealth, 0); });
			std::ranges::sort(scratch.MyTargets, [&](UnitId _a, UnitId _b)
			{
				const auto& a = Unit(_a); const auto& b = Unit(_b); const auto ar = a.MyHealth / a.MyStats.MyMaxHealth, br = b.MyHealth / b.MyStats.MyMaxHealth;
				if (std::islessgreater(ar, br)) return std::isless(ar, br);
				if (a.MyBlocking.size() != b.MyBlocking.size()) return a.MyBlocking.size() > b.MyBlocking.size();
				const auto ad = Distance(origin, RulePosition(a)), bd = Distance(origin, RulePosition(b));
				return std::islessgreater(ad, bd) ? std::isless(ad, bd) : _a < _b;
			});
			if (!scratch.MyTargets.empty()) addZone(RulePosition(Unit(scratch.MyTargets.front())), {}, 0, 0);
			return;
		}
		if (_kit.MySkill == Thorn2SkillKind::SEA)
		{
			scratch.MyTargets.clear();
			for (const auto id : _MyAllyIds)
			{
				const auto& ally = Unit(id);
				if (ally.MyAlive && ally.MyOwner == unit.MyOwner && ally.MyKind == UnitKind::OPERATOR && std::isgreater(ally.MyHealth, 0)) scratch.MyTargets.push_back(id);
			}
			std::ranges::sort(scratch.MyTargets, [&](UnitId _a, UnitId _b)
			{
				const auto& a = Unit(_a); const auto& b = Unit(_b);
				if (a.MyStats.MyBlockCount != b.MyStats.MyBlockCount) return a.MyStats.MyBlockCount < b.MyStats.MyBlockCount;
				const auto ad = Distance(origin, RulePosition(a)), bd = Distance(origin, RulePosition(b));
				return std::islessgreater(ad, bd) ? std::isless(ad, bd) : _a < _b;
			});
			if (scratch.MyTargets.size() > _kit.MyAnchors) scratch.MyTargets.resize(_kit.MyAnchors);
			for (const auto id : scratch.MyTargets) addZone(RulePosition(Unit(id)), {}, id, _kit.MyInterval - 0.25);
			return;
		}
		scratch.MyTargets.clear();
		for (const auto id : _MyEnemyIds) if (TargetableEnemy(Unit(id), unit.MyDefinition.MyAttack) && InRuleRange(_unit, id)) scratch.MyTargets.push_back(id);
		SortOperatorTargets(_unit, scratch.MyTargets, static_cast<unsigned>(scratch.MyTargets.size()));
		std::optional<WorldPoint> landing;
		const auto ground = std::ranges::find_if(scratch.MyTargets, [&](UnitId _id) { return !Unit(_id).Flying(); });
		if (ground != scratch.MyTargets.end()) landing = Unit(*ground).MyPosition;
		else if (!scratch.MyTargets.empty()) landing = Unit(scratch.MyTargets.front()).MyPosition;
		else
		{
			const auto forward = RotateOffset({.MyColumn = 1}, unit.MyFacing);
			const auto row = static_cast<int>(std::floor(origin.MyY + 0.5)), column = static_cast<int>(std::floor(origin.MyX + 0.5)); int farthest = 0;
			for (const auto key : RuleRangeKeys(unit))
			{
				const int r = key / FieldColumns, c = key % FieldColumns;
				if (_MyGrid && !_MyGrid->InRect(r, c)) continue;
				const int lateral = (r - row) * forward.MyColumn - (c - column) * forward.MyRow;
				const int distance = (r - row) * forward.MyRow + (c - column) * forward.MyColumn;
				if (!lateral && distance > farthest) { farthest = distance; landing = WorldPoint{.MyX = static_cast<double>(c), .MyY = static_cast<double>(r)}; }
			}
		}
		if (!landing) return;
		const auto dx = landing->MyX - origin.MyX, dy = landing->MyY - origin.MyY; const auto length = std::hypot(dx, dy);
		const auto speed = _kit.MySpeed / (std::isgreater(length, 0) ? length : 1);
		addZone(*landing, {.MyX = dx * speed, .MyY = dy * speed}, 0, 0);
	}

	void BattleCore::Thorn2Zones(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)]; if (unit.MyOperatorHooksReleased || unit.MyAlchemyZones.empty()) return;
		const auto& kit = std::get<Thorn2Kit>(*unit.MyDefinition.MyOperatorKit); const auto rect = _MyGrid ? _MyGrid->Rect() : FieldRect{};
		auto& scratch = AcquireAttackScratch(); const AlchemyScratchGuard guard{.MyDepth = _MyAttackDepth};
		scratch.MySeen.reserve(_MyEnemyIds.size()); scratch.MyBuffIds.reserve(_MyEnemyIds.size());
		for (std::size_t n = 0; n < unit.MyAlchemyZones.size(); ++n)
		{
			auto& zone = unit.MyAlchemyZones[n]; zone.MyElapsed += 0.25; zone.MyAccumulator += 0.25;
			if (kit.MySkill == Thorn2SkillKind::GUARD)
			{
				const auto row = static_cast<int>(std::floor(zone.MyPoint.MyY + 0.5)), column = static_cast<int>(std::floor(zone.MyPoint.MyX + 0.5));
				scratch.MyTargets.clear();
				for (const auto id : _MyAllyIds)
				{
					const auto& ally = Unit(id); const auto point = RulePosition(ally);
					if (!ally.MyAlive || ally.MyKind == UnitKind::DEVICE || (id != _unit && ally.MyStatuses.Has(CombatStatus::ISOLATED)) ||
						std::abs(static_cast<int>(std::floor(point.MyY + 0.5)) - row) > 1 || std::abs(static_cast<int>(std::floor(point.MyX + 0.5)) - column) > 1) continue;
					scratch.MyTargets.push_back(id);
				}
				for (const auto id : scratch.MyTargets) (void)AddBuff(id, {.MyKey = "thorn2:bastion", .MyDuration = 0.5,
					.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::DEFENSE_FLAT, .MyValue = kit.MyDefense}}});
				if (std::isgreaterequal(zone.MyAccumulator, 1 - 1e-9))
				{
					zone.MyAccumulator -= 1; const auto heal = unit.MyStats.MyAttack * kit.MyHealingScale;
					if (std::isgreater(heal, 0)) for (const auto id : scratch.MyTargets) if (std::isless(Unit(id).MyHealth, Unit(id).MyStats.MyMaxHealth)) (void)Heal(_unit, id, heal);
				}
				continue;
			}
			if (kit.MySkill == Thorn2SkillKind::SEA)
			{
				if (zone.MyAnchor && Unit(zone.MyAnchor).MyAlive) zone.MyPoint = RulePosition(Unit(zone.MyAnchor));
				const auto steps = std::min(kit.MyMaxSteps, std::floor((zone.MyElapsed - 0.25 + 1e-9) / kit.MyInterval));
				FoesInRadius(zone.MyPoint, 1.5, scratch.MyTargets);
				for (const auto id : scratch.MyTargets)
				{
					const auto found = std::ranges::find(scratch.MySeen, id);
					if (found == scratch.MySeen.end()) { scratch.MySeen.push_back(id); scratch.MyBuffIds.push_back(static_cast<std::uint64_t>(steps)); }
					else { auto& value = scratch.MyBuffIds[static_cast<std::size_t>(found - scratch.MySeen.begin())]; value = std::max(value, static_cast<std::uint64_t>(steps)); }
				}
				zone.MyBurnTargets.clear();
				if (std::isgreaterequal(zone.MyAccumulator, kit.MyInterval - 1e-9))
				{
					zone.MyAccumulator -= kit.MyInterval; zone.MyBurnScale = kit.MyDamageRamp.Value(steps);
					zone.MyBurnTargets.assign(scratch.MyTargets.begin(), scratch.MyTargets.end());
				}
				continue;
			}
			zone.MyPoint.MyX = std::clamp(zone.MyPoint.MyX + zone.MyVelocity.MyX * 0.25, static_cast<double>(rect.MyFirstColumn), static_cast<double>(rect.MyLastColumn));
			zone.MyPoint.MyY = std::clamp(zone.MyPoint.MyY + zone.MyVelocity.MyY * 0.25, static_cast<double>(rect.MyFirstRow), static_cast<double>(rect.MyLastRow));
			const auto radius = kit.MyRadius + kit.MyGrowth * zone.MyElapsed;
			FoesInRadius(zone.MyPoint, radius, scratch.MyTargets);
			std::erase_if(scratch.MyTargets, [&](UnitId _id) { return Unit(_id).Flying(); });
			for (const auto id : scratch.MyTargets) (void)AddBuff(id, {.MyKey = "thorn2:rot", .MyDuration = 0.5,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALING_TAKEN_MULTIPLIER, .MyValue = kit.MyHealingMultiplier}}});
			if (std::isless(zone.MyAccumulator, 1 - 1e-9)) continue;
			zone.MyAccumulator -= 1;
			for (const auto id : scratch.MyTargets) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * kit.MyDamageScale, .MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
			const auto heal = unit.MyStats.MyAttack * kit.MyHealingScale;
			if (std::isgreater(heal, 0))
			{
				scratch.MyTargets.clear();
				for (const auto id : _MyAllyIds)
				{
					const auto& ally = Unit(id); const auto distance = Distance(RulePosition(ally), zone.MyPoint);
					if (ally.MyAlive && !ally.MyHidden && ally.MyKind != UnitKind::DEVICE && std::islessequal(distance * distance, radius * radius + 1e-9)) scratch.MyTargets.push_back(id);
				}
				for (const auto id : scratch.MyTargets) if (std::isless(Unit(id).MyHealth, Unit(id).MyStats.MyMaxHealth)) (void)Heal(_unit, id, heal);
			}
		}
		for (std::size_t i = 0; i < scratch.MySeen.size(); ++i)
		{
			const auto id = scratch.MySeen[i]; const auto steps = static_cast<double>(scratch.MyBuffIds[i]);
			if (Unit(id).MyAlive) (void)AddBuff(id, {.MyKey = "thorn2:sea", .MyDuration = 0.5, .MyModifiers = std::vector<AttributeChange>{
				{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyAttackRamp.Value(steps)}, {.MyAttribute = Attribute::DEFENSE_PERCENT, .MyValue = kit.MyDefenseRamp.Value(steps)},
				{.MyAttribute = Attribute::RESISTANCE_MULTIPLIER, .MyValue = 1 + kit.MyResistanceRamp.Value(steps)}}});
		}
		for (std::size_t n = 0; n < unit.MyAlchemyZones.size(); ++n)
		{
			auto& zone = unit.MyAlchemyZones[n];
			for (const auto id : zone.MyBurnTargets) if (Unit(id).MyAlive) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * zone.MyBurnScale, .MyType = DamageType::ARTS,
				.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
			zone.MyBurnTargets.clear();
		}
		std::erase_if(unit.MyAlchemyZones, [](const AlchemyZone& _zone) { return !std::isless(_zone.MyElapsed, _zone.MyDuration - 1e-9); });
		if (unit.MyAlive && !unit.MyAlchemyZones.empty() && std::isgreater(kit.MyZoneSp, 0)) (void)AddBuff(_unit, {.MyKey = "thorn2:module", .MyDuration = 0.5,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::SP_RECOVERY_FLAT, .MyValue = kit.MyZoneSp}}});
	}
}
