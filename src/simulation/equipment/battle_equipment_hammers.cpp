#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::InstallHammer(EquipmentRuntime& _runtime)
	{
		const auto& unit = Unit(_runtime.MyUnit);
		const auto& p = EquipmentDefinition(_runtime).MyParameters;
		auto found = std::ranges::find(_MyHammers, unit.MyId, &HammerRuntime::MyUnit);
		if (found == _MyHammers.end())
		{
			found = _MyHammers.emplace(_MyHammers.end(), HammerRuntime{.MyUnit = unit.MyId,
				.MySpeedKey = "item:hammer#" + std::to_string(unit.MyId) + ":aspd"});
			_MyHammerFields.resize(_MyPlayers.size());
		}
		if (found->MyReferences++ == 0)
		{
			_runtime.MyHammer = static_cast<std::size_t>(found - _MyHammers.begin());
			_runtime.MyHammerGeneration = ++found->MyGeneration;
			Schedule(ScheduledAction{.MyAt = Time(), .MyKind = ScheduledKind::HAMMER, .MySource = unit.MyId,
				.MyHandle = *_runtime.MyHammer, .MyInterval = 0.5, .MyVersion = found->MyGeneration});
		}
		if (p.MyKind == EquipmentEffectKind::STEAM_HEART)
		{
			++found->MySteam;
			if (!found->MySteamParameters) found->MySteamParameters = p;
		}
		else
		{
			const auto type = static_cast<std::size_t>(p.MyKind) - static_cast<std::size_t>(EquipmentEffectKind::HAMMER_BURN);
			++found->MyOwned[type];
			if (!_runtime.MyLend) found->MyFieldKinds.set(type);
			if (!found->MyParameters[type]) found->MyParameters[type] = p;
		}
		RefreshHammer(*found);
	}

	void BattleCore::ReleaseHammer(EquipmentRuntime& _runtime)
	{
		const auto found = std::ranges::find(_MyHammers, _runtime.MyUnit, &HammerRuntime::MyUnit);
		if (found == _MyHammers.end() || !found->MyReferences) return;
		const auto kind = EquipmentDefinition(_runtime).MyParameters.MyKind;
		if (kind == EquipmentEffectKind::STEAM_HEART) --found->MySteam;
		else --found->MyOwned[static_cast<std::size_t>(kind) - static_cast<std::size_t>(EquipmentEffectKind::HAMMER_BURN)];
		if (--found->MyReferences == 0)
		{
			found->MySpeed = 0; (void)RemoveBuff(found->MyUnit, found->MySpeedKey);
		}
		else RefreshHammer(*found);
	}

	const EquipmentParameters* BattleCore::HammerParameters(const HammerRuntime& _runtime, HammerKind _kind) const
	{
		const auto& own = _runtime.MyParameters[static_cast<std::size_t>(_kind)];
		return own ? &*own : _runtime.MySteamParameters ? &*_runtime.MySteamParameters : nullptr;
	}

	unsigned BattleCore::HammerMultiplier(const HammerRuntime& _runtime, HammerKind _kind)
	{
		const auto type = static_cast<std::size_t>(_kind);
		const auto& unit = Unit(_runtime.MyUnit);
		if (!_runtime.MySteam || !EquipmentMember(unit, "victoriaShip")) return _runtime.MyOwned[type];
		auto& field = _MyHammerFields[unit.MyOwner];
		if (Time() - field.MyAt >= 0.5 - 1e-9)
		{
			field.MyAt = Time(); field.MyKinds.reset();
			for (const auto& hammer : _MyHammers)
			{
				const auto& other = Unit(hammer.MyUnit);
				if (other.MyAlive && other.MyOwner == unit.MyOwner) field.MyKinds |= hammer.MyFieldKinds;
			}
		}
		return _runtime.MyOwned[type] + (field.MyKinds.test(type) ? 1U : 0U);
	}

	void BattleCore::RefreshHammer(HammerRuntime& _runtime)
	{
		const auto* p = HammerParameters(_runtime, HammerKind::SPEED);
		const auto value = p ? (p->MyKind == EquipmentEffectKind::STEAM_HEART ? p->MyThreshold : p->MyValue) * HammerMultiplier(_runtime, HammerKind::SPEED) : 0;
		if (value == _runtime.MySpeed) return;
		_runtime.MySpeed = value;
		if (value == 0) (void)RemoveBuff(_runtime.MyUnit, _runtime.MySpeedKey);
		else (void)AddBuff(_runtime.MyUnit, BuffDefinition{.MyKey = _runtime.MySpeedKey,
			.MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_SPEED, value}}, .MyPersistent = true, .MyAllowDead = true});
	}

	void BattleCore::NotifyHammer(HammerRuntime& _runtime, ContentEvent& _event)
	{
		const auto& unit = Unit(_runtime.MyUnit);
		if (_event.MySource != unit.MyId) return;
		if (_event.MyKind == ContentEventKind::DAMAGED && !_event.MyElement && _event.MyDamage.MyType == DamageType::ARTS &&
			_event.MyAmount > 0 && !HasTag(_event.MyDamage.MyTags, DamageTag::ITEM) && _event.MyTarget &&
			Unit(_event.MyTarget).MySide == UnitSide::ENEMY && Unit(_event.MyTarget).MyAlive && Unit(_event.MyTarget).MyHealth > 0)
		{
			const auto* p = HammerParameters(_runtime, HammerKind::BURN);
			const auto n = p ? HammerMultiplier(_runtime, HammerKind::BURN) : 0;
			if (n) (void)DealElement(unit.MyId, _event.MyTarget, ElementHit{.MyElement = Element::BURN,
				.MyAmount = _event.MyAmount * p->MyValue * n, .MyTags = static_cast<DamageTags>(DamageTag::ITEM)});
		}
		if (_event.MyKind == ContentEventKind::ATTACK && unit.MyDefinition.MyIdentity.MyMeleePosition)
		{
			const auto* p = HammerParameters(_runtime, HammerKind::TREMBLE);
			const auto n = p ? HammerMultiplier(_runtime, HammerKind::TREMBLE) : 0;
			if (!n) return;
			const auto chance = std::min(1.0, p->MyProbability * n);
			for (const auto id : _event.MyTargets)
				if (Unit(id).MySide == UnitSide::ENEMY && Unit(id).MyAlive && chance > 0 && (chance >= 1 || _MyRandom.Next() < chance))
					(void)ApplyStatus(id, CombatStatus::TREMBLE, p->MyDuration, unit.MyId);
		}
	}

	bool BattleCore::HoldsUndying(UnitId _unit) const
	{
		const auto& unit = Unit(_unit);
		if (unit.MyStatuses.Has(CombatStatus::UNDYING)) return true;
		const auto found = std::ranges::find(_MyHammers, _unit, &HammerRuntime::MyUnit);
		return found != _MyHammers.end() && found->MyHeldDeployment == unit.MyDeploySequence && std::isless(Time(), found->MyUntil);
	}

	void BattleCore::HammerFatal(ContentEvent& _event, bool _held)
	{
		if (_event.MyKind != ContentEventKind::FATAL || !_event.MyUnit || _event.MyPrevented) return;
		const auto& unit = Unit(_event.MyUnit);
		if (_held && HoldsUndying(unit.MyId)) { _event.MyPrevented = true; return; }
		const auto found = std::ranges::find(_MyHammers, unit.MyId, &HammerRuntime::MyUnit);
		if (found == _MyHammers.end()) return;
		auto& hammer = *found;
		if (_held)
		{
			// 已开始的不死窗口属于部署；后续装备借出到期也不结束该窗口。
			if (HoldsUndying(unit.MyId)) _event.MyPrevented = true;
			return;
		}
		if (hammer.MyLockDeployment == unit.MyDeploySequence) return;
		const auto* p = HammerParameters(hammer, HammerKind::UNDYING);
		const auto n = p ? HammerMultiplier(hammer, HammerKind::UNDYING) : 0;
		if (!n) return;
		hammer.MyLockDeployment = hammer.MyHeldDeployment = unit.MyDeploySequence;
		hammer.MyUntil = Time() + (p->MyKind == EquipmentEffectKind::STEAM_HEART ? p->MyExtra : p->MyDuration) * n;
		_event.MyPrevented = true;
	}
}
