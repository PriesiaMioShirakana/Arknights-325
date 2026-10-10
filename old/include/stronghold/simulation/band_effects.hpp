#ifndef STRONGHOLD_SIMULATION_BAND_EFFECTS_HPP
#define STRONGHOLD_SIMULATION_BAND_EFFECTS_HPP
#include <array>
#include <string>

namespace Stronghold
{
	enum class BandBattleKind
	{
		ACTIVE_BOND_STATS, REVIVE, DEATH_LAYERS, DEPLOY_COOLDOWN, FRONT_SHIELD,
		SKILL_END_SP, DEATH_ATTACK, WEAKNESS, SAME_NAME_ATTACK, ELITE_STATS
	};

	struct BandStatStep
	{
		unsigned MyCount{};
		double MyAttack{};
		double MyHealth{};
	};

	struct BandBattleParameters
	{
		BandBattleKind MyKind{};
		double MyValue{};
		double MyHealth{};
		unsigned MyMaximum{};
		bool MyByTier{};
		std::array<BandStatStep, 9> MySteps{};
		unsigned MyStepCount{};
	};

	// 单局输入拥有字符串；规则参数固定大小，进入战斗后不查询动态黑板。
	struct BattleBandEffect
	{
		std::string MyKey{};
		std::string MyBond{};
		BandBattleParameters MyParameters{};
	};
}
#endif
