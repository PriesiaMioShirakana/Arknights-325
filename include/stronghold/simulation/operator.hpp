#ifndef STRONGHOLD_SIMULATION_OPERATOR_HPP
#define STRONGHOLD_SIMULATION_OPERATOR_HPP
#include <stronghold/simulation/skill.hpp>

namespace Stronghold
{
	struct ContentEvent;

	class OperatorBase
	{
	public:
		OperatorBase(Battle& _battle, UnitId _unit) noexcept;
		virtual ~OperatorBase() = default;
		OperatorBase(const OperatorBase&) = delete;
		OperatorBase& operator=(const OperatorBase&) = delete;

		[[nodiscard]] UnitId Unit() const noexcept { return _MyUnit; }
		[[nodiscard]] const CombatUnit& State() const;
		[[nodiscard]] const CombatDefinition& Definition() const;
		[[nodiscard]] const BattlePlayerState& Owner() const;
		[[nodiscard]] SkillBase& Skill() const;
		bool Attack(std::span<const UnitId> _targets = {}, bool _noAmmo = false);
		bool Deploy(bool _free = true, std::optional<WorldPoint> _tile = std::nullopt, bool _keepSp = false);
		void Retreat(bool _permanent = false, RemovalReason _reason = RemovalReason::RETREAT);
		bool Relocate(WorldPoint _position);
		void Handle(ContentEvent& _event);

	protected:
		[[nodiscard]] Battle& BattleView() const noexcept { return _MyBattle; }
		virtual void OnDeploy(ContentEvent& _event);
		virtual void OnTick(ContentEvent& _event);
		virtual void OnDamaged(ContentEvent& _event);
		virtual void OnDeath(ContentEvent& _event);
		virtual void OnEvent(ContentEvent& _event);

	private:
		Battle& _MyBattle;
		UnitId _MyUnit{};
	};
}
#endif
