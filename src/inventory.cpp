#include <limits>
#include <stronghold/domain/preparation.hpp>
#include "preparation_hooks.hpp"

namespace Stronghold
{
	namespace
	{
		std::optional<std::size_t> EmptySlot(std::span<const std::optional<Piece>> _slots)
		{
			for (auto i = _slots.size(); i > 0; --i) if (!_slots[i - 1]) return i - 1;
			return {};
		}
	}

	void EconomySession::ResolveTemporary(Player& _player)
	{
		auto& view = _player.MyView;
		for (auto& piece : view.MyTemporary)
			if (piece && piece->MyTemporaryDue.value_or(view.MyPreparationsEnded) <= view.MyPreparationsEnded)
			{
				const auto uid = piece->MyUid;
				if (piece->MyPoolCopies) (void)ReturnCopies(_player, _MyCatalog.At(piece->MyId).MyBaseId, piece->MyPoolCopies);
				piece.reset(); RemoveTokens(view, uid);
			}
		FillHand(view);
	}

	bool EconomySession::Stow(PlayerView& _view, Piece& _piece, bool _temporary) const
	{
		if (!_temporary)
			if (const auto slot = EmptySlot(_view.MyHand))
			{
				_piece.MyTemporaryDue.reset(); _view.MyHand[*slot] = std::move(_piece); return true;
			}
		if (const auto slot = EmptySlot(_view.MyTemporary))
		{
			_piece.MyTemporaryDue = _view.MyPreparationsEnded + (_MyPhase == PreparationPhase::PREPARING && _view.MyReady ? 1U : 0U);
			_view.MyTemporary[*slot] = std::move(_piece); return true;
		}
		return false;
	}

	void EconomySession::FillHand(PlayerView& _view) const
	{
		// 两个区域均从右向左填充；临时区也从右侧取出，保留原版规定的溢出处理顺序。
		for (auto i = _view.MyTemporary.size(); i > 0; --i)
		{
			if (!_view.MyTemporary[i - 1]) continue;
			const auto slot = EmptySlot(_view.MyHand); if (!slot) break;
			auto piece = std::move(*_view.MyTemporary[i - 1]); _view.MyTemporary[i - 1].reset();
			piece.MyTemporaryDue.reset(); _view.MyHand[*slot] = std::move(piece);
		}
	}

	Piece EconomySession::Detach(PlayerView& _view, const PieceLocation& _location)
	{
		Piece piece = std::move(_location.Get(_view));
		if (_location.MyEquippedIndex)
		{
			auto& items = _location.Slot(_view)->MyItems;
			items.erase(items.begin() + static_cast<std::ptrdiff_t>(*_location.MyEquippedIndex));
		}
		else if (_location.MyOnBoard) ClearBoard(_view, _location.MyIndex);
		else _location.Slot(_view).reset();
		piece.MyTemporaryDue.reset();
		return piece;
	}

	std::vector<EconomySession::PieceLocation> EconomySession::MergeLocations(const PlayerView& _view, const Definition& _definition) const
	{
		std::vector<PieceLocation> matches; matches.reserve(_view.MyHand.size() + _view.MyTemporary.size() + 36);
		const auto scan = [&](const auto& _pieces, bool _board, bool _temporary, bool _equipped)
		{
			for (std::size_t i = 0; i < _pieces.size(); ++i)
			{
				const auto& piece = _pieces[i]; if (!piece) continue;
				if (!_equipped && piece->MyId == _definition.MyId) matches.emplace_back(PieceLocation{.MyOnBoard = _board, .MyIndex = i, .MyInTemporary = _temporary});
				if (_equipped)
					for (std::size_t j = 0; j < piece->MyItems.size(); ++j)
						if (piece->MyItems[j].MyId == _definition.MyId) matches.emplace_back(PieceLocation{.MyOnBoard = _board, .MyIndex = i, .MyInTemporary = _temporary, .MyEquippedIndex = j});
			}
		};
		scan(_view.MyTemporary, false, true, false); scan(_view.MyHand, false, false, false);
		if (_definition.MyKind == PieceKind::CHESS) scan(_view.MyBoard, true, false, false);
		else { scan(_view.MyBoard, true, false, true); scan(_view.MyHand, false, false, true); scan(_view.MyTemporary, false, true, true); }
		return matches;
	}

