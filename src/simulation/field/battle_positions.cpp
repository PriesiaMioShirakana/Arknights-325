#include "battle_core.hpp"

namespace Stronghold
{
	bool BattleCore::UsesInitialPosition(const CombatUnit& _unit) const noexcept
	{
		return _MyInput.MyRulePositionMode == RulePositionMode::INITIAL && _unit.MySide == UnitSide::ALLY;
	}

	WorldPoint BattleCore::RulePosition(const CombatUnit& _unit) const noexcept
	{
		return UsesInitialPosition(_unit) ? _unit.MyHome : _unit.MyPosition;
	}

	const std::bitset<FieldTiles>& BattleCore::RuleRange(const CombatUnit& _unit) const noexcept
	{
		return UsesInitialPosition(_unit) && _unit.MyKind != UnitKind::DEVICE ? _unit.MyInitialRuleRangeMask : _unit.MyRangeMask;
	}

	std::span<const int> BattleCore::RuleRangeKeys(const CombatUnit& _unit) const noexcept
	{
		return UsesInitialPosition(_unit) && _unit.MyKind != UnitKind::DEVICE ? _unit.MyInitialRuleRangeKeys : _unit.MyRangeKeys;
	}

	bool BattleCore::InRuleRange(UnitId _source, UnitId _target) const
	{
		const auto& target = Unit(_target);
		return BodyInRange(target, RuleRange(_source), RulePosition(target));
	}
}
