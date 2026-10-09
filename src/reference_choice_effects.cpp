#include <stronghold/adapters/reference_choices.hpp>

namespace Stronghold
{
	std::vector<BattleChoiceEffect> MakeChoiceBattleEffects(const ChoiceRewardView& _view)
	{
		std::vector<BattleChoiceEffect> result; result.reserve(_view.MyBattleEffects.size());
		const auto rules = ReferenceChoiceBattleRules();
		for (const auto& state : _view.MyBattleEffects)
		{
			const auto rule = std::ranges::find(rules, state.MyRule, &ChoiceBattleRule::MyId);
			if (rule == rules.end()) continue; // 纯地形卡由地图构建器应用，不创建空的属性效果。
			auto& effect = result.emplace_back(BattleChoiceEffect{.MyKey = state.MyId, .MyGate = rule->MyGate,
				.MyPreparationPassed = state.MyPreparationPassed, .MySelfHeal = rule->MySelfHeal,
				.MyOperatorModifiers = {rule->MyOperatorModifiers.begin(), rule->MyOperatorModifiers.end()},
				.MyFullHealthModifiers = {rule->MyFullHealthModifiers.begin(), rule->MyFullHealthModifiers.end()}});
			effect.MyEnemies.reserve(rule->MyEnemies.size());
			for (const auto& enemy : rule->MyEnemies) effect.MyEnemies.emplace_back(ChoiceEnemyModifiers{.MyRank = enemy.MyRank,
				.MyModifiers = {enemy.MyModifiers.begin(), enemy.MyModifiers.end()}});
		}
		return result;
	}
}
