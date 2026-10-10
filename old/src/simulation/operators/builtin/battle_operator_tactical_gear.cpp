#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct TacticalScratchGuard
		{
			std::size_t& MyDepth;

			~TacticalScratchGuard() { --MyDepth; }
		};
	}

	std::optional<WorldPoint> BattleCore::TacticalSummonTile(UnitId _unit) const
	{
		const auto& unit = Unit(_unit);
		const auto origin = RulePosition(unit);
		const int row = static_cast<int>(std::floor(origin.MyY + 0.5)), column = static_cast<int>(std::floor(origin.MyX + 0.5));
		const auto forward = RotateOffset({.MyColumn = 1}, unit.MyFacing);
		UnitId leading = 0;
		double remaining = std::numeric_limits<double>::infinity();
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (!enemy.MyAlive || enemy.MyHidden) continue;
			const auto distance = RemainingDistance(id);
			if (std::isless(distance, remaining - 1e-9) || (leading && std::islessequal(std::abs(distance - remaining), 1e-9) && enemy.MySpawnSequence < Unit(leading).MySpawnSequence))
			{
				leading = id;
				remaining = distance;
			}
		}
		const auto focus = leading ? Unit(leading).MyPosition : origin;
		std::optional<WorldPoint> best;
		std::array<double, 5> bestScore{};
		for (const auto key : RuleRangeKeys(unit))
		{
			const int r = key / FieldColumns, c = key % FieldColumns;
			const WorldPoint point{.MyX = static_cast<double>(c), .MyY = static_cast<double>(r)};
			if (!FieldGrid::InBounds(r, c) || ReservedTile(point) || (_MyGrid && (!_MyGrid->InRect(r, c) || !_MyGrid->CanStand(r, c)))) continue;
			double cover = 0;
			for (const auto id : _MyEnemyIds)
			{
				const auto& enemy = Unit(id);
				if (enemy.MyAlive && !enemy.MyHidden && std::islessequal(std::abs(std::floor(enemy.MyPosition.MyY + 0.5) - r), 1) &&
					std::islessequal(std::abs(std::floor(enemy.MyPosition.MyX + 0.5) - c), 1)) ++cover;
			}
			// 覆盖人数、地面通行、距最前敌人的距离、朝向内的格序依次决定位置。
			// 最后两项使用局部坐标，使不同朝向下的同分选址保持一致。
			const std::array<double, 5> score{-cover, !_MyGrid || _MyGrid->GroundPassable(r, c, true) ? -1.0 : 0.0, Distance(point, focus),
				static_cast<double>((r - row) * forward.MyColumn - (c - column) * forward.MyRow),
				static_cast<double>((c - column) * forward.MyColumn + (r - row) * forward.MyRow)};
			bool better = !best;
			if (!better)
				for (std::size_t i = 0; i < score.size(); ++i)
				{
					if (std::isless(score[i], bestScore[i] - 1e-9)) { better = true; break; }
					if (std::isgreater(score[i], bestScore[i] + 1e-9)) break;
				}
			if (better) { best = point; bestScore = score; }
		}
		return best;
	}

	void BattleCore::RosmonTargets(CombatUnit& _unit, std::vector<UnitId>& _targets)
	{
		const auto& kit = std::get<RosmonKit>(*_unit.MyDefinition.MyOperatorKit);
		if (kit.MySkill != RosmonSkillKind::WISH || !_unit.MySkill.MyActive) return;
		const auto& profile = EffectiveAttack(_unit);
		_targets.clear();
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (enemy.MyBlockedBy && TargetableEnemy(enemy, profile) && (enemy.MyBlockedBy == _unit.MyId || InRuleRange(_unit.MyId, id))) _targets.push_back(id);
		}
		SortOperatorTargets(_unit.MyId, _targets, 0, &profile);
		const auto limit = std::max<std::size_t>(1, profile.MyMaxTargets);
		if (_targets.size() > limit) _targets.resize(limit);
	}

	void BattleCore::RosmonStable(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<RosmonKit>(*unit.MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch();
		const TacticalScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (ally.MyAlive && ally.MyOwner == unit.MyOwner && ally.MyKind == UnitKind::OPERATOR && ally.MyDefinition.MyOperatorProfession == OperatorProfession::CASTER) scratch.MyTargets.push_back(id);
		}
		if (scratch.MyTargets.empty()) return;
		const auto target = scratch.MyTargets[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MyTargets.size()))];
		(void)AddBuff(_unit, {.MyKey = "rosmon:stable", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyStableAttack}}});
		(void)AddBuff(target, {.MyKey = "rosmon:stable", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyStableAttack}}});
	}

	void BattleCore::RosmonGearTick(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyBlocking.empty()) return;
		const auto defense = unit.MyDefinition.MyTokenKit->MyBlockedDefense;
		if (!std::islessgreater(defense, 0)) return;
		auto& scratch = AcquireAttackScratch();
		const TacticalScratchGuard guard{.MyDepth = _MyAttackDepth};
		scratch.MyTargets.assign(unit.MyBlocking.begin(), unit.MyBlocking.end());
		for (const auto id : scratch.MyTargets)
			if (Unit(id).MyAlive) (void)AddBuff(id, {.MyKey = "token:rosmonGear", .MySource = _unit, .MyDuration = 0.2, .MyRefresh = BuffRefresh::EXTEND,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::DEFENSE_FLAT, .MyValue = defense}}});
	}

	void BattleCore::RosmonSkill(UnitId _unit, const RosmonKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY && std::islessgreater(_kit.MyStableAttack, 0))
			Schedule({.MyAt = Time(), .MyKind = ScheduledKind::ROSMON_STABLE, .MySource = _unit});
		if (_event.MyKind == ContentEventKind::DAMAGED && unit.MySkill.MyActive && std::isgreater(_kit.MyStun, 0) && _event.MyTarget &&
			Unit(_event.MyTarget).MyAlive && Unit(_event.MyTarget).MySide == UnitSide::ENEMY &&
			(_event.MyDamage.MyIsAttack || HasTag(_event.MyDamage.MyTags, DamageTag::AFTERSHOCK)) && std::isless(_MyRandom.Next(), _kit.MyStunChance))
			(void)ApplyStatus(_event.MyTarget, CombatStatus::STUN, _kit.MyStun, _unit);
		if (_kit.MySkill == RosmonSkillKind::NERVES)
		{
			if (_event.MyKind == ContentEventKind::SKILL_START)
			{
				unit.MyRosmonRadius = unit.MyDefinition.MyAttack.MySplashRadius;
				unit.MyRosmonShocks = unit.MyDefinition.MyProfession.MyShockCount;
				unit.MyRosmonSaved = true;
				unit.MyDefinition.MyAttack.MySplashRadius = std::max(unit.MyRosmonRadius, 1.5);
				unit.MyDefinition.MyProfession.MyShockCount = static_cast<unsigned>(std::max<std::int64_t>(0, static_cast<std::int64_t>(unit.MyRosmonShocks) + _kit.MyExtraShocks));
			}
			else if (_event.MyKind == ContentEventKind::SKILL_ENDING && unit.MyRosmonSaved)
			{
				unit.MyDefinition.MyAttack.MySplashRadius = unit.MyRosmonRadius;
				unit.MyDefinition.MyProfession.MyShockCount = unit.MyRosmonShocks;
				unit.MyRosmonSaved = false;
			}
		}
		else if (_kit.MySkill == RosmonSkillKind::WISH && _event.MyKind == ContentEventKind::SKILL_START)
		{
			const auto* body = FindTokenTemplate(_unit, _kit.MyGear);
			if (!body) return;
			// 每次召唤后重选位置；装备自行计时，技能结束不会提前移除装备。
			for (unsigned i = 0; i < 2; ++i)
				if (const auto tile = TacticalSummonTile(_unit)) (void)SpawnToken({.MyDefinition = *body, .MyPosition = *tile, .MyOwnerUnit = _unit});
		}
	}
}
