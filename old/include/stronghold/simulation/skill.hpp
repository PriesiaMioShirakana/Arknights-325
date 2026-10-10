#ifndef STRONGHOLD_SIMULATION_SKILL_HPP
#define STRONGHOLD_SIMULATION_SKILL_HPP
#include <stronghold/simulation/combat_types.hpp>

namespace Stronghold
{
	class Battle;
	class BattleCore;

	// 共用状态机直接管理战场中的技能状态；注册派生类仅扩展阶段钩子。
	// Battle::Skill 返回的对象跨召唤和 Battle 移动保持稳定，战场析构后失效。
	class SkillBase
	{
	public:
		SkillBase(Battle& _battle, UnitId _unit) noexcept;
		virtual ~SkillBase() = default;
		SkillBase(const SkillBase&) = delete;
		SkillBase& operator=(const SkillBase&) = delete;

		[[nodiscard]] UnitId Unit() const noexcept { return _MyUnit; }

		[[nodiscard]] const SkillDefinition& Definition() const;
		[[nodiscard]] const SkillState& State() const;
		[[nodiscard]] const std::bitset<FieldTiles>& Range() const;
		[[nodiscard]] bool TriggerSatisfied() const;
		void SetTrigger(SkillTrigger _trigger, std::vector<RangeOffset> _range = {});
		[[nodiscard]] double SpCost() const;
		[[nodiscard]] double SpTotal() const;
		[[nodiscard]] bool CanStart(bool _free = false, SkillReason _reason = SkillReason::MANUAL) const;
		bool Start(bool _free = false, SkillReason _reason = SkillReason::MANUAL);
		void End(SkillReason _reason = SkillReason::STOPPED);
		void Tick();
		void Reset(bool _initial = false, std::optional<double> _carrySp = std::nullopt);
		double GainSp(double _amount, SpReason _reason = SpReason::GRANTED, bool _silent = false);
		void SetSpTotal(double _total);
		void SetSpCostMultiplier(double _multiplier);
		void AddAmmo(double _amount);
		void Extend(double _seconds);
		void AddCharge(int _amount = 1);
		void NormalizeSp();

	protected:
		[[nodiscard]] Battle& BattleView() const noexcept { return _MyBattle; }

		[[nodiscard]] BattleCore& Core() const noexcept;
		virtual bool CanActivate(SkillReason _reason) const;
		virtual void BeforeStart(SkillReason _reason);
		virtual void OnStart(SkillReason _reason);
		virtual void AfterStart(SkillReason _reason);
		virtual void OnEnding(SkillReason _reason);
		virtual void OnEnd(SkillReason _reason);
		virtual void OnTick(double _delta);

	private:
		Battle& _MyBattle;
		UnitId _MyUnit{};
	};
}
#endif
