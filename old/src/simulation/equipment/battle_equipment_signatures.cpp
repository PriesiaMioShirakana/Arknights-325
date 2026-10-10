#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::BuildEnemyIndex()
	{
		for (auto& bucket : _MyEnemyBuckets) bucket.clear();
		for (const auto id : _MyEnemyIds)
		{
			const auto& unit = Unit(id); if (!unit.MyAlive || unit.MyHidden) continue;
			const auto tiles = BodyTiles(unit);
			for (int row = tiles.MyFirstRow; row <= tiles.MyLastRow; ++row)
				for (int column = tiles.MyFirstColumn; column <= tiles.MyLastColumn; ++column)
				{
					auto& bucket = _MyEnemyBuckets[static_cast<std::size_t>(FieldGrid::Key(row, column))];
					if (bucket.capacity() == 0) bucket.reserve(8);
					bucket.push_back(id);
				}
		}
	}

	void BattleCore::EquipmentEnemies(EquipmentRuntime& _runtime, const AttackProfile& _profile)
	{
		const auto& unit = Unit(_runtime.MyUnit);
		auto& targets = _runtime.MyTargets;
		targets.clear(); targets.reserve(_MyEnemyIds.size());
		// 与原版 tile buckets 相同：先射程格顺序，再出生顺序；巨型敌人只加入一次。
		for (const auto key : unit.MyRangeKeys)
			for (const auto id : _MyEnemyBuckets[static_cast<std::size_t>(key)])
				if (const auto& enemy = Unit(id); TargetableEnemy(enemy, _profile) && (!enemy.MyDefinition.MyHitArea || !std::ranges::contains(targets, id)))
					targets.push_back(id);
	}

	void BattleCore::NotifyEquipmentSignature(EquipmentRuntime& _runtime, ContentEvent& _event)
	{
		const auto& unit = Unit(_runtime.MyUnit);
		const auto& effect = EquipmentDefinition(_runtime);
		const auto& p = effect.MyParameters;
		const bool own = _event.MyUnit == unit.MyId, source = _event.MySource == unit.MyId;
		const auto partner = [&] { return !effect.MyPartner.empty() && CarriesEquipment(unit, effect.MyPartner); };
		switch (p.MyKind)
		{
		case EquipmentEffectKind::KNIGHT_CREED:
			if (_event.MyKind == ContentEventKind::SKILL_START && own && EquipmentMember(unit, "kazimierzShip"))
			{
				_runtime.MyReadyAt = Time() + p.MyDuration;
				const auto kind = unit.MyDefinition.MySkill.MyKind;
				if (partner() && (kind == SkillKind::DURATION || kind == SkillKind::AMMO || kind == SkillKind::TOGGLE))
				{
					_runtime.MyCombo = true; _runtime.MyDoomed = false;
					(void)ApplyEquipmentBuff(_runtime, unit.MyId, BuffDefinition{.MyKey = _runtime.MyBuffKey,
						.MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, p.MyValue}}});
				}
			}
			if (_event.MyKind == ContentEventKind::SKILL_END && own && _runtime.MyCombo)
			{
				_runtime.MyCombo = false; (void)RemoveBuff(unit.MyId, _runtime.MyBuffKey);
				if (_runtime.MyDoomed && _event.MySkillReason != SkillReason::DEATH)
				{
					_runtime.MyDoomed = false;
					Schedule(ScheduledAction{.MyAt = Time(), .MyKind = ScheduledKind::EQUIPMENT_RETREAT, .MySource = unit.MyId});
				}
			}
			if (_event.MyKind == ContentEventKind::FATAL && own && !_event.MyPrevented && _runtime.MyCombo)
			{
				_event.MyPrevented = true; _runtime.MyDoomed = true;
			}
			break;
		case EquipmentEffectKind::PARTNER_HEAL:
			if (_event.MyKind == ContentEventKind::BUFF_TICK && own && _event.MyBuff == _runtime.MyTickBuff && unit.MyAlive && partner())
				(void)Heal(unit.MyId, unit.MyId, unit.MyStats.MyMaxHealth * p.MyValue, HealOptions{.MySelf = true});
			break;
		case EquipmentEffectKind::PARTNER_TRUE_DAMAGE:
			if (_event.MyKind == ContentEventKind::DAMAGED && source && !_event.MyElement && !HasTag(_event.MyDamage.MyTags, DamageTag::ITEM) &&
				Unit(_event.MyTarget).MyAlive && Unit(_event.MyTarget).MySide == UnitSide::ENEMY && p.MyValue > 0 && partner())
				(void)DealDamage(unit.MyId, _event.MyTarget, DamageInfo{.MyAmount = unit.MyStats.MyAttack * p.MyValue,
					.MyType = DamageType::TRUE_DAMAGE, .MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::ITEM)});
			break;
		case EquipmentEffectKind::EXTRA_BULLET:
			if (_event.MyKind == ContentEventKind::ATTACK && source && EquipmentMember(unit, "lateranoShip") &&
				std::ranges::any_of(_event.MyTargets, [&](UnitId _id) { return Unit(_id).MySide == UnitSide::ENEMY; }) &&
				p.MyProbability > 0 && (p.MyProbability >= 1 || _MyRandom.Next() < p.MyProbability))
			{
				EquipmentEnemies(_runtime, EffectiveAttack(unit));
				if (_runtime.MyTargets.empty()) break;
				const auto target = _runtime.MyTargets[_MyRandom.Index(static_cast<std::uint32_t>(_runtime.MyTargets.size()))];
				(void)DealDamage(unit.MyId, target, DamageInfo{.MyAmount = unit.MyStats.MyAttack * (partner() ? p.MyExtra : p.MyValue),
					.MyType = DamageType::PHYSICAL, .MyTags = static_cast<DamageTags>(DamageTag::ITEM)});
			}
			break;
		case EquipmentEffectKind::SKILL_COMPASS:
			if (_event.MyKind == ContentEventKind::DEPLOY && own && p.MyValue > 0) (void)GainSp(unit.MyId, p.MyValue, SpReason::INITIAL);
			if (_event.MyKind == ContentEventKind::SKILL_END && own && !_runtime.MyUsed && _event.MySkillReason != SkillReason::DEATH && EquipmentMember(unit, "sargonShip"))
			{
				_runtime.MyUsed = 1;
				if (unit.MyAlive && p.MyExtra > 0) (void)GainSp(unit.MyId, p.MyExtra);
			}
			if (_event.MyKind == ContentEventKind::SKILL_START && own && p.MyThreshold > 0 && EquipmentMember(unit, "sargonShip") && partner())
				for (std::size_t i = 0; i < _MyAllyIds.size(); ++i)
				{
					const auto& ally = Unit(_MyAllyIds[i]);
					if (ally.MyAlive && ally.MyOwner == unit.MyOwner && EquipmentMember(ally, "sargonShip")) (void)GainSp(ally.MyId, p.MyThreshold);
				}
			break;
		case EquipmentEffectKind::STEALTH_CHARGE:
			if (_event.MyKind == ContentEventKind::DEATH && own) { _runtime.MyBonus = 0; _runtime.MyArmed = false; }
			if (_event.MyKind == ContentEventKind::DAMAGED && source && _runtime.MyArmed && _event.MyAmount > 0 && !HasTag(_event.MyDamage.MyTags, DamageTag::ITEM))
			{
				_runtime.MyArmed = false;
				if (partner() && Unit(_event.MyTarget).MyAlive && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
					(void)DealDamage(unit.MyId, _event.MyTarget, DamageInfo{.MyAmount = unit.MyStats.MyAttack * p.MyThreshold,
						.MyType = DamageType::TRUE_DAMAGE, .MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::ITEM)});
				_runtime.MyBonus = 0; (void)RemoveBuff(unit.MyId, _runtime.MyBuffKey);
			}
			break;
		default: break;
		}
	}
}
