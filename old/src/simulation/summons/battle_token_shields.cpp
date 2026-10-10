#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::InstallTokenKit(CombatUnit& _unit)
	{
		const auto& kit = _unit.MyDefinition.MyTokenKit;
		if (kit && kit->MyKind == TokenKitKind::MANIFOLD) InstallManifold(_unit);
		if (!kit || kit->MyKind != TokenKitKind::CAT_SHIELD || !kit->MyCatShield) return;
		StatusFlags flags; flags.set(static_cast<std::size_t>(CombatStatus::INVULNERABLE));
		(void)AddBuff(_unit.MyId, {.MyKey = "token:catDevice", .MyFlags = flags, .MyPersistent = true, .MyAllowDead = true});
		Schedule({.MyAt = Time() + kit->MyCatShield->MyInterval, .MyKind = ScheduledKind::CAT_SHIELD, .MySource = _unit.MyId, .MyInterval = kit->MyCatShield->MyInterval});
	}

	void BattleCore::CatShieldGive(UnitId _unit, UnitId _target, double _ratio)
	{
		const auto& unit = Unit(_unit);
		const auto& rule = *unit.MyDefinition.MyTokenKit->MyCatShield;
		const auto health = unit.MyOwnerUnit ? Unit(unit.MyOwnerUnit).MyStats.MyMaxHealth : unit.MyStats.MyMaxHealth;
		const auto cap = health * rule.MyMaxRatio, amount = health * _ratio;
		if (!std::isgreater(cap, 0) || !std::isgreater(amount, 0)) return;
		auto& target = _MyUnits[Index(_target)];
		const auto found = std::ranges::find(target.MyBuffs, "cathy:shield", [](const CombatBuff& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
		if (found != target.MyBuffs.end()) found->MyDefinition.MyShield.MyHealth = std::min(cap, found->MyDefinition.MyShield.MyHealth + amount);
		else (void)AddBuff(_target, {.MyKey = "cathy:shield", .MySource = _unit, .MyShield = {.MyHealth = std::min(cap, amount)}});
	}

	void BattleCore::CatShieldConnect(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		UnitId best = 0;
		for (const auto key : RuleRangeKeys(unit))
		{
			for (const auto id : _MyAllyIds)
			{
				const auto& ally = Unit(id);
				if (!ally.MyAlive || ally.MyKind != UnitKind::OPERATOR || ally.MyOwner != unit.MyOwner) continue;
				const auto point = RulePosition(ally);
				if (static_cast<int>(std::floor(point.MyY + 0.5)) != key / FieldColumns || static_cast<int>(std::floor(point.MyX + 0.5)) != key % FieldColumns) continue;
				if (ally.MyShieldDevice && ally.MyShieldDevice != _unit && Unit(ally.MyShieldDevice).MyAlive) continue;
				best = id; break;
			}
			if (best) break;
		}
		if (unit.MyShieldRecipient && Unit(unit.MyShieldRecipient).MyShieldDevice == _unit) _MyUnits[Index(unit.MyShieldRecipient)].MyShieldDevice = 0;
		unit.MyShieldRecipient = best;
		if (!best) return;
		_MyUnits[Index(best)].MyShieldDevice = _unit;
		CatShieldGive(_unit, best, unit.MyDefinition.MyTokenKit->MyCatShield->MyMaxRatio);
	}

	void BattleCore::CatShieldTick(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive) return;
		const auto target = unit.MyShieldRecipient;
		if (!target || !Unit(target).MyAlive || Unit(target).MyKind != UnitKind::OPERATOR) { CatShieldConnect(_unit); return; }
		const auto& rule = *unit.MyDefinition.MyTokenKit->MyCatShield;
		const auto* owner = unit.MyOwnerUnit ? &Unit(unit.MyOwnerUnit) : nullptr;
		const bool skill = owner && owner->MyAlive && owner->MySkill.MyActive && IsTimedSkill(owner->MyDefinition.MySkill.MyKind);
		const auto rate = skill ? owner->MyDefinition.MyDeviceShieldRate.value_or(rule.MyRefill) : rule.MyRefill;
		if (skill || std::isgreaterequal(Time() - Unit(target).MyLastHitAt, rule.MyIdle - 1e-9)) CatShieldGive(_unit, target, rate * rule.MyInterval);
	}
}
