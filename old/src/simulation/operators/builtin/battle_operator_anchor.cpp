#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct AnchorScratchGuard
		{
			std::size_t& MyDepth;

			~AnchorScratchGuard() { --MyDepth; }
		};
	}

	bool BattleCore::OnOwnBoard(std::size_t _player, int _row, int _column) const
	{
		if (_player >= _MyInput.MyPlayers.size()) return false;
		const auto& input = _MyInput.MyPlayers[_player];
		const int firstRow = _MyInput.MyBossBattle ? 2 : 9;
		const int firstColumn = input.MyRightHalf.value_or(input.MyMirrorDeployment) ? 10 : 2;
		return _row >= firstRow && _row <= firstRow + 3 && _column >= firstColumn && _column <= firstColumn + 8;
	}

	bool BattleCore::IsAbyssal(UnitId _unit) const
	{
		constexpr std::array<std::string_view, 5> Characters{"char_143_ghost", "char_263_skadi", "char_474_glady", "char_4145_ulpia", "char_1023_ghost2"};
		const auto& identity = Unit(_unit).MyDefinition.MyIdentity;
		return identity.MyGroupId == "abyssal" || std::ranges::contains(Characters, identity.MyCharacterId);
	}

	void BattleCore::UlpiaHurt(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::DAMAGED || !_event.MyTarget || _event.MyElement || !std::isgreater(_event.MyAmount, 0) || HasTag(_event.MyDamage.MyTags, DamageTag::HP_LOSS)) return;
		const auto& unit = Unit(_event.MyTarget);
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<UlpiaKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || unit.MyOperatorHooksReleased || !unit.MyAlive || !std::isgreater(unit.MyHealth, 0)) return;
		const auto value = (std::isless(unit.MyHealth / unit.MyStats.MyMaxHealth, kit->MyHealthThreshold) ? kit->MyLowHeal : kit->MyHeal) *
			(unit.MySkill.MyActive && kit->MySkill == UlpiaSkillKind::BOUNDARY ? kit->MyTalentScale : 1);
		if (std::isgreater(value, 0)) (void)Heal(unit.MyId, unit.MyId, value, {.MySelf = true});
	}

	void BattleCore::UlpiaContact(UnitId _unit, const UlpiaKit& _kit)
	{
		const auto& unit = Unit(_unit);
		auto& scratch = AcquireAttackScratch(); const AnchorScratchGuard guard{.MyDepth = _MyAttackDepth};
		OperatorEnemiesInGrid(_unit, _kit.MyRange.empty() ? std::span<const RangeOffset>(unit.MyDefinition.MyRange) : _kit.MyRange, scratch.MyTargets, true);
		UnitId main = 0; int best = 3;
		for (const auto id : scratch.MyTargets)
		{
			const auto& enemy = Unit(id); if (enemy.Flying()) continue;
			const int rank = !InRuleRange(_unit, id) && !enemy.MyBlockedBy ? 0 : !enemy.MyBlockedBy ? 1 : 2;
			if (rank < best) { best = rank; main = id; }
		}
		if (!main) return;
		const auto origin = Unit(main).MyPosition;
		FoesInRadius(origin, 1.5, scratch.MyTargets);
		std::erase_if(scratch.MyTargets, [&](UnitId _id) { return Unit(_id).Flying() || Unit(_id).MyStatuses.Has(CombatStatus::SLEEP); });
		std::ranges::sort(scratch.MyTargets, [&](UnitId _a, UnitId _b)
		{
			if (_a == main || _b == main) return _a == main && _b != main;
			const auto da = Distance(Unit(_a).MyPosition, origin), db = Distance(Unit(_b).MyPosition, origin);
			return std::islessgreater(da, db) ? std::isless(da, db) : Unit(_a).MySpawnSequence < Unit(_b).MySpawnSequence;
		});
		if (scratch.MyTargets.size() > _kit.MyTargets) scratch.MyTargets.resize(_kit.MyTargets);
		for (const auto id : scratch.MyTargets)
		{
			(void)PullToFront(id, _unit, _kit.MyForce);
			if (Unit(id).MyAlive) (void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyScale,
				.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
		}
	}

	void BattleCore::UlpiaAnchor(UnitId _unit, const UlpiaKit& _kit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		const auto origin = RulePosition(unit); const auto forward = RotateOffset({.MyColumn = 1}, unit.MyFacing);
		const auto row = static_cast<int>(std::floor(origin.MyY + 0.5)), column = static_cast<int>(std::floor(origin.MyX + 0.5));
		const auto pointAt = [&](unsigned _distance) { return WorldPoint{.MyX = static_cast<double>(column + forward.MyColumn * static_cast<int>(_distance)),
			.MyY = static_cast<double>(row + forward.MyRow * static_cast<int>(_distance))}; };
		unsigned stop = 0;
		if (!std::ranges::any_of(unit.MyBlocking, [&](UnitId _id) { return Unit(_id).MyAlive && Unit(_id).MyBlockedBy == _unit; }))
		{
			const AttackProfile filter{.MyCanHitFlying = true};
			for (unsigned d = 1; d <= _kit.MyReach; ++d)
			{
				const auto point = pointAt(d); const auto r = static_cast<int>(point.MyY), c = static_cast<int>(point.MyX);
				if (!FieldGrid::InBounds(r, c) || (_MyGrid && (!_MyGrid->InRect(r, c) || _MyGrid->Obstacle(r, c, ObstacleKind::BLOCK) || _MyGrid->Obstacle(r, c, ObstacleKind::CRATE)))) break;
				stop = d;
				if (std::ranges::any_of(_MyEnemyIds, [&](UnitId _id) { return TargetableEnemy(Unit(_id), filter) && BodyOnTile(Unit(_id), r, c); })) break;
			}
		}
		const auto point = pointAt(stop);
		auto& scratch = AcquireAttackScratch(); const AnchorScratchGuard guard{.MyDepth = _MyAttackDepth};
		FoesInRadius(point, _kit.MyRadius, scratch.MyTargets);
		for (const auto id : scratch.MyTargets)
		{
			(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MyScale, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
			if (Unit(id).MyAlive) (void)ApplyStatus(id, CombatStatus::STUN, _kit.MyStun, _unit);
		}
		if (!stop || !unit.MyAlive) return;
		std::optional<WorldPoint> destination;
		for (const auto candidate : {point, pointAt(stop + 1)})
		{
			const auto r = static_cast<int>(candidate.MyY), c = static_cast<int>(candidate.MyX);
			if (!OnOwnBoard(unit.MyOwner, r, c) || ReservedTile(candidate) || (_MyGrid && (!_MyGrid->InRect(r, c) || !_MyGrid->CanStand(r, c) ||
				_MyGrid->Obstacle(r, c, ObstacleKind::BLOCK) || _MyGrid->Obstacle(r, c, ObstacleKind::CRATE)))) continue;
			destination = candidate; break;
		}
		if (!destination) return;
		const auto home = unit.MyPosition;
		if (!MoveRedeploy(_unit, *destination) || !unit.MyAlive || !unit.MySkill.MyActive) return;
		unit.MyDownAtHome = true; unit.MyAnchorHome = home; unit.MyAnchorMarker = 0;
		if (const auto* token = FindTokenTemplate(_unit, _kit.MyToken))
		{
			auto body = *token; body.MySkill = {}; body.MyGenericSkill = nullptr; body.MyAttack.MyDisabled = true;
			unit.MyAnchorMarker = SpawnToken({.MyDefinition = std::move(body), .MyPosition = home, .MyOwnerUnit = _unit, .MyUntargetable = true});
		}
	}

	void BattleCore::UlpiaSkill(UnitId _unit, const UlpiaKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::BEFORE_KILL && _event.MyUnit && Unit(_event.MyUnit).MySide == UnitSide::ENEMY && unit.MyAlive)
		{
			(void)AddBuff(_unit, {.MyKey = "ulpia:blood", .MyMaxStacks = _kit.MyMaxStacks, .MyRefresh = BuffRefresh::STACK,
				.MyModifiers = std::vector<AttributeChange>(_kit.MyGrowth.begin(), _kit.MyGrowth.end())});
			if (!_kit.MySharedGrowth.empty()) for (std::size_t i = 0, count = _MyAllyIds.size(); i < count; ++i)
			{
				const auto& ally = Unit(_MyAllyIds[i]);
				if (ally.MyId != _unit && ally.MyAlive && ally.MyKind == UnitKind::OPERATOR && IsAbyssal(ally.MyId))
					(void)AddBuff(ally.MyId, {.MyKey = "ulpia:bloodShare", .MyMaxStacks = _kit.MySharedMaxStacks, .MyRefresh = BuffRefresh::STACK,
						.MyModifiers = std::vector<AttributeChange>(_kit.MySharedGrowth.begin(), _kit.MySharedGrowth.end())});
			}
		}
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			if (_kit.MySkill == UlpiaSkillKind::CONTACT) UlpiaContact(_unit, _kit);
			else if (_kit.MySkill == UlpiaSkillKind::PATH) UlpiaAnchor(_unit, _kit);
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING && _kit.MySkill == UlpiaSkillKind::PATH)
		{
			const auto home = std::exchange(unit.MyAnchorHome, std::nullopt); const auto marker = std::exchange(unit.MyAnchorMarker, UnitId{});
			if (unit.MyAlive) unit.MyDownAtHome = false;
			if (!home) return;
			if (marker && Unit(marker).MyAlive) Retreat(marker, true, RemovalReason::EXPIRED);
			if (unit.MyAlive) (void)MoveRedeploy(_unit, *home, true);
		}
	}
}
