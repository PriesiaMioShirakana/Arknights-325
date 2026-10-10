#include <algorithm>
#include <limits>
#include <stdexcept>
#include <stronghold/domain/preparation.hpp>
#include <type_traits>
#include "preparation_hooks.hpp"

namespace Stronghold
{
	namespace
	{
		std::size_t FreeSlot(const PlayerView& _player)
		{
			for (auto i = _player.MyHand.size(); i > 0; --i)
				if (!_player.MyHand[i - 1])
					return i - 1;
			return _player.MyHand.size();
		}
	} // namespace

	EconomySession::EconomySession(
		const Catalog& _catalog,
		std::string_view _mode,
		std::span<const Seat> _players,
		std::uint32_t _seed,
		bool _experimental,
		bool _independentPools,
		const std::set<std::string, std::less<>>& _banned,
		BoardLayout _board
	)
		: _MyCatalog(_catalog), _MyRules(_catalog.Rules(_mode)), _MyRandom(DeriveSeed(_seed, u"shop"))
	{
		for (const auto terrain : _board.MyTiles)
			if (terrain != Terrain::BLOCKED && terrain != Terrain::GROUND && terrain != Terrain::HIGH)
				throw std::invalid_argument("invalid board terrain");
		for (const auto& group : PoolGroups(_players, _experimental, _independentPools))
		{
			const auto poolIndex = _MyPools.size();
			_MyPools.emplace_back(_MyCatalog, _banned, group.MyScale);
			for (const auto& id : group.MyPlayers)
			{
				Player player;
				player.MyPool = poolIndex;
				player.MySeat = std::ranges::find(_players, id, &Seat::MyPlayerId)->MySeat;
				player.MyView.MyPlayerId = id;
				player.MyView.MyUpgradePrice = _MyRules.UpgradeAt(1);
				player.MyView.MyHand.resize(_MyRules.MyHandSize);
				player.MyView.MyTemporary.resize(_MyRules.MyTemporarySize);
				player.MyView.MyLayout = _board;
				player.MyView.MyDeployCap = _MyRules.MyDeployCap;
				_MyPlayers.push_back(std::move(player));
			}
		}
	}

	void EconomySession::BeginRound(int _round, bool _openPreparation)
	{
		if (_MyPhase == PreparationPhase::PREPARING || _MyPhase == PreparationPhase::ROUND_START || _round != _MyRound + 1 || _round > 10000)
			throw std::invalid_argument("invalid round transition");
		auto next = *this;
		next._MyRound = _round;
		next._MyPhase = PreparationPhase::ROUND_START;
		for (auto& player : next._MyPlayers)
		{
			auto& view = player.MyView;
			if (!view.MyAlive) continue;
			std::int64_t income = next._MyRules.IncomeAt(_round);
			if (next._MyHooks) next._MyHooks->Income(next, view.MyPlayerId, income);
			if (view.MyFunds > std::numeric_limits<std::int64_t>::max() - view.MyPendingFunds ||
				view.MyFunds + view.MyPendingFunds > std::numeric_limits<std::int64_t>::max() - income) throw std::overflow_error("preparation income overflow");
			view.MyRoundStatistics = {};
			AddFunds(view, view.MyPendingFunds + income);
			view.MyPendingFunds = 0;
			if (_round > 1)
				view.MyUpgradePrice = std::max(0, view.MyUpgradePrice - 1);
			view.MyReady = false;
			const auto order = view.MyBoardOrder;
			for (std::size_t n = 0; n < view.MyBoardOrderSize; ++n)
				if (const auto& piece = view.MyBoard[order[n]]; piece && !piece->IsToken()) next.GrantTokens(player, *piece);
			next.RollShop(player, true);
			next.LiftOutOfRange(player); next.FillHand(view);

			view.MyFrozen = false;
			for (auto& slot : view.MyShop)
				if (slot)
					slot->MyFrozen = false;
		}
		if (_openPreparation)
		{
			next._MyPhase = PreparationPhase::PREPARING;
			std::vector<EconomyEvent> events;
			for (auto& player : next._MyPlayers)
				if (player.MyView.MyAlive) { next.CheckItemMerges(player, events); next.LiftOutOfRange(player); next.FillHand(player.MyView); }
		}
		++next._MyRevision;
		Commit(std::move(next));
	}

