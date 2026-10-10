#include "battle_core.hpp"
#include <stronghold/simulation/unit.hpp>

namespace Stronghold
{
	Unit::Unit(Battle& _battle, UnitId _unit) : Unit(_battle.World(), _unit) {}

	Unit::Unit(Environment& _environment, UnitId _unit)
		: _MyEnvironment(_environment), _MyState(_environment._MyCore._MyUnits[_environment._MyCore.Index(_unit)]) {}

	BattleCore& Unit::Core() const noexcept
	{
		return _MyEnvironment._MyCore;
	}

	Battle& Unit::BattleView() const noexcept
	{
		return _MyEnvironment.BattleView();
	}

	const BattlePlayerState& Unit::Owner() const
	{
		return _MyEnvironment.Owner(Id());
	}

	SkillBase& Unit::Skill() const
	{
		return _MyEnvironment.Skill(Id());
	}

	WorldPoint Unit::RulePosition() const
	{
		return _MyEnvironment.RulePosition(Id());
	}

	const std::bitset<FieldTiles>& Unit::Range() const
	{
		return _MyEnvironment.RuleRange(Id());
	}

	bool Unit::Attack(std::span<const UnitId> _targets, bool _noAmmo)
	{
		if (!_MyEnvironment.Started() || _MyEnvironment.Finished() || !_MyState.MyAlive) return false;
		return DoAttack(_targets, _noAmmo);
	}

	bool Unit::DoAttack(std::span<const UnitId> _targets, bool _noAmmo)
	{
		return Core().ForceAttack(Id(), _targets, _noAmmo);
	}

	bool Unit::Relocate(WorldPoint _position)
	{
		if (!std::isfinite(_position.MyX) || !std::isfinite(_position.MyY)) throw std::invalid_argument("invalid unit position");
		if (!_MyEnvironment.Started() || _MyEnvironment.Finished() || !_MyState.MyAlive) return false;
		return DoRelocate(_position);
	}

	bool Unit::DoRelocate(WorldPoint _position)
	{
		return Core().Relocate(Id(), _position);
	}

	bool EnemyBase::DoRelocate(WorldPoint _position)
	{
		const auto origin = State().MyPosition;
		const WorldPoint direction{.MyX = _position.MyX - origin.MyX, .MyY = _position.MyY - origin.MyY};
		return std::isgreater(Core().Displace(Id(), direction, std::hypot(direction.MyX, direction.MyY)), 0);
	}

	bool Unit::Exit(RemovalReason _reason, bool _permanent)
	{
		if (_reason == RemovalReason::KILLED || _reason == RemovalReason::LEAK ||
			static_cast<unsigned>(_reason) > static_cast<unsigned>(RemovalReason::RAID))
			throw std::invalid_argument("use damage or route resolution for death and leak");
		if (!_MyEnvironment.Started() || _MyEnvironment.Finished() || !_MyState.MyAlive || _MyState.MyRemoving) return false;
		return DoExit(_reason, _permanent);
	}

	bool Unit::DoExit(RemovalReason _reason, bool _permanent)
	{
		Core().RemoveUnit(_MyState, _reason, 0, _permanent);
		return !_MyState.MyAlive;
	}

	void Unit::Handle(ContentEvent& _event)
	{
		const bool participant = _event.MySource == Id() || _event.MyTarget == Id();
		switch (_event.MyKind)
		{
		case ContentEventKind::BEFORE_DAMAGE: if (participant) OnBeforeDamage(_event); break;
		case ContentEventKind::DAMAGED:
			if (participant && !_event.MyElement) OnAfterDamage(_event);
			if (_event.MyTarget == Id() || _event.MyUnit == Id()) OnDamaged(_event);
			break;
		case ContentEventKind::BEFORE_HEAL: if (participant) OnBeforeHeal(_event); break;
		case ContentEventKind::HEALED: if (participant) OnAfterHeal(_event); break;
		case ContentEventKind::DEPLOY: if (_event.MyUnit == Id()) OnDeploy(_event); break;
		case ContentEventKind::TICK: if (_MyState.MyAlive) OnTick(_event); break;
		case ContentEventKind::DEATH: if (_event.MyUnit == Id()) OnDeath(_event); break;
		default: break;
		}
		OnEvent(_event);
	}

	void Unit::OnBeforeDamage(ContentEvent&) {}

	void Unit::OnAfterDamage(const ContentEvent&) {}

	void Unit::OnBeforeHeal(ContentEvent&) {}

	void Unit::OnAfterHeal(const ContentEvent&) {}

	void Unit::OnDeploy(ContentEvent&) {}

	void Unit::OnTick(ContentEvent&) {}

	void Unit::OnDamaged(ContentEvent&) {}

	void Unit::OnDeath(ContentEvent&) {}

	void Unit::OnEvent(ContentEvent&) {}
}
