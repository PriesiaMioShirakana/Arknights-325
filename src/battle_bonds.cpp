#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		constexpr auto Slot(AddonBondKind _kind) { return static_cast<unsigned>(_kind); }
	}

	double Battle::AddonLayers(const AddonBondRuntime& _state, AddonBondKind _kind) const
	{
		return BondLayers(_MyPlayers[_state.MyPlayer].MyPlayerId, AddonBondIds[Slot(_kind)]);
	}

	void Battle::ApplyEliteSpCost(CombatUnit& _unit, const AddonBondParameters& _parameters)
	{
		if (_unit.MyDefinition.MySkill.MyKind == SkillKind::NONE || _unit.MyEliteSpCostApplied) return;
		_unit.MyEliteSpCostApplied = true;
		_unit.MySkill.MySpCostMultiplier *= _parameters.MySpCostMultiplier;
		NormalizeSp(_unit);
	}

	void Battle::InstallAddonBonds()
	{
		if (std::ranges::none_of(_MyInput.MyPlayers, [](const auto& _player) { return !_player.MyAddonBonds.empty(); })) return;
		_MyAddonBonds.resize(_MyPlayers.size());
		bool aura = false, raid = false;
		for (std::size_t i = 0; i < _MyPlayers.size(); ++i)
		{
			auto& state = _MyAddonBonds[i]; state.MyPlayer = i;
			const auto& player = _MyInput.MyPlayers[i];
			state.MyOperators.reserve(player.MyUnits.size());
			for (const auto id : _MyAllyIds)
				if (Unit(id).MyOwner == i && Unit(id).MyKind == UnitKind::OPERATOR) state.MyOperators.push_back(id);
			for (const auto& effect : player.MyAddonBonds)
			{
				if (effect.MyKind >= AddonBondKind::COUNT) throw std::invalid_argument("invalid addon bond kind");
				const auto slot = Slot(effect.MyKind);
				if (state.MyParameters[slot]) throw std::invalid_argument("duplicate addon bond");
				const auto& p = effect.MyParameters;
				for (const auto value : {p.MyAttack, p.MyAttackPerLayer, p.MyHealth, p.MyHealthPerLayer, p.MyDefense, p.MyDefensePerLayer,
					p.MyAttackSpeed, p.MyAttackSpeedPerLayer, p.MyDefenseIgnore, p.MyResistanceIgnore, p.MyProbability, p.MyProbabilityPerLayer,
					p.MySp, p.MyExtraSp, p.MyDamageMultiplier, p.MyDamagePerLayer, p.MyLowHealthMultiplier, p.MyHealthThreshold, p.MyDuration,
					p.MyResistance, p.MyCooldown, p.MyThornDamage, p.MyThornPerLayer, p.MyFragile, p.MyRedeployDelta, p.MyMilestoneAttackSpeed,
					p.MyIdleTime, p.MyEliteDamageMultiplier, p.MySpCostMultiplier})
					if (!std::isfinite(value)) throw std::invalid_argument("invalid addon bond parameter");
				if (std::isnan(p.MyMilestone) || p.MyDuration < 0 || p.MyCooldown < 0 || p.MySpCostMultiplier < 0)
					throw std::invalid_argument("invalid addon bond interval or multiplier");
				const auto bond = std::ranges::find(player.MyBonds, AddonBondIds[slot], &BondLayer::MyId);
				if (bond == player.MyBonds.end() || !bond->MyActive) continue;
				state.MyParameters[slot] = &p; state.MyTiers[slot] = std::max(1U, bond->MyTier);
				auto& members = state.MyMembers[slot]; members.reserve(state.MyOperators.size());
				for (const auto id : state.MyOperators)
					if (std::ranges::contains(Unit(id).MyDefinition.MyIdentity.MyBonds, AddonBondIds[slot])) members.push_back(id);
				aura |= effect.MyKind == AddonBondKind::SKILLFUL;
				raid |= effect.MyKind == AddonBondKind::RAID && !members.empty();
				_MyBondHitEffects |= effect.MyKind == AddonBondKind::STEADFAST && state.MyTiers[slot] >= 2;
			}
			if (state.MyTiers[Slot(AddonBondKind::STEADFAST)] >= 2)
				state.MyShareFrames.emplace_back().reserve(state.MyMembers[Slot(AddonBondKind::STEADFAST)].size());
			state.MyAura.reserve(state.MyOperators.size()); state.MyNextAura.reserve(state.MyOperators.size());
			RefreshAddonBonds(i, true);
		}
		if (aura) Schedule({.MyAt = 0.25, .MyKind = ScheduledKind::BOND_AURA});
		_MyEquipmentEnemyQueries |= raid;
		if (raid) Schedule({.MyAt = 0.25, .MyKind = ScheduledKind::BOND_RAID});
	}

	void Battle::RefreshAddonBonds(std::size_t _player, bool _initial)
	{
		auto& state = _MyAddonBonds[_player];
		const auto passive = [&](UnitId _id, std::string_view _key, std::initializer_list<AttributeChange> _mods)
		{
			(void)AddBuff(_id, BuffDefinition{.MyKey = std::string(_key), .MyModifiers = std::vector<AttributeChange>(_mods), .MyPersistent = true, .MyAllowDead = true});
		};
		const auto members = [&](AddonBondKind _kind) -> const auto& { return state.MyMembers[Slot(_kind)]; };
		const auto tier = [&](AddonBondKind _kind) { return state.MyTiers[Slot(_kind)]; };
		if (const auto p = state.MyParameters[Slot(AddonBondKind::PRECISE)])
		{
			const bool wide = tier(AddonBondKind::PRECISE) >= 2;
			for (const auto id : state.MyOperators)
				if (std::ranges::contains(members(AddonBondKind::PRECISE), id) || (wide && !Unit(id).MyDefinition.MyIdentity.MyMeleePosition))
					passive(id, "bond:preciShip", {{Attribute::ATTACK_PERCENT, p->MyAttack + p->MyAttackPerLayer * AddonLayers(state, AddonBondKind::PRECISE)},
						{Attribute::DEFENSE_IGNORE_PERCENT, wide ? p->MyDefenseIgnore : 0}, {Attribute::RESISTANCE_IGNORE_PERCENT, wide ? p->MyResistanceIgnore : 0}});
		}
		if (const auto p = state.MyParameters[Slot(AddonBondKind::STEADFAST)])
			for (const auto id : state.MyOperators) passive(id, "bond:steadShip", {{Attribute::HEALTH_PERCENT, p->MyHealth + p->MyHealthPerLayer * AddonLayers(state, AddonBondKind::STEADFAST)}});
		if (const auto p = state.MyParameters[Slot(AddonBondKind::DEPUTY)])
			for (const auto id : state.MyOperators) passive(id, "bond:deputShip", {{Attribute::DEFENSE_PERCENT, p->MyDefense + p->MyDefensePerLayer * AddonLayers(state, AddonBondKind::DEPUTY)},
				{Attribute::REDEPLOY_MULTIPLIER, std::max(0.0, 1 + p->MyRedeployDelta)}});
		if (const auto p = state.MyParameters[Slot(AddonBondKind::RAID)])
		{
			const auto layers = AddonLayers(state, AddonBondKind::RAID);
			if (layers >= p->MyMilestone)
				for (const auto id : state.MyOperators) passive(id, "bond:raidShip:aspd", {{Attribute::ATTACK_SPEED, p->MyMilestoneAttackSpeed}});
			for (const auto id : members(AddonBondKind::RAID))
			{
				auto& unit = _MyUnits[Index(id)];
				const auto found = std::ranges::find(unit.MyBuffs, "bond:raidShip", [](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey; });
				if (found != unit.MyBuffs.end())
				{
					found->MyDefinition.MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, p->MyAttack + p->MyAttackPerLayer * layers},
						{Attribute::HEALTH_PERCENT, p->MyHealth + p->MyHealthPerLayer * layers}, {Attribute::ATTACK_SPEED, p->MyAttackSpeed + p->MyAttackSpeedPerLayer * layers}};
					Recalculate(unit);
				}
			}
		}
		if (const auto p = state.MyParameters[Slot(AddonBondKind::ARCANE)])
		{
			const auto multiplier = p->MyDamageMultiplier + p->MyDamagePerLayer * AddonLayers(state, AddonBondKind::ARCANE);
			state.MyArcaneMultiplier = std::max(0.0, multiplier);
			state.MyArcaneLowMultiplier = std::max(0.0, multiplier * p->MyLowHealthMultiplier);
		}
		if (const auto p = state.MyParameters[Slot(AddonBondKind::SKILLFUL)])
		{
			const auto layers = AddonLayers(state, AddonBondKind::SKILLFUL);
			state.MyAuraSpeed = p->MyAttackSpeed + p->MyAttackSpeedPerLayer * layers;
			state.MyAuraWide = layers >= p->MyMilestone;
		}
		if (!_initial) return;
		if (const auto p = state.MyParameters[Slot(AddonBondKind::ASSIST)])
			for (const auto id : state.MyOperators)
			{
				passive(id, "bond:emptyShip", {{Attribute::PHYSICAL_TAKEN_MULTIPLIER, std::max(0.0, 1 - p->MyResistance)}, {Attribute::ARTS_TAKEN_MULTIPLIER, std::max(0.0, 1 - p->MyResistance)}});
				if (std::ranges::contains(members(AddonBondKind::ASSIST), id))
					passive(id, "bond:emptyShip:dmg", {{Attribute::DAMAGE_DEALT_MULTIPLIER, Unit(id).MyDefinition.MyIdentity.MyGolden ? p->MyEliteDamageMultiplier : p->MyDamageMultiplier}});
			}
		if (const auto p = state.MyParameters[Slot(AddonBondKind::SOLO)])
			for (const auto id : members(AddonBondKind::SOLO)) passive(id, "bond:soloShip", {{Attribute::ATTACK_PERCENT, p->MyAttack}, {Attribute::HEALTH_PERCENT, p->MyHealth}});
		if (const auto p = state.MyParameters[Slot(AddonBondKind::ELITE)])
			for (const auto id : state.MyOperators) if (Unit(id).MyDefinition.MyIdentity.MyGolden)
			{
				passive(id, "bond:suntShip", {{Attribute::ATTACK_PERCENT, p->MyAttack}});
				if (tier(AddonBondKind::ELITE) >= 2) ApplyEliteSpCost(_MyUnits[Index(id)], *p);
			}
	}

	void Battle::UpdateBondAura(std::size_t _player)
	{
		auto& state = _MyAddonBonds[_player];
		if (!state.MyParameters[Slot(AddonBondKind::SKILLFUL)]) return;
		auto& next = state.MyNextAura; next.clear();
		std::array<UnitId, FieldTiles> occupied{};
		for (const auto id : _MyAllyIds)
		{
			const auto& unit = Unit(id);
			if (!unit.MyAlive || unit.MyRemoved || unit.MyKind == UnitKind::DEVICE) continue;
			const auto point = RulePosition(unit);
			const int row = static_cast<int>(std::floor(point.MyY + 0.5)), column = static_cast<int>(std::floor(point.MyX + 0.5));
			if (FieldGrid::InBounds(row, column)) occupied[static_cast<unsigned>(FieldGrid::Key(row, column))] = id;
		}
		const auto add = [&](UnitId _id) { if (!std::ranges::contains(next, _id)) next.push_back(_id); };
		constexpr std::array<RangeOffset, 4> near{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
		constexpr std::array<RangeOffset, 8> wide{{{1, -1}, {1, 0}, {1, 1}, {0, -1}, {0, 1}, {-1, -1}, {-1, 0}, {-1, 1}}};
		const std::span<const RangeOffset> offsets = state.MyAuraWide ? std::span<const RangeOffset>(wide) : near;
		for (const auto id : state.MyMembers[Slot(AddonBondKind::SKILLFUL)])
		{
			const auto& member = Unit(id);
			const bool alive = member.MyAlive && !member.MyRemoved;
			if (!alive && (member.MyRemovalReason != RemovalReason::KILLED || !IsDown(id))) continue;
			if (alive) add(id);
			const auto point = alive || UsesInitialPosition(member) ? RulePosition(member) : RestPosition(id);
			for (const auto offset : offsets)
			{
				const int row = static_cast<int>(std::floor(point.MyY + 0.5)) + offset.MyRow, column = static_cast<int>(std::floor(point.MyX + 0.5)) + offset.MyColumn;
				if (!FieldGrid::InBounds(row, column)) continue;
				const auto allyId = occupied[static_cast<unsigned>(FieldGrid::Key(row, column))];
				if (allyId && Unit(allyId).MyKind == UnitKind::OPERATOR && Unit(allyId).MyOwner == _player) add(allyId);
			}
		}
		for (const auto id : state.MyAura) if (!std::ranges::contains(next, id)) (void)RemoveBuff(id, "bond:skillfulShip");
		for (const auto id : next)
		{
			auto& unit = _MyUnits[Index(id)];
			const auto found = std::ranges::find(unit.MyBuffs, "bond:skillfulShip", [](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey; });
			if (found == unit.MyBuffs.end())
				(void)AddBuff(id, BuffDefinition{.MyKey = "bond:skillfulShip", .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_SPEED, state.MyAuraSpeed}}});
			else if (found->MyDefinition.MyModifiers->front().MyValue != state.MyAuraSpeed)
			{
				found->MyDefinition.MyModifiers->front().MyValue = state.MyAuraSpeed;
				Recalculate(unit);
			}
		}
		state.MyAura.swap(next);
	}
}
