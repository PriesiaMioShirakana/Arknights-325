#include "battle_core.hpp"
#include <stronghold/simulation/component_registry.hpp>
#include <tuple>

namespace Stronghold
{
	void EffectExecutor::Select(const Battle& _battle, const EffectContext& _context, const SelectorDefinition& _selector, std::vector<EffectTarget>& _output)
	{
		auto& core = *_battle._MyView;
		_output.clear();
		const auto owner = _context.MyOwner != NoPlayer ? _context.MyOwner : _context.MySource ? core.Unit(_context.MySource).MyOwner : NoPlayer;
		if (owner != NoPlayer) (void)core._MyPlayers.at(owner);
		if (static_cast<unsigned>(_selector.MyKind) > static_cast<unsigned>(SelectorKind::CUSTOM) ||
			static_cast<unsigned>(_selector.MyRange) > static_cast<unsigned>(SelectorRange::OFFSETS) ||
			static_cast<unsigned>(_selector.MyOrder) > static_cast<unsigned>(SelectorOrder::RANDOM) ||
			!std::isfinite(_selector.MyRadius) || std::isless(_selector.MyRadius, 0) || (_selector.MyMaximumHealthRatio && !std::isfinite(*_selector.MyMaximumHealthRatio)))
			throw std::invalid_argument("invalid selector bounds");
		if (_selector.MyKind == SelectorKind::CUSTOM)
		{
			core.CustomSelector(_selector.MyCustom).Select(_battle, _context, _output);
			if (_selector.MyLimit && _output.size() > _selector.MyLimit) _output.resize(_selector.MyLimit);
			return;
		}
		const auto* source = _context.MySource ? &core.Unit(_context.MySource) : nullptr;
		const auto side = source ? source->MySide : UnitSide::ALLY;
		const auto center = _selector.MyCenter.value_or(source ? core.RulePosition(*source) : WorldPoint{});
		if (!std::isfinite(center.MyX) || !std::isfinite(center.MyY)) throw std::invalid_argument("invalid selector center");
		std::bitset<FieldTiles> relativeMask;
		if (_selector.MyRange == SelectorRange::OFFSETS && source)
		{
			const auto position = core.RulePosition(*source);
			for (const auto offset : _selector.MyOffsets)
			{
				// 过大的相对格不可能落到战场，先跳过也避免旋转 INT_MIN 的取负溢出。
				if (offset.MyRow < -FieldTiles || offset.MyRow > FieldTiles || offset.MyColumn < -FieldTiles || offset.MyColumn > FieldTiles) continue;
				const auto rotated = RotateOffset(offset, source->MyFacing);
				const auto row = static_cast<int>(std::floor(position.MyY + 0.5)) + rotated.MyRow;
				const auto column = static_cast<int>(std::floor(position.MyX + 0.5)) + rotated.MyColumn;
				if (FieldGrid::InBounds(row, column)) relativeMask.set(static_cast<std::size_t>(FieldGrid::Key(row, column)));
			}
		}
		const auto& mask = _selector.MyRange == SelectorRange::OFFSETS ? relativeMask :
			_selector.MyRange == SelectorRange::SOURCE && source ? core.RuleRange(*source) : _selector.MyMask;
		const auto inRange = [&](WorldPoint _position)
		{
			if (_selector.MyRange == SelectorRange::GLOBAL) return true;
			if (_selector.MyRange == SelectorRange::RADIUS) return std::islessequal(Distance(center, _position), _selector.MyRadius);
			const auto row = static_cast<int>(std::floor(_position.MyY + 0.5));
			const auto column = static_cast<int>(std::floor(_position.MyX + 0.5));
			if (!FieldGrid::InBounds(row, column)) return false;
			return mask[FieldGrid::Key(row, column)];
		};
		if (_selector.MyKind == SelectorKind::TERRAIN)
		{
			_output.reserve(FieldTiles);
			for (int row = 0; row < FieldRows; ++row)
				for (int column = 0; column < FieldColumns; ++column)
				{
					if (core._MyGrid && !core._MyGrid->InRect(row, column)) continue;
					const auto tile = core._MyGrid ? core._MyGrid->Tile(row, column) : FieldTile{};
					if ((_selector.MyTerrain && tile.MyTerrain != *_selector.MyTerrain) || (_selector.MyBuild && tile.MyBuild != *_selector.MyBuild) ||
						!inRange({.MyX = static_cast<double>(column), .MyY = static_cast<double>(row)})) continue;
					_output.emplace_back(EffectTarget{.MyKind = EffectTargetKind::TERRAIN, .MyTile = {.MyRow = row, .MyColumn = column}});
				}
		}
		else if (_selector.MyKind == SelectorKind::PLAYERS || _selector.MyKind == SelectorKind::OWNER || _selector.MyKind == SelectorKind::TEAMMATES)
		{
			_output.reserve(core._MyPlayers.size());
			for (std::size_t i = 0; i < core._MyPlayers.size(); ++i)
			{
				if (_selector.MyKind == SelectorKind::OWNER && i != owner) continue;
				if (_selector.MyKind != SelectorKind::OWNER && !_selector.MyIncludeVirtualPlayers && core._MyPlayers[i].MyKind == BattlePlayerKind::VIRTUAL) continue;
				if (_selector.MyKind == SelectorKind::TEAMMATES && (owner == NoPlayer || i == owner || !core.Teammates(owner, i))) continue;
				_output.emplace_back(EffectTarget{.MyKind = EffectTargetKind::PLAYER, .MyPlayer = i});
			}
		}
		else
		{
			_output.reserve(core._MyUnits.size());
			const AttackProfile profile{.MyCanHitFlying = _selector.MyCanHitFlying, .MyHitSleep = _selector.MyHitSleep, .MyGroundOnly = _selector.MyGroundOnly};
			const bool mayRepeat = _selector.MyKind == SelectorKind::PROVIDED ||
				(_selector.MyOrder == SelectorOrder::RANGE_KEYS && source && _selector.MyRange == SelectorRange::SOURCE);
			const auto append = [&](UnitId _id, std::optional<int> _key = std::nullopt)
			{
				if (!_id) return;
				const auto& unit = core.Unit(_id);
				if ((_selector.MyAliveOnly && !unit.MyAlive) || (!_selector.MyIncludeHidden && unit.MyHidden) ||
					(_selector.MyExcludeSource && _id == _context.MySource) || (_selector.MyUnitKind && unit.MyKind != *_selector.MyUnitKind) ||
					(_selector.MyProfession && unit.MyDefinition.MyOperatorProfession != *_selector.MyProfession) ||
					(!_selector.MyNation.empty() && unit.MyDefinition.MyIdentity.MyNationId != _selector.MyNation)) return;
				if (_selector.MyWoundedOnly && !std::isless(unit.MyHealth, unit.MyStats.MyMaxHealth - 1e-6)) return;
				if (_selector.MyMaximumHealthRatio && std::isgreater(unit.MyHealth / unit.MyStats.MyMaxHealth, *_selector.MyMaximumHealthRatio)) return;
				if ((_selector.MySpType && unit.MyDefinition.MySkill.MySpType != *_selector.MySpType) ||
					(_selector.MyAttackHurtSpOnly && unit.MyDefinition.MySkill.MySpType != SpType::ATTACK && unit.MyDefinition.MySkill.MySpType != SpType::HURT) ||
					(_selector.MyEliteOnly && unit.MySpawnTag != EnemySpawnTag::BOSS && !unit.MyDefinition.MyElite && !unit.MyDefinition.MyLeader) ||
					(_selector.MyRespectIsolation && unit.MyId != _context.MySource && unit.MyStatuses.Has(CombatStatus::ISOLATED)) ||
					(unit.MySide == UnitSide::ENEMY && unit.Flying() && (!_selector.MyCanHitFlying || _selector.MyGroundOnly))) return;
				switch (_selector.MyKind)
				{
				case SelectorKind::ENEMIES: if (unit.MySide == side) return; break;
				case SelectorKind::ALLIES: if (unit.MySide != side) return; break;
				case SelectorKind::OWN_UNITS: if (unit.MySide != side || unit.MyOwner != owner) return; break;
				case SelectorKind::TEAMMATE_UNITS:
					if (unit.MySide != side || owner == NoPlayer || unit.MyOwner == owner || !core.Teammates(owner, unit.MyOwner)) return;
					break;
				default: break;
				}
				if (_selector.MyTargetable && unit.MySide == UnitSide::ENEMY && !core.TargetableEnemy(unit, profile)) return;
				if (_key)
				{
					if (unit.MySide == UnitSide::ALLY)
					{
						const auto point = core.RulePosition(unit);
						if (*_key / FieldColumns != static_cast<int>(std::floor(point.MyY + 0.5)) || *_key % FieldColumns != static_cast<int>(std::floor(point.MyX + 0.5))) return;
					}
					else
					{
						const auto tiles = BodyTiles(unit);
						if (*_key / FieldColumns < tiles.MyFirstRow || *_key / FieldColumns > tiles.MyLastRow ||
							*_key % FieldColumns < tiles.MyFirstColumn || *_key % FieldColumns > tiles.MyLastColumn) return;
					}
				}
				else if (unit.MySide == UnitSide::ENEMY && (_selector.MyRange == SelectorRange::SOURCE || _selector.MyRange == SelectorRange::MASK || _selector.MyRange == SelectorRange::OFFSETS))
				{
					if (!BodyInRange(unit, mask)) return;
				}
				else if (!inRange(core.RulePosition(unit))) return;
				if (mayRepeat && std::ranges::any_of(_output, [&](const EffectTarget& _target) { return _target.MyUnit == _id; })) return;
				_output.emplace_back(EffectTarget{.MyUnit = _id});
			};
			switch (_selector.MyKind)
			{
			case SelectorKind::SELF: append(_context.MySource); break;
			case SelectorKind::EVENT_UNIT: append(_context.MyEventUnit); break;
			case SelectorKind::EVENT_SOURCE: append(_context.MyEventSource); break;
			case SelectorKind::EVENT_TARGET: append(_context.MyEventTarget); break;
			case SelectorKind::PROVIDED: for (const auto id : _context.MyProvided) append(id); break;
			default:
				if (_selector.MyOrder == SelectorOrder::RANGE_KEYS && source && _selector.MyRange == SelectorRange::SOURCE)
					for (const auto key : core.RuleRangeKeys(*source)) for (const auto& unit : core._MyUnits) append(unit.MyId, key);
				else for (const auto& unit : core._MyUnits) append(unit.MyId);
				break;
			}
			if (_selector.MyOrder == SelectorOrder::ATTACK_PRIORITY && source && std::ranges::all_of(_output, [&](const EffectTarget& _target) { return core.Unit(_target.MyUnit).MySide == UnitSide::ENEMY; }))
			{
				auto& ids = core._MyEffectSortIds;
				ids.clear(); ids.reserve(_output.size());
				for (const auto& target : _output) ids.emplace_back(target.MyUnit);
				core.SortOperatorTargets(source->MyId, ids, _selector.MyLimit);
				_output.clear();
				for (const auto id : ids) _output.emplace_back(EffectTarget{.MyUnit = id});
			}
			else if (_selector.MyOrder == SelectorOrder::LOWEST_HEALTH_RATIO || _selector.MyOrder == SelectorOrder::NEAREST || _selector.MyOrder == SelectorOrder::HEAVIEST)
			{
				const auto score = [&](const CombatUnit& _unit)
				{
					if (_selector.MyOrder == SelectorOrder::LOWEST_HEALTH_RATIO) return _unit.MyHealth / _unit.MyStats.MyMaxHealth;
					if (_selector.MyOrder == SelectorOrder::HEAVIEST) return -_unit.MyStats.MyMass;
					return _unit.MySide == UnitSide::ENEMY ? BodyDistance(_unit, center) : Distance(center, core.RulePosition(_unit));
				};
				std::ranges::sort(_output, [&](const EffectTarget& _left, const EffectTarget& _right)
				{
					const auto& left = core.Unit(_left.MyUnit); const auto& right = core.Unit(_right.MyUnit);
					return std::tuple{score(left), left.MyDeploySequence, left.MyId} < std::tuple{score(right), right.MyDeploySequence, right.MyId};
				});
			}
		}
		// 原版随机多目标只在候选超过上限时洗牌；候选较少时不能额外消耗随机流。
		if (_selector.MyOrder == SelectorOrder::RANDOM && (!_selector.MyLimit || _output.size() > _selector.MyLimit))
			core._MyRandom.Shuffle(_output.begin(), _output.end());
		if (_selector.MyLimit && _output.size() > _selector.MyLimit) _output.resize(_selector.MyLimit);
	}

}
