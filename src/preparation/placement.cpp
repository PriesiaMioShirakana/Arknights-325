#include <stronghold/domain/preparation.hpp>

namespace Stronghold
{
	std::expected<ChangeSet, CommandError> EconomySession::SetBoardLayout(std::string_view _player, BoardLayout _layout)
	{
		const auto found = std::ranges::find_if(_MyPlayers, [&](const Player& _value) { return _value.MyView.MyPlayerId == _player; });
		if (found == _MyPlayers.end()) return std::unexpected(CommandError::UNKNOWN_PLAYER);
		if (!found->MyView.MyAlive) return std::unexpected(CommandError::ELIMINATED);
		for (const auto tile : _layout.MyTiles)
			if (tile != Terrain::GROUND && tile != Terrain::HIGH && tile != Terrain::BLOCKED) return std::unexpected(CommandError::BAD_TILE);
		auto next = *this;
		auto& player = next._MyPlayers[static_cast<std::size_t>(found - _MyPlayers.begin())];
		auto& view = player.MyView;
		view.MyLayout = _layout;
		ChangeSet changes;
		// 原版先回收干员，再回收召唤物；一个干员成功撤回后，它的全部召唤物立即移除。
		for (const bool tokens : {false, true})
			for (std::size_t i = 0; i < view.MyBoard.size(); ++i)
			{
				auto& slot = view.MyBoard[i];
				if (!slot || slot->IsToken() != tokens || next.Legal(player, *slot, BoardPosition::FromIndex(i))) continue;
				auto piece = std::move(*slot); next.ClearBoard(view, i);
				const auto uid = piece.MyUid; const auto id = piece.MyId;
				if (!(tokens ? next.ReturnToken(view, piece) : next.Stow(view, piece)))
				{ next.SetBoard(view, i, std::move(piece)); continue; }
				if (!tokens) next.RemoveTokens(view, uid);
				changes.MyEvents.emplace_back(EconomyEvent{.MyKind = EventKind::MOVED, .MyDefinitionId = id, .MyUid = uid});
			}
		next.LiftOutOfRange(player);
		next.FillHand(view);
		changes.MyRevision = ++next._MyRevision;
		Commit(std::move(next)); return changes;
	}

	std::optional<EconomySession::PieceLocation> EconomySession::Locate(const PlayerView& _view, PieceUid _uid)
	{
		const auto scan = [&](const auto& _pieces, bool _board, bool _temporary) -> std::optional<PieceLocation>
		{
			for (std::size_t i = 0; i < _pieces.size(); ++i)
			{
				const auto& piece = _pieces[i]; if (!piece) continue;
				if (piece->MyUid == _uid) return PieceLocation{.MyOnBoard = _board, .MyIndex = i, .MyInTemporary = _temporary};
				for (std::size_t j = 0; j < piece->MyItems.size(); ++j)
					if (piece->MyItems[j].MyUid == _uid) return PieceLocation{.MyOnBoard = _board, .MyIndex = i, .MyInTemporary = _temporary, .MyEquippedIndex = j};
			}
			return {};
		};
		if (const auto found = scan(_view.MyHand, false, false)) return found;
		if (const auto found = scan(_view.MyTemporary, false, true)) return found;
		if (const auto found = scan(_view.MyBoard, true, false)) return found;
		return std::nullopt;
	}

