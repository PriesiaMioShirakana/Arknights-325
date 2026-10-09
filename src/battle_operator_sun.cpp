#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		constexpr std::array<RangeOffset, 5> SunCross{{{}, {.MyRow = 1}, {.MyRow = -1}, {.MyColumn = 1}, {.MyColumn = -1}}};

		struct SunScratchGuard
		{
			std::size_t& MyDepth;

			~SunScratchGuard() { --MyDepth; }
		};
	}

	UnitId Battle::LastDeployedOperator(std::size_t _owner) const
	{
		return _owner < _MyLastDeployedOperators.size() ? _MyLastDeployedOperators[_owner] : 0;
	}

	void Battle::FreeSummonTiles(UnitId _unit, std::span<const RangeOffset> _grid, std::vector<std::uint64_t>& _tiles, bool _ground) const
	{
		_tiles.clear(); const auto& unit = Unit(_unit); const auto point = RulePosition(unit);
		const int row = static_cast<int>(std::floor(point.MyY + 0.5)), column = static_cast<int>(std::floor(point.MyX + 0.5));
		for (const auto offset : _grid)
		{
			const auto rotated = RotateOffset(offset, unit.MyFacing); const auto r = row + rotated.MyRow, c = column + rotated.MyColumn;
			if ((r == row && c == column) || !FieldGrid::InBounds(r, c) || ReservedTile({.MyX = static_cast<double>(c), .MyY = static_cast<double>(r)}) ||
				(_MyGrid && (!_MyGrid->InRect(r, c) || !_MyGrid->CanStand(r, c) || (_ground && !_MyGrid->GroundPassable(r, c, true))))) continue;
			_tiles.push_back(static_cast<std::uint64_t>(FieldGrid::Key(r, c)));
		}
	}

	std::optional<WorldPoint> Battle::BestSummonTile(std::span<const std::uint64_t> _tiles) const
	{
		std::optional<WorldPoint> best; double nearest = std::numeric_limits<double>::infinity();
		for (const auto key : _tiles)
		{
			const WorldPoint point{.MyX = static_cast<double>(key % FieldColumns), .MyY = static_cast<double>(key / FieldColumns)};
			double distance = std::numeric_limits<double>::infinity();
			for (const auto id : _MyEnemyIds) if (Unit(id).MyAlive && !Unit(id).MyHidden) distance = std::min(distance, Distance(point, Unit(id).MyPosition));
			if (!best || std::isless(distance, nearest - 1e-9)) { best = point; nearest = distance; }
		}
		return best;
	}

	void Battle::Nearl2Dawn(UnitId _unit, const Nearl2Kit& _kit)
	{
		const auto& unit = Unit(_unit);
		if (!std::isgreater(_kit.MyDawnScale, 0)) return;
		auto& scratch = AcquireAttackScratch(); const SunScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyEnemyIds) if (Unit(id).MyAlive && !Unit(id).MyHidden && OperatorInGrid(_unit, id, _kit.MyDawnRange)) scratch.MyTargets.push_back(id);
		for (const auto id : scratch.MyTargets)
		{
			for (unsigned hit = 0; hit < unit.MyDawnTimes && Unit(id).MyAlive; ++hit)
				(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyDawnScale, .MyType = DamageType::TRUE_DAMAGE, .MyTags = static_cast<DamageTags>(DamageTag::TALENT)});
			if (Unit(id).MyAlive && std::isgreater(_kit.MyDawnStun, 0)) (void)ApplyStatus(id, CombatStatus::STUN, _kit.MyDawnStun, _unit);
		}
	}

	void Battle::Nearl2Fatal(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::FATAL || !_event.MyUnit || _event.MyPrevented) return;
		auto& unit = _MyUnits[Index(_event.MyUnit)]; const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<Nearl2Kit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || !kit->MyCanStand || unit.MyNearlStood || unit.MyOperatorHooksReleased) return;
		_event.MyPrevented = true; unit.MyNearlStood = true;
		(void)AddBuff(unit.MyId, {.MyKey = "nearl2:stand", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALTH_MULTIPLIER, .MyValue = kit->MyStandHealthMultiplier}, {.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = kit->MyStandSpeed}}});
		unit.MyHealth = std::max(1.0, unit.MyStats.MyMaxHealth * kit->MyStandHealth); Nearl2Dawn(unit.MyId, *kit);
	}

	void Battle::Nearl2Skill(UnitId _unit, const Nearl2Kit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		const auto previous = LastDeployedOperator(unit.MyOwner);
		const bool combo = previous && previous != _unit && std::ranges::contains(Unit(previous).MyDefinition.MyIdentity.MyBonds, "kazimierzShip");
		if (_event.MyKind == ContentEventKind::DEPLOY) { unit.MyNearlStood = false; unit.MyDawnTimes = combo ? 2 : 1; Nearl2Dawn(_unit, _kit); }
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _kit.MyDefault && unit.MySkill.MyActive && _event.MyDamage.MyIsAttack && _event.MyTarget)
		{
			const auto blocker = Unit(_event.MyTarget).MyBlockedBy;
			if (blocker && (blocker == _unit || blocker == unit.MySunSword)) _event.MyDamage.MyType = DamageType::TRUE_DAMAGE;
		}
		if (_kit.MySkill == Nearl2SkillKind::NIGHT)
		{
			if (_event.MyKind == ContentEventKind::SKILL_START)
			{
				unit.MyNearlCombo = combo;
				if (_kit.MyShieldHits && std::isgreater(unit.MyDefinition.MySkill.MyDuration, 0)) (void)AddBuff(_unit,
					{.MyKey = "nearl2:shield", .MyDuration = unit.MyDefinition.MySkill.MyDuration, .MyShield = {.MyHits = static_cast<int>(_kit.MyShieldHits)}});
			}
			if (_event.MyKind == ContentEventKind::SKILL_ENDING)
			{
				(void)RemoveBuff(_unit, "nearl2:shield");
				if (_event.MySkillReason != SkillReason::DURATION || !unit.MyAlive || unit.MyHidden) return;
				const auto multiplier = unit.MyNearlCombo ? _kit.MyComboRespawn : _kit.MyRespawnMultiplier;
				(void)Retreat(_unit, false, RemovalReason::RETREAT);
				if (std::isfinite(unit.MyRespawnAt) && std::islessgreater(multiplier, 1)) unit.MyRespawnAt = Time() + (unit.MyRespawnAt - Time()) * multiplier;
			}
			return;
		}
		if (_kit.MySkill != Nearl2SkillKind::SUN) return;
		if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			const auto sword = std::exchange(unit.MySunSword, 0);
			if (sword && Unit(sword).MyAlive) (void)Retreat(sword, true, RemovalReason::EXPIRED);
			return;
		}
		if (_event.MyKind != ContentEventKind::SKILL_START) return;
		auto& scratch = AcquireAttackScratch(); const SunScratchGuard guard{.MyDepth = _MyAttackDepth};
		FreeSummonTiles(_unit, SunCross, scratch.MyBuffIds);
		const auto tile = BestSummonTile(scratch.MyBuffIds); const auto* body = FindTokenTemplate(_unit, _kit.MySword);
		unit.MySunSword = tile && body ? SpawnToken({.MyDefinition = *body, .MyPosition = *tile, .MyOwnerUnit = _unit, .MyDuration = unit.MySkill.MyTimeLeft}) : 0;
		if (unit.MySunSword && Unit(unit.MySunSword).MyDefinition.MyTokenKit) return;
		const auto point = unit.MySunSword ? RulePosition(Unit(unit.MySunSword)) : RulePosition(unit);
		const int row = static_cast<int>(std::floor(point.MyY + 0.5)), column = static_cast<int>(std::floor(point.MyX + 0.5));
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (enemy.MyAlive && !enemy.MyHidden && std::ranges::any_of(SunCross, [&](const auto _offset) { return BodyOnTile(enemy, row + _offset.MyRow, column + _offset.MyColumn); })) scratch.MyTargets.push_back(id);
		}
		for (const auto id : scratch.MyTargets)
		{
			(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MySunScale, .MyType = DamageType::TRUE_DAMAGE, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
			if (Unit(id).MyAlive && std::isgreater(_kit.MySunStun, 0)) (void)ApplyStatus(id, CombatStatus::STUN, _kit.MySunStun, _unit);
		}
	}
}
