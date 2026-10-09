#include <stronghold/domain/preparation.hpp>

namespace Stronghold
{
	std::expected<void, CommandError> EconomySession::ConfigureSummons(std::string_view _player, const SummonCatalog& _catalog)
	{
		const auto found = std::ranges::find_if(_MyPlayers, [&](const Player& _value) { return _value.MyView.MyPlayerId == _player; });
		if (found == _MyPlayers.end()) return std::unexpected(CommandError::UNKNOWN_PLAYER);
		if (_MyPhase != PreparationPhase::IDLE || _MyNextUid) return std::unexpected(CommandError::WRONG_PHASE);
		for (const auto& owner : _catalog.Owners())
		{
			const auto* definition = _MyCatalog.Find(owner.MyId);
			if (!definition || definition->MyKind != PieceKind::CHESS) return std::unexpected(CommandError::BAD_TARGET);
		}
		for (const auto& token : _catalog.Tokens())
			if (_MyCatalog.Find(token.MyId)) return std::unexpected(CommandError::BAD_TARGET);
		found->MySummons = std::cref(_catalog);
		++_MyRevision;
		return {};
	}

	void EconomySession::ClearBoard(PlayerView& _view, std::size_t _index)
	{
		_view.MyBoard.at(_index).reset();
		const auto end = _view.MyBoardOrder.begin() + static_cast<std::ptrdiff_t>(_view.MyBoardOrderSize);
		const auto found = std::find(_view.MyBoardOrder.begin(), end, _index);
		if (found != end) { std::move(found + 1, end, found); --_view.MyBoardOrderSize; }
	}

	void EconomySession::SetBoard(PlayerView& _view, std::size_t _index, Piece _piece)
	{
		if (!_view.MyBoard.at(_index)) _view.MyBoardOrder.at(_view.MyBoardOrderSize++) = _index;
		_piece.MyTemporaryDue.reset();
		_view.MyBoard[_index] = std::move(_piece);
	}

	std::optional<bool> EconomySession::InOwnerRange(
		const Player& _player,
		const Piece& _token,
		BoardPosition _position,
		std::optional<OwnerPosition> _owner
	) const
	{
		if (!_player.MySummons) return {};
		if (!_owner)
			for (std::size_t i = 0; i < _player.MyView.MyBoard.size(); ++i)
			{
				const auto& piece = _player.MyView.MyBoard[i];
				if (piece && !piece->IsToken() && piece->MyUid == _token.MyOwnerUid)
				{ _owner.emplace(OwnerPosition{.MyPiece = *piece, .MyPosition = BoardPosition::FromIndex(i), .MyFacing = piece->MyFacing}); break; }
			}
		if (!_owner) return {};
		const auto* definition = _player.MySummons->get().Owner(_owner->MyPiece.MyId);
		if (!definition) return {};
		for (const auto& offset : definition->MyRange)
		{
			// 先提升再旋转，允许外部静态目录使用任意 int 偏移，避免取负或相加溢出。
			const auto row = static_cast<std::int64_t>(offset.MyRow), col = static_cast<std::int64_t>(offset.MyColumn);
			std::int64_t dr = row, dc = col;
			switch (_owner->MyFacing)
			{
			case Facing::RIGHT: break;
			case Facing::UP: dr = col; dc = -row; break;
			case Facing::LEFT: dr = -row; dc = -col; break;
			case Facing::DOWN: dr = -col; dc = row; break;
			}
			if (_owner->MyPosition.MyRow + dr == _position.MyRow && _owner->MyPosition.MyColumn + dc == _position.MyColumn) return true;
		}
		return false;
	}

	bool EconomySession::Legal(const Player& _player, const Piece& _piece, BoardPosition _position, std::optional<OwnerPosition> _owner) const
	{
		if (!_piece.IsToken()) return _player.MyView.MyLayout.CanPlace(PlacementOf(_player.MyView, _MyCatalog.At(_piece.MyId)), _position);
		const auto* definition = _player.MySummons ? _player.MySummons->get().Token(_piece.MyId) : nullptr;
		if (!definition || !_player.MyView.MyLayout.CanPlace(definition->MyPlacement, _position)) return false;
		const auto inside = InOwnerRange(_player, _piece, _position, _owner);
		return !inside || ((!definition->MyInsideOwnerRange || *inside) && (!definition->MyOutsideOwnerRange || !*inside));
	}

	bool EconomySession::ReturnToken(PlayerView& _view, Piece& _token, std::optional<std::size_t> _preferred, bool _allowTemporary) const
	{
		for (const auto slots : {std::span(_view.MyHand), std::span(_view.MyTemporary)})
			for (auto& piece : slots)
				if (piece && piece->IsToken() && piece->MyOwnerUid == _token.MyOwnerUid && piece->MyId == _token.MyId)
				{ piece->MyCount += _token.MyCount; return true; }
		if (_preferred && !_view.MyHand[*_preferred])
		{ _token.MyTemporaryDue.reset(); _view.MyHand[*_preferred] = std::move(_token); return true; }
		if (_allowTemporary) return Stow(_view, _token);
		for (auto i = _view.MyHand.size(); i > 0; --i)
			if (!_view.MyHand[i - 1]) { _token.MyTemporaryDue.reset(); _view.MyHand[i - 1] = std::move(_token); return true; }
		return false;
	}