	std::optional<CommandError> EconomySession::Move(Player& _player, const MoveToBoard& _command)
	{
		auto& view = _player.MyView;
		const auto location = Locate(view, _command.MyUid);
		if (!location || location->MyEquippedIndex) return CommandError::BAD_TARGET;
		auto& source = location->Slot(view);
		if ((!source->IsToken() && _MyCatalog.At(source->MyId).MyKind != PieceKind::CHESS) ||
			(_command.MyFacing != Facing::UP && _command.MyFacing != Facing::RIGHT && _command.MyFacing != Facing::DOWN && _command.MyFacing != Facing::LEFT))
			return CommandError::BAD_TARGET;
		if (!_command.MyPosition.InField()) return CommandError::BAD_TILE;
		const auto index = _command.MyPosition.Index();
		auto& target = view.MyBoard[index];
		std::optional<OwnerPosition> ownerAfter;
		if (source->IsToken() && location->MyOnBoard && target && target->MyUid == source->MyOwnerUid)
			ownerAfter.emplace(OwnerPosition{.MyPiece = *target, .MyPosition = BoardPosition::FromIndex(location->MyIndex), .MyFacing = target->MyFacing});
		if (!Legal(_player, *source, _command.MyPosition, ownerAfter)) return CommandError::BAD_TILE;
		if (location->MyOnBoard && location->MyIndex == index)
		{
			if (source->MyFacing == _command.MyFacing) return {};
			if (!source->IsToken() && !RoomForReorient(_player, *source, _command.MyPosition, _command.MyFacing)) return CommandError::HAND_FULL;
			source->MyFacing = _command.MyFacing;
			return {};
		}
		const auto ownerUid = source->MyUid;
		const auto token = source->IsToken();
		if (location->MyOnBoard)
		{
			if (target && !(target->IsToken() && !token && target->MyOwnerUid == ownerUid) &&
				!Legal(_player, *target, BoardPosition::FromIndex(location->MyIndex))) return CommandError::BAD_TILE;
			const auto swappedOwner = target && !target->IsToken() ? target->MyUid : 0;
			source->MyFacing = _command.MyFacing;
			if (target) std::swap(source, target); // 两个已有键互换值，保留各自的插入次序。
			else
			{
				auto piece = std::move(*source); ClearBoard(view, location->MyIndex); SetBoard(view, index, std::move(piece));
			}
			if (swappedOwner) LiftTokens(_player, swappedOwner, token ? ownerUid : 0);
			if (!token) LiftTokens(_player, ownerUid);
			return {};
		}
		if (token)
		{
			if (target) return CommandError::BAD_TILE;
			if (!std::ranges::any_of(view.MyBoard, [&](const auto& _piece) { return _piece && _piece->MyUid == source->MyOwnerUid; }))
				return CommandError::BAD_TARGET;
			if (source->MyCount > 1)
			{
				--source->MyCount;
				SetBoard(view, index, Piece{.MyUid = ++_MyNextUid, .MyId = source->MyId, .MyFacing = _command.MyFacing, .MyOwnerUid = source->MyOwnerUid});
			}
			else
			{
				auto piece = Detach(view, *location); piece.MyFacing = _command.MyFacing; SetBoard(view, index, std::move(piece));
			}
			return {};
		}
		if ((!target || target->IsToken()) && view.DeployCount() >= view.MyDeployCap) return CommandError::BOARD_FULL;
		auto piece = Detach(view, *location);
		if (target)
		{
			auto displaced = std::move(*target); ClearBoard(view, index);
			if (displaced.IsToken()) (void)ReturnToken(view, displaced, !location->MyInTemporary ? std::optional(location->MyIndex) : std::nullopt);
			else
			{
				RemoveTokens(view, displaced.MyUid);
				if (location->MyInTemporary) displaced.MyTemporaryDue = view.MyPreparationsEnded;
				location->Slot(view) = std::move(displaced);
			}
		}
		piece.MyFacing = _command.MyFacing; SetBoard(view, index, std::move(piece));
		GrantTokens(_player, *view.MyBoard[index]);
		return {};
	}

	std::optional<CommandError> EconomySession::Move(Player& _player, const MoveToHand& _command)
	{
		auto& view = _player.MyView;
		const auto location = Locate(view, _command.MyUid);
		if (!location || location->MyEquippedIndex || _command.MySlot >= view.MyHand.size()) return CommandError::BAD_TARGET;
		auto& source = location->Slot(view);
		if (!location->MyOnBoard && !location->MyInTemporary && location->MyIndex == _command.MySlot) return {};
		auto& target = view.MyHand[_command.MySlot];
		if (!location->MyOnBoard)
		{
			std::swap(source, target); target->MyTemporaryDue.reset();
			if (source && location->MyInTemporary) source->MyTemporaryDue = view.MyPreparationsEnded;
			return {};
		}
		if (source->IsToken())
		{
			auto piece = Detach(view, *location);
			if (ReturnToken(view, piece, !target ? std::optional(_command.MySlot) : std::nullopt, false)) return {};
			// 原版先删棋盘键，撤回失败再插回；位置不变，但这个键移动到遍历顺序的末尾。
			SetBoard(view, location->MyIndex, std::move(piece));
			return CommandError::HAND_FULL;
		}
		const auto ownStack = [&](const auto& _piece) { return _piece && _piece->IsToken() && _piece->MyOwnerUid == source->MyUid; };
		if (!target || ownStack(target))
		{
			auto piece = Detach(view, *location); RemoveTokens(view, piece.MyUid); target = std::move(piece); return {};
		}
		if (!target->IsToken() && _MyCatalog.At(target->MyId).MyKind == PieceKind::CHESS)
		{
			if (!Legal(_player, *target, BoardPosition::FromIndex(location->MyIndex))) return CommandError::BAD_TILE;
			target->MyFacing = source->MyFacing;
			std::swap(source, target);
			RemoveTokens(view, target->MyUid); GrantTokens(_player, *source);
			return {};
		}
		std::optional<std::size_t> destination;
		for (auto i = view.MyHand.size(); i > 0; --i) if (!view.MyHand[i - 1]) { destination = i - 1; break; }
		if (!destination)
			for (std::size_t i = 0; i < view.MyHand.size(); ++i) if (ownStack(view.MyHand[i])) { destination = i; break; }
		if (!destination) return CommandError::HAND_FULL;
		auto piece = Detach(view, *location); RemoveTokens(view, piece.MyUid); view.MyHand[*destination] = std::move(piece);
		return {};
	}
} // namespace Stronghold