	std::string EconomySession::GrantDefinition(PlayerView& _view, PieceUid _uid, std::string_view _fallback,
		std::span<const EconomyEvent> _events) const
	{
		if (const auto location = Locate(_view, _uid)) return location->Get(_view).MyId;
		// 获得时的连锁赠送可能再次合成来源；收据仍报告本次获得的身份。
		for (auto it = _events.rbegin(); it != _events.rend(); ++it)
			if (it->MyUid == _uid && !it->MyDefinitionId.empty()) return it->MyDefinitionId;
		return std::string(_fallback);
	}

	std::optional<PieceUid> EconomySession::Acquire(Player& _player, const Definition& _definition, std::vector<EconomyEvent>& _events, GrantOptions _options)
	{
		auto& view = _player.MyView;
		if (!Selected(view, _definition)) return {};
		if (_definition.MyKind == PieceKind::CHESS) ++view.MyRoundStatistics.MyGainedChess;
		Piece piece{.MyUid = ++_MyNextUid, .MyId = _definition.MyId, .MyDeferredMerge = _definition.MyKind == PieceKind::ITEM && (_options.MyDeferItemMerge || (_MyHooks && _MyHooks->MyPrepEndDepth > 0))};
		if (_definition.MyKind == PieceKind::CHESS && _options.MyFromPool)
			piece.MyPoolCopies = TakeCopies(_player, _definition.MyBaseId, _definition.MyGolden ? _MyRules.MyGoldenCopies : 1);
		std::optional<PieceUid> uid;
		if (!piece.MyDeferredMerge && _definition.MyMergeCount > 1 && MergeLocations(view, _definition).size() + 1 >= static_cast<std::size_t>(_definition.MyMergeCount))
			uid = MergePieces(_player, _definition, std::move(piece), _events);
		else
		{
			const auto incoming = piece.MyUid;
			if (Stow(view, piece, _options.MyToTemporary)) uid = incoming;
			else if (piece.MyPoolCopies) (void)ReturnCopies(_player, _definition.MyBaseId, piece.MyPoolCopies);
		}
		if (uid && _MyHooks)
		{
			LiftOutOfRange(_player); FillHand(view);
			_MyHooks->Acquired(*this, view.MyPlayerId, *uid, _events);
			LiftOutOfRange(_player); FillHand(view);
			if (_definition.MyKind == PieceKind::CHESS)
				_MyHooks->RefreshBonds(*this, static_cast<std::size_t>(&_player - _MyPlayers.data()));
		}
		return uid;
	}

