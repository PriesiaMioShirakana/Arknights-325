#include <stronghold/simulation/component_registry.hpp>

namespace Stronghold
{
	ComponentReference ComponentRegistry::Register(std::string _id, ComponentKind _kind, Value _value)
	{
		if (_MySealed) throw std::logic_error("component registry is sealed");
		if (_id.empty() || _id.size() > 256) throw std::invalid_argument("invalid component id");
		if (std::ranges::any_of(_MyEntries, [&](const Entry& _entry) { return _entry.MyKind == _kind && _entry.MyId == _id; }))
			throw std::invalid_argument("duplicate component registration");
		if (_MyEntries.size() >= std::numeric_limits<std::uint32_t>::max()) throw std::length_error("component registry is full");
		_MyEntries.emplace_back(std::move(_id), _kind, std::move(_value));
		return {.MyKind = _kind, .MyRegistration = static_cast<std::uint32_t>(_MyEntries.size())};
	}

	ComponentReference ComponentRegistry::RegisterBuff(std::string _id, BuffDefinition _definition)
	{
		return Register(std::move(_id), ComponentKind::BUFF, std::move(_definition));
	}

	ComponentReference ComponentRegistry::RegisterMechanism(std::string _id, MechanismDefinition _definition)
	{
		return Register(std::move(_id), ComponentKind::MECHANISM, std::move(_definition));
	}

	ComponentReference ComponentRegistry::Find(ComponentKind _kind, std::string_view _id) const
	{
		for (std::size_t i = 0; i < _MyEntries.size(); ++i)
			if (_MyEntries[i].MyKind == _kind && _MyEntries[i].MyId == _id)
				return {.MyKind = _kind, .MyRegistration = static_cast<std::uint32_t>(i + 1)};
		throw std::out_of_range("unregistered component");
	}

	void ComponentRegistry::Validate(ComponentReference _reference, ComponentKind _kind) const
	{
		if (!_MySealed) throw std::logic_error("seal component registry before use");
		if (_reference.MyKind != _kind || !_reference.MyRegistration || _reference.MyRegistration > _MyEntries.size() ||
			_MyEntries[_reference.MyRegistration - 1].MyKind != _kind)
			throw std::invalid_argument("component tag/registration mismatch");
	}

	ComponentKind ComponentRegistry::Kind(std::size_t _index) const
	{
		return _MyEntries.at(_index).MyKind;
	}

	std::unique_ptr<SkillBase> ComponentRegistry::CreateSkill(ComponentReference _reference, Battle& _battle, UnitId _unit) const
	{
		Validate(_reference, ComponentKind::CUSTOM_SKILL);
		return std::get<SkillFactory>(_MyEntries[_reference.MyRegistration - 1].MyValue)(_battle, _unit);
	}

	std::unique_ptr<OperatorBase> ComponentRegistry::CreateOperator(ComponentReference _reference, Battle& _battle, UnitId _unit) const
	{
		Validate(_reference, ComponentKind::CUSTOM_OPERATOR);
		return std::get<OperatorFactory>(_MyEntries[_reference.MyRegistration - 1].MyValue)(_battle, _unit);
	}

	std::unique_ptr<SelectorBase> ComponentRegistry::CreateSelector(ComponentReference _reference) const
	{
		Validate(_reference, ComponentKind::CUSTOM_SELECTOR);
		return std::get<SelectorFactory>(_MyEntries[_reference.MyRegistration - 1].MyValue)();
	}

	std::unique_ptr<OperationBase> ComponentRegistry::CreateOperation(ComponentReference _reference) const
	{
		Validate(_reference, ComponentKind::CUSTOM_OPERATION);
		return std::get<OperationFactory>(_MyEntries[_reference.MyRegistration - 1].MyValue)();
	}

	const BuffDefinition& ComponentRegistry::Buff(ComponentReference _reference) const
	{
		Validate(_reference, ComponentKind::BUFF);
		return std::get<BuffDefinition>(_MyEntries[_reference.MyRegistration - 1].MyValue);
	}

	const MechanismDefinition& ComponentRegistry::Mechanism(ComponentReference _reference) const
	{
		Validate(_reference, ComponentKind::MECHANISM);
		return std::get<MechanismDefinition>(_MyEntries[_reference.MyRegistration - 1].MyValue);
	}
}
