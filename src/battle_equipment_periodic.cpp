#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	bool Battle::EquipmentMember(const CombatUnit& _unit, std::string_view _bond) const
	{
		if (_unit.MyKind != UnitKind::OPERATOR) return false;
		const auto& bonds = _unit.MyDefinition.MyIdentity.MyBonds;
		if (std::ranges::contains(bonds, _bond)) return true;
		// 所有调用方传入核心盟约；调和需与该核心盟约同时激活。
		if (!std::ranges::contains(bonds, std::string_view("maniShip"))) return false;
		const auto& active = _MyPlayers[_unit.MyOwner].MyBonds;
		const auto has = [&](std::string_view _id)
			{ const auto found = std::ranges::find(active, _id, &BondLayer::MyId); return found != active.end() && found->MyActive; };
		return has("maniShip") && has(_bond);
	}

	bool Battle::CarriesEquipment(const CombatUnit& _unit, std::string_view _base) const
	{
		const auto matches = [&](std::string_view _id)
		{
			if (_id.ends_with("_a") || _id.ends_with("_b")) _id.remove_suffix(2);
			return _id == _base;
		};
		if (std::ranges::any_of(_unit.MyDefinition.MyIdentity.MyItems, matches)) return true;
		return std::ranges::any_of(_MyEquipmentLends, [&](const EquipmentLend& _grant)
			{ return _grant.MyActive && _grant.MyUnit == _unit.MyId && matches(_grant.MyItem); });
	}

	void Battle::EquipmentSolvent(EquipmentRuntime& _runtime)
	{
		const auto& unit = Unit(_runtime.MyUnit);
		const auto value = EquipmentDefinition(_runtime).MyParameters.MyValue;
		const DamageInfo damage{.MyAmount = value, .MyType = DamageType::TRUE_DAMAGE, .MyCanDodge = false,
			.MySourceless = true, .MyTags = DamageTag::DOT | DamageTag::PERIODIC};
		(void)DealDamage(unit.MyId, unit.MyId, damage);
		// 本次 tick 击倒携带者也仍结算全场传播；多携带者对同一个敌人类友军每秒最多一次。
		for (std::size_t i = 0; i < _MyAllyIds.size(); ++i)
		{
			auto& ally = _MyUnits[Index(_MyAllyIds[i])];
			if (ally.MyId == unit.MyId || !ally.MyAlive || ally.MyKind == UnitKind::DEVICE ||
				!ally.MyDefinition.MyId.starts_with("enemy_") || Time() - ally.MySolventAt < 1 - 1e-6) continue;
			ally.MySolventAt = Time();
			(void)DealDamage(0, ally.MyId, damage);
		}
	}

	void Battle::EquipmentPeriodic(std::size_t _index)
	{
		auto& runtime = _MyEquipment[_index];
		if (!EquipmentActive(runtime)) return;
		const auto& unit = Unit(runtime.MyUnit);
		const auto& effect = EquipmentDefinition(runtime);
		const auto& p = effect.MyParameters;
		if (p.MyKind == EquipmentEffectKind::STEALTH_CHARGE && runtime.MyDeployment != unit.MyDeploySequence)
		{
			runtime.MyDeployment = unit.MyDeploySequence; runtime.MyBonus = 0; runtime.MyArmed = false; runtime.MyWasStealth = false;
		}
		if (!unit.MyAlive) return;
		if (p.MyKind == EquipmentEffectKind::STEALTH_CHARGE && EquipmentMember(unit, "siracusaShip"))
		{
			const bool stealth = unit.MyStatuses.Has(CombatStatus::STEALTH);
			if (stealth && runtime.MyBonus < p.MyExtra)
			{
				runtime.MyBonus = std::min(p.MyExtra, runtime.MyBonus + p.MyValue * 0.25);
				(void)ApplyEquipmentBuff(runtime, unit.MyId, BuffDefinition{.MyKey = runtime.MyBuffKey,
					.MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, runtime.MyBonus}}});
			}
			if (runtime.MyWasStealth && !stealth && runtime.MyBonus > 0) runtime.MyArmed = true;
			runtime.MyWasStealth = stealth;
		}
		if (p.MyKind == EquipmentEffectKind::KNIGHT_CREED && Time() < runtime.MyReadyAt)
		{
			EquipmentEnemies(runtime, AttackProfile{.MyCanHitFlying = true});
			for (std::size_t i = 0; i < runtime.MyTargets.size(); ++i)
			{
				const auto& enemy = Unit(runtime.MyTargets[i]);
				(void)ApplyEquipmentBuff(runtime, enemy.MyId, BuffDefinition{.MyKey = effect.MyKey + ":aura", .MyDuration = 0.6,
					.MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_SPEED, -(1 - p.MyExtra) * enemy.MyDefinition.MyStats.MyAttackSpeed},
					{Attribute::MOVE_MULTIPLIER, p.MyThreshold}}});
			}
		}
		if (p.MyKind == EquipmentEffectKind::COLD_DAMAGE && EquipmentMember(unit, "kjeragShip"))
		{
			const auto scale = !effect.MyPartner.empty() && CarriesEquipment(unit, effect.MyPartner) ? p.MyExtra : p.MyValue;
			EquipmentEnemies(runtime, AttackProfile{.MyCanHitFlying = true});
			for (std::size_t i = 0; i < runtime.MyTargets.size(); ++i)
			{
				const auto& enemy = Unit(runtime.MyTargets[i]);
				if (enemy.MyStatuses.Has(CombatStatus::COLD) || enemy.MyStatuses.Has(CombatStatus::FREEZE))
					(void)DealDamage(unit.MyId, enemy.MyId, DamageInfo{.MyAmount = unit.MyStats.MyAttack * scale,
						.MyType = DamageType::ARTS, .MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::ITEM)});
			}
		}
		if (p.MyKind == EquipmentEffectKind::SIDE_ATTACK_SPEED)
			for (const auto row : {1, -1})
			{
				const auto offset = RotateOffset({row, 0}, unit.MyFacing);
				for (std::size_t i = 0; i < _MyAllyIds.size(); ++i)
				{
					const auto& ally = Unit(_MyAllyIds[i]);
					if (!ally.MyAlive || ally.MyKind != UnitKind::OPERATOR || ally.MyOwner != unit.MyOwner ||
						ally.MyPosition.MyX != unit.MyPosition.MyX + offset.MyColumn || ally.MyPosition.MyY != unit.MyPosition.MyY + offset.MyRow) continue;
					(void)ApplyEquipmentBuff(runtime, ally.MyId, BuffDefinition{.MyKey = runtime.MyBuffKey, .MyDuration = 0.6,
						.MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_SPEED, p.MyValue}}});
					break;
				}
			}
	}

	void Battle::NotifyEquipmentLate(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE) return;
		for (const auto& runtime : _MyEquipment)
		{
			if (_event.MyTarget != runtime.MyUnit || !EquipmentActive(runtime)) continue;
			const auto& unit = Unit(runtime.MyUnit);
			const auto& p = EquipmentDefinition(runtime).MyParameters;
			if (unit.MyRemoved || p.MyKind != EquipmentEffectKind::FLAT_DAMAGE_REDUCTION || !(p.MyValue > 0)) continue;
			const auto& damage = _event.MyDamage;
			const CombatStats neutral;
			const auto& source = _event.MySource ? Unit(_event.MySource).MyStats : neutral;
			const auto base = Mitigate(damage.MyAmount * damage.MyMultiplier, damage.MyType, Mitigation{
				.MyDefense = unit.MyStats.MyDefense, .MyResistance = unit.MyStats.MyResistance,
				.MyDefenseIgnorePercent = source.MyDefenseIgnorePercent + damage.MyDefenseIgnorePercent,
				.MyDefenseIgnoreFlat = source.MyDefenseIgnoreFlat + damage.MyDefenseIgnoreFlat,
				.MyResistanceIgnorePercent = source.MyResistanceIgnorePercent + damage.MyResistanceIgnorePercent,
				.MyResistanceIgnoreFlat = source.MyResistanceIgnoreFlat + damage.MyResistanceIgnoreFlat});
			if (base > 0) _event.MyDamage.MyMultiplier *= std::max(0.0, base - p.MyValue) / base;
		}
	}
}
