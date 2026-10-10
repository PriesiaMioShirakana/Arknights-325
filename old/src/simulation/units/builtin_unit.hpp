#ifndef STRONGHOLD_SIMULATION_BUILTIN_UNIT_HPP
#define STRONGHOLD_SIMULATION_BUILTIN_UNIT_HPP
#include <stronghold/simulation/unit.hpp>

namespace Stronghold
{
	// 内置对象原位存入 variant，自动行动和结算继续静态调用；CUSTOM 才堆分配。
	template <class _Base>
	class BuiltinUnit final : public _Base
	{
	public:
		using _Base::_Base;
	};

	using BuiltinEnemy = BuiltinUnit<EnemyBase>;
	using BuiltinSummon = BuiltinUnit<SummonBase>;
	using BuiltinDevice = BuiltinUnit<DeviceBase>;
}
#endif
