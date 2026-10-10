#include "battle_core.hpp"
#include <tuple>

namespace Stronghold
{
	const ComponentRegistry& BattleCore::Registry() const
	{
		if (!_MyInput.MyComponentRegistry) throw std::invalid_argument("custom component requires a registry");
		return _MyInput.MyComponentRegistry->get();
	}

	void BattleCore::PrepareComponents()
	{
		_MyEffectTasks.reserve(_MyInput.MySpawns.size() + 16);
		_MyCustomOperators.reserve(16);
		if (!_MyInput.MyComponentRegistry) return;
		const auto& registry = Registry();
		if (!registry.Sealed()) throw std::logic_error("seal component registry before starting a battle");
		_MyCustomSelectors.resize(registry.Size());
		_MyCustomOperations.resize(registry.Size());
		for (std::size_t i = 0; i < registry.Size(); ++i)
		{
			const ComponentReference reference{.MyKind = registry.Kind(i), .MyRegistration = static_cast<std::uint32_t>(i + 1)};
			if (reference.MyKind == ComponentKind::CUSTOM_SELECTOR) _MyCustomSelectors[i] = registry.CreateSelector(reference);
			if (reference.MyKind == ComponentKind::CUSTOM_OPERATION) _MyCustomOperations[i] = registry.CreateOperation(reference);
		}
	}

	const SelectorBase& BattleCore::CustomSelector(ComponentReference _reference) const
	{
		Registry().Validate(_reference, ComponentKind::CUSTOM_SELECTOR);
		return *_MyCustomSelectors.at(_reference.MyRegistration - 1);
	}

	const OperationBase& BattleCore::CustomOperationHandler(ComponentReference _reference) const
	{
		Registry().Validate(_reference, ComponentKind::CUSTOM_OPERATION);
		return *_MyCustomOperations.at(_reference.MyRegistration - 1);
	}

	void BattleCore::EnsureUnitComponents(UnitId _unit)
	{
		(void)Index(_unit);
		while (_MyComponents.size() < _unit)
		{
			const auto id = static_cast<UnitId>(_MyComponents.size() + 1);
			auto& components = _MyComponents.emplace_back(_MyView, id);
			const auto& definition = Unit(id).MyDefinition;
			if (definition.MySkill.MyCustom.MyKind != ComponentKind::BUILTIN)
				components.MyCustomSkill = Registry().CreateSkill(definition.MySkill.MyCustom, _MyView, id);
			else if (definition.MySkill.MyCustom.MyRegistration) throw std::invalid_argument("invalid builtin skill registration");
			if (definition.MyCustomOperator.MyKind != ComponentKind::BUILTIN)
			{
				if (Unit(id).MySide != UnitSide::ALLY) throw std::invalid_argument("custom operator requires an allied unit");
				components.MyCustomOperator = Registry().CreateOperator(definition.MyCustomOperator, _MyView, id);
				_MyCustomOperators.emplace_back(id);
			}
			else if (definition.MyCustomOperator.MyRegistration) throw std::invalid_argument("invalid builtin operator registration");
		}
	}

	SkillBase& BattleCore::Skill(UnitId _unit)
	{
		EnsureUnitComponents(_unit);
		auto& components = _MyComponents[Index(_unit)];
		return components.MyCustomSkill ? *components.MyCustomSkill : components.MySkill;
	}

	OperatorBase& BattleCore::Operator(UnitId _unit)
	{
		if (Unit(_unit).MySide != UnitSide::ALLY) throw std::invalid_argument("operator interface requires an allied unit");
		EnsureUnitComponents(_unit);
		auto& components = _MyComponents[Index(_unit)];
		return components.MyCustomOperator ? *components.MyCustomOperator : components.MyOperator;
	}

	std::uint64_t BattleCore::AttachMechanism(UnitId _unit, MechanismDefinition _definition, std::string_view _playerId)
	{
		if (!std::isfinite(_definition.MyDelay) || std::isless(_definition.MyDelay, 0) ||
			!std::isfinite(_definition.MyInterval) || std::isless(_definition.MyInterval, 0) ||
			static_cast<unsigned>(_definition.MyEvent) > static_cast<unsigned>(ContentEventKind::LAYER_GAIN) ||
			static_cast<unsigned>(_definition.MyScope) > static_cast<unsigned>(MechanismScope::GLOBAL))
			throw std::invalid_argument("invalid mechanism timing or scope");
		const auto owner = _playerId.empty() ? (_unit ? Unit(_unit).MyOwner : NoPlayer) : Owner(_playerId);
		if (_unit) (void)Index(_unit);
		if (Finished()) return 0;
		_MyMechanisms.emplace_back(MechanismState{.MyDefinition = std::move(_definition), .MyUnit = _unit, .MyOwner = owner});
		return _MyMechanisms.size();
	}

	std::uint64_t BattleCore::AttachMechanism(UnitId _unit, ComponentReference _reference, std::string_view _playerId)
	{
		return AttachMechanism(_unit, Registry().Mechanism(_reference), _playerId);
	}

	bool BattleCore::RemoveMechanism(std::uint64_t _handle)
	{
		if (!_handle || _handle > _MyMechanisms.size()) throw std::out_of_range("unknown mechanism handle");
		auto& state = _MyMechanisms[_handle - 1];
		if (state.MyRemoved) return false;
		state.MyRemoved = true;
		return true;
	}

