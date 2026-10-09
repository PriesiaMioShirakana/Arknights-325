#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	bool Battle::UsesInitialPosition(const CombatUnit& _unit) const noexcept
	{
		return _MyInput.MyRulePositionMode == RulePositionMode::INITIAL && _unit.MySide == UnitSide::ALLY;
	}

	WorldPoint Battle::RulePosition(const CombatUnit& _unit) const noexcept
	{
		return UsesInitialPosition(_unit) ? _unit.MyHome : _unit.MyPosition;
	}

	const std::bitset<FieldTiles>& Battle::RuleRange(const CombatUnit& _unit) const noexcept
	{
		return UsesInitialPosition(_unit) && _unit.MyKind != UnitKind::DEVICE ? _unit.MyInitialRuleRangeMask : _unit.MyRangeMask;
	}

	std::span<const int> Battle::RuleRangeKeys(const CombatUnit& _unit) const noexcept
	{
		return UsesInitialPosition(_unit) && _unit.MyKind != UnitKind::DEVICE ? _unit.MyInitialRuleRangeKeys : _unit.MyRangeKeys;
	}

	bool Battle::InRuleRange(UnitId _source, UnitId _target) const
	{
		const auto& target = Unit(_target);
		return BodyInRange(target, RuleRange(_source), RulePosition(target));
	}
}
