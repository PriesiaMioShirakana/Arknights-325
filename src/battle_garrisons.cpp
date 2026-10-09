#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	bool Battle::GarrisonOnField(UnitId _unit) const
	{
		return _unit && Unit(_unit).MyAlive && !Unit(_unit).MyRemoved;
	}

	bool Battle::GarrisonSourceActive(UnitId _unit) const
	{
		if (!_unit) return false;
		const auto& unit = Unit(_unit);
		return GarrisonOnField(_unit) || (_MyInput.MyGarrisonEffectsAfterExit && unit.MySide == UnitSide::ALLY &&
			unit.MyKind == UnitKind::OPERATOR && unit.MyDeploySequence != 0);
	}

	void Battle::AddGarrison(UnitId _unit, std::size_t _rule, bool _granted)
	{
		for (auto& state : _MyGarrisons)
			if (state.MyUnit == _unit && state.MyRule == _rule)
			{
				state.MyGranted &= _granted; // 同名自身特质优先，重复授予不叠加或重置收益上限。
				return;
			}
		const auto& rule = _MyInput.MyGarrisonRules.MyEffects[_rule];
		const auto index = _MyGarrisons.size();
		auto& state = _MyGarrisons.emplace_back(); state.MyUnit = _unit; state.MyRule = _rule; state.MyKey = "gar:" + std::string(rule.MyId);
		state.MyGranted = _granted;
		state.MyUsed.reserve(_MyPlayers[Unit(_unit).MyOwner].MyBonds.size());
		auto& group = _MyGarrisonGroups[static_cast<unsigned>(rule.MyKind)];
		if (group.empty()) _MyGarrisonOrder.push_back(rule.MyKind);
		group.push_back(index);
	}

	void Battle::InstallGarrisons()
	{
		const auto rules = _MyInput.MyGarrisonRules.MyEffects;
		_MyGarrisonOrder.reserve(static_cast<unsigned>(BattleGarrisonKind::COUNT));
		std::string_view previous;
		for (const auto& rule : rules)
		{
			if (rule.MyId.empty() || (!previous.empty() && rule.MyId <= previous) || rule.MyKind >= BattleGarrisonKind::COUNT ||
				rule.MyBondTarget > GarrisonBondTarget::HIGHEST || rule.MyGainAmount > GarrisonGainAmount::TIER || rule.MyCondition > GarrisonCondition::COLUMN ||
				rule.MyGrantTarget > GarrisonGrantTarget::ALL || rule.MyAmmoScope > GarrisonAmmoScope::ADJACENT) throw std::invalid_argument("invalid battle garrison rule");
			for (const auto n : {rule.MyAmount, rule.MyMaximum, rule.MyCheckCount, rule.MyEvery, rule.MyProbability, rule.MyExtra, rule.MyDivisor,
				rule.MyAttack, rule.MyHealth, rule.MyDefense, rule.MyAttackSpeed, rule.MyHpRegen, rule.MySpRegen, rule.MyRedeploy, rule.MyDuration, rule.MyDamageScale})
				if (!std::isfinite(n)) throw std::invalid_argument("invalid battle garrison parameter");
			if (rule.MyEvery < 1 || rule.MyDivisor < 1 || rule.MyDuration < 0) throw std::invalid_argument("invalid battle garrison interval");
			previous = rule.MyId;
		}
		for (const auto id : _MyAllyIds)
		{
			const auto& unit = Unit(id);
			if (unit.MyKind != UnitKind::OPERATOR) continue;
			for (const auto& gid : unit.MyDefinition.MyIdentity.MyGarrisons)
			{
				const auto found = std::ranges::lower_bound(rules, gid, {}, &BattleGarrisonRule::MyId);
				if (found != rules.end() && found->MyId == gid) AddGarrison(id, static_cast<std::size_t>(found - rules.begin()));
			}
		}
		const auto count = _MyGarrisons.size();
		for (std::size_t i = 0; i < count; ++i) if (rules[_MyGarrisons[i].MyRule].MyKind == BattleGarrisonKind::GRANT) GrantGarrison(i);
		for (const auto kind : _MyGarrisonOrder)
			if (kind == BattleGarrisonKind::BASE_ATTRIBUTES || kind == BattleGarrisonKind::COMMON_ATTRIBUTES)
				for (const auto index : _MyGarrisonGroups[static_cast<unsigned>(kind)]) ApplyGarrisonAttributes(index);
		for (const auto kind : {BattleGarrisonKind::ATTRIBUTES_BY_BOND, BattleGarrisonKind::REDEPLOY_BY_BOND, BattleGarrisonKind::DEPLOY_ATTRIBUTES, BattleGarrisonKind::STATUS_DAMAGE})
			_MyGarrisonReaders |= !_MyGarrisonGroups[static_cast<unsigned>(kind)].empty();
		RefreshGarrisons();
	}

	void Battle::GrantGarrison(std::size_t _index)
	{
		const auto& state = _MyGarrisons[_index]; const auto& unit = Unit(state.MyUnit);
		const auto rules = _MyInput.MyGarrisonRules.MyEffects; const auto& rule = rules[state.MyRule];
		const auto found = std::ranges::lower_bound(rules, rule.MyGrantedGarrison, {}, &BattleGarrisonRule::MyId);
		if (found == rules.end() || found->MyId != rule.MyGrantedGarrison) return;
		const auto grant = static_cast<std::size_t>(found - rules.begin());
		const auto eligible = [&](const CombatUnit& u) { return u.MyKind == UnitKind::OPERATOR && !u.MyRemoved && u.MyOwner == unit.MyOwner; };
		const auto give = [&](const CombatUnit& u)
		{
			bool member = rule.MyRequiredBond.empty() || std::ranges::contains(u.MyDefinition.MyIdentity.MyBonds, rule.MyRequiredBond);
			if (!member && std::ranges::contains(CoreBondIds, rule.MyRequiredBond) && std::ranges::contains(u.MyDefinition.MyIdentity.MyBonds, std::string_view("maniShip")))
			{
				const auto& bonds = _MyPlayers[u.MyOwner].MyBonds; const auto mani = std::ranges::find(bonds, std::string_view("maniShip"), &BondLayer::MyId);
				member = mani != bonds.end() && mani->MyActive;
			}
			if (member) AddGarrison(u.MyId, grant, true);
		};
		if (rule.MyGrantTarget == GarrisonGrantTarget::ALL)
		{
			for (const auto id : _MyAllyIds) if (eligible(Unit(id))) give(Unit(id));
			return;
		}
		if (rule.MyGrantTarget == GarrisonGrantTarget::ROW_RIGHT)
		{
			UnitId best = 0; const auto mirror = _MyInput.MyPlayers[unit.MyOwner].MyMirrorDeployment;
			for (const auto id : _MyAllyIds)
			{
				const auto& target = Unit(id);
				if (!eligible(target) || target.MyHome.MyY != unit.MyHome.MyY) continue;
				if (!best || (mirror ? target.MyHome.MyX < Unit(best).MyHome.MyX : target.MyHome.MyX > Unit(best).MyHome.MyX)) best = id;
			}
			if (best) give(Unit(best));
			return;
		}
		if (rule.MyGrantTarget == GarrisonGrantTarget::SELF_FRONT) give(unit);
		const auto offset = RotateOffset(RangeOffset{0, 1}, unit.MyFacing);
		for (const auto id : _MyAllyIds)
		{
			const auto& target = Unit(id);
			if (eligible(target) && target.MyHome.MyX == unit.MyHome.MyX + offset.MyColumn && target.MyHome.MyY == unit.MyHome.MyY + offset.MyRow)
			{ give(target); break; }
		}
	}

	bool Battle::RevokeGrantedGarrisons(UnitId _unit)
	{
		if (_MyInput.MyRetainGrantedGarrisonsAfterExit || Unit(_unit).MyKind != UnitKind::OPERATOR) return false;
		bool revoked = false;
		// 先停用全部获授记录，再移除属性；回调不能再次激活已撤销的读层效果。
		for (auto& state : _MyGarrisons)
			if (state.MyUnit == _unit && state.MyGranted && !state.MyRetired)
			{
				state.MyRetired = true;
				revoked = true;
			}
		if (revoked)
			for (const auto& state : _MyGarrisons)
				if (state.MyUnit == _unit && state.MyGranted && state.MyRetired) (void)RemoveBuff(_unit, state.MyKey);
		return revoked;
	}

	double Battle::GarrisonSteps(const BattleGarrisonRuntime& _state) const
	{
		const auto& rule = _MyInput.MyGarrisonRules.MyEffects[_state.MyRule]; const auto& player = _MyPlayers[Unit(_state.MyUnit).MyOwner];
		double layers = 0;
		for (const auto id : rule.MyBonds)
		{
			const auto found = std::ranges::find(player.MyBonds, id, &BondLayer::MyId);
			if (found != player.MyBonds.end() && found->MyActive) layers += found->MyLayers;
		}
		return std::floor(layers / rule.MyDivisor);
	}

	void Battle::GainGarrisonLayers(std::size_t _index)
	{
		auto& state = _MyGarrisons[_index]; const auto& unit = Unit(state.MyUnit);
		if (state.MyRetired) return;
		const auto& rule = _MyInput.MyGarrisonRules.MyEffects[state.MyRule]; const auto& player = _MyPlayers[unit.MyOwner];
		if (!_MyInput.MyLayerGainsEnabled.value_or(!_MyInput.MyBossBattle)) return;
		double row = 0, col = 0;
		if (rule.MyCondition != GarrisonCondition::NONE || rule.MyGainAmount == GarrisonGainAmount::ROW_COUNT)
			for (const auto id : _MyAllyIds)
			{
				const auto& target = Unit(id);
				if (target.MyKind != UnitKind::OPERATOR || target.MyOwner != unit.MyOwner || !GarrisonOnField(id)) continue;
				const auto point = RulePosition(target), origin = RulePosition(unit);
				row += point.MyY == origin.MyY; col += point.MyX == origin.MyX;
			}
		if ((rule.MyCondition == GarrisonCondition::ROW && row < rule.MyCheckCount) || (rule.MyCondition == GarrisonCondition::COLUMN && col < rule.MyCheckCount)) return;
		double amount = std::floor(rule.MyAmount * (rule.MyGainAmount == GarrisonGainAmount::ROW_COUNT ? row : rule.MyGainAmount == GarrisonGainAmount::TIER ? unit.MyDefinition.MyIdentity.MyTier : 1));
		const bool whole = rule.MyBondTarget == GarrisonBondTarget::HIGHEST && rule.MyMaximum > 0;
		if (whole) amount = std::min(amount, std::max(0.0, std::floor(rule.MyMaximum) - state.MyWholeUsed));
		if (!(amount > 0)) return;
		double total = 0;
		const auto gain = [&](std::string_view id)
		{
			const auto active = std::ranges::find(player.MyBonds, id, &BondLayer::MyId);
			if (active == player.MyBonds.end() || !active->MyActive) return;
			double add = amount;
			auto used = std::ranges::find(state.MyUsed, id, &BondLayer::MyId);
			if (!whole && rule.MyMaximum > 0)
			{
				if (used == state.MyUsed.end()) { state.MyUsed.push_back({.MyId = std::string(id)}); used = std::prev(state.MyUsed.end()); }
				add = std::min(add, std::max(0.0, std::floor(rule.MyMaximum) - used->MyLayers));
			}
			if (!(add > 0)) return;
			const auto got = AddBondLayers(player.MyPlayerId, id, add, {.MySource = unit.MyId, .MyReason = "garrison"});
			if (got > 0) { total += got; if (!whole && rule.MyMaximum > 0) used->MyLayers += add; }
		};
		if (rule.MyBondTarget == GarrisonBondTarget::SELF) for (const auto& id : unit.MyDefinition.MyIdentity.MyBonds) gain(id);
		else if (rule.MyBondTarget == GarrisonBondTarget::HIGHEST)
		{
			std::string_view best; double highest = -1;
			for (const auto id : _MyInput.MyGarrisonRules.MyBondOrder)
			{
				const auto b = std::ranges::find(player.MyBonds, id, &BondLayer::MyId);
				if (b != player.MyBonds.end() && b->MyActive && b->MyLayers > highest) { highest = b->MyLayers; best = id; }
			}
			if (!best.empty()) gain(best);
		}
		else for (const auto id : rule.MyBonds) gain(id);
		if (whole && total > 0) state.MyWholeUsed += amount;
	}
}
