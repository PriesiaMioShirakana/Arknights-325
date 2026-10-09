#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	const EquipmentEffect& Battle::EquipmentDefinition(const EquipmentRuntime& _runtime) const
	{
		return _runtime.MyLend ? _runtime.MyLentEffect : Unit(_runtime.MyUnit).MyDefinition.MyEquipmentEffects[_runtime.MyEffect];
	}

	bool Battle::EquipmentActive(const EquipmentRuntime& _runtime) const
	{
		return !_runtime.MyLend || _MyEquipmentLends[*_runtime.MyLend].MyActive;
	}

	void Battle::TrackEquipmentBuff(std::size_t _lend, UnitId _target, std::string _key)
	{
		auto& buffs = _MyEquipmentLends[_lend].MyBuffs;
		if (!std::ranges::any_of(buffs, [&](const EquipmentBuffOwner& _buff) { return _buff.MyUnit == _target && _buff.MyKey == _key; }))
			buffs.push_back({_target, std::move(_key)});
	}

	std::uint64_t Battle::ApplyEquipmentBuff(EquipmentRuntime& _runtime, UnitId _target, BuffDefinition _buff)
	{
		const auto key = _runtime.MyLend ? _buff.MyKey : std::string{};
		const auto id = AddBuff(_target, std::move(_buff));
		if (id && _runtime.MyLend) TrackEquipmentBuff(*_runtime.MyLend, _target, key);
		return id;
	}

	void Battle::CheckEquipmentFront(EquipmentRuntime& _runtime, bool _initial)
	{
		const auto& unit = Unit(_runtime.MyUnit);
		const auto& p = EquipmentDefinition(_runtime).MyParameters;
		const auto offset = RotateOffset({0, 1}, unit.MyFacing);
		const bool occupied = std::ranges::any_of(_MyAllyIds, [&](UnitId _id)
		{
			const auto& other = Unit(_id); const auto position = _initial ? other.MyHome : other.MyPosition;
			return _id != unit.MyId && other.MyKind == UnitKind::OPERATOR && !other.MyRemoved && (_initial || other.MyAlive) &&
				position.MyX == unit.MyPosition.MyX + offset.MyColumn && position.MyY == unit.MyPosition.MyY + offset.MyRow;
		});
		if (occupied) (void)RemoveBuff(unit.MyId, _runtime.MyBuffKey);
		else (void)ApplyEquipmentBuff(_runtime, unit.MyId, BuffDefinition{.MyKey = _runtime.MyBuffKey,
			.MyModifiers = std::vector<AttributeChange>{{Attribute::HEALTH_PERCENT, p.MyValue}}});
	}

	void Battle::InstallEquipmentLend(std::size_t _index, const EquipmentTemplate& _definition)
	{
		auto& grant = _MyEquipmentLends[_index];
		const auto prefix = "item:" + grant.MyItem + "@lend";
		grant.MyBuffs.reserve(_definition.MyStats.size() + _definition.MyEffects.size() + 8);
		grant.MyEffects.reserve(_definition.MyEffects.size());
		for (const auto& stat : _definition.MyStats)
		{
			const auto key = prefix + ":stat:" + std::string(stat.MyBuff);
			if (AddBuff(grant.MyUnit, BuffDefinition{.MyKey = key,
				.MyModifiers = std::vector<AttributeChange>(stat.MyModifiers.begin(), stat.MyModifiers.end()), .MyPersistent = true, .MyAllowDead = true}))
				TrackEquipmentBuff(_index, grant.MyUnit, key);
		}
		for (const auto& effect : _definition.MyEffects)
		{
			const auto handle = _MyEquipment.size();
			_MyEquipment.emplace_back(EquipmentRuntime{.MyUnit = grant.MyUnit, .MyLend = _index,
				.MyLentEffect = EquipmentEffect{prefix, effect.MyParameters, std::string(effect.MyPartner)}});
			grant.MyEffects.push_back(handle);
			InitializeEquipment(handle);
		}
	}

	std::size_t Battle::LendEquipment(UnitId _from, UnitId _to, unsigned _maximumTier, double _duration)
	{
		const auto& from = Unit(_from); const auto& to = Unit(_to);
		if (_from == _to || from.MyKind != UnitKind::OPERATOR || to.MyKind != UnitKind::OPERATOR ||
			!to.MyAlive || to.MyRemoved || Finished() || !std::isfinite(_duration) || !(_duration > 0) || !std::isfinite(Time() + _duration)) return 0;
		std::size_t count = 0;
		for (const auto& item : from.MyDefinition.MyIdentity.MyItems)
		{
			const auto& templates = _MyInput.MyEquipmentTemplates;
			const auto definition = std::ranges::lower_bound(templates, item, {}, &EquipmentTemplate::MyId);
			if (definition == templates.end() || definition->MyId != item || definition->MyTier > _maximumTier ||
				(definition->MyStats.empty() && definition->MyEffects.empty())) continue;
			auto found = std::ranges::find_if(_MyEquipmentLends, [&](const EquipmentLend& _grant)
				{ return _grant.MyActive && _grant.MyUnit == _to && _grant.MyItem == item; });
			std::size_t index;
			if (found == _MyEquipmentLends.end())
			{
				index = _MyEquipmentLends.size();
				_MyEquipmentLends.emplace_back(EquipmentLend{.MyUnit = _to, .MyItem = item});
				InstallEquipmentLend(index, *definition);
			}
			else index = static_cast<std::size_t>(found - _MyEquipmentLends.begin());
			auto& grant = _MyEquipmentLends[index];
			grant.MyUntil = Time() + _duration;
			Schedule(ScheduledAction{.MyAt = grant.MyUntil, .MyKind = ScheduledKind::EQUIPMENT_EXPIRE,
				.MySource = _to, .MyHandle = index, .MyVersion = ++grant.MyVersion});
			++count;
		}
		return count;
	}

	void Battle::ExpireEquipmentLend(std::size_t _index)
	{
		auto& grant = _MyEquipmentLends[_index];
		if (!grant.MyActive) return;
		grant.MyActive = false;
		for (std::size_t i = 0; i < grant.MyBuffs.size(); ++i)
		{
			const auto buff = grant.MyBuffs[i];
			(void)RemoveBuff(buff.MyUnit, buff.MyKey);
		}
		for (const auto effect : grant.MyEffects)
		{
			auto& runtime = _MyEquipment[effect];
			const auto kind = EquipmentDefinition(runtime).MyParameters.MyKind;
			if (kind >= EquipmentEffectKind::HAMMER_BURN && kind <= EquipmentEffectKind::STEAM_HEART) ReleaseHammer(runtime);
		}
	}

	std::vector<Battle::LentEquipmentView> Battle::LentEquipment(UnitId _unit) const
	{
		(void)Unit(_unit);
		std::vector<LentEquipmentView> result; result.reserve(_MyEquipmentLends.size());
		for (const auto& grant : _MyEquipmentLends)
			if (grant.MyActive && grant.MyUnit == _unit) result.push_back({_unit, grant.MyItem, grant.MyUntil});
		return result;
	}
}