	void EconomySession::BeginPreparation()
	{
		if (_MyPhase != PreparationPhase::ROUND_START) throw std::invalid_argument("not waiting for preparation");
		auto next = *this; next._MyPhase = PreparationPhase::PREPARING;
		std::vector<EconomyEvent> events;
		for (auto& player : next._MyPlayers)
			if (player.MyView.MyAlive)
			{
				player.MyView.MyReady = false; next.CheckItemMerges(player, events); next.LiftOutOfRange(player); next.FillHand(player.MyView);
				if (next._MyHooks) next._MyHooks->PreparationStarted(next, player.MyView.MyPlayerId, events);
			}
		++next._MyRevision; Commit(std::move(next));
	}

	void EconomySession::ApplyPreparationDeadline()
	{
		if (_MyPhase != PreparationPhase::PREPARING) throw std::invalid_argument("not preparing");
		for (auto& player : _MyPlayers)
			if (player.MyView.MyAlive && !player.MyView.MyReady) { ResolveTemporary(player); player.MyView.MyReady = true; }
		++_MyRevision;
	}

	void EconomySession::EndPreparation()
	{
		if (_MyPhase != PreparationPhase::PREPARING)
			throw std::invalid_argument("not preparing");
		for (auto& player : _MyPlayers)
		{
			auto& view = player.MyView;
			if (!view.MyAlive) continue;
			// 已准备之后溢出的奖励留到下次可操作的准备阶段；到期只清理，不产生出售收益。
			ResolveTemporary(player);
			++view.MyPreparationsEnded;
			if (!view.MyKeepRemainingFunds) view.MyFunds = 0;
			view.MyReady = true;
			view.MyOffers.clear();
			for (auto& slot : view.MyShop)
				if (slot && (!slot->MyFrozen || slot->MySold))
					slot.reset();
		}
		_MyPhase = PreparationPhase::CLOSED;
		++_MyRevision;
	}

	void EconomySession::ApplySettlement(int _round, std::span<const EconomySettlement> _players)
	{
		if (_MyPhase != PreparationPhase::CLOSED || _round != _MyRound || _round <= _MySettledRound || _players.size() != _MyPlayers.size())
			throw std::invalid_argument("invalid economy settlement");
		auto next = *this;
		std::vector<bool> seen(_MyPlayers.size());
		for (const auto& result : _players)
		{
			const auto it = std::ranges::find_if(next._MyPlayers, [&](const Player& _player) { return _player.MyView.MyPlayerId == result.MyPlayerId; });
			if (it == next._MyPlayers.end() || result.MyFunds < 0) throw std::invalid_argument("invalid settlement player");
			const auto index = static_cast<std::size_t>(it - next._MyPlayers.begin());
			if (seen[index]) throw std::invalid_argument("duplicate settlement player");
			seen[index] = true;
			auto& view = it->MyView;
			if (!view.MyAlive)
			{
				if (!result.MyEliminated || result.MyFunds) throw std::invalid_argument("cannot revive a finalized economy player");
				continue;
			}
			if (!result.MyEliminated)
			{
				if (view.MyPendingFunds > std::numeric_limits<std::int64_t>::max() - result.MyFunds) throw std::overflow_error("pending income overflow");
				view.MyPendingFunds += result.MyFunds;
				continue;
			}
			
			const auto release = [&](auto& _pieces)
			{
				for (auto& piece : _pieces)
				{
					if (!piece) continue;
					if (piece->MyPoolCopies) (void)next.ReturnCopies(*it, _MyCatalog.At(piece->MyId).MyBaseId, piece->MyPoolCopies);
					piece.reset();
				}
			};
			release(view.MyHand); release(view.MyBoard); release(view.MyTemporary); view.MyBoardOrderSize = 0;
			view.MyAlive = false; view.MyReady = false; view.MyFunds = 0; view.MyPendingFunds = 0;
			view.MyShop.clear(); view.MyOffers.clear();
		}
		next._MySettledRound = _round; ++next._MyRevision;
		Commit(std::move(next));
	}

