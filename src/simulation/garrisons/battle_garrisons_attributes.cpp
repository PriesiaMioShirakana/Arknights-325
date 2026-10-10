#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::RefreshGarrisons()
	{
		for (const auto kind : {BattleGarrisonKind::ATTRIBUTES_BY_BOND, BattleGarrisonKind::REDEPLOY_BY_BOND, BattleGarrisonKind::DEPLOY_ATTRIBUTES, BattleGarrisonKind::STATUS_DAMAGE})
			for (const auto index : _MyGarrisonGroups[static_cast<unsigned>(kind)]) RefreshGarrison(index);
	}

	void BattleCore::RefreshGarrison(std::size_t _index)
	{
		auto& state = _MyGarrisons[_index]; const auto& rule = _MyInput.MyGarrisonRules.MyEffects[state.MyRule];
		if (state.MyRetired) return;
		const auto steps = GarrisonSteps(state);
		if (steps == state.MySteps) return;
		state.MySteps = steps;
		if (rule.MyKind == BattleGarrisonKind::STATUS_DAMAGE) return;
		if (rule.MyKind == BattleGarrisonKind::DEPLOY_ATTRIBUTES)
		{
			const auto& buffs = Unit(state.MyUnit).MyBuffs;
			const auto found = std::ranges::find_if(buffs, [&](const CombatBuff& b) { return b.MyDefinition.MyKey == state.MyKey; });
			if (found != buffs.end() && found->MyRemaining > 0) ApplyGarrisonAttributes(_index, found->MyRemaining);
			return;
		}
		ApplyGarrisonAttributes(_index);
	}

	void BattleCore::ApplyGarrisonAttributes(std::size_t _index, double _duration)
	{
		const auto& state = _MyGarrisons[_index]; const auto& rule = _MyInput.MyGarrisonRules.MyEffects[state.MyRule];
		if (state.MyRetired) return;
		const bool timed = rule.MyKind == BattleGarrisonKind::DEPLOY_ATTRIBUTES;
		const bool reader = timed || rule.MyKind == BattleGarrisonKind::ATTRIBUTES_BY_BOND || rule.MyKind == BattleGarrisonKind::REDEPLOY_BY_BOND;
		if (reader && (!(state.MySteps > 0) || (timed && !(_duration > 0)))) { (void)RemoveBuff(state.MyUnit, state.MyKey); return; }
		std::vector<AttributeChange> mods; mods.reserve(6);
		const auto add = [&](Attribute type, double n) { if (n != 0) mods.push_back({type, n}); };
		if (rule.MyKind == BattleGarrisonKind::REDEPLOY_BY_BOND)
			mods.push_back({Attribute::REDEPLOY_MULTIPLIER, std::max(0.05, 1 + rule.MyRedeploy * state.MySteps)});
		else if (rule.MyKind == BattleGarrisonKind::BASE_ATTRIBUTES)
		{
			if (rule.MyAttack > 0) mods.push_back({Attribute::ATTACK_MULTIPLIER, rule.MyAttack});
			if (rule.MyHealth > 0) mods.push_back({Attribute::HEALTH_MULTIPLIER, rule.MyHealth});
			if (rule.MyDefense > 0) mods.push_back({Attribute::DEFENSE_MULTIPLIER, rule.MyDefense});
		}
		else
		{
			const double k = reader ? state.MySteps : 1;
			add(timed ? Attribute::ATTACK_FLAT : Attribute::ATTACK_PERCENT, rule.MyAttack * k);
			add(timed ? Attribute::HEALTH_FLAT : Attribute::HEALTH_PERCENT, rule.MyHealth * k);
			if (!timed)
			{
				add(Attribute::DEFENSE_PERCENT, rule.MyDefense * k); add(Attribute::ATTACK_SPEED, rule.MyAttackSpeed * k);
				add(Attribute::HEALTH_REGEN, rule.MyHpRegen * k); add(Attribute::SP_RECOVERY_FLAT, rule.MySpRegen * k);
			}
		}
		(void)AddBuff(state.MyUnit, {.MyKey = state.MyKey, .MyDuration = timed ? _duration : std::numeric_limits<double>::infinity(),
			.MyModifiers = std::move(mods), .MyPersistent = !timed, .MyAllowDead = !timed});
	}
}
