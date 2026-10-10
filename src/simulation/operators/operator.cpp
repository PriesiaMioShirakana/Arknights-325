#include <stronghold/simulation/battle.hpp>
#include <stronghold/simulation/operator.hpp>

namespace Stronghold
{
	OperatorBase::OperatorBase(Battle& _battle, UnitId _unit) noexcept : _MyBattle(_battle), _MyUnit(_unit) {}

	const CombatUnit& OperatorBase::State() const
	{
		return _MyBattle.Unit(_MyUnit);
	}

	const CombatDefinition& OperatorBase::Definition() const
	{
		return State().MyDefinition;
	}

	const BattlePlayerState& OperatorBase::Owner() const
	{
		return _MyBattle.UnitOwner(_MyUnit);
	}

	SkillBase& OperatorBase::Skill() const
	{
		return _MyBattle.Skill(_MyUnit);
	}

	bool OperatorBase::Attack(std::span<const UnitId> _targets, bool _noAmmo)
	{
		return _MyBattle.ForceAttack(_MyUnit, _targets, _noAmmo);
	}

	bool OperatorBase::Deploy(bool _free, std::optional<WorldPoint> _tile, bool _keepSp)
	{
		return _MyBattle.Redeploy(_MyUnit, _free, _tile, _keepSp);
	}

	void OperatorBase::Retreat(bool _permanent, RemovalReason _reason)
	{
		_MyBattle.Retreat(_MyUnit, _permanent, _reason);
	}

	bool OperatorBase::Relocate(WorldPoint _position)
	{
		return _MyBattle.Relocate(_MyUnit, _position);
	}

	void OperatorBase::Handle(ContentEvent& _event)
	{
		switch (_event.MyKind)
		{
		case ContentEventKind::DEPLOY: if (_event.MyUnit == _MyUnit) OnDeploy(_event); break;
		case ContentEventKind::TICK: if (State().MyAlive) OnTick(_event); break;
		case ContentEventKind::DAMAGED: if (_event.MyTarget == _MyUnit || _event.MyUnit == _MyUnit) OnDamaged(_event); break;
		case ContentEventKind::DEATH: if (_event.MyUnit == _MyUnit) OnDeath(_event); break;
		default: break;
		}
		OnEvent(_event);
	}

	void OperatorBase::OnDeploy(ContentEvent&) {}

	void OperatorBase::OnTick(ContentEvent&) {}

	void OperatorBase::OnDamaged(ContentEvent&) {}

	void OperatorBase::OnDeath(ContentEvent&) {}

	void OperatorBase::OnEvent(ContentEvent&) {}
}
