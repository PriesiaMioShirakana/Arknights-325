#include "battle_core.hpp"

namespace Stronghold
{
	bool BattleCore::ApplyStrongest(UnitId _target, std::string _key, double _duration, BuffStrength _strength, UnitId _source)
	{
		auto& unit = _MyUnits[Index(_target)];
		std::vector<AttributeChange> modifiers; modifiers.reserve(_strength.MySecondAttribute ? 2 : 1);
		modifiers.push_back({_strength.MyAttribute, _strength.MyOffset + _strength.MyScale * _strength.MyValue});
		if (_strength.MySecondAttribute) modifiers.push_back({*_strength.MySecondAttribute, _strength.MyOffset + _strength.MyScale * _strength.MyValue});
		BuffDefinition next{.MyKey = std::move(_key), .MySource = _source, .MyDuration = _duration,
			.MyModifiers = std::move(modifiers), .MyStrength = _strength};
		ValidateBuff(next);
		if (_source) (void)Index(_source);
		if (!unit.MyAlive || Finished() || !(_duration > 0)) return false;
		const auto found = std::ranges::find(unit.MyBuffs, next.MyKey, [](const CombatBuff& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
		if (found == unit.MyBuffs.end() || !found->MyDefinition.MyStrength) { (void)AddBuff(_target, std::move(next)); return true; }
		auto& old = *found->MyDefinition.MyStrength;
		const auto oldEnd = Time() + found->MyRemaining, newEnd = Time() + _duration;
		if (std::abs(_strength.MyValue) > std::abs(old.MyValue) + 1e-12)
		{
			if (oldEnd > newEnd) next.MyStrength->MyTail = BuffStrengthTail{old.MyValue, oldEnd};
			if (old.MyTail && old.MyTail->MyUntil > newEnd && (!next.MyStrength->MyTail || old.MyTail->MyUntil > next.MyStrength->MyTail->MyUntil))
				next.MyStrength->MyTail = old.MyTail;
			(void)AddBuff(_target, std::move(next));
		}
		else if (std::abs(_strength.MyValue) < std::abs(old.MyValue) - 1e-12)
		{
			if (newEnd > oldEnd && (!old.MyTail || newEnd > old.MyTail->MyUntil)) old.MyTail = BuffStrengthTail{_strength.MyValue, newEnd};
		}
		else if (_duration > found->MyRemaining)
		{
			found->MyRemaining = _duration;
			found->MyDefinition.MyDuration = std::max(found->MyDefinition.MyDuration, _duration);
		}
		return true;
	}
}