	std::optional<PieceUid> EconomySession::MergePieces(Player& _player, const Definition& _definition, std::optional<Piece> _incoming, std::vector<EconomyEvent>& _events)
	{
		auto& view = _player.MyView;
		const auto matches = MergeLocations(view, _definition);
		const auto take = static_cast<std::size_t>(_definition.MyMergeCount) - (_incoming ? 1U : 0U);
		if (_definition.MyMergeCount <= 1 || matches.size() < take) return {};
		std::array<PieceUid, 99> chosen{}; // Catalog 将合成数限制为 2..99；不为每次合成再分配 UID 列表。
		for (std::size_t i = 0; i < take; ++i) chosen[i] = matches[i].Get(view).MyUid;
		std::vector<Piece> returned; returned.reserve(take * _MyRules.MyEquipmentPerChess);
		std::optional<std::size_t> tile, fallbackTile;
		Facing direction = Facing::RIGHT, fallbackDirection = Facing::RIGHT;
		std::optional<PieceUid> firstHolder; std::size_t firstItemSlot = 0;
		int copies = _incoming ? _incoming->MyPoolCopies : 0;
		for (std::size_t i = 0; i < take; ++i)
		{
			const auto location = *Locate(view, chosen[i]);
			if (location.MyEquippedIndex && !firstHolder) { firstHolder = location.Slot(view)->MyUid; firstItemSlot = *location.MyEquippedIndex; }
			if (_definition.MyKind == PieceKind::CHESS && location.MyOnBoard)
			{
				const auto position = BoardPosition::FromIndex(location.MyIndex);
				const auto better = [&](std::optional<std::size_t> _old) { return !_old || position.MyColumn < BoardPosition::FromIndex(*_old).MyColumn; };
				if (better(fallbackTile)) { fallbackTile = location.MyIndex; fallbackDirection = location.Get(view).MyFacing; }
				if (view.MyLayout.CanPlace(PlacementOf(view, _MyCatalog.At(_definition.MyGoldenId)), position) && better(tile)) { tile = location.MyIndex; direction = location.Get(view).MyFacing; }
			}
			auto old = Detach(view, location); copies += old.MyPoolCopies; RemoveTokens(view, old.MyUid);
			for (auto& item : old.MyItems) returned.emplace_back(std::move(item));
		}
		Piece result{.MyUid = ++_MyNextUid, .MyId = _definition.MyGoldenId, .MyPoolCopies = copies, .MyFacing = direction};
		const auto uid = result.MyUid;
		bool stored = false;
		if (tile) { SetBoard(view, *tile, std::move(result)); stored = true; }
		else stored = Stow(view, result);
		if (!stored && fallbackTile) { result.MyFacing = fallbackDirection; SetBoard(view, *fallbackTile, std::move(result)); stored = true; }
		if (!stored && firstHolder)
			if (const auto holder = Locate(view, *firstHolder))
			{
				auto& items = holder->Get(view).MyItems;
				items.emplace(items.begin() + static_cast<std::ptrdiff_t>(std::min(firstItemSlot, items.size())), std::move(result)); stored = true;
			}
		for (auto& item : returned)
		{
			if (Stow(view, item)) continue;
			if (stored)
			{
				auto& items = Locate(view, uid)->Get(view).MyItems;
				if (items.size() < _MyRules.MyEquipmentPerChess) { items.emplace_back(std::move(item)); continue; }
			}
			_events.emplace_back(EconomyEvent{.MyKind = EventKind::DESTROYED, .MyDefinitionId = item.MyId, .MyUid = item.MyUid});
		}
		if (_definition.MyKind == PieceKind::CHESS) CheckItemMerges(_player, _events);
		if (!stored) { if (copies) (void)ReturnCopies(_player, _definition.MyBaseId, copies); return {}; }
		if (_definition.MyKind == PieceKind::CHESS) ++view.MyStatistics.MyChessMerges;
		else ++view.MyStatistics.MyItemMerges;
		_events.emplace_back(EconomyEvent{.MyKind = EventKind::MERGED, .MyDefinitionId = _definition.MyGoldenId, .MyUid = uid});
		if (_definition.MyKind == PieceKind::CHESS)
		{
			const auto location = Locate(view, uid);
			if (location && location->MyOnBoard) GrantTokens(_player, location->Get(view));
			QueueReward(_player);
		}
		if (_MyHooks) _MyHooks->Merged(*this, view.MyPlayerId, _events);
		return uid;
	}

	void EconomySession::CheckItemMerges(Player& _player, std::vector<EconomyEvent>& _events)
	{
		auto& view = _player.MyView;
		std::vector<std::string_view> order; order.reserve(view.MyHand.size() + view.MyTemporary.size() + 36 * _MyRules.MyEquipmentPerChess);
		for (unsigned guard = 0; guard < 20; ++guard)
		{
			order.clear();
			const auto add = [&](const Piece& _piece)
			{
				if (_piece.IsToken()) return;
				const auto& definition = _MyCatalog.At(_piece.MyId);
				if (definition.MyKind == PieceKind::ITEM && definition.MyMergeCount > 1 && std::ranges::find(order, definition.MyId) == order.end()) order.emplace_back(definition.MyId);
			};
			for (const auto& piece : view.MyHand) if (piece) add(*piece);
			for (const auto& piece : view.MyTemporary) if (piece) add(*piece);
			for (const auto& pieces : {std::span<const std::optional<Piece>>(view.MyBoard), std::span<const std::optional<Piece>>(view.MyHand), std::span<const std::optional<Piece>>(view.MyTemporary)})
				for (const auto& piece : pieces) if (piece) for (const auto& item : piece->MyItems) add(item);
			bool merged = false;
			for (const auto id : order)
			{
				const auto& definition = _MyCatalog.At(id);
				if (MergeLocations(view, definition).size() < static_cast<std::size_t>(definition.MyMergeCount)) continue;
				merged = MergePieces(_player, definition, {}, _events).has_value(); break;
			}
			if (!merged) break;
		}
	}