	void BattleCore::NotifyComponents(ContentEvent& _event)
	{
		if (_MyCustomOperators.empty() && _MyMechanisms.empty()) return;
		if (_MyMechanismDepth >= 32) throw std::runtime_error("component event recursion limit");
		++_MyMechanismDepth;
		struct Guard
		{
			unsigned& MyDepth;
			~Guard() { --MyDepth; }
		} guard{_MyMechanismDepth};
		const auto operatorCount = _MyCustomOperators.size();
		for (std::size_t i = 0; i < operatorCount; ++i)
		{
			const auto id = _MyCustomOperators[i];
			const auto& unit = Unit(id);
			const bool global = _event.MyKind == ContentEventKind::BATTLE_START || _event.MyKind == ContentEventKind::BATTLE_END ||
				_event.MyKind == ContentEventKind::LAYER_GAIN || (_event.MyKind == ContentEventKind::TICK && unit.MyAlive);
			if (!global && id != _event.MyUnit && id != _event.MySource && id != _event.MyTarget) continue;
			if (_event.MySkillReason == SkillReason::PASSIVE && _event.MyKind == ContentEventKind::SKILL_START && id != _event.MyUnit) continue;
			_MyComponents[Index(id)].MyCustomOperator->Handle(_event);
		}
		const auto count = _MyMechanisms.size();
		for (std::size_t i = 0; i < count && !Finished(); ++i)
		{
			auto& state = _MyMechanisms[i]; const auto& definition = state.MyDefinition;
			if (state.MyRemoved || definition.MyEvent != _event.MyKind || std::isless(Time() + 1e-9, state.MyReadyAt)) continue;
			if (_event.MyKind == ContentEventKind::SKILL_START && _event.MySkillReason == SkillReason::PASSIVE && state.MyUnit != _event.MyUnit) continue;
			if (definition.MyScope == MechanismScope::SOURCE && state.MyUnit != _event.MyUnit && state.MyUnit != _event.MySource && state.MyUnit != _event.MyTarget &&
				_event.MyKind != ContentEventKind::TICK && _event.MyKind != ContentEventKind::BATTLE_START && _event.MyKind != ContentEventKind::BATTLE_END) continue;
			if (definition.MyScope == MechanismScope::OWNER)
			{
				const auto participant = _event.MyUnit ? _event.MyUnit : _event.MySource ? _event.MySource : _event.MyTarget;
				if (participant ? Unit(participant).MyOwner != state.MyOwner : _event.MyPlayer != state.MyOwner &&
					_event.MyKind != ContentEventKind::TICK && _event.MyKind != ContentEventKind::BATTLE_START && _event.MyKind != ContentEventKind::BATTLE_END) continue;
			}
			const auto* source = state.MyUnit ? &Unit(state.MyUnit) : nullptr;
			if (source && definition.MyRequiresSource && (!source->MyAlive || source->MyHidden) &&
				!(definition.MyGarrison && _MyInput.MyGarrisonEffectsAfterExit && source->MyDeploySequence)) continue;
			if (definition.MyDuringSkill && (!source || !source->MySkill.MyActive)) continue;
			EffectContext context{.MySource = state.MyUnit, .MyOwner = state.MyOwner, .MyEventUnit = _event.MyUnit, .MyEventSource = _event.MySource,
				.MyEventTarget = _event.MyTarget, .MyEventAmount = _event.MyAmount, .MyProvided = _event.MyTargets};
			if (definition.MyNeedsTarget)
			{
				std::vector<EffectTarget> targets;
				EffectExecutor::Select(_MyView, context, definition.MyCondition, targets);
				if (targets.empty()) continue;
			}
			state.MyReadyAt = Time() + definition.MyInterval;
			if (std::isgreater(definition.MyDelay, 0))
			{
				EffectTask task{.MyAt = Time() + definition.MyDelay, .MySequence = ++_MyEffectTaskSequence, .MyMechanism = i + 1,
					.MyDeployment = source ? source->MyDeploySequence : 0, .MyActivation = source ? source->MySkill.MyActivations : 0, .MyContext = context,
					.MyTargets = std::vector<UnitId>(_event.MyTargets.begin(), _event.MyTargets.end())};
				task.MyContext.MyProvided = {};
				_MyEffectTasks.emplace_back(std::move(task));
				std::push_heap(_MyEffectTasks.begin(), _MyEffectTasks.end(), [](const EffectTask& _left, const EffectTask& _right)
				{
					return std::tuple{_left.MyAt, _left.MySequence} > std::tuple{_right.MyAt, _right.MySequence};
				});
			}
			else EffectExecutor::Execute(_MyView, context, definition.MyEffects);
		}
	}

	void BattleCore::TickEffectTasks()
	{
		while (!_MyEffectTasks.empty() && !Finished() && std::islessequal(_MyEffectTasks.front().MyAt, Time() + 1e-9))
		{
			std::pop_heap(_MyEffectTasks.begin(), _MyEffectTasks.end(), [](const EffectTask& _left, const EffectTask& _right)
			{
				return std::tuple{_left.MyAt, _left.MySequence} > std::tuple{_right.MyAt, _right.MySequence};
			});
			auto task = std::move(_MyEffectTasks.back()); _MyEffectTasks.pop_back();
			const auto& state = _MyMechanisms[task.MyMechanism - 1]; const auto& definition = state.MyDefinition;
			if (state.MyRemoved) continue;
			if (state.MyUnit)
			{
				const auto& source = Unit(state.MyUnit);
				const bool retained = definition.MyGarrison && _MyInput.MyGarrisonEffectsAfterExit && source.MyDeploySequence;
				if (definition.MyRequiresSource && ((!source.MyAlive || source.MyHidden) && !retained)) continue;
				if (definition.MyRequiresSource && source.MyDeploySequence != task.MyDeployment) continue;
				if (definition.MyDuringSkill && (!source.MySkill.MyActive || source.MySkill.MyActivations != task.MyActivation)) continue;
			}
			task.MyContext.MyProvided = task.MyTargets;
			EffectExecutor::Execute(_MyView, task.MyContext, definition.MyEffects);
		}
	}
}
