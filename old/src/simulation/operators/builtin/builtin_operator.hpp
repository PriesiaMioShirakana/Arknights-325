#ifndef STRONGHOLD_SIMULATION_BUILTIN_OPERATOR_HPP
#define STRONGHOLD_SIMULATION_BUILTIN_OPERATOR_HPP
#include <stronghold/simulation/operator.hpp>

namespace Stronghold
{
	// 新增内置内容的共同入口。周期回复等状态留在组件内，不扩展每个战场单位的公开状态。
	// 核心按具体类型直接调用；外部仍使用 OperatorBase 的共同接口。
	class BuiltinOperator final : public OperatorBase
	{
	public:
		BuiltinOperator(Battle& _battle, UnitId _unit);
		[[nodiscard]] static bool Supports(const CombatDefinition& _definition) noexcept;
		void HandleBuiltin(ContentEvent& _event);
		void BeforeAttack(std::vector<UnitId>& _targets);
		void EachHit(UnitId _target, const AttackProfile& _profile, bool _main);

	protected:
		void OnEvent(ContentEvent& _event) override;

	private:
		void ResetPulses();
		void TickPulses();
		void GrantDeploymentRandom();
		void IgniteResistance(UnitId _target);
		std::vector<double> _MyPulseAt;
		std::uint64_t _MyDeployment{};
	};
}
#endif