	std::expected<GrantResult, CommandError> EconomySession::GrantPiece(std::string_view _player, std::string_view _definition, GrantOptions _options)
	{
		const auto found = std::ranges::find_if(_MyPlayers, [&](const Player& _p) { return _p.MyView.MyPlayerId == _player; });
		if (found == _MyPlayers.end()) return std::unexpected(CommandError::UNKNOWN_PLAYER);
		if (!found->MyView.MyAlive) return std::unexpected(CommandError::ELIMINATED);
		const auto* definition = _MyCatalog.Find(_definition); if (!definition || !Selected(found->MyView, *definition)) return std::unexpected(CommandError::BAD_TARGET);
		auto next = *this; auto& player = next._MyPlayers[static_cast<std::size_t>(found - _MyPlayers.begin())];
		GrantResult result; result.MyPiece = next.Acquire(player, *definition, result.MyChanges.MyEvents, _options);
		next.LiftOutOfRange(player); next.FillHand(player.MyView);
		if (result.MyPiece)
		{
			result.MyChanges.MyEvents.emplace_back(EconomyEvent{.MyKind = EventKind::GRANTED,
				.MyDefinitionId = next.GrantDefinition(player.MyView, *result.MyPiece, definition->MyId, result.MyChanges.MyEvents), .MyUid = *result.MyPiece});
		}
		result.MyChanges.MyRevision = ++next._MyRevision; Commit(std::move(next)); return result;
	}

	std::optional<CommandError> EconomySession::Equip(Player& _player, const EquipItem& _command, std::vector<EconomyEvent>& _events)
	{
		auto& view = _player.MyView; const auto source = Locate(view, _command.MyItem), target = Locate(view, _command.MyTarget);
		if (!source || !target || source->MyOnBoard || source->MyEquippedIndex || target->MyEquippedIndex || source->Get(view).IsToken() || target->Get(view).IsToken()) return CommandError::BAD_TARGET;
		const auto& itemDef = _MyCatalog.At(source->Get(view).MyId);
		if (itemDef.MyKind != PieceKind::ITEM || itemDef.MyItemUse != ItemUse::EQUIPMENT || _MyCatalog.At(target->Get(view).MyId).MyKind != PieceKind::CHESS) return CommandError::BAD_TARGET;
		auto& items = target->Get(view).MyItems;
		if (_command.MyReplace && std::ranges::find(items, *_command.MyReplace, &Piece::MyUid) == items.end()) return CommandError::BAD_TARGET;
		auto item = Detach(view, *source);
		if (itemDef.MyMergeCount > 1 && MergeLocations(view, itemDef).size() + 1 >= static_cast<std::size_t>(itemDef.MyMergeCount))
		{
			if (MergePieces(_player, itemDef, std::move(item), _events)) { FillHand(view); return {}; }
			return CommandError::BAD_TARGET;
		}
		if (items.size() >= _MyRules.MyEquipmentPerChess)
		{
			const auto old = _command.MyReplace ? std::ranges::find(items, *_command.MyReplace, &Piece::MyUid) : items.begin();
			_events.emplace_back(EconomyEvent{.MyKind = EventKind::DESTROYED, .MyDefinitionId = old->MyId, .MyUid = old->MyUid});
			items.erase(old);
		}
		items.reserve(_MyRules.MyEquipmentPerChess);
		_events.emplace_back(EconomyEvent{.MyKind = EventKind::EQUIPPED, .MyDefinitionId = item.MyId, .MyUid = item.MyUid});
		items.emplace_back(std::move(item)); ++view.MyStatistics.MyItemsEquipped; CheckItemMerges(_player, _events); FillHand(view); return {};
	}
}
