#ifndef STRONGHOLD_SIMULATION_CHOICE_EFFECTS_HPP
#define STRONGHOLD_SIMULATION_CHOICE_EFFECTS_HPP
#include <optional>
#include <string>
#include <stronghold/domain/choice_conditions.hpp>
#include <stronghold/simulation/attributes.hpp>
#include <vector>

namespace Stronghold
{
	enum class ChoiceEnemyRank { ANY, NORMAL, ELITE, BOSS };
	struct ChoiceEnemyModifiers
	{
		ChoiceEnemyRank MyRank{};
		std::vector<AttributeChange> MyModifiers{};
	};

	// 输入拥有全部字符串和修正数组；Battle 移动或静态规则目录销毁不会留下借用。
	// 同一次选卡使用一个唯一键，重复获得同一卡片使用不同键，因此增益自然叠加。
	struct BattleChoiceEffect
	{
		std::string MyKey{};
		ChoiceGate MyGate{};
		std::optional<bool> MyPreparationPassed{};
		double MySelfHeal{};
		std::vector<AttributeChange> MyOperatorModifiers{};
		std::vector<AttributeChange> MyFullHealthModifiers{};
		std::vector<ChoiceEnemyModifiers> MyEnemies{};
	};
}
#endif
