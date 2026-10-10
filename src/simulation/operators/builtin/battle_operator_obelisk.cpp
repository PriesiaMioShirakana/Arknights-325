#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::TokenBurst(UnitId _unit, UnitId _credit, double _scale, double _stun, std::span<const RangeOffset> _range, DamageType _type, unsigned _hits)
	{
		constexpr std::array<RangeOffset, 9> Nine{{{.MyRow = -1, .MyColumn = -1}, {.MyRow = -1}, {.MyRow = -1, .MyColumn = 1},
			{.MyColumn = -1}, {}, {.MyColumn = 1}, {.MyRow = 1, .MyColumn = -1}, {.MyRow = 1}, {.MyRow = 1, .MyColumn = 1}}};
		auto& scratch = AcquireAttackScratch();
		struct Guard
		{
			std::size_t& MyDepth;

			~Guard() { --MyDepth; }
		};
		const Guard guard{.MyDepth = _MyAttackDepth};
		const auto range = _range.empty() ? std::span<const RangeOffset>(Nine) : _range;
		const bool intrinsic = _credit == _unit;
		if (intrinsic)
		{
			for (const auto id : _MyEnemyIds) if (Unit(id).MyAlive && !Unit(id).MyHidden && OperatorInGrid(_unit, id, range)) scratch.MyTargets.push_back(id);
		}
		else OperatorEnemiesInGrid(_unit, range, scratch.MyTargets);
		const auto& token = Unit(_unit);
		const auto owner = token.MyOwnerUnit ? token.MyOwnerUnit : _unit;
		const auto attack = Unit(owner).MyStats.MyAttack;
		for (const auto id : scratch.MyTargets)
		{
			for (unsigned hit = 0; hit < _hits && Unit(id).MyAlive && std::isgreater(_scale, 0); ++hit) (void)DealDamage(_credit, id, {.MyAmount = (intrinsic ? attack : Unit(owner).MyStats.MyAttack) * _scale, .MyType = _type,
				.MyCanDodge = !intrinsic, .MyTags = (intrinsic ? DamageTag::SUMMON : DamageTag::SKILL) | DamageTag::BURST, .MyIsSkill = true});
			if (Unit(id).MyAlive && std::isgreater(_stun, 0)) (void)ApplyStatus(id, CombatStatus::STUN, _stun, _credit);
		}
	}

	void BattleCore::BeewaxSummon(UnitId _unit, const BeewaxKit& _kit)
	{
		const auto& unit = Unit(_unit);
		const auto* token = FindTokenTemplate(_unit, _kit.MyToken);
		if (!token) return;
		auto& scratch = AcquireAttackScratch();
		struct Guard
		{
			std::size_t& MyDepth;

			~Guard() { --MyDepth; }
		};
		const Guard guard{.MyDepth = _MyAttackDepth};
		GenericEnemies(_unit, scratch.MyTargets, 1, true);
		UnitId reference = scratch.MyTargets.empty() ? 0 : scratch.MyTargets.front();
		if (!reference)
			for (const auto id : _MyEnemyIds)
				if (Unit(id).MyAlive && !Unit(id).MyHidden && (!reference || std::isless(RemainingDistance(id), RemainingDistance(reference)))) reference = id;
		const auto origin = reference ? Unit(reference).MyPosition : RulePosition(unit);
		std::optional<WorldPoint> best; double nearest = std::numeric_limits<double>::infinity();
		for (const auto key : RuleRangeKeys(unit))
		{
			const int row = key / FieldColumns, column = key % FieldColumns;
			const WorldPoint point{.MyX = static_cast<double>(column), .MyY = static_cast<double>(row)};
			if ((_MyGrid && (!_MyGrid->InRect(row, column) || !_MyGrid->CanStand(row, column))) || ReservedTile(point)) continue;
			if (std::ranges::any_of(_MyAllyIds, [&](UnitId _id)
			{
				const auto& ally = Unit(_id);
				return !ally.MyRemoved && ally.MyKind != UnitKind::DEVICE && std::isless(Distance(ally.MyHome, point), 1e-9);
			})) continue;
			const auto distance = Distance(point, origin) + (_MyGrid && !_MyGrid->GroundPassable(row, column) ? 5 : 0);
			if (std::isless(distance, nearest - 1e-9)) { nearest = distance; best = point; }
		}
		if (!best) return;
		auto body = *token;
		const auto life = body.MyTokenKit && body.MyTokenKit->MyLifetimeSpecified ? body.MyTokenKit->MyLifetime : _kit.MyLifetime;
		const auto range = body.MyTokenKit ? body.MyTokenKit->MyBurstRange : std::span<const RangeOffset>{};
		body.MySkill = {}; body.MyGenericSkill = nullptr; body.MyAttack.MyDisabled = true;
		if (body.MyTokenKit) { body.MyTokenKit->MyBurstSuppressed = true; body.MyTokenKit->MyLifetime = 0; }
		const auto spawned = SpawnToken({.MyDefinition = std::move(body), .MyPosition = *best, .MyOwnerUnit = _unit, .MyDuration = life});
		if (spawned) TokenBurst(spawned, _unit, _kit.MyBurstScale, _kit.MyStun, range);
	}

	void BattleCore::BeewaxSkill(UnitId _unit, const BeewaxKit& _kit, const ContentEvent& _event)
	{
		const auto& unit = Unit(_unit);
		if (_event.MyKind == ContentEventKind::DEPLOY || _event.MyKind == ContentEventKind::SKILL_START || _event.MyKind == ContentEventKind::SKILL_END)
		{
			if (unit.MyAlive && !unit.MySkill.MyActive) (void)AddBuff(_unit, {.MyKey = "beewax:regen",
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALTH_REGEN_RATIO, .MyValue = _kit.MyRegenRatio}}});
			else (void)RemoveBuff(_unit, "beewax:regen");
		}
		if (!_kit.MySummon) return;
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			if (std::islessgreater(_kit.MyKeepDefense, 0) || std::islessgreater(_kit.MyKeepResistance, 0))
				(void)AddBuff(_unit, {.MyKey = "beewax:keep", .MyModifiers = std::vector<AttributeChange>{
					{.MyAttribute = Attribute::DEFENSE_PERCENT, .MyValue = _kit.MyKeepDefense}, {.MyAttribute = Attribute::RESISTANCE_FLAT, .MyValue = _kit.MyKeepResistance}}});
			BeewaxSummon(_unit, _kit);
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING) (void)RemoveBuff(_unit, "beewax:keep");
	}
}
