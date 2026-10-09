#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		struct DeploymentScratchGuard
		{
			std::size_t& MyDepth;

			~DeploymentScratchGuard() { --MyDepth; }
		};
	}

	void Battle::GravelSkill(UnitId _unit, const GravelKit& _kit, const ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		const auto key = _kit.MyShadow ? "gravel:shadow" : "gravel:rats";
		if (_event.MyKind == ContentEventKind::SKILL_START && std::isgreater(_kit.MyDuration, 0))
		{
			unit.MyGravelTicks = 0;
			if (_kit.MyShadow && std::isgreater(_kit.MyDefense, 0))
				unit.MyGravelBuff = AddBuff(_unit, {.MyKey = key, .MyDuration = _kit.MyDuration,
					.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::DEFENSE_PERCENT, .MyValue = _kit.MyDefense}}, .MyInterval = 1, .MyNotifyTick = true});
			else if (!_kit.MyShadow && std::isgreater(_kit.MyShieldRatio, 0))
				unit.MyGravelBuff = AddBuff(_unit, {.MyKey = key, .MyDuration = _kit.MyDuration, .MyInterval = 1,
					.MyShield = {.MyHealth = unit.MyStats.MyMaxHealth * _kit.MyShieldRatio}, .MyNotifyTick = true});
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING) (void)RemoveBuff(_unit, key);
		else if (_event.MyKind == ContentEventKind::BUFF_TICK && _event.MyBuff == unit.MyGravelBuff)
		{
			const auto found = std::ranges::find(unit.MyBuffs, _event.MyBuff, &CombatBuff::MyId);
			if (found == unit.MyBuffs.end() || !std::isgreater(_kit.MyDuration, 0)) return;
			const auto previous = std::max(0.0, _kit.MyDuration - unit.MyGravelTicks);
			const auto next = std::max(0.0, _kit.MyDuration - ++unit.MyGravelTicks);
			if (_kit.MyShadow && found->MyDefinition.MyModifiers)
				found->MyDefinition.MyModifiers->front().MyValue = _kit.MyDefense * next / _kit.MyDuration;
			else if (std::isgreater(previous, 0)) found->MyDefinition.MyShield.MyHealth *= next / previous;
			Recalculate(unit);
		}
	}

	void Battle::TippiHit(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MyTarget || _event.MyCancel) return;
		auto& unit = _MyUnits[Index(_event.MyTarget)];
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<TippiKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || !unit.MyAlive || unit.MyOperatorHooksReleased) return;
		const bool dodgeable = _event.MyDamage.MyType == DamageType::PHYSICAL || _event.MyDamage.MyType == DamageType::ARTS;
		bool dodged = false;
		if (kit->MyOnDamage && unit.MySkill.MyCharges && !unit.MySkill.MyActive && !unit.MyHidden && !unit.MyStatuses.Has(CombatStatus::STUN) &&
			!unit.MyStatuses.Has(CombatStatus::SILENCE) && ActivateSkill(unit.MyId, false, SkillReason::TRIGGER)) dodged = dodgeable;
		else if (dodgeable && std::isgreater(kit->MyStackTime, 0) && std::isgreaterequal(Time() - unit.MyTippiLastHit + 1e-9, kit->MyStackTime))
			dodged = std::isless(_MyRandom.Next(), kit->MyProbability);
		unit.MyTippiLastHit = Time();
		if (dodged) _event.MyCancel = true;
	}

	void Battle::AkkordTick(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<AkkordKit>(*unit.MyDefinition.MyOperatorKit);
		if (!std::islessgreater(kit.MyAllyAttack, 0)) return;
		const bool wanted = unit.MyAlive && std::ranges::any_of(_MyAllyIds, [&](UnitId _id)
		{
			const auto& ally = Unit(_id);
			return _id != _unit && ally.MyAlive && !ally.MyHidden && ally.MyKind == UnitKind::OPERATOR &&
				!ally.MyStatuses.Has(CombatStatus::ISOLATED) && InRuleRange(_unit, _id);
		});
		const bool present = std::ranges::any_of(unit.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "akkord:sync"; });
		if (wanted && !present) (void)AddBuff(_unit, {.MyKey = "akkord:sync", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyAllyAttack}}});
		else if (!wanted && present) (void)RemoveBuff(_unit, "akkord:sync");
	}

	void Battle::AkkordSonic(UnitId _unit, const AkkordKit& _kit)
	{
		if (!std::isgreater(_kit.MySonicScale, 0)) return;
		auto& scratch = AcquireAttackScratch(); const DeploymentScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (id != _unit && ally.MyAlive && !ally.MyHidden && ally.MyKind == UnitKind::OPERATOR &&
				!ally.MyStatuses.Has(CombatStatus::ISOLATED) && InRuleRange(_unit, id)) scratch.MyTargets.push_back(id);
		}
		const AttackProfile filter{.MyCanHitFlying = true, .MyHitSleep = true};
		for (const auto id : scratch.MyTargets)
		{
			const auto point = RulePosition(Unit(id));
			for (std::size_t i = 0, count = _MyEnemyIds.size(); i < count; ++i)
			{
				const auto enemy = _MyEnemyIds[i];
				if (TargetableEnemy(Unit(enemy), filter) && std::islessequal(BodyDistance(Unit(enemy), point), _kit.MyRadius + 1e-9))
					(void)DealDamage(_unit, enemy, {.MyAmount = Unit(_unit).MyStats.MyAttack * _kit.MySonicScale, .MyType = DamageType::ARTS,
						.MyTags = static_cast<DamageTags>(DamageTag::SONIC), .MyIsSplash = true, .MyIsSkill = true});
			}
		}
	}
}
