#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::PrecisionBeforeAttack(CombatUnit& _unit, const PrecisionKit& _kit, std::vector<UnitId>& _targets)
	{
		_unit.MyPrecisionBoost = std::isless(_MyRandom.Next(), _kit.MyProbability);
		if (!_unit.MyPrecisionBoost) return;
		auto& scratch = AcquireAttackScratch();
		struct Guard
		{
			std::size_t& MyDepth;

			~Guard() { --MyDepth; }
		};
		const Guard guard{.MyDepth = _MyAttackDepth};
		const auto& profile = EffectiveAttack(_unit);
		for (const auto id : _MyEnemyIds)
			if (!std::ranges::contains(_targets, id) && TargetableEnemy(Unit(id), profile) && InRuleRange(_unit.MyId, id)) scratch.MyTargets.push_back(id);
		SortOperatorTargets(_unit.MyId, scratch.MyTargets, 1, &profile);
		if (!scratch.MyTargets.empty()) _targets.push_back(scratch.MyTargets.front());
	}

	void Battle::PrecisionDeploySp(UnitId _unit)
	{
		const auto& target = Unit(_unit);
		if (target.MySide != UnitSide::ALLY || target.MyKind != UnitKind::OPERATOR || target.MyDefinition.MyOperatorProfession != OperatorProfession::PIONEER ||
			target.MyDefinition.MySkill.MyKind == SkillKind::NONE || (target.MySkill.MyActive && IsTimedSkill(target.MyDefinition.MySkill.MyKind))) return;
		for (const auto id : _MyInitialSpCarriers)
		{
			const auto& source = Unit(id);
			if (source.MyOperatorHooksReleased || source.MyOwner != target.MyOwner) continue;
			(void)GainSp(_unit, std::get<PrecisionKit>(*source.MyDefinition.MyOperatorKit).MyInitialSp, SpReason::INITIAL);
		}
	}

	void Battle::PrecisionSkill(UnitId _unit, const PrecisionKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::SKILL_START && _kit.MyProgressive) unit.MyPrecisionHits = 0;
		if (_event.MyKind == ContentEventKind::ATTACK && _kit.MyVolleyExtra) unit.MyPrecisionBoost = false;
		if (!_event.MyDamage.MyIsAttack || !_event.MyTarget || Unit(_event.MyTarget).MySide != UnitSide::ENEMY) return;
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE)
		{
			if (_kit.MyVolleyExtra)
			{
				if (std::isgreater(_kit.MyLowHealthRatio, 0) && std::isgreater(_kit.MyLowHealthScale, 0) &&
					std::isless(Unit(_event.MyTarget).MyHealth / Unit(_event.MyTarget).MyStats.MyMaxHealth, _kit.MyLowHealthRatio)) _event.MyDamage.MyAmount *= _kit.MyLowHealthScale;
				if (unit.MyPrecisionBoost) _event.MyDamage.MyMultiplier *= _kit.MyScale;
			}
			else if (std::isless(_MyRandom.Next(), _kit.MyProbability))
			{
				_event.MyDamage.MyMultiplier *= _kit.MyScale;
				_event.MyDamage.MyPrecisionCritical = true;
			}
		}
		else if (_event.MyKind == ContentEventKind::DAMAGED && !_kit.MyVolleyExtra)
		{
			if (_kit.MyProgressive && unit.MySkill.MyActive) ++unit.MyPrecisionHits;
			if (_event.MyDamage.MyPrecisionCritical && Unit(_event.MyTarget).MyAlive)
				(void)ApplyStatus(_event.MyTarget, CombatStatus::STUN, _kit.MyStun, _unit);
		}
	}
}
