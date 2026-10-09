#include <cmath>
#include "preparation_hooks.hpp"

namespace Stronghold
{
	const PreparationGarrisonRule* PreparationContent::GarrisonRule(std::string_view _id) const
	{
		const auto found = std::ranges::find(_MyGarrisons.MyEffects, _id, &PreparationGarrisonRule::MyId);
		return found == _MyGarrisons.MyEffects.end() ? nullptr : &*found;
	}

	std::span<const std::string_view> PreparationContent::Garrisons(const Player& _player, std::string_view _definition) const
	{
		const auto found = std::ranges::find(_player.MyRoster, _definition, &ContentPoolRoster::MyId);
		return found == _player.MyRoster.end() ? std::span<const std::string_view>{} : found->MyGarrisons;
	}

	void PreparationContent::RunGarrisons(EconomySession& _economy, std::vector<Player>& _players, std::size_t _index,
		GarrisonEvent _event, Random& _random, std::vector<EconomyEvent>& _events, const Piece* _piece,
		std::span<const PieceUid> _refreshed) const
	{
		if (_MyGarrisons.MyEffects.empty()) return;
		auto& view = _economy._MyPlayers[_index].MyView;
		const auto run = [&](const Piece& _value, bool _hand)
		{
			if (_value.IsToken() || _economy._MyCatalog.At(_value.MyId).MyKind != PieceKind::CHESS) return;
			const auto snapshot = _value; // 同一触发期间赠送/合成可使来源不再持有；来源快照仍须完成本次特质。
			RunPieceGarrisons(_economy, _players, _index, _event, snapshot, snapshot, _hand, false, _random, _events);
		};
		if (_event == GarrisonEvent::GAIN || _event == GarrisonEvent::SOLD)
		{
			if (_piece) run(*_piece, false);
			return;
		}
		if (_event == GarrisonEvent::REFRESH)
		{
			for (const auto uid : _refreshed)
				if (const auto loc = _economy.Locate(view, uid); loc && !loc->MyInTemporary && !loc->MyEquippedIndex)
					run(loc->Get(view), !loc->MyOnBoard);
			return;
		}
		if (_event != GarrisonEvent::ROUND_START && _event != GarrisonEvent::PREP_END) return;
		std::vector<Piece> board; board.reserve(view.MyBoard.size());
		for (const auto& piece : view.MyBoard) if (piece && !piece->IsToken()) board.push_back(*piece);
		for (const auto& piece : board) run(piece, false);
		for (std::size_t i = 0; i < view.MyHand.size(); ++i) if (view.MyHand[i]) run(*view.MyHand[i], true);
	}

	void PreparationContent::RunPieceGarrisons(EconomySession& _economy, std::vector<Player>& _players, std::size_t _index,
		GarrisonEvent _event, const Piece& _source, const Piece& _self, bool _hand, bool _trigger,
		Random& _random, std::vector<EconomyEvent>& _events) const
	{
		std::optional<PreparationHooks::Scope> scope;
		if (_trigger)
		{
			scope.emplace(*_economy._MyHooks);
			if (!scope->MyEntered) return;
		}
		for (const auto id : Garrisons(_players[_index], _source.MyId))
		{
			const auto* rule = GarrisonRule(id);
			if (!rule || rule->MyEvent != _event || (!_trigger && _hand && rule->MyBoardOnly)) continue;
			unsigned repeat = 1;
			if (_event == GarrisonEvent::GAIN)
			{
				const auto states = ItemBondStates(_economy, _players[_index], _index);
				const auto invest = std::ranges::find(states, std::string_view("investShip"), &BondState::MyId);
				if (invest != states.end() && invest->MyActive)
					repeat = _MyGarrisons.MyInvestRepeat + (invest->MyLayers >= _MyGarrisons.MyInvestLayer ? 1U : 0U);
			}
			for (unsigned i = 0; i < repeat; ++i) ApplyGarrison(_economy, _players, _index, *rule, _self, _hand, _trigger, _random, _events);
		}
	}