	std::optional<ShopSlot> EconomySession::RollSlot(Player& _player, PieceKind _kind)
	{
		auto& pool = _MyPools[_player.MyPool];
		RollOptions options;
		options.MyMaxTier = _player.MyView.MyLevel;
		options.MyExtra = _player.MyView.MyPrivateStock;
		options.MyShopLevel = _player.MyView.MyLevel;
		auto id = _kind == PieceKind::CHESS ? pool.Roll(_MyRandom, options) : pool.RollItem(_MyRandom, _player.MyView.MyLevel, _MyCatalog);
		if (!id)
			return std::nullopt;
		return ShopSlot{*id, _MyCatalog.At(*id).MyPrice, _player.MyView.MyFrozen, false};
	}

	void EconomySession::RollShop(Player& _player, bool _keepFrozen, bool _onlyNew)
	{
		auto& view = _player.MyView;
		const auto layout = _MyRules.LayoutAt(view.MyLevel);
		auto old = std::move(view.MyShop);
		const auto append = [&](PieceKind _kind, int _count, int _oldCount, int _offset)
		{
			for (int i = 0; i < _count; ++i)
			{
				const auto index = static_cast<std::size_t>(_offset + i);
				const bool existing = i < _oldCount && index < old.size();
				if (existing && (_onlyNew || (_keepFrozen && old[index] && old[index]->MyFrozen && !old[index]->MySold)))
					view.MyShop.emplace_back(std::move(old[index]));
				else
					view.MyShop.emplace_back(RollSlot(_player, _kind));
				if (view.MyShop.back())
					view.MyShop.back()->MyFrozen = view.MyFrozen;
			}
		};
		view.MyShop.clear();
		view.MyShop.reserve(static_cast<std::size_t>(layout.MyChess + layout.MyItems));
		append(PieceKind::CHESS, layout.MyChess, _player.MyLayout.MyChess, 0);
		append(PieceKind::ITEM, layout.MyItems, _player.MyLayout.MyItems, _player.MyLayout.MyChess);
		_player.MyLayout = layout;
	}

	void EconomySession::QueueReward(Player& _player)
	{
		auto& view = _player.MyView;
		std::vector<ShopSlot> slots;
		RollOptions options;
		options.MyExtra = view.MyPrivateStock; options.MyShopLevel = view.MyLevel;
		const auto tier = std::min(view.MyLevel + _MyRules.MyRewardTierOffset, _MyRules.MyRewardMaxTier);
		for (int i = 0; i < _MyRules.MyRewardCount; ++i)
		{
			std::optional<std::string> id;
			for (int t = tier; t >= 1 && !id; --t)
			{
				options.MyExactTier = t;
				id = _MyPools[_player.MyPool].Roll(_MyRandom, options);
			}
			if (!id)
				break;
			options.MyExcluded.insert(*id);
			slots.push_back({*id, _MyRules.MyRewardPrice, false, false});
		}
		if (!slots.empty())
			view.MyOffers.push_back(std::move(slots));
	}

	std::expected<ChangeSet, CommandError> EconomySession::Execute(const CommandEnvelope& _command)
	{
		const auto it = std::ranges::find_if(_MyPlayers, [&](const Player& _player) { return _player.MyView.MyPlayerId == _command.MyPlayerId; });
		if (it == _MyPlayers.end())
			return std::unexpected(CommandError::UNKNOWN_PLAYER);
		if (!it->MyView.MyAlive) return std::unexpected(CommandError::ELIMINATED);
		if (_command.MyRound != _MyRound)
			return std::unexpected(CommandError::STALE_ROUND);
		if (_MyPhase != PreparationPhase::PREPARING)
			return std::unexpected(CommandError::WRONG_PHASE);
		if (it->MyView.MyReady && !std::holds_alternative<SetReady>(_command.MyCommand))
			return std::unexpected(CommandError::READY);
		// Value transaction: rule rejection or allocation failure cannot leave partial purchases or RNG draws.
		auto next = *this;
		auto& player = next._MyPlayers[static_cast<std::size_t>(it - _MyPlayers.begin())];
		ChangeSet changes;
		if (auto error = next.Apply(player, _command.MyCommand, changes.MyEvents))
		{
			// PlayerPlacement._moveToHand 的满手召唤物撤回会 delete/set 原棋盘键。
			// 此次序会决定后续回收堆叠保留哪个 UID，必须保留；其余失败命令仍完全回滚。
			if (*error == CommandError::HAND_FULL)
				if (const auto* move = std::get_if<MoveToHand>(&_command.MyCommand))
					if (const auto old = Locate(it->MyView, move->MyUid); old && old->MyOnBoard && it->MyView.MyBoard[old->MyIndex]->IsToken())
						it->MyView.MyBoardOrder = player.MyView.MyBoardOrder;
			return std::unexpected(*error);
		}
		next.LiftOutOfRange(player); next.FillHand(player.MyView);
		changes.MyRevision = ++next._MyRevision;
		Commit(std::move(next));
		return changes;
	}

