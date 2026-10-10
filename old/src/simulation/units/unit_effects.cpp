#include "battle_core.hpp"
#include <stronghold/simulation/unit.hpp>

namespace Stronghold
{
	double Unit::DealDamage(UnitId _target, const DamageInfo& _damage)
	{
		return World().GetUnit(_target).TakeDamage(_damage, Id());
	}

	double Unit::Heal(UnitId _target, double _amount, HealOptions _options)
	{
		return World().GetUnit(_target).ReceiveHealing(_amount, Id(), _options);
	}

	bool Unit::ApplyStatus(CombatStatus _status, const StatusApplication& _application)
	{
		return Core().ApplyStatus(Id(), _status, _application);
	}

	bool Unit::RemoveStatus(CombatStatus _status)
	{
		return Core().RemoveStatus(Id(), _status);
	}

	std::uint64_t Unit::AddBuff(BuffDefinition _definition)
	{
		return Core().AddBuff(Id(), std::move(_definition));
	}

	std::size_t Unit::RemoveBuff(std::string_view _key)
	{
		return Core().RemoveBuff(Id(), _key);
	}

	bool Unit::RemoveBuff(std::uint64_t _handle)
	{
		return Core().RemoveBuff(Id(), _handle);
	}

	bool Unit::ApplyStrongest(std::string _key, double _duration, BuffStrength _strength, UnitId _source)
	{
		return Core().ApplyStrongest(Id(), std::move(_key), _duration, _strength, _source);
	}

	double Unit::TakeElement(ElementHit _hit, UnitId _source)
	{
		return Core().DealElement(_source, Id(), _hit);
	}

	double Unit::ReduceElement(double _amount, std::optional<Element> _element)
	{
		return Core().ReduceElement(Id(), _amount, _element);
	}

	double Unit::Push(double _force, const PushOptions& _options)
	{
		return Core().Push(Id(), _force, _options);
	}

	double Unit::PullToFront(UnitId _source, double _force)
	{
		return Core().PullToFront(Id(), _source, _force);
	}
}
