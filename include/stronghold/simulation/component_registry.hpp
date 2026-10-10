#ifndef STRONGHOLD_SIMULATION_COMPONENT_REGISTRY_HPP
#define STRONGHOLD_SIMULATION_COMPONENT_REGISTRY_HPP
#include <stronghold/simulation/effects.hpp>
#include <stronghold/simulation/operator.hpp>

namespace Stronghold
{
	class ComponentRegistry final
	{
	public:
		explicit ComponentRegistry(std::size_t _capacity = 32) { _MyEntries.reserve(_capacity); }

		template <class _Skill> requires std::derived_from<_Skill, SkillBase> && std::constructible_from<_Skill, Battle&, UnitId>
		ComponentReference RegisterSkill(std::string _id)
		{
			return Register(std::move(_id), ComponentKind::CUSTOM_SKILL, SkillFactory{[](Battle& _battle, UnitId _unit) -> std::unique_ptr<SkillBase>
			{
				return std::make_unique<_Skill>(_battle, _unit);
			}});
		}

		template <class _Operator> requires std::derived_from<_Operator, OperatorBase> && std::constructible_from<_Operator, Battle&, UnitId>
		ComponentReference RegisterOperator(std::string _id)
		{
			return Register(std::move(_id), ComponentKind::CUSTOM_OPERATOR, OperatorFactory{[](Battle& _battle, UnitId _unit) -> std::unique_ptr<OperatorBase>
			{
				return std::make_unique<_Operator>(_battle, _unit);
			}});
		}

		template <class _Selector> requires std::derived_from<_Selector, SelectorBase> && std::default_initializable<_Selector>
		ComponentReference RegisterSelector(std::string _id)
		{
			return Register(std::move(_id), ComponentKind::CUSTOM_SELECTOR, SelectorFactory{[]() -> std::unique_ptr<SelectorBase>
			{
				return std::make_unique<_Selector>();
			}});
		}

		template <class _Operation> requires std::derived_from<_Operation, OperationBase> && std::default_initializable<_Operation>
		ComponentReference RegisterOperation(std::string _id)
		{
			return Register(std::move(_id), ComponentKind::CUSTOM_OPERATION, OperationFactory{[]() -> std::unique_ptr<OperationBase>
			{
				return std::make_unique<_Operation>();
			}});
		}

		ComponentReference RegisterBuff(std::string _id, BuffDefinition _definition);
		ComponentReference RegisterMechanism(std::string _id, MechanismDefinition _definition);
		void Seal() noexcept { _MySealed = true; }
		[[nodiscard]] bool Sealed() const noexcept { return _MySealed; }
		[[nodiscard]] std::size_t Size() const noexcept { return _MyEntries.size(); }
		[[nodiscard]] ComponentReference Find(ComponentKind _kind, std::string_view _id) const;
		void Validate(ComponentReference _reference, ComponentKind _kind) const;
		[[nodiscard]] ComponentKind Kind(std::size_t _index) const;
		[[nodiscard]] std::unique_ptr<SkillBase> CreateSkill(ComponentReference _reference, Battle& _battle, UnitId _unit) const;
		[[nodiscard]] std::unique_ptr<OperatorBase> CreateOperator(ComponentReference _reference, Battle& _battle, UnitId _unit) const;
		[[nodiscard]] std::unique_ptr<SelectorBase> CreateSelector(ComponentReference _reference) const;
		[[nodiscard]] std::unique_ptr<OperationBase> CreateOperation(ComponentReference _reference) const;
		[[nodiscard]] const BuffDefinition& Buff(ComponentReference _reference) const;
		[[nodiscard]] const MechanismDefinition& Mechanism(ComponentReference _reference) const;

	private:
		using SkillFactory = std::unique_ptr<SkillBase> (*)(Battle&, UnitId);
		using OperatorFactory = std::unique_ptr<OperatorBase> (*)(Battle&, UnitId);
		using SelectorFactory = std::unique_ptr<SelectorBase> (*)();
		using OperationFactory = std::unique_ptr<OperationBase> (*)();
		using Value = std::variant<SkillFactory, OperatorFactory, SelectorFactory, OperationFactory, BuffDefinition, MechanismDefinition>;

		struct Entry
		{
			std::string MyId;
			ComponentKind MyKind{};
			Value MyValue;
		};

		ComponentReference Register(std::string _id, ComponentKind _kind, Value _value);
		std::vector<Entry> _MyEntries;
		bool _MySealed{};
	};
}
#endif