	void PreparationContent::ApplyGarrison(EconomySession& _economy, std::vector<Player>& _players, std::size_t _index,
		const PreparationGarrisonRule& _rule, const Piece& _self, bool _hand, bool _trigger,
		Random& _random, std::vector<EconomyEvent>& _events) const
	{
		auto& player = _players[_index];
		auto& owned = _economy._MyPlayers[_index];
		auto& view = owned.MyView;
		const auto location = _economy.Locate(view, _self.MyUid);
		const auto self = location ? location->Get(view) : _self;
		const auto position = location && location->MyOnBoard ? std::optional(BoardPosition::FromIndex(location->MyIndex)) : std::nullopt;
		const auto states = [&] { return ItemBondStates(_economy, player, _index); };
		const auto add = [&](std::string_view _bond, double _n, bool _active)
		{
			if (!(_n > 0)) return;
			if (_active)
			{
				const auto current = states();
				const auto bond = std::ranges::find(current, _bond, &BondState::MyId);
				if (bond == current.end() || !bond->MyActive) return;
			}
			(void)AddContentLayers(_economy, _players, _index, _bond, std::floor(_n), _random, _events);
		};
		const auto addAll = [&](std::span<const std::string_view> _bonds, double _n, bool _active)
		{
			for (const auto bond : _bonds) add(bond, _n, _active);
		};
		const auto ownBonds = [&] { return ItemBonds(player, self); };
		const auto top = [&]() -> std::string_view
		{
			std::string_view best;
			double layers = -1;
			for (const auto& bond : states()) if (bond.MyActive && bond.MyLayers > layers) { best = bond.MyId; layers = bond.MyLayers; }
			return best;
		};
		const auto boardChess = [&](const std::optional<Piece>& _piece)
		{
			return _piece && !_piece->IsToken() && _economy._MyCatalog.At(_piece->MyId).MyKind == PieceKind::CHESS;
		};
		const auto tiers = [&](std::string_view _bond)
		{
			std::array<bool, 7> seen{};
			for (const auto& piece : view.MyBoard)
				if (boardChess(piece) && std::ranges::contains(ItemBonds(player, *piece), _bond))
					seen[static_cast<std::size_t>(_economy._MyCatalog.At(piece->MyId).MyTier)] = true;
			return static_cast<double>(std::ranges::count(seen, true));
		};
		const auto rowCount = [&]
		{
			if (!position) return 0;
			int count = 0;
			for (std::size_t i = 0; i < view.MyBoard.size(); ++i)
				if (BoardPosition::FromIndex(i).MyRow == position->MyRow && boardChess(view.MyBoard[i])) ++count;
			return count;
		};
		const auto front = [&](PieceUid _uid, int _distance = 1) -> std::optional<Piece>
		{
			const auto loc = _economy.Locate(view, _uid);
			if (!loc || !loc->MyOnBoard) return {};
			auto tile = BoardPosition::FromIndex(loc->MyIndex);
			switch (loc->Get(view).MyFacing)
			{
			case Facing::UP: tile.MyRow += _distance; break;
			case Facing::RIGHT: tile.MyColumn += _distance; break;
			case Facing::DOWN: tile.MyRow -= _distance; break;
			case Facing::LEFT: tile.MyColumn -= _distance; break;
			}
			return tile.InField() && boardChess(view.MyBoard[tile.Index()]) ? view.MyBoard[tile.Index()] : std::nullopt;
		};
		const auto hasEvent = [&](const Piece& _piece, GarrisonEvent _event)
		{
			return std::ranges::any_of(Garrisons(player, _piece.MyId), [&](auto _id)
				{ const auto* rule = GarrisonRule(_id); return rule && rule->MyEvent == _event; });
		};
		const auto trigger = [&](const Piece& _piece, GarrisonEvent _event, const Piece* _as = nullptr)
		{
			const auto source = _economy.Locate(view, _piece.MyUid);
			const auto target = _economy.Locate(view, _as ? _as->MyUid : _piece.MyUid);
			if (!source || !target || _event == GarrisonEvent::PRICE) return;
			const auto sourceSnapshot = source->Get(view), targetSnapshot = target->Get(view);
			RunPieceGarrisons(_economy, _players, _index, _event, sourceSnapshot, targetSnapshot,
				!target->MyOnBoard && !target->MyInTemporary, true, _random, _events);
		};
		const auto grant = [&](std::string_view _id)
		{
			const auto* definition = _economy._MyCatalog.Find(_id);
			if (!definition || !EconomySession::Selected(view, *definition)) return false;
			if (definition->MyKind == PieceKind::CHESS)
				if (const auto* stock = _economy.Stock(owned, definition->MyBaseId); stock && stock->MyRemaining < 1) return false;
			const auto uid = _economy.Acquire(owned, *definition, _events);
			_economy.LiftOutOfRange(owned); _economy.FillHand(view);
			if (uid) _events.emplace_back(EconomyEvent{.MyKind = EventKind::GRANTED, .MyDefinitionId = _economy.GrantDefinition(view, *uid, definition->MyId, _events), .MyUid = *uid});
			return uid.has_value();
		};
		const auto pool = [&](std::string_view _pool)
		{
			return RollContentPool(_economy._MyCatalog, _economy, player.MyId, _MyPools, player.MyRoster, _random, _pool, view.MyLevel);
		};
		const auto pending = [&](double _n)
		{
			const auto n = static_cast<std::int64_t>(std::trunc(_n));
			if (n <= 0) return;
			if (view.MyPendingFunds > std::numeric_limits<std::int64_t>::max() - n) throw std::overflow_error("garrison pending funds overflow");
			view.MyPendingFunds += n;
		};
		const auto repeat = std::clamp(static_cast<int>(std::ceil(_rule.MyCount)), 0, 10);
		using Kind = GarrisonKind;
		switch (_rule.MyKind)
		{
		case Kind::ADD_BOND: addAll(_rule.MyBonds, _rule.MyCount, _rule.MyRequireActive); break;
		case Kind::ADD_BOND_CHESS_ALL: addAll(ownBonds(), _rule.MyCount, _rule.MyRequireActive); break;
		case Kind::ADD_MULTIPLE_BOND:
			for (std::size_t i = 0; i < _rule.MyBonds.size(); ++i)
				add(_rule.MyBonds[i], _rule.MyBondCounts.empty() ? 0 : _rule.MyBondCounts[std::min(i, _rule.MyBondCounts.size() - 1)], _rule.MyRequireActive);
			break;
		case Kind::ADD_BOND_METHOD:
		{
			double count = 0;
			switch (_rule.MyMethod)
			{
			case GarrisonMethod::SHOP_LEVEL: count = view.MyLevel; break;
			case GarrisonMethod::ROUND_GAINED: count = static_cast<double>(view.MyRoundStatistics.MyGainedChess); break;
			case GarrisonMethod::HAND_COUNT: count = static_cast<double>(std::ranges::count_if(view.MyHand, boardChess)); break;
			case GarrisonMethod::SAME_ROW: count = rowCount(); break;
			case GarrisonMethod::BOND_TIERS:
				for (const auto bond : _rule.MyBonds) add(bond, tiers(bond) * _rule.MyMultiplier, _rule.MyRequireActive);
				return;
			case GarrisonMethod::NONE: return;
			}
			addAll(_rule.MyBonds, count * _rule.MyMultiplier, _rule.MyRequireActive);
			break;
		}
		case Kind::ADD_BOND_ACTIVATED_MOST_LAYER:
			if (const auto bond = top(); !bond.empty()) add(bond, _rule.MyCount, true);
			break;
		case Kind::ADD_ACT_BOND_DIFF_LV_MOST_LAYER:
			if (const auto bond = top(); !bond.empty()) add(bond, tiers(bond) * _rule.MyMultiplier, true);
			break;
		case Kind::ADD_BOND_IN_HAND:
			for (std::size_t i = 0; i < view.MyHand.size(); ++i)
				if (boardChess(view.MyHand[i])) addAll(ItemBonds(player, *view.MyHand[i]), _rule.MyCount, true);
			break;
		case Kind::ADD_BOND_POSITION:
			if (position)
			{
				const auto other = front(self.MyUid, _rule.MyBehind ? -1 : 1);
				addAll(ownBonds(), _rule.MyCount, true);
				if (other && other->MyUid != self.MyUid) addAll(ItemBonds(player, *other), _rule.MyCount, true);
			}
			break;
		case Kind::ADD_BOND_ROUND_COIN_COST:
			addAll(_rule.MyBonds, std::floor(static_cast<double>(view.MyRoundStatistics.MySpent) / std::max(1.0, _rule.MyCount)) * _rule.MyLayer, _rule.MyRequireActive);
			break;
		case Kind::ADD_REFRESH_CNT_MULTIPLIER_BOND_LAYER:
		{
			const auto n = static_cast<double>(view.MyRoundStatistics.MyRefreshes) * _rule.MyMultiplier;
			addAll(_rule.MyBonds, _rule.MyMaximum > 0 ? std::min(n, _rule.MyMaximum) : n, _rule.MyRequireActive);
			break;
		}
		case Kind::GAIN_BOND_LAYER_BY_REFRESH_CNT:
			if (!_trigger && location)
			{
				auto found = std::ranges::find(player.MyGarrisonCounters, self.MyUid, &GarrisonCounter::MyUid);
				if (found == player.MyGarrisonCounters.end()) found = player.MyGarrisonCounters.insert(found, GarrisonCounter{.MyUid = self.MyUid});
				if (found->MyRound != _economy.Round()) { found->MyRound = _economy.Round(); found->MyRefreshes = 0; }
				if (static_cast<double>(++found->MyRefreshes) == _rule.MyRefreshCount) addAll(_rule.MyBonds, _rule.MyLayer, _rule.MyRequireActive);
			}
			break;
		case Kind::GAIN_EQUIP:
			for (int i = 0; i < repeat; ++i) (void)grant(_rule.MyChess);
			break;
		case Kind::GAIN_FREE_REFRESH_COUNT:
		{
			const auto count = static_cast<std::uint64_t>(std::max(0.0, std::trunc(_rule.MyCount)));
			if (view.MyFreeRefreshes > UINT64_MAX - count) throw std::overflow_error("garrison refresh count overflow");
			view.MyFreeRefreshes += count;
			break;
		}
		case Kind::GAIN_RANDOM_EQUIP_CHESS_IN_POOL:
			if (_rule.MyRounds.empty() || std::ranges::contains(_rule.MyRounds, _economy.Round()))
				for (int i = 0; i < repeat; ++i)
				{
					const auto result = pool(_rule.MyPool);
					const auto id = result && result->MyKind == PieceKind::ITEM ? std::optional(result->MyId) :
						RollContentItem(_economy._MyCatalog, _MyPools, _random, {.MyMaxTier = view.MyLevel});
					if (id) (void)grant(*id);
				}
			break;
		case Kind::POOL_EQUIP:
			for (int i = 0; i < repeat; ++i)
			{
				if (!_rule.MyGoldenWeights.empty())
				{
					double total = 0; for (const auto& weight : _rule.MyGoldenWeights) total += weight.MyWeight;
					auto value = _random.Next() * total;
					const auto* chosen = &_rule.MyGoldenWeights.back();
					for (const auto& weight : _rule.MyGoldenWeights) if ((value -= weight.MyWeight) < 0) { chosen = &weight; break; }
					(void)grant(chosen->MyId);
				}
				else if (const auto result = pool(_rule.MyPool); result && result->MyKind == PieceKind::ITEM) (void)grant(result->MyId);
			}
			break;
		case Kind::POOL_CHAR:
			if (!_rule.MySameRow || rowCount() >= _rule.MyCheckCount)
				for (int i = 0; i < repeat; ++i)
					if (const auto result = pool(_rule.MyPool); result && result->MyKind == PieceKind::CHESS) (void)grant(result->MyId);
			break;
		case Kind::MOST_BOND:
			if (!_rule.MySameRow || rowCount() >= _rule.MyCheckCount)
			{
				std::vector<std::string_view> best; best.reserve(_MyBonds.size());
				std::int64_t count = 0;
				for (const auto& bond : states())
				{
					if (bond.MyCount > count) { count = bond.MyCount; best.clear(); }
					if (bond.MyCount == count && count > 0) best.push_back(bond.MyId);
				}
				_random.Shuffle(best.begin(), best.end());
				for (const auto bond : best)
				{
					std::vector<std::string_view> members; members.reserve(player.MyRoster.size());
					for (const auto& entry : player.MyRoster) if (std::ranges::contains(entry.MyBonds, bond)) members.push_back(entry.MyId);
					RollOptions options; options.MyMaxTier = 6; options.MyIncluded = members;
					if (const auto id = _economy.RollChess(player.MyId, _random, options); id && grant(*id)) break;
				}
			}
			break;
		case Kind::ONCE_GOLD: pending(_rule.MyCount); break;
		case Kind::ONCE_GOLD_WITH_BOND_CONDITION:
		{
			const auto current = states();
			if (!_hand || std::ranges::any_of(current, [&](const auto& _bond) { return _bond.MyActive && std::ranges::contains(_rule.MyBonds, _bond.MyId); })) pending(_rule.MyCount);
			break;
		}
		case Kind::SELL_CHESS_GAIN_SPECIAL_GOODS:
		{
			const auto poolId = _rule.MyLevelPools[static_cast<std::size_t>(std::clamp(view.MyLevel, 1, 6) - 1)];
			std::vector<std::string> ids; ids.reserve(static_cast<std::size_t>(_economy._MyRules.MyRewardCount));
			for (int i = 0; i < _economy._MyRules.MyRewardCount * 4 && static_cast<int>(ids.size()) < _economy._MyRules.MyRewardCount; ++i)
				if (const auto result = pool(poolId); result && result->MyKind == PieceKind::CHESS && !std::ranges::contains(ids, result->MyId)) ids.push_back(result->MyId);
			if (!ids.empty())
			{
				std::vector<ShopSlot> slots; slots.reserve(ids.size());
				for (auto& id : ids) slots.push_back({.MyId = std::move(id), .MyPrice = _economy._MyRules.MyRewardPrice});
				view.MyOffers.push_back(std::move(slots));
			}
			// Empty fixed-tier pools have no available candidates for the fallback offer either.
			break;
		}
		case Kind::TRIGGER_ANOTHER:
			if (!_rule.MyFarthest)
			{
				if (const auto other = front(self.MyUid); other && other->MyUid != self.MyUid) trigger(*other, _rule.MyTrigger);
			}
			else
			{
				std::optional<Piece> chosen;
				for (int row = 12; row >= 9 && !chosen; --row)
					for (int col = 10; col >= 2; --col)
					{
						const auto& piece = view.MyBoard[BoardPosition{row, col}.Index()];
						if (boardChess(piece) && piece->MyUid != self.MyUid && hasEvent(*piece, _rule.MyTrigger)) { chosen = *piece; break; }
					}
				if (chosen) trigger(*chosen, _rule.MyTrigger);
			}
			break;
		case Kind::TRIGGER_FRONT_COUNT:
			for (int i = 1; i <= std::min(8, static_cast<int>(std::floor(_rule.MyCount))); ++i)
				if (const auto other = front(self.MyUid, i); other && other->MyUid != self.MyUid) trigger(*other, _rule.MyTrigger);
			break;
		case Kind::FRONT_SAME_EFFECT_PREP_FIN:
		case Kind::FRONT_SAME_EFFECT_PREP_START:
		{
			auto target = front(self.MyUid);
			for (int i = 0; i < 8 && target; ++i)
			{
				bool found = false, same = true;
				for (const auto id : Garrisons(player, target->MyId))
					if (const auto* rule = GarrisonRule(id); rule && rule->MyEvent == _rule.MyEvent)
					{ found = true; same &= rule->MyKind == _rule.MyKind; }
				if (!found) return;
				if (!same) break;
				target = front(target->MyUid);
			}
			if (target && target->MyUid != self.MyUid) trigger(*target, _rule.MyEvent, &self);
			break;
		}
		case Kind::CHESS_PRICE: break; // Pure price queries run before strategy and bond modifiers.
		}
	}
}
