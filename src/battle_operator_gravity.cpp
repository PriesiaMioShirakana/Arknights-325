#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::AglinaSkill(UnitId _unit, const AglinaKit& _kit, ContentEvent& _event)
	{
		if (_kit.MySkill != AglinaSkillKind::GRAVITY) return;
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			for (std::size_t i = 0, count = _MyEnemyIds.size(); i < count; ++i)
			{
				const auto id = _MyEnemyIds[i]; const auto& buffs = Unit(id).MyBuffs;
				const auto found = std::ranges::find(buffs, "aglina:weightless", [](const auto& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
				if (found != buffs.end() && found->MyDefinition.MySource == _unit) (void)RemoveBuff(id, found->MyId);
			}
			return;
		}
		if (_event.MyKind == ContentEventKind::SKILL_START) unit.MyGravityAccumulator = 0;
		else if (_event.MyKind == ContentEventKind::SKILL_TICK)
		{
			unit.MyGravityAccumulator += _event.MyDelta;
			if (std::isless(unit.MyGravityAccumulator, 0.25)) return;
			unit.MyGravityAccumulator = 0;
		}
		else return;
		for (std::size_t i = 0, count = _MyEnemyIds.size(); i < count; ++i)
		{
			const auto id = _MyEnemyIds[i]; const auto& enemy = Unit(id);
			if (!enemy.MyAlive || std::ranges::any_of(enemy.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "aglina:weightless"; })) continue;
			(void)AddBuff(id, {.MyKey = "aglina:weightless", .MySource = _unit, .MyDuration = std::max(0.1, unit.MySkill.MyTimeLeft),
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::MASS_FLAT, .MyValue = -1}}, .MyStatus = CombatStatus::WEIGHTLESS});
		}
	}
}
