#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::NotifyEgirDeath(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::DEATH || _event.MyRemovalReason != RemovalReason::KILLED || _event.MyRevived || !_event.MyUnit) return;
		for (auto& state : _MyCoreBonds)
		{
			const auto& effect = _MyInput.MyPlayers[state.MyPlayer].MyCoreBonds[state.MyEffect];
			if (effect.MyKind != CoreBondKind::EGIR || !state.MyPower || !effect.MyParameters.MyReviveMaximum ||
				!std::ranges::contains(state.MyMembers, _event.MyUnit) || std::ranges::contains(state.MyKnocked, _event.MyUnit)) continue;
			state.MyKnocked.push_back(_event.MyUnit);
			if (_MyEgirDevouring) { state.MyTargets.push_back(_event.MyUnit); continue; }
			if (state.MyCount >= effect.MyParameters.MyReviveMaximum || Unit(_event.MyUnit).MyAlive || Unit(_event.MyUnit).MyRemoved) continue;
			if (Redeploy(_event.MyUnit)) ++state.MyCount;
		}
	}

	void BattleCore::FlushEgirRevives()
	{
		for (auto& state : _MyCoreBonds)
		{
			const auto& player = _MyInput.MyPlayers[state.MyPlayer];
			const auto& effect = player.MyCoreBonds[state.MyEffect];
			if (effect.MyKind != CoreBondKind::EGIR || !state.MyPower) continue;
			auto& batch = state.MyTargets;
			std::erase_if(batch, [&](UnitId _id) { return Unit(_id).MyAlive || Unit(_id).MyRemoved; });
			std::ranges::sort(batch, [&](UnitId _left, UnitId _right)
			{
				const auto a = RulePosition(_left), b = RulePosition(_right);
				if (a.MyX != b.MyX) return player.MyMirrorDeployment ? a.MyX > b.MyX : a.MyX < b.MyX;
				return a.MyY < b.MyY || (a.MyY == b.MyY && _left < _right);
			});
			for (const auto id : batch)
			{
				if (state.MyCount >= effect.MyParameters.MyReviveMaximum) break;
				if (Redeploy(id)) ++state.MyCount;
			}
			batch.clear();
		}
	}

	void BattleCore::DevourEgir(std::size_t _index)
	{
		auto& state = _MyCoreBonds[_index];
		const auto& player = _MyInput.MyPlayers[state.MyPlayer];
		const auto& p = player.MyCoreBonds[state.MyEffect].MyParameters;
		const auto downAtStart = [&](const CombatUnit& _unit)
		{
			return _unit.MyKind == UnitKind::OPERATOR && !_unit.MyAlive && _unit.MyRemovalReason == RemovalReason::FORCED_EXIT &&
				_unit.MyCarry && _unit.MyCarry->MyDown && IsDown(_unit.MyId);
		};
		const auto member = [&](const CombatUnit& _unit)
		{
			if (_unit.MyKind != UnitKind::OPERATOR) return false;
			const auto& bonds = _unit.MyDefinition.MyIdentity.MyBonds;
			if (std::ranges::contains(bonds, std::string_view("egirShip"))) return true;
			if (!std::ranges::contains(bonds, std::string_view("maniShip")) || _unit.MyOwner >= _MyPlayers.size()) return false;
			return std::ranges::any_of(_MyPlayers[_unit.MyOwner].MyBonds, [](const BondLayer& _bond) { return _bond.MyId == "maniShip" && _bond.MyActive; });
		};
		const auto front = [&](UnitId _id) -> UnitId
		{
			const auto& unit = Unit(_id); const auto delta = RotateOffset({0, 1}, unit.MyFacing);
			const auto origin = RulePosition(unit);
			const WorldPoint point{origin.MyX + delta.MyColumn, origin.MyY + delta.MyRow};
			for (const auto id : _MyAllyIds)
			{
				const auto& target = Unit(id);
				const auto position = RulePosition(target);
				if (target.MyAlive && target.MyKind != UnitKind::DEVICE && position.MyX == point.MyX && position.MyY == point.MyY)
					return target.MyKind == UnitKind::OPERATOR ? id : 0;
			}
			for (const auto id : _MyAllyIds) if (IsDown(id))
			{
				const auto rest = UsesInitialPosition(Unit(id)) ? RulePosition(id) : RestPosition(id);
				if (rest.MyX == point.MyX && rest.MyY == point.MyY) return downAtStart(Unit(id)) ? id : 0;
			}
			return 0;
		};
		std::vector<UnitId> order; order.reserve(state.MyMembers.size());
		for (const auto id : state.MyMembers) if ((Unit(id).MyAlive && !Unit(id).MyRemoved) || downAtStart(Unit(id))) order.push_back(id);
		std::ranges::sort(order, [&](UnitId _left, UnitId _right)
		{
			const auto a = RulePosition(_left), b = RulePosition(_right);
			if (a.MyX != b.MyX) return player.MyMirrorDeployment ? a.MyX > b.MyX : a.MyX < b.MyX;
			return a.MyY > b.MyY || (a.MyY == b.MyY && _left < _right);
		});
		struct Mark
		{
			UnitId MySource{};
			UnitId MyTarget{};
			std::uint64_t MyDeployment{};
		};

		std::vector<std::pair<UnitId, std::vector<UnitId>>> marked; marked.reserve(order.size());
		std::vector<Mark> marks; marks.reserve(order.size() * std::min<std::size_t>(64, _MyAllyIds.size()));
		for (const auto id : order)
		{
			std::vector<UnitId> mine; mine.reserve(std::min<std::size_t>(64, _MyAllyIds.size()));
			auto current = id;
			for (unsigned guard = 0; guard < 64; ++guard)
			{
				const auto target = front(current);
				if (!target || target == id || std::ranges::contains(mine, target)) break;
				const auto prior = std::ranges::find(marked, target, [](const auto& _entry) { return _entry.first; });
				if (prior != marked.end() && std::ranges::contains(prior->second, id)) break;
				mine.push_back(target);
				if (!member(Unit(target))) break;
				current = target;
			}
			double attack = 0, block = 0;
			for (const auto target : mine)
			{
				attack += Unit(target).MyDefinition.MyStats.MyAttack; block += Unit(target).MyDefinition.MyStats.MyBlockCount;
				marks.push_back({id, target, 0});
			}
			marked.emplace_back(id, std::move(mine));
			std::vector<AttributeChange> modifiers; modifiers.reserve(2);
			if (attack > 0) modifiers.push_back({Attribute::ATTACK_FINAL, attack});
			if (block > 0) modifiers.push_back({Attribute::BLOCK_COUNT, block});
			if (!modifiers.empty()) (void)AddBuff(id, BuffDefinition{.MyKey = "bond:egir:devour", .MyModifiers = std::move(modifiers), .MyPersistent = true, .MyAllowDead = true});
		}
		for (auto& mark : marks) mark.MyDeployment = Unit(mark.MyTarget).MyDeploySequence;
		std::vector<UnitId> layered; layered.reserve(_MyAllyIds.size());
		for (const auto& mark : marks)
		{
			const auto& target = Unit(mark.MyTarget);
			if (!target.MyAlive || target.MyDeploySequence != mark.MyDeployment) continue;
			if (p.MyDevourDamage > 0)
			{
				const auto amount = Mitigate(p.MyDevourDamage, DamageType::PHYSICAL, Mitigation{.MyDefense = target.MyStats.MyDefense});
				(void)ApplyHealthLoss(mark.MySource, target.MyId, amount, false, DamageInfo{.MyAmount = amount, .MyType = DamageType::TRUE_DAMAGE,
					.MyNoSp = true, .MyTags = DamageTag::HP_LOSS | DamageTag::BOND});
			}
			if (!std::ranges::contains(layered, target.MyId))
			{
				layered.push_back(target.MyId);
				(void)AddBondLayers(player.MyPlayerId, "egirShip", target.MyDefinition.MyIdentity.MyTier, {.MySource = mark.MySource, .MyReason = "bond"});
			}
		}
	}
}
