#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct MerchantScratchGuard
		{
			std::size_t& MyDepth;

			~MerchantScratchGuard() { --MyDepth; }
		};

		bool HasCoins(const CombatUnit& _unit, const Swire2Kit& _kit, double _time)
		{
			return std::isgreaterequal(_unit.MySwireCoins, _kit.MyCoinCost) && !std::isless(_time, _unit.MySwireBombHold);
		}
	}

	void BattleCore::Swire2Pay(UnitId _unit, const Swire2Kit& _kit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (!unit.MyAlive || unit.MyOperatorHooksReleased || unit.MyOwner == NoPlayer) return;
		const auto cost = unit.MyDefinition.MyProfession.MyMerchantCost;
		auto& player = _MyPlayers[unit.MyOwner];
		if (std::isless(player.MyDp, cost)) { Retreat(_unit, false, RemovalReason::MERCHANT); return; }
		(void)AddDp(player.MyPlayerId, -cost);
		if (std::isgreater(_kit.MyModuleAttack, 0)) (void)AddBuff(_unit, {.MyKey = "trait:swire2_module", .MyMaxStacks = _kit.MyModuleStacks,
			.MyRefresh = BuffRefresh::STACK, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _kit.MyModuleAttack}}});
		if (unit.MySkill.MyActive)
		{
			unit.MySwireCoins = std::min(_kit.MyCoinCap, unit.MySwireCoins + _kit.MyPaymentCoins);
			(void)AddBuff(_unit, {.MyKey = "talent:swire2_buyer", .MyMaxStacks = _kit.MyPaymentStacks, .MyRefresh = BuffRefresh::STACK,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _kit.MyPaymentAttack}}});
		}
		ContentEvent payment{.MyKind = ContentEventKind::MERCHANT_PAY, .MyUnit = _unit, .MyAmount = cost};
		NotifyContent(payment);
	}

	void BattleCore::Swire2Fatal(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::FATAL || !_event.MyUnit || _event.MyPrevented) return;
		auto& unit = _MyUnits[Index(_event.MyUnit)];
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<Swire2Kit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || unit.MyOperatorHooksReleased || unit.MyOwner == NoPlayer) return;
		const auto cost = kit->MyReviveCost * std::pow(kit->MyReviveCostScale, unit.MySwireSaves);
		auto& player = _MyPlayers[unit.MyOwner];
		if (!std::isfinite(cost) || std::isless(player.MyDp + 1e-9, cost)) return;
		(void)AddDp(player.MyPlayerId, -cost);
		++unit.MySwireSaves;
		_event.MyPrevented = true;
		unit.MyHealth = std::max(1.0, unit.MyStats.MyMaxHealth * kit->MyReviveHealth);
	}

	void BattleCore::Swire2Skill(UnitId _unit, const Swire2Kit& _kit, const ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY)
		{
			unit.MySwireCoins = unit.MyDefinition.MySkill.MyKind == SkillKind::PASSIVE ? std::min(_kit.MyCoinCap, _kit.MyStartCoins) : 0;
			unit.MySwireSaves = 0;
		}
		if (_event.MyKind == ContentEventKind::SKILL_START && unit.MyDefinition.MySkill.MyKind != SkillKind::PASSIVE)
			unit.MySwireCoins = std::min(_kit.MyCoinCap, unit.MySwireCoins + _kit.MyStartCoins);
		if (_kit.MySkill != Swire2SkillKind::CASH) return;
		if (_event.MyKind == ContentEventKind::BEFORE_KILL && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY && unit.MySkill.MyActive)
			unit.MySwireCoins = std::min(_kit.MyCoinCap, unit.MySwireCoins + 1);
		if (_event.MyKind == ContentEventKind::SKILL_ENDING && _event.MySkillReason == SkillReason::MANUAL && unit.MyAlive) Swire2Cash(_unit, _kit);
		if (_event.MyKind != ContentEventKind::SKILL_TICK || !unit.MySkill.MyActive || std::isless(unit.MySwireCoins + 1e-9, std::max(1.0, std::floor(_kit.MyCoinCap)))) return;
		const AttackProfile filter{.MyCanHitFlying = false};
		if (std::ranges::any_of(_MyEnemyIds, [&](UnitId _id) { return TargetableEnemy(Unit(_id), filter) && InRuleRange(_unit, _id); })) EndSkill(_unit, SkillReason::MANUAL);
	}

	void BattleCore::Swire2Heal(UnitId _unit, const Swire2Kit& _kit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (!unit.MyAlive || std::isless(unit.MySwireCoins, _kit.MyCoinCost) || !std::isgreater(_kit.MyHealScale, 0) || std::isless(Time(), unit.MySwireHealAt - 1e-9)) return;
		UnitId best = 0;
		const auto origin = RulePosition(unit);
		const int row = static_cast<int>(std::floor(origin.MyY + 0.5)), column = static_cast<int>(std::floor(origin.MyX + 0.5));
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (!ally.MyAlive || ally.MyHidden || ally.MyKind == UnitKind::DEVICE || !std::isless(ally.MyHealth / ally.MyStats.MyMaxHealth, _kit.MyHealRatio)) continue;
			if (id != _unit && (ally.MyStatuses.Has(CombatStatus::NO_HEAL) || ally.MyDefinition.MyAttack.MyNoHeal)) continue;
			if (id == _unit && ally.MyStatuses.Has(CombatStatus::HEAL_FREE)) continue;
			const auto point = RulePosition(ally);
			if (std::abs(static_cast<int>(std::floor(point.MyY + 0.5)) - row) > 1 || std::abs(static_cast<int>(std::floor(point.MyX + 0.5)) - column) > 1) continue;
			const auto ratio = ally.MyHealth / ally.MyStats.MyMaxHealth;
			const auto previous = best ? Unit(best).MyHealth / Unit(best).MyStats.MyMaxHealth : 1;
			if (!best || std::isless(ratio, previous) || (!std::islessgreater(ratio, previous) && ally.MyDeploySequence < Unit(best).MyDeploySequence)) best = id;
		}
		if (!best) return;
		unit.MySwireCoins -= _kit.MyCoinCost;
		unit.MySwireHealAt = Time() + unit.MyStats.AttackInterval();
		(void)Heal(_unit, best, unit.MyStats.MyAttack * _kit.MyHealScale);
	}

	std::size_t BattleCore::Swire2BombTiles(UnitId _unit, std::array<WorldPoint, 9>& _tiles) const
	{
		constexpr std::array<RangeOffset, 9> Grid{{{.MyRow = 2}, {.MyRow = 1}, {.MyColumn = -2}, {.MyColumn = -1}, {}, {.MyColumn = 1}, {.MyColumn = 2}, {.MyRow = -1}, {.MyRow = -2}}};
		const auto& source = Unit(_unit);
		const auto origin = RulePosition(source);
		std::size_t count = 0;
		for (const auto offset : Grid)
		{
			const auto local = RotateOffset(offset, source.MyFacing);
			const int row = static_cast<int>(std::floor(origin.MyY + 0.5)) + local.MyRow, column = static_cast<int>(std::floor(origin.MyX + 0.5)) + local.MyColumn;
			const WorldPoint point{.MyX = static_cast<double>(column), .MyY = static_cast<double>(row)};
			if (row < 0 || row >= FieldRows || column < 0 || column >= FieldColumns || ReservedTile(point) ||
				(_MyGrid && (!_MyGrid->InRect(row, column) || !_MyGrid->GroundPassable(row, column) || !_MyGrid->CanStand(row, column)))) continue;
			_tiles[count++] = point;
		}
		return count;
	}

	bool BattleCore::Swire2ThrowDue(UnitId _unit, const Swire2Kit& _kit)
	{
		if (!HasCoins(Unit(_unit), _kit, Time())) return false;
		std::array<WorldPoint, 9> tiles;
		return Swire2BombTiles(_unit, tiles) != 0;
	}

	void BattleCore::Swire2Tick(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (!unit.MyAlive || unit.MyHidden || unit.MyStatuses.Has(CombatStatus::STUN) || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<Swire2Kit>(*unit.MyDefinition.MyOperatorKit);
		if (kit.MySkill == Swire2SkillKind::HEAL)
		{
			if (!unit.MyHadAttackTarget) Swire2Heal(_unit, kit);
			return;
		}
		if (kit.MySkill != Swire2SkillKind::BOMB || std::isgreater(unit.MyAttackCooldown, 1e-9) || unit.MyStatuses.Has(CombatStatus::DISARM) || !HasCoins(unit, kit, Time())) return;
		std::array<WorldPoint, 9> tiles;
		const auto count = Swire2BombTiles(_unit, tiles);
		if (!count) return;
		auto& scratch = AcquireAttackScratch(); const MerchantScratchGuard guard{.MyDepth = _MyAttackDepth};
		auto profile = unit.MyDefinition.MyAttack; profile.MyCanHitFlying = false;
		for (std::size_t i = 0; i < count; ++i)
			for (const auto enemy : _MyEnemyIds)
				if (TargetableEnemy(Unit(enemy), profile) && BodyOnTile(Unit(enemy), static_cast<int>(tiles[i].MyY), static_cast<int>(tiles[i].MyX)) && !std::ranges::contains(scratch.MyTargets, enemy)) scratch.MyTargets.push_back(enemy);
		SortOperatorTargets(_unit, scratch.MyTargets, 0, &profile);
		std::optional<WorldPoint> selected;
		for (const auto enemy : scratch.MyTargets)
		{
			const auto point = Unit(enemy).MyPosition;
			for (std::size_t i = 0; i < count; ++i)
				if (static_cast<int>(std::floor(point.MyY + 0.5)) == static_cast<int>(tiles[i].MyY) && static_cast<int>(std::floor(point.MyX + 0.5)) == static_cast<int>(tiles[i].MyX)) { selected = tiles[i]; break; }
			if (selected) break;
		}
		if (!selected) selected = tiles[_MyRandom.Index(static_cast<std::uint32_t>(count))];
		const auto* definition = FindTokenTemplate(_unit, kit.MyToken);
		UnitId bomb = 0;
		if (definition)
		{
			auto body = *definition;
			const auto mature = body.MyTokenKit && std::isfinite(body.MyTokenKit->MyMatureTime) ? body.MyTokenKit->MyMatureTime : 3;
			body.MySkill = {}; body.MyGenericSkill = nullptr; body.MyAttack.MyDisabled = true;
			body.MyTokenKit = TokenKitDefinition{.MyKind = TokenKitKind::CHAMPAGNE, .MyBurstScale = kit.MyDamageScale, .MySluggish = kit.MySluggish, .MyMatureTime = mature, .MyManagedBomb = true};
			bomb = SpawnToken({.MyDefinition = std::move(body), .MyPosition = *selected, .MyOwnerUnit = _unit, .MyUntargetable = true});
		}
		if (!bomb) { unit.MySwireBombHold = Time() + unit.MyStats.AttackInterval(); return; }
		unit.MySwireCoins -= kit.MyCoinCost;
	}

	void BattleCore::Swire2Cash(UnitId _unit, const Swire2Kit& _kit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		const auto coins = std::exchange(unit.MySwireCoins, 0);
		constexpr std::array<RangeOffset, 5> Grid{{{.MyRow = 1, .MyColumn = 1}, {}, {.MyColumn = 1}, {.MyColumn = 2}, {.MyRow = -1, .MyColumn = 1}}};
		auto& scratch = AcquireAttackScratch(); const MerchantScratchGuard guard{.MyDepth = _MyAttackDepth};
		OperatorEnemiesInGrid(_unit, Grid, scratch.MyTargets);
		std::erase_if(scratch.MyTargets, [&](UnitId _id) { return Unit(_id).Flying(); });
		for (const auto enemy : unit.MyBlocking)
			if (Unit(enemy).MyAlive && !Unit(enemy).Flying() && !std::ranges::contains(scratch.MyTargets, enemy)) scratch.MyTargets.push_back(enemy);
		for (unsigned i = 0; std::isless(static_cast<double>(i), coins); ++i)
		{
			scratch.MySeen.clear();
			for (const auto id : scratch.MyTargets) if (Unit(id).MyAlive && !Unit(id).Flying()) scratch.MySeen.push_back(id);
			if (scratch.MySeen.empty()) break;
			const auto target = scratch.MySeen[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MySeen.size()))];
			(void)DealDamage(_unit, target, {.MyAmount = unit.MyStats.MyAttack * _kit.MyDamageScale, .MyType = DamageType::PHYSICAL,
				.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
			if (Unit(target).MyAlive) (void)Push(target, _kit.MyForce, {.MyFrom = unit.MyPosition});
		}
	}

	void BattleCore::ChampagneTick(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || !unit.MyDefinition.MyTokenKit) return;
		const auto& kit = *unit.MyDefinition.MyTokenKit;
		UnitId target = 0;
		const auto origin = RulePosition(unit);
		const int row = static_cast<int>(std::floor(origin.MyY + 0.5)), column = static_cast<int>(std::floor(origin.MyX + 0.5));
		for (const auto id : _MyEnemyIds)
		{
			const auto& enemy = Unit(id);
			if (!enemy.MyAlive || enemy.MyHidden || enemy.Flying() || (kit.MyManagedBomb && enemy.MyStatuses.Has(CombatStatus::UNTARGETABLE))) continue;
			const bool touching = kit.MyManagedBomb || enemy.MyDefinition.MyHitArea ? BodyOnTile(enemy, row, column) :
				std::islessequal(std::abs(enemy.MyPosition.MyX - column), 0.5) && std::islessequal(std::abs(enemy.MyPosition.MyY - row), 0.5);
			if (touching && (!target || enemy.MySpawnSequence < Unit(target).MySpawnSequence)) target = id;
			if (target && kit.MyManagedBomb) break;
		}
		if (!target) return;
		const auto credit = kit.MyManagedBomb && unit.MyOwnerUnit ? unit.MyOwnerUnit : _unit;
		const auto attacker = unit.MyOwnerUnit ? unit.MyOwnerUnit : _unit;
		const auto amount = Unit(attacker).MyStats.MyAttack * kit.MyBurstScale;
		const unsigned hits = std::isgreaterequal(Time() - unit.MyDeployedAt, kit.MyMatureTime - 1e-9) ? 2 : 1;
		for (unsigned i = 0; i < hits && Unit(target).MyAlive; ++i)
			(void)DealDamage(credit, target, {.MyAmount = kit.MyManagedBomb ? Unit(attacker).MyStats.MyAttack * kit.MyBurstScale : amount,
				.MyType = DamageType::PHYSICAL, .MyCanDodge = !kit.MyManagedBomb,
				.MyTags = (kit.MyManagedBomb ? DamageTag::SKILL : DamageTag::SUMMON) | DamageTag::TRAP, .MyIsSkill = true});
		if (Unit(target).MyAlive && std::isgreater(kit.MySluggish, 0)) (void)ApplyStatus(target, CombatStatus::SLUGGISH, kit.MySluggish, credit);
		Retreat(_unit, true, RemovalReason::EXPIRED);
	}
}
