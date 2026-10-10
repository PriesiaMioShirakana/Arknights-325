#include <stronghold/domain/special_draft.hpp>

namespace Stronghold
{
	SpecialDraft::SpecialDraft(SpecialDraftRules _rules, std::span<const Seat> _players, std::vector<ChoiceCard> _cards, Random& _random, double _now)
		: _MyRules(_rules), _MyCards(std::move(_cards)), _MyNow(_now)
	{
		_MyRules.MyUntimed = _MyRules.MyUntimed || _MyRules.MySolo;
		if (!std::isfinite(_now) || !std::isfinite(_rules.MyFirstTurnSeconds) || !std::isfinite(_rules.MyTurnSeconds) ||
			!std::isgreater(_rules.MyFirstTurnSeconds, 0) || !std::isgreater(_rules.MyTurnSeconds, 0) ||
			!std::isfinite(_now + std::max(_rules.MyFirstTurnSeconds, _rules.MyTurnSeconds)) || _MyCards.size() > UINT32_MAX)
			throw std::invalid_argument("invalid special draft configuration");
		std::vector<Seat> seats(_players.begin(), _players.end());
		std::ranges::stable_sort(seats, {}, &Seat::MySeat);
		_MyPlayers.reserve(seats.size()); _MyOrder.reserve(seats.size());
		_MyTaken.resize(_MyCards.size()); _MyAvailable.reserve(_MyCards.size());
		for (const auto& seat : seats)
		{
			if (seat.MyPlayerId.empty() || Player(seat.MyPlayerId)) throw std::invalid_argument("invalid special draft player");
			_MyOrder.emplace_back(_MyPlayers.size()); _MyPlayers.emplace_back(ChoicePlayer{.MyPlayerId = seat.MyPlayerId});
		}
		if (!seats.empty() && !_MyCards.empty() && !_MyRules.MySolo) _random.Shuffle(_MyOrder.begin(), _MyOrder.end());
		StartTurn();
	}

	std::optional<std::size_t> SpecialDraft::Player(std::string_view _id) const
	{
		const auto found = std::ranges::find(_MyPlayers, _id, &ChoicePlayer::MyPlayerId);
		return found == _MyPlayers.end() ? std::nullopt : std::optional(static_cast<std::size_t>(found - _MyPlayers.begin()));
	}

	void SpecialDraft::StartTurn()
	{
		while (_MyTurn < _MyOrder.size())
		{
			const auto& player = _MyPlayers[_MyOrder[_MyTurn]];
			if (player.MyAlive && !player.MyCard) break;
			++_MyTurn;
		}
		if (_MyTurn == _MyOrder.size() || std::ranges::all_of(_MyTaken, [](const auto& _owner) { return _owner.has_value(); })) { Finish(); return; }
		_MyDeadline = _MyRules.MyUntimed ? 0 : _MyNow + (_MyTurn == 0 ? _MyRules.MyFirstTurnSeconds : _MyRules.MyTurnSeconds);
	}

	std::string_view SpecialDraft::CurrentPlayer() const noexcept
	{ return _MyComplete ? std::string_view{} : std::string_view(_MyPlayers[_MyOrder[_MyTurn]].MyPlayerId); }

	ChoiceAward SpecialDraft::Apply(std::size_t _player, std::size_t _card)
	{
		_MyPlayers[_player].MyCard = _card; _MyTaken[_card] = _player; StartTurn();
		return ChoiceAward{.MyPlayer = _player, .MyCard = _card};
	}

	std::expected<ChoiceAward, ChoiceDraftError> SpecialDraft::Pick(std::string_view _player, std::size_t _card)
	{
		if (_MyComplete) return std::unexpected(ChoiceDraftError::COMPLETE);
		const auto player = Player(_player);
		if (!player) return std::unexpected(ChoiceDraftError::UNKNOWN_PLAYER);
		if (!_MyPlayers[*player].MyAlive) return std::unexpected(ChoiceDraftError::ELIMINATED);
		if (_MyPlayers[*player].MyCard) return std::unexpected(ChoiceDraftError::ALREADY_PICKED);
		if (CurrentPlayer() != _player) return std::unexpected(ChoiceDraftError::NOT_YOUR_TURN);
		if (_card >= _MyCards.size()) return std::unexpected(ChoiceDraftError::BAD_CARD);
		if (_MyTaken[_card]) return std::unexpected(ChoiceDraftError::TAKEN);
		return Apply(*player, _card);
	}

	std::optional<ChoiceAward> SpecialDraft::Advance(double _now, Random& _random)
	{
		if (!std::isfinite(_now) || std::isless(_now, _MyNow) || !std::isfinite(_now + std::max(_MyRules.MyFirstTurnSeconds, _MyRules.MyTurnSeconds)))
			throw std::invalid_argument("invalid special draft time");
		_MyNow = _now;
		if (_MyComplete || _MyRules.MyUntimed || std::isless(_now, _MyDeadline)) return std::nullopt;
		_MyAvailable.clear();
		for (std::size_t i = 0; i < _MyTaken.size(); ++i) if (!_MyTaken[i]) _MyAvailable.emplace_back(i);
		// 只在真正发生超时选择时消耗一次随机数，包括只余一张卡的情况。
		const auto card = _MyAvailable[_random.Index(static_cast<std::uint32_t>(_MyAvailable.size()))];
		return Apply(_MyOrder[_MyTurn], card);
	}

	bool SpecialDraft::Eliminate(std::string_view _player)
	{
		const auto player = Player(_player);
		if (!player || !_MyPlayers[*player].MyAlive) return false;
		_MyPlayers[*player].MyAlive = false;
		if (!_MyComplete) StartTurn();
		return true;
	}

	void SpecialDraft::Finish() noexcept { _MyComplete = true; _MyDeadline = 0; }
}
