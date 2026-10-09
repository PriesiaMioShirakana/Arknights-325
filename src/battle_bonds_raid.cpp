#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::PollRaidBond(std::size_t _player)
	{
		auto& state = _MyAddonBonds[_player];
		constexpr auto slot = static_cast<unsigned>(AddonBondKind::RAID);
		const auto p = state.MyParameters[slot]; if (!p) return;
		bool sorted = false;
		auto& targets = state.MyRaidTargets;
		auto& reach = state.MyRaidReach;
		const auto onBoard = [&](int _row, int _column)
		{
			const int firstRow = _MyInput.MyBossBattle ? 2 : 9;
			if (_row < firstRow || _row > firstRow + 3) return false;
			return std::ranges::any_of(_MyInput.MyPlayers, [&](const BattlePlayerInput& _input)
			{
				const int firstColumn = _input.MyRightHalf.value_or(_input.MyMirrorDeployment) ? 10 : 2;
				return _column >= firstColumn && _column <= firstColumn + 8;
			});
		};
		for (const auto id : state.MyMembers[slot])
		{
			const auto& unit = Unit(id);
			if (!unit.MyAlive || unit.MyRemoved || unit.MyHidden || unit.MyStatuses.Has(CombatStatus::STUN)) continue;
			const auto& skill = unit.MySkill;
			const auto& definition = unit.MyDefinition.MySkill;
			const bool deploySkill = skill.MyActive && (definition.MyKind == SkillKind::PASSIVE || definition.MySpType == SpType::NONE);
			const bool ready = definition.MyKind != SkillKind::NONE && (deploySkill || (definition.MyKind != SkillKind::PASSIVE && skill.MyCharges > 0 && !(skill.MyActive && IsTimedSkill(definition.MyKind))));
			if (!ready && Time() - std::max({unit.MyLastAttackAt, unit.MyDeployedAt, unit.MyLastRaidAt}) < p->MyIdleTime - 1e-9) continue;
			const auto& profile = EffectiveAttack(unit);
			bool inRange = false;
			for (const auto key : RuleRangeKeys(unit))
				if (std::ranges::any_of(_MyEnemyBuckets[static_cast<std::size_t>(key)], [&](UnitId _enemy) { return TargetableEnemy(Unit(_enemy), profile); })) { inRange = true; break; }
			if (inRange) continue;
			if (!sorted)
			{
				targets.clear(); targets.reserve(_MyEnemyIds.size());
				bool own = false;
				for (const auto enemyId : _MyEnemyIds)
				{
					const auto& enemy = Unit(enemyId);
					if (!TargetableEnemy(enemy, AttackProfile{.MyCanHitFlying = false})) continue;
					if (enemy.MyOwner == _player)
					{
						if (!own) { own = true; targets.clear(); }
					}
					else if (own) continue;
					targets.emplace_back(enemyId, RemainingDistance(enemyId));
				}
				std::ranges::sort(targets, [](const auto& _a, const auto& _b) { return _a.second < _b.second || (_a.second == _b.second && _a.first < _b.first); });
				sorted = true;
			}
			if (targets.empty()) continue;
			reach.clear(); reach.reserve(unit.MyDefinition.MyRange.size() + 64);
			std::array<int, 201> maxima; maxima.fill(std::numeric_limits<int>::min());
			std::array<int, 201> rows{}; unsigned rowCount = 0;
			for (const auto offset : unit.MyDefinition.MyRange)
			{
				reach.push_back(RotateOffset(offset, unit.MyFacing));
				auto& maximum = maxima[static_cast<unsigned>(offset.MyRow + 100)];
				if (maximum == std::numeric_limits<int>::min()) rows[rowCount++] = offset.MyRow;
				maximum = std::max(maximum, offset.MyColumn);
			}
			for (unsigned i = 0; i < rowCount; ++i)
				for (int extra = 1; extra <= std::min(FieldColumns, unit.MyStats.MyRangeExtend); ++extra)
					reach.push_back(RotateOffset({rows[i], maxima[static_cast<unsigned>(rows[i] + 100)] + extra}, unit.MyFacing));
			if (!_MyGrid) _MyGrid.emplace();
			const bool ranged = !unit.MyDefinition.MyIdentity.MyMeleePosition;
			const auto* paths = !ranged && unit.MyStats.MyBlockCount > 0 ? &GroundPathTiles() : nullptr;
			const auto forward = RotateOffset({0, 1}, unit.MyFacing);
			for (std::size_t candidate = 0; candidate < std::min<std::size_t>(8, targets.size()); ++candidate)
			{
				const auto& enemy = Unit(targets[candidate].first);
				const auto body = BodyTiles(enemy);
				const int er = static_cast<int>(std::floor(enemy.MyPosition.MyY + 0.5)), ec = static_cast<int>(std::floor(enemy.MyPosition.MyX + 0.5));
				std::optional<WorldPoint> best;
				int priority = 2, bestRow = 0, bestColumn = 0;
				double distance = std::numeric_limits<double>::infinity();
				for (int br = body.MyFirstRow; br <= body.MyLastRow; ++br)
					for (int bc = body.MyFirstColumn; bc <= body.MyLastColumn; ++bc)
						for (const auto offset : reach)
						{
							const int row = br - offset.MyRow, column = bc - offset.MyColumn, dr = row - er, dc = column - ec;
							if (std::abs(dr) > 2 || std::abs(dc) > 2 || !_MyGrid->InRect(row, column) || !onBoard(row, column) || !_MyGrid->CanStand(row, column, ranged)) continue;
							const WorldPoint point{static_cast<double>(column), static_cast<double>(row)};
							if (ReservedTile(point)) continue;
							const int currentPriority = paths && _MyGrid->Tile(row, column).MyWalkable &&
								(paths->test(static_cast<std::size_t>(FieldGrid::Key(row, column))) || BodyOnTile(enemy, row, column)) ? 0 : 1;
							const double currentDistance = std::max(std::abs(dr), std::abs(dc)) + 0.01 * (std::abs(dr) + std::abs(dc));
							const int localRow = dr * forward.MyColumn - dc * forward.MyRow, localColumn = dr * forward.MyRow + dc * forward.MyColumn;
							if (currentPriority < priority || (currentPriority == priority && (currentDistance < distance - 1e-9 ||
								(std::abs(currentDistance - distance) <= 1e-9 && (localRow < bestRow || (localRow == bestRow && localColumn < bestColumn))))))
							{
								best = point; priority = currentPriority; distance = currentDistance; bestRow = localRow; bestColumn = localColumn;
							}
						}
				if (!best) continue;
				sorted = false;
				Retreat(id, false, RemovalReason::RAID);
				if (unit.MyAlive) continue;
				const bool landed = Redeploy(id, true, best, true);
				if (!landed && !Redeploy(id, true, std::nullopt, true)) continue;
				_MyUnits[Index(id)].MyLastRaidAt = Time();
				if (landed)
				{
					const auto layers = AddonLayers(state, AddonBondKind::RAID);
					(void)AddBuff(id, BuffDefinition{.MyKey = "bond:raidShip", .MyModifiers = std::vector<AttributeChange>{
						{Attribute::ATTACK_PERCENT, p->MyAttack + p->MyAttackPerLayer * layers}, {Attribute::HEALTH_PERCENT, p->MyHealth + p->MyHealthPerLayer * layers},
						{Attribute::ATTACK_SPEED, p->MyAttackSpeed + p->MyAttackSpeedPerLayer * layers}}});
				}
				break;
			}
		}
	}
}