	std::optional<CommandError> EconomySession::Apply(Player& _player, const PreparationCommand& _command, std::vector<EconomyEvent>& _events)
	{
		auto& view = _player.MyView;
		return std::visit(
			[&](const auto& _action) -> std::optional<CommandError>
			{
				using Action = std::decay_t<decltype(_action)>;
				if constexpr (std::is_same_v<Action, Buy> || std::is_same_v<Action, PickReward>)
				{
					ShopSlot* slot = nullptr;
					if constexpr (std::is_same_v<Action, Buy>)
					{
						if (_action.MySlot < view.MyShop.size() && view.MyShop[_action.MySlot])
							slot = &*view.MyShop[_action.MySlot];
					}
					else
					{
						if (!view.MyOffers.empty() && _action.MySlot < view.MyOffers.front().size())
							slot = &view.MyOffers.front()[_action.MySlot];
					}
					if (!slot)
						return CommandError::BAD_TARGET;
					if (slot->MySold)
						return CommandError::SOLD_OUT;
					std::int64_t price = slot->MyPrice;
					if constexpr (std::is_same_v<Action, Buy>)
						if (_MyHooks) price = _MyHooks->Price(*this, view.MyPlayerId, *slot);
					if (view.MyFunds < price) return CommandError::NO_FUNDS;
					const auto& definition = _MyCatalog.At(slot->MyId);
					if (!Selected(view, definition)) return CommandError::BAD_TARGET;
					const auto* stock = Stock(_player, definition.MyBaseId);
					const auto copies = definition.MyGolden ? _MyRules.MyGoldenCopies : 1;
					if (definition.MyKind == PieceKind::CHESS && stock && stock->MyRemaining < copies)
						return CommandError::SOLD_OUT;
					if (FreeSlot(view) == view.MyHand.size())
						return CommandError::HAND_FULL;
					if (view.MyFunds < price)
						return CommandError::NO_FUNDS;
					(void)Spend(view, price);
					slot->MySold = true;
					if constexpr (std::is_same_v<Action, PickReward>)
						view.MyOffers.erase(view.MyOffers.begin());
					const auto uid = Acquire(_player, definition, _events);
					if (!uid) throw std::logic_error("purchase lost a piece despite a free hand slot");
					if constexpr (std::is_same_v<Action, Buy>)
					{
						++view.MyStatistics.MyBuys; ++view.MyRoundStatistics.MyBuys;
						ApplyPurchaseUpgrade(_player, *uid, _events);
					}
					constexpr auto Kind = std::is_same_v<Action, Buy> ? EventKind::PURCHASED : EventKind::REWARD_PICKED;
					_events.push_back({Kind, GrantDefinition(view, *uid, definition.MyId, _events), *uid, price});
					if (_MyHooks)
					{
						if constexpr (std::is_same_v<Action, Buy>) _MyHooks->Bought(*this, view.MyPlayerId, definition.MyId, _events);
						_MyHooks->Spent(*this, view.MyPlayerId, price, _events);
					}
				}
				else if constexpr (std::is_same_v<Action, Refresh>)
				{
					const auto price = view.MyFreeRefreshes ? 0 : _MyRules.MyRefreshPrice;
					if (view.MyFunds < price) return CommandError::NO_FUNDS;
					if (view.MyFreeRefreshes) --view.MyFreeRefreshes;
					else (void)Spend(view, price);
					RollShop(_player, false);
					++view.MyStatistics.MyRefreshes; ++view.MyRoundStatistics.MyRefreshes;
					_events.emplace_back(EconomyEvent{.MyKind = EventKind::REFRESHED, .MyDefinitionId = {}, .MyAmount = price});
					if (_MyHooks) { _MyHooks->Refreshed(*this, view.MyPlayerId, _events); _MyHooks->Spent(*this, view.MyPlayerId, price, _events); }
				}
				else if constexpr (std::is_same_v<Action, Freeze>)
				{
					view.MyFrozen = !view.MyFrozen;
					for (auto& slot : view.MyShop)
						if (slot && !slot->MySold)
							slot->MyFrozen = view.MyFrozen;
					_events.push_back({EventKind::FROZEN, {}, 0, view.MyFrozen});
				}
				else if constexpr (std::is_same_v<Action, LevelUp>)
				{
					if (view.MyLevel >= _MyRules.MyMaxLevel)
						return CommandError::MAX_LEVEL;
					if (view.MyFunds < view.MyUpgradePrice)
						return CommandError::NO_FUNDS;
					const auto price = view.MyUpgradePrice;
					(void)Spend(view, price);
					++view.MyLevel;
					view.MyUpgradePrice = _MyRules.UpgradeAt(view.MyLevel);
					RollShop(_player, false, true);
					_events.push_back({EventKind::LEVELLED, {}, 0, price});
					if (_MyHooks) { _MyHooks->Levelled(*this, view.MyPlayerId, _events); _MyHooks->Spent(*this, view.MyPlayerId, price, _events); }
				}
				else if constexpr (std::is_same_v<Action, SetReady>)
				{
					if (_action.MyReady && std::ranges::any_of(view.MyTemporary, [](const auto& _piece) { return _piece.has_value(); })) return CommandError::TEMP_NOT_EMPTY;
					if (!_action.MyReady)
						for (auto& piece : view.MyTemporary)
							if (piece && piece->MyTemporaryDue && *piece->MyTemporaryDue > view.MyPreparationsEnded) piece->MyTemporaryDue = view.MyPreparationsEnded;
					view.MyReady = _action.MyReady;
					_events.push_back({EventKind::READINESS, {}, 0, view.MyReady});
				}
				else if constexpr (std::is_same_v<Action, Sell> || std::is_same_v<Action, DestroyItem>)
				{
					const auto location = Locate(view, _action.MyUid);
					if (!location || location->MyEquippedIndex)
						return CommandError::BAD_TARGET;
					auto& slot = location->Slot(view);
					if (slot->IsToken()) return CommandError::BAD_TARGET;
					const auto& definition = _MyCatalog.At(slot->MyId);
					if constexpr (std::is_same_v<Action, Sell>)
					{
						if (definition.MyKind != PieceKind::CHESS)
							return CommandError::BAD_TARGET;
						const auto room = std::ranges::count_if(view.MyHand, [](const auto& _piece) { return !_piece; }) +
							std::ranges::count_if(view.MyTemporary, [](const auto& _piece) { return !_piece; }) + (!location->MyOnBoard ? 1 : 0);
						if (slot->MyItems.size() > static_cast<std::size_t>(room)) return CommandError::HAND_FULL;
						if (view.MyFunds > std::numeric_limits<std::int64_t>::max() - definition.MySellPrice) throw std::overflow_error("sale income overflow");
						auto piece = Detach(view, *location); RemoveTokens(view, piece.MyUid);
						for (auto& item : piece.MyItems)
							if (!Stow(view, item)) throw std::logic_error("sale lost equipment despite reserved space");
						(void)ReturnCopies(_player, definition.MyBaseId, piece.MyPoolCopies);
						std::int64_t gain = definition.MySellPrice;
						piece.MyItems.clear();
						if (_MyHooks) _MyHooks->Sold(*this, view.MyPlayerId, piece, gain, _events);
						AddFunds(view, gain);
						++view.MyStatistics.MySells; ++view.MyRoundStatistics.MySells;
						_events.emplace_back(EconomyEvent{.MyKind = EventKind::SOLD, .MyDefinitionId = definition.MyId, .MyUid = piece.MyUid, .MyAmount = gain});
						CheckItemMerges(_player, _events);
					}
					else
					{
						if (definition.MyKind != PieceKind::ITEM)
							return CommandError::BAD_TARGET;
						_events.emplace_back(EconomyEvent{.MyKind = EventKind::DESTROYED, .MyDefinitionId = definition.MyId, .MyUid = slot->MyUid});
						slot.reset();
					}
				}
				else if constexpr (std::is_same_v<Action, EquipItem>)
				{
					if (const auto error = Equip(_player, _action, _events)) return error;
				}
				else if constexpr (std::is_same_v<Action, UseArt>)
					return CommandError::BAD_TARGET; // 法术必须通过 PreparationContent 事务执行。
				else if constexpr (std::is_same_v<Action, MoveToBoard> || std::is_same_v<Action, MoveToHand>)
				{
					if (const auto error = Move(_player, _action))
						return error;
					_events.push_back({EventKind::MOVED, {}, _action.MyUid, 0});
				}
				return std::nullopt;
			},
			_command
		);
	}

