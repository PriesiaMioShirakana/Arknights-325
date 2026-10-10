#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		const DiyOperatorKit* Rules(const CombatDefinition& _definition) noexcept
		{
			return _definition.MyOperatorKit ? std::get_if<DiyOperatorKit>(_definition.MyOperatorKit) : nullptr;
		}

		struct SelectionGuard
		{
			std::size_t& MyDepth;

			~SelectionGuard() { --MyDepth; }
		};
	}

	BuiltinOperator::BuiltinOperator(Battle& _battle, UnitId _unit) : OperatorBase(_battle, _unit)
	{
		if (const auto* kit = Rules(Definition())) _MyPulseAt.resize(kit->MyPeriodicSp.size());
	}

	bool BuiltinOperator::Supports(const CombatDefinition& _definition) noexcept
	{
		const auto* kit = Rules(_definition);
		return kit && (kit->MyKind == DiyOperatorKind::AMGOAT || kit->MyKind == DiyOperatorKind::CHEN || !kit->MyPeriodicSp.empty());
	}

	void BuiltinOperator::ResetPulses()
	{
		const auto& pulses = Rules(Definition())->MyPeriodicSp;
		_MyDeployment = State().MyDeploySequence;
		for (std::size_t i = 0; i < pulses.size(); ++i)
		{
			if (!std::isfinite(pulses[i].MyInterval) || !std::isgreater(pulses[i].MyInterval, 0))
				throw std::invalid_argument("invalid periodic operator SP interval");
			_MyPulseAt[i] = Core().Time() + pulses[i].MyInterval;
		}
	}

	void BuiltinOperator::TickPulses()
	{
		auto& core = Core(); const auto& unit = State();
		if (!unit.MyAlive || unit.MyHidden || unit.MyDeploySequence != _MyDeployment) return;
		const auto& pulses = Rules(Definition())->MyPeriodicSp;
		for (std::size_t i = 0; i < pulses.size(); ++i)
		{
			const auto& pulse = pulses[i];
			if (std::isgreater(_MyPulseAt[i], core.Time() + 1e-9)) continue;
			_MyPulseAt[i] += pulse.MyInterval;
			if (pulse.MyProbability && !std::isless(core._MyRandom.Next(), *pulse.MyProbability)) continue;
			auto& scratch = core.AcquireAttackScratch(); const SelectionGuard guard{.MyDepth = core._MyAttackDepth};
			const EffectContext context{.MySource = Unit(), .MyOwner = unit.MyOwner};
			EffectExecutor::Select(BattleView(), context, {.MyKind = pulse.MySelf ? SelectorKind::SELF : SelectorKind::ALLIES,
				.MyAttackHurtSpOnly = pulse.MyAttackHurtOnly, .MyRespectIsolation = true}, scratch.MySelections);
			for (const auto& target : scratch.MySelections)
			{
				EffectExecutor::Apply(BattleView(), context, target, SpOperation{.MyAmount = {.MyFlat = pulse.MyAmount}, .MyReason = SpReason::TALENT});
				if (core.Finished() || !unit.MyAlive || unit.MyDeploySequence != _MyDeployment) return;
			}
		}
	}

	void BuiltinOperator::GrantDeploymentRandom()
	{
		auto& core = Core(); const auto& kit = *Rules(Definition());
		auto& scratch = core.AcquireAttackScratch(); const SelectionGuard guard{.MyDepth = core._MyAttackDepth};
		const EffectContext context{.MySource = Unit(), .MyOwner = State().MyOwner};
		if (kit.MyInitialEliteMaximum)
			EffectExecutor::Select(BattleView(), context, {.MyKind = SelectorKind::ENEMIES, .MyRange = SelectorRange::SOURCE,
				.MyLimit = 1, .MyCanHitFlying = Definition().MyAttack.MyCanHitFlying, .MyEliteOnly = true}, scratch.MySelections);
		const bool maximum = !scratch.MySelections.empty();
		const auto draw = [&](double _first, double _last)
		{
			if (!std::isgreater(_last, _first)) return 0.0;
			return maximum ? std::ceil(_last) - 1 : std::floor(_first + core._MyRandom.Next() * (_last - _first));
		};
		const auto deployment = State().MyDeploySequence;
		const auto sp = draw(kit.MyInitialSpMinimum, kit.MyInitialSpMaximum);
		if (std::isgreater(sp, 0)) (void)Skill().GainSp(sp, SpReason::TALENT);
		if (!State().MyAlive || State().MyDeploySequence != deployment || core.Finished()) return;
		const auto speed = draw(kit.MyInitialSpeedMinimum, kit.MyInitialSpeedMaximum);
		if (std::isgreater(speed, 0)) EffectExecutor::Apply(BattleView(), context, {.MyUnit = Unit()}, BuffOperation{
			.MyDefinition = {.MyKey = "talent:amgoat:wildfire",
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = speed}}}});
	}

	void BuiltinOperator::IgniteResistance(UnitId _target)
	{
		const auto& kit = *Rules(Definition());
		if (!_target || !BattleView().Unit(_target).MyAlive || !std::islessgreater(kit.MyResistanceCut, 0)) return;
		EffectExecutor::Apply(BattleView(), {.MySource = Unit(), .MyOwner = State().MyOwner}, {.MyUnit = _target},
			StrongestBuffOperation{.MyKey = "amgoat:ignite", .MyDuration = kit.MyResistanceCutDuration,
				.MyStrength = {.MyValue = std::max(-1.0, kit.MyResistanceCut), .MyAttribute = Attribute::RESISTANCE_MULTIPLIER, .MyOffset = 1}});
	}

	void BuiltinOperator::HandleBuiltin(ContentEvent& _event)
	{
		const auto* kit = Rules(Definition());
		if (!kit || State().MyOperatorHooksReleased) return;
		if (_event.MyKind == ContentEventKind::DEPLOY && _event.MyUnit == Unit())
		{
			ResetPulses();
			if (kit->MyKind == DiyOperatorKind::AMGOAT) GrantDeploymentRandom();
		}
		if (_event.MyKind == ContentEventKind::TICK) TickPulses();
		if (_event.MyKind == ContentEventKind::BEFORE_STATUS && _event.MyTarget == Unit() && kit->MyKind == DiyOperatorKind::CHEN &&
			kit->MySkill == 3 && State().MySkill.MyActive && (_event.MyStatus == CombatStatus::STUN || _event.MyStatus == CombatStatus::FREEZE)) _event.MyCancel = true;
		if (_event.MySource != Unit() || !_event.MyTarget) return;
		const auto& target = BattleView().Unit(_event.MyTarget); const auto& damage = _event.MyDamage;
		if (target.MySide != UnitSide::ENEMY) return;
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE)
		{
			if (kit->MyKind == DiyOperatorKind::AMGOAT && kit->MySkill == 2 && damage.MyIsAttack && damage.MyIsSkill &&
				!damage.MyIsSplash && !HasTag(damage.MyTags, DamageTag::AMGOAT_IGNITE)) IgniteResistance(_event.MyTarget);
			if (kit->MyKind == DiyOperatorKind::CHEN && (damage.MyIsSkill || State().MySkill.MyActive)) _event.MyDamage.MyMultiplier *= kit->MySkillDamageMultiplier;
		}
		if (_event.MyKind == ContentEventKind::DAMAGED && kit->MyKind == DiyOperatorKind::AMGOAT && std::isgreater(kit->MyEliteHitSp, 0) &&
			damage.MyIsAttack && !damage.MyIsSplash && !HasTag(damage.MyTags, DamageTag::AMGOAT_IGNITE) &&
			(target.MySpawnTag == EnemySpawnTag::BOSS || target.MyDefinition.MyElite || target.MyDefinition.MyLeader))
			(void)Skill().GainSp(kit->MyEliteHitSp, SpReason::TRAIT);
	}

	void BuiltinOperator::OnEvent(ContentEvent& _event)
	{
		HandleBuiltin(_event);
	}

	void BuiltinOperator::BeforeAttack(std::vector<UnitId>& _targets)
	{
		const auto* kit = Rules(Definition());
		if (!kit || kit->MyKind != DiyOperatorKind::AMGOAT || kit->MySkill != 3 || !State().MySkill.MyActive || _targets.size() <= kit->MyLavaTargets) return;
		auto& core = Core(); auto& scratch = core.AcquireAttackScratch(); const SelectionGuard guard{.MyDepth = core._MyAttackDepth};
		EffectExecutor::Select(BattleView(), {.MySource = Unit(), .MyOwner = State().MyOwner, .MyProvided = _targets},
			{.MyKind = SelectorKind::PROVIDED, .MyOrder = SelectorOrder::RANDOM, .MyLimit = kit->MyLavaTargets}, scratch.MySelections);
		_targets.clear();
		for (const auto& target : scratch.MySelections) _targets.emplace_back(target.MyUnit);
	}

	void BuiltinOperator::EachHit(UnitId _target, const AttackProfile& _profile, bool _main)
	{
		const auto* kit = Rules(Definition());
		if (!_main || !_target || !kit || !_profile.MySkillDamage) return;
		const EffectContext context{.MySource = Unit(), .MyOwner = State().MyOwner};
		if (kit->MyKind == DiyOperatorKind::CHEN && kit->MySkill == 1 && std::isgreater(kit->MyStunDuration, 0))
			EffectExecutor::Apply(BattleView(), context, {.MyUnit = _target}, StatusOperation{.MyStatus = CombatStatus::STUN, .MyApplication = {.MyDuration = kit->MyStunDuration}});
		if (kit->MyKind != DiyOperatorKind::AMGOAT || kit->MySkill != 2) return;
		auto& core = Core(); auto& scratch = core.AcquireAttackScratch(); const SelectionGuard guard{.MyDepth = core._MyAttackDepth};
		const auto attack = State().MyStats.MyAttack;
		EffectExecutor::Select(BattleView(), context, {.MyKind = SelectorKind::ENEMIES, .MyRange = SelectorRange::RADIUS,
			.MyCenter = BattleView().Unit(_target).MyPosition, .MyRadius = 1.5}, scratch.MySelections);
		for (const auto& target : scratch.MySelections)
		{
			if (target.MyUnit == _target) continue;
			EffectExecutor::Apply(BattleView(), context, target, DamageOperation{.MyAmount = {.MyFlat = attack * kit->MyIgniteScale},
				.MyDamage = {.MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::AMGOAT_IGNITE), .MyIsAttack = true, .MyIsSplash = true, .MyIsSkill = true}});
			IgniteResistance(target.MyUnit);
		}
		if (std::isgreater(BattleView().Unit(_target).MyHealth, 0))
			EffectExecutor::Apply(BattleView(), context, {.MyUnit = _target}, DamageOperation{.MyAmount = {.MyFlat = attack * kit->MyIgniteSecondScale},
				.MyDamage = {.MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::AMGOAT_IGNITE), .MyIsAttack = true, .MyIsSkill = true}});
	}
}
