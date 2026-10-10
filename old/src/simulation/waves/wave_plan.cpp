#include <stronghold/simulation/wave_plan.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Stronghold
{
	std::vector<EnemySpawn> ExpandWavePlan(const WavePlan& _plan)
	{
		if (_plan.MyPlannerId.empty()) throw std::invalid_argument("wave plan requires a planner id");
		std::size_t count = 0;
		for (const auto& order : _plan.MyOrders)
		{
			if (!std::isfinite(order.MyInterval) || std::isless(order.MyInterval, 0) ||
				!std::isfinite(order.MyTemplate.MyTime) || std::isless(order.MyTemplate.MyTime, 0))
				throw std::invalid_argument("invalid wave plan timing");
			if (order.MyCount > std::numeric_limits<std::size_t>::max() - count) throw std::length_error("wave plan is too large");
			count += order.MyCount;
		}
		std::vector<EnemySpawn> spawns;
		spawns.reserve(count);
		for (const auto& order : _plan.MyOrders)
			for (unsigned i = 0; i < order.MyCount; ++i)
			{
				const auto time = order.MyTemplate.MyTime + i * order.MyInterval;
				if (!std::isfinite(time)) throw std::invalid_argument("wave plan time overflow");
				auto& spawn = spawns.emplace_back(order.MyTemplate);
				spawn.MyTime = time;
				spawn.MyPlannerId = _plan.MyPlannerId;
			}
		return spawns;
	}
}