	std::optional<PlayerView> EconomySession::View(std::string_view _playerId) const
	{
		for (const auto& player : _MyPlayers)
			if (player.MyView.MyPlayerId == _playerId)
				return player.MyView;
		return std::nullopt;
	}

	std::vector<PublicPlayerView> EconomySession::PublicView() const
	{
		std::vector<PublicPlayerView> result;
		result.reserve(_MyPlayers.size());
		for (const auto& player : _MyPlayers)
			result.emplace_back(PublicPlayerView{.MyPlayerId = player.MyView.MyPlayerId, .MyLevel = player.MyView.MyLevel,
				.MyReady = player.MyView.MyReady, .MyBoard = player.MyView.MyBoard, .MyAlive = player.MyView.MyAlive});
		return result;
	}

	bool EconomySession::PoolConservationHolds() const
	{
		for (const auto& player : _MyPlayers)
		{
			const auto& view = player.MyView;
			std::array<bool, 36> seen{};
			if (view.MyBoardOrderSize > view.MyBoard.size()) return false;
			for (std::size_t n = 0; n < view.MyBoardOrderSize; ++n)
			{
				const auto i = view.MyBoardOrder[n];
				if (i >= view.MyBoard.size() || seen[i] || !view.MyBoard[i]) return false;
				seen[i] = true;
			}
			for (std::size_t i = 0; i < view.MyBoard.size(); ++i) if (seen[i] != view.MyBoard[i].has_value()) return false;
		}
		for (const auto& player : _MyPlayers)
			for (const auto& entry : player.MyView.MyPrivateStock)
			{
				std::int64_t held = 0;
				const auto count = [&](const auto& _pieces)
				{
					for (const auto& piece : _pieces)
						if (piece && !piece->IsToken() && _MyCatalog.At(piece->MyId).MyBaseId == entry.MyId) held += piece->MyPoolCopies;
				};
				count(player.MyView.MyHand); count(player.MyView.MyBoard); count(player.MyView.MyTemporary);
				if (entry.MyStock.MyRemaining < 0 || entry.MyStock.MyRemaining + held != entry.MyStock.MyCapacity) return false;
			}
		for (std::size_t i = 0; i < _MyPools.size(); ++i)
			for (const auto& [id, stock] : _MyPools[i].Entries())
			{
				std::int64_t held = 0;
				for (const auto& player : _MyPlayers)
					if (player.MyPool == i)
					{
						for (const auto& piece : player.MyView.MyHand)
							if (piece && !piece->IsToken() && _MyCatalog.At(piece->MyId).MyBaseId == id)
								held += piece->MyPoolCopies;
						for (const auto& piece : player.MyView.MyTemporary)
							if (piece && !piece->IsToken() && _MyCatalog.At(piece->MyId).MyBaseId == id) held += piece->MyPoolCopies;
						for (const auto& piece : player.MyView.MyBoard)
							if (piece && !piece->IsToken() && _MyCatalog.At(piece->MyId).MyBaseId == id)
								held += piece->MyPoolCopies;
					}
				if (stock.MyRemaining < 0 || stock.MyRemaining + held != stock.MyCapacity)
					return false;
			}
		return true;
	}
} // namespace Stronghold
