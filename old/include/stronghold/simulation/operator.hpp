#ifndef STRONGHOLD_SIMULATION_OPERATOR_HPP
#define STRONGHOLD_SIMULATION_OPERATOR_HPP
#include <stronghold/simulation/unit.hpp>

namespace Stronghold
{
	struct ContentEvent;

	// 干员的公共操作统一进入战斗核心，派生类按事件增加规则，不另建一套状态。
	class OperatorBase : public Stronghold::Unit
	{
	public:
		using Stronghold::Unit::Unit;
		[[nodiscard]] UnitId Unit() const noexcept { return Id(); }

		bool Deploy(bool _free = true, std::optional<WorldPoint> _tile = std::nullopt, bool _keepSp = false);
		void Retreat(bool _permanent = false, RemovalReason _reason = RemovalReason::RETREAT);
	};
}
#endif
