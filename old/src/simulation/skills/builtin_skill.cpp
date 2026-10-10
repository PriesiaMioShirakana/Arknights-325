#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		constexpr std::array<double, 10> ShadowlessHitTimes{0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.2, 2.833};
	}

	bool BuiltinSkill::CanActivate(SkillReason) const
	{
		const auto& unit = Core().Unit(Unit());
		if (const auto* diy = unit.MyDefinition.MyOperatorKit ? std::get_if<DiyOperatorKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
			diy && diy->MyKind == DiyOperatorKind::CHEN && diy->MySkill == 3)
		{
			EffectExecutor::Select(BattleView(), {.MySource = Unit(), .MyOwner = unit.MyOwner},
				{.MyKind = SelectorKind::ENEMIES, .MyRange = SelectorRange::OFFSETS, .MyLimit = 1,
					.MyTargetable = false, .MyCanHitFlying = false, .MyOffsets = diy->MyCastRange}, _MyCandidates);
			if (_MyCandidates.empty()) return false;
		}
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<PapyrsKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		return !kit || !kit->MyLockSkill || Core().PapyrsTarget(Unit());
	}

	void BuiltinSkill::BeforeStart(SkillReason)
	{
		Core().DiyOperatorBeforeStart(Unit());
	}

	void BuiltinSkill::OnStart(SkillReason _reason)
	{
		if (_reason == SkillReason::PASSIVE) return;
		const auto& unit = Core().Unit(Unit());
		if (const auto* diy = unit.MyDefinition.MyOperatorKit ? std::get_if<DiyOperatorKit>(unit.MyDefinition.MyOperatorKit) : nullptr)
		{
			if (diy->MyKind == DiyOperatorKind::AMGOAT && diy->MySkill == 1)
			{
				if (_MyCastDeployment != unit.MyDeploySequence) { _MyCastDeployment = unit.MyDeploySequence; _MyCasts = 0; }
				const auto modifiers = ++_MyCasts > 1 ? diy->MyLaterCastModifiers : diy->MyFirstCastModifiers;
				(void)BattleView().AddBuff(Unit(), {.MyKey = "skill:amgoat:duet", .MyModifiers = std::vector<AttributeChange>(modifiers.begin(), modifiers.end())});
			}
			if (diy->MyKind == DiyOperatorKind::CHEN && diy->MySkill == 2)
			{
				const std::array<EffectOperation, 2> operations{
					DamageOperation{.MyAmount = {.MySourceAttack = diy->MyCastScale}, .MyDamage = {.MyType = DamageType::ARTS,
						.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true}},
					DamageOperation{.MyAmount = {.MySourceAttack = diy->MyCastScale}, .MyDamage = {.MyType = DamageType::PHYSICAL,
						.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true}}};
				EffectExecutor::Execute(BattleView(), {.MySource = Unit(), .MyOwner = unit.MyOwner},
					{.MyKind = SelectorKind::ENEMIES, .MyRange = SelectorRange::OFFSETS, .MyOrder = SelectorOrder::ATTACK_PRIORITY,
						.MyLimit = diy->MyCastTargets, .MyOffsets = diy->MyCastRange}, operations);
			}
			if (diy->MyKind == DiyOperatorKind::CHEN && diy->MySkill == 3)
			{
				Core().ReleaseBlocked(Core()._MyUnits[Core().Index(Unit())]);
				_MySequence.Start(BattleView(), Unit(), {.MyHitTimes = std::span(ShadowlessHitTimes).first(std::min<std::size_t>(ShadowlessHitTimes.size(), diy->MySlashCount)),
					.MySelector = {.MyKind = SelectorKind::ENEMIES, .MyRange = SelectorRange::OFFSETS, .MyOrder = SelectorOrder::NEAREST,
						.MyCanHitFlying = false, .MyOffsets = diy->MyCastRange},
					.MyDamage = {.MyAmount = {.MySourceAttack = diy->MyCastScale}, .MyDamage = {.MyType = DamageType::PHYSICAL,
						.MyTags = DamageTag::SKILL | DamageTag::SLASH, .MyIsSkill = true}},
					.MyLastHitStatus = StatusOperation{.MyStatus = CombatStatus::STUN, .MyApplication = {.MyDuration = diy->MyStunDuration}}});
			}
		}
		if (const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<Texas2Kit>(unit.MyDefinition.MyOperatorKit) : nullptr)
			Core().Texas2Start(Unit(), *kit, _reason);
	}

	void BuiltinSkill::AfterStart(SkillReason)
	{
		const auto& unit = Core().Unit(Unit());
		if (unit.MyDefinition.MyOperatorKit && std::holds_alternative<Texas2Kit>(*unit.MyDefinition.MyOperatorKit)) Core().Texas2FinishStart(Unit());
	}

	void BuiltinSkill::TickBuiltin(double _delta)
	{
		const auto& unit = Core().Unit(Unit());
		if (const auto* diy = unit.MyDefinition.MyOperatorKit ? std::get_if<DiyOperatorKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
			diy && diy->MyKind == DiyOperatorKind::CHEN && diy->MySkill == 3) _MySequence.Tick(BattleView(), Unit(), _delta);
	}

	void BuiltinSkill::OnEnding(SkillReason)
	{
		_MySequence.Clear();
		const auto& unit = Core().Unit(Unit());
		if (const auto* diy = unit.MyDefinition.MyOperatorKit ? std::get_if<DiyOperatorKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
			diy && diy->MyKind == DiyOperatorKind::AMGOAT && diy->MySkill == 1) (void)BattleView().RemoveBuff(Unit(), "skill:amgoat:duet");
	}
}