	void EconomySession::GrantTokens(Player& _player, const Piece& _owner)
	{
		if (!_player.MySummons || _owner.IsToken()) return;
		const auto* owner = _player.MySummons->get().Owner(_owner.MyId);
		if (!owner) return;
		auto& view = _player.MyView;
		for (const auto& allowance : owner->MyTokens)
		{
			std::uint64_t held = 0;
			Piece* stack = nullptr;
			for (const auto slots : {std::span(view.MyHand), std::span(view.MyTemporary)})
				for (auto& piece : slots)
					if (piece && piece->IsToken() && piece->MyOwnerUid == _owner.MyUid && piece->MyId == allowance.MyId)
					{ held += piece->MyCount; if (!stack) stack = &*piece; }
			for (const auto& piece : view.MyBoard)
				if (piece && piece->IsToken() && piece->MyOwnerUid == _owner.MyUid && piece->MyId == allowance.MyId) held += piece->MyCount;
			if (held >= allowance.MyCount) continue;
			const auto missing = static_cast<unsigned>(allowance.MyCount - held);
			if (stack) { stack->MyCount += missing; continue; }
			Piece token{.MyUid = ++_MyNextUid, .MyId = allowance.MyId, .MyOwnerUid = _owner.MyUid, .MyCount = missing};
			(void)Stow(view, token); // 满仓时本轮无法发放；下轮依据实际剩余数重新补发。
		}
	}

	void EconomySession::RemoveTokens(PlayerView& _view, PieceUid _owner)
	{
		for (std::size_t i = 0; i < _view.MyBoard.size(); ++i)
			if (_view.MyBoard[i] && _view.MyBoard[i]->MyOwnerUid == _owner) ClearBoard(_view, i);
		for (const auto slots : {std::span(_view.MyHand), std::span(_view.MyTemporary)})
			for (auto& piece : slots) if (piece && piece->MyOwnerUid == _owner) piece.reset();
	}

	void EconomySession::LiftTokens(Player& _player, PieceUid _owner, PieceUid _keep)
	{
		auto& view = _player.MyView;
		const auto order = view.MyBoardOrder;
		const auto count = view.MyBoardOrderSize;
		for (std::size_t n = 0; n < count; ++n)
		{
			const auto i = order[n]; auto& piece = view.MyBoard[i];
			if (!piece || piece->MyOwnerUid != _owner || piece->MyUid == _keep) continue;
			auto token = std::move(*piece); ClearBoard(view, i);
			if (!ReturnToken(view, token)) SetBoard(view, i, std::move(token));
		}
	}

	void EconomySession::LiftOutOfRange(Player& _player)
	{
		if (!_player.MySummons) return;
		auto& view = _player.MyView;
		const auto order = view.MyBoardOrder;
		const auto count = view.MyBoardOrderSize;
		for (std::size_t n = 0; n < count; ++n)
		{
			const auto i = order[n]; auto& piece = view.MyBoard[i];
			if (!piece || !piece->IsToken()) continue;
			const auto* definition = _player.MySummons->get().Token(piece->MyId);
			const auto inside = InOwnerRange(_player, *piece, BoardPosition::FromIndex(i));
			if (!definition || !inside || ((!definition->MyInsideOwnerRange || *inside) && (!definition->MyOutsideOwnerRange || !*inside))) continue;
			auto token = std::move(*piece); ClearBoard(view, i);
			(void)ReturnToken(view, token); // 范围外不能保留；丢失的堆叠在下轮补发，不扣拥有者库存。
		}
	}

	bool EconomySession::RoomForReorient(const Player& _player, const Piece& _owner, BoardPosition _position, Facing _facing) const
	{
		if (!_player.MySummons) return true;
		const auto& view = _player.MyView;
		std::array<std::string_view, 36> needs{};
		std::size_t count = 0;
		for (std::size_t i = 0; i < view.MyBoard.size(); ++i)
		{
			const auto& piece = view.MyBoard[i];
			if (!piece || piece->MyOwnerUid != _owner.MyUid) continue;
			const auto* definition = _player.MySummons->get().Token(piece->MyId);
			const auto inside = InOwnerRange(_player, *piece, BoardPosition::FromIndex(i), OwnerPosition{.MyPiece = _owner, .MyPosition = _position, .MyFacing = _facing});
			if (!definition || !inside || ((!definition->MyInsideOwnerRange || *inside) && (!definition->MyOutsideOwnerRange || !*inside))) continue;
			const auto stack = [&](const auto& _slot) { return _slot && _slot->MyOwnerUid == _owner.MyUid && _slot->MyId == piece->MyId; };
			if (std::ranges::any_of(view.MyHand, stack) || std::ranges::any_of(view.MyTemporary, stack)) continue;
			if (!std::ranges::contains(std::span(needs).first(count), piece->MyId)) needs[count++] = piece->MyId;
		}
		const auto empty = [](const auto& _piece) { return !_piece; };
		return count <= static_cast<std::size_t>(std::ranges::count_if(view.MyHand, empty) + std::ranges::count_if(view.MyTemporary, empty));
	}
}
