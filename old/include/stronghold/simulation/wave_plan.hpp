#ifndef STRONGHOLD_SIMULATION_WAVE_PLAN_HPP
#define STRONGHOLD_SIMULATION_WAVE_PLAN_HPP
#include <stronghold/simulation/combat_types.hpp>

namespace Stronghold
{
	// 规划者选择敌人定义、路线、数量与出现时间；单位由战斗系统自动行动。
	// 模板中的实际持有者、漏怪结算玩家与规划者身份相互独立。
	struct WaveSpawnOrder
	{
		EnemySpawn MyTemplate{};
		unsigned MyCount{1};
		double MyInterval{};
	};

	struct WavePlan
	{
		std::string MyPlannerId{};
		std::vector<WaveSpawnOrder> MyOrders;
	};

	// 按订单顺序展开，战场构造时统一按出现时间稳定排序。模板中的借用定义数据
	// 仍须覆盖战场生命周期；敌人目录、路线许可和预算由未来的比赛模式校验。
	[[nodiscard]] std::vector<EnemySpawn> ExpandWavePlan(const WavePlan& _plan);
}
#endif
