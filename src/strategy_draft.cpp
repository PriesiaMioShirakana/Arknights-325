#include <cmath>
#include <stronghold/domain/strategy_draft.hpp>

namespace Stronghold
{
	StrategyDraft::StrategyDraft(StrategyDraftRules _rules, std::span<const Seat> _players, std::vector<StrategyOption> _options, Random& _random, double _now)
		: _MyRules(std::move(_rules)), _MyOptions(std::move(_options)), _MyNow(_now)
	{
		_MyRules.MyUntimed = _MyRules.MyUntimed || _MyRules.MySolo;
		if (_players.empty() || _MyOptions.empty() || !std::isfinite(_now) || !std::isfinite(_MyRules.MyTurnSeconds) ||
			!std::isgreater(_MyRules.MyTurnSeconds, 0) || !std::isfinite(_now + _MyRules.MyTurnSeconds)) throw std::invalid_argument("invalid draft rules");
		for (std::size_t i = 0; i < _MyOptions.size(); ++i)
		{
			if (_MyOptions[i].MyId.empty() || _MyOptions[i].MyStartingLife <= 0) throw std::invalid_argument("invalid strategy option");
			for (std::size_t j = 0; j < i; ++j) if (_MyOptions[j].MyId == _MyOptions[i].MyId) throw std::invalid_argument("duplicate strategy option");
		}
		if (!Option(_MyRules.MyDefaultId)) throw std::invalid_argument("default strategy unavailable");
		std::vector<Seat> seats(_players.begin(), _players.end());
		std::ranges::stable_sort(seats, {}, &Seat::MySeat);
		_MyPlayers.reserve(seats.size()); _MyOrder.reserve(seats.size());
		for (const auto& seat : seats)
		{
			if (seat.MyPlayerId.empty() || Player(seat.MyPlayerId)) throw std::invalid_argument("invalid draft seat");
			_MyOrder.emplace_back(_MyPlayers.size());
			_MyPlayers.emplace_back(StrategyPick{.MyPlayerId = seat.MyPlayerId, .MySkipsLeft = _MyRules.MySolo ? 0 : _MyRules.MySkips});
		}
		if (!_MyRules.MySolo) _random.Shuffle(_MyOrder.begin(), _MyOrder.end());
		StartTurn();
	}

	const StrategyOption* StrategyDraft::Option(std::string_view _id) const
	{
		const auto found = std::ranges::find(_MyOptions, _id, &StrategyOption::MyId);
		return found == _MyOptions.end() ? nullptr : &*found;
	}

	std::optional<std::size_t> StrategyDraft::Player(std::string_view _id) const
	{
		const auto found = std::ranges::find(_MyPlayers, _id, &StrategyPick::MyPlayerId);
		return found == _MyPlayers.end() ? std::nullopt : std::optional(static_cast<std::size_t>(found - _MyPlayers.begin()));
	}

	bool StrategyDraft::Taken(std::string_view _strategy, std::string_view _player) const
	{
		return std::ranges::any_of(_MyPlayers, [&](const StrategyPick& _pick) { return _pick.MyPlayerId != _player && _pick.MyStrategyId == _strategy; });
	}

	std::string_view StrategyDraft::Default(std::string_view _player) const
	{
		if (!Taken(_MyRules.MyDefaultId, _player)) return _MyRules.MyDefaultId;
		for (const auto& option : _MyOptions) if (!Taken(option.MyId, _player)) return option.MyId;
		// 选项不足的自定义规则仍保留原版最后回退，不因重复值陷入无法结束的状态。
		return _MyRules.MyDefaultId;
	}

	std::string_view StrategyDraft::TimeoutChoice(std::string_view _player) const
	{
		const auto player = Player(_player);
		if (!player) throw std::out_of_range("unknown draft player");
		const auto& focus = _MyPlayers[*player].MyFocus;
		return !focus.empty() && Option(focus) && !Taken(focus, _player) ? std::string_view(focus) : Default(_player);
	}

	std::string_view StrategyDraft::CurrentPlayer() const noexcept
	{ return Complete() ? std::string_view{} : std::string_view(_MyPlayers[_MyOrder[_MyTurn]].MyPlayerId); }

	void StrategyDraft::StartTurn()
	{
		while (_MyTurn < _MyOrder.size() && !_MyPlayers[_MyOrder[_MyTurn]].MyStrategyId.empty()) ++_MyTurn;
		_MyDeadline = Complete() || _MyRules.MyUntimed ? 0 : _MyNow + _MyRules.MyTurnSeconds;
	}

	void StrategyDraft::Apply(std::size_t _player, std::string_view _strategy)
	{
		auto& player = _MyPlayers[_player];
		const auto& option = *Option(_strategy);
		player.MyStrategyId = option.MyId;
		player.MyStartingLife = option.MyStartingLife;
		StartTurn();
	}

	std::expected<void, DraftError> StrategyDraft::Focus(std::string_view _player, std::string_view _strategy)
	{
		if (Complete()) return std::unexpected(DraftError::COMPLETE);
		const auto player = Player(_player);
		if (!player) return std::unexpected(DraftError::UNKNOWN_PLAYER);
		if (!_MyPlayers[*player].MyStrategyId.empty()) return std::unexpected(DraftError::ALREADY_PICKED);
		if (!_strategy.empty() && !Option(_strategy)) return std::unexpected(DraftError::BAD_STRATEGY);
		_MyPlayers[*player].MyFocus = _strategy;
		return {};
	}

	std::expected<void, DraftError> StrategyDraft::Pick(std::string_view _player, std::string_view _strategy)
	{
		if (Complete()) return std::unexpected(DraftError::COMPLETE);
		const auto player = Player(_player);
		if (!player) return std::unexpected(DraftError::UNKNOWN_PLAYER);
		if (!_MyPlayers[*player].MyStrategyId.empty()) return std::unexpected(DraftError::ALREADY_PICKED);
		if (CurrentPlayer() != _player) return std::unexpected(DraftError::NOT_YOUR_TURN);
		if (!Option(_strategy)) return std::unexpected(DraftError::BAD_STRATEGY);
		if (Taken(_strategy, _player)) return std::unexpected(DraftError::TAKEN);
		Apply(*player, _strategy);
		return {};
	}

	std::expected<void, DraftError> StrategyDraft::Skip(std::string_view _player)
	{
		if (Complete()) return std::unexpected(DraftError::COMPLETE);
		const auto player = Player(_player);
		if (!player) return std::unexpected(DraftError::UNKNOWN_PLAYER);
		if (_MyRules.MySolo) return std::unexpected(DraftError::SOLO_SKIP);
		if (!_MyPlayers[*player].MyStrategyId.empty()) return std::unexpected(DraftError::ALREADY_PICKED);
		if (CurrentPlayer() != _player) return std::unexpected(DraftError::NOT_YOUR_TURN);
		if (!_MyPlayers[*player].MySkipsLeft) return std::unexpected(DraftError::NO_SKIP);
		if (_MyOrder.size() - _MyTurn <= 1) return std::unexpected(DraftError::NOBODY_TO_PASS);
		--_MyPlayers[*player].MySkipsLeft;
		// 固定数量座位在既有容量中旋转，无需删除后重新分配。
		std::rotate(_MyOrder.begin() + static_cast<std::ptrdiff_t>(_MyTurn), _MyOrder.begin() + static_cast<std::ptrdiff_t>(_MyTurn + 1), _MyOrder.end());
		StartTurn();
		return {};
	}

	void StrategyDraft::Advance(double _now)
	{
		if (!std::isfinite(_now) || std::isless(_now, _MyNow) || !std::isfinite(_now + _MyRules.MyTurnSeconds)) throw std::invalid_argument("invalid draft time");
		_MyNow = _now;
		if (!Complete() && !_MyRules.MyUntimed && std::isgreaterequal(_now, _MyDeadline))
		{
			const auto index = _MyOrder[_MyTurn];
			Apply(index, TimeoutChoice(_MyPlayers[index].MyPlayerId));
		}
	}

	bool StrategyDraft::AssignDefault(std::string_view _player)
	{
		const auto player = Player(_player);
		if (!player || !_MyPlayers[*player].MyStrategyId.empty()) return false;
		Apply(*player, Default(_player));
		return true;
	}

	void StrategyDraft::Finish()
	{
		for (std::size_t i = 0; i < _MyPlayers.size(); ++i)
			if (_MyPlayers[i].MyStrategyId.empty()) Apply(i, Default(_MyPlayers[i].MyPlayerId));
	}

	StrategyDraftView StrategyDraft::View() const
	{
		StrategyDraftView view{.MyPlayers = _MyPlayers, .MyTurn = _MyTurn, .MyDeadline = _MyDeadline, .MyComplete = Complete()};
		view.MyOrder.reserve(_MyOrder.size());
		for (const auto index : _MyOrder) view.MyOrder.emplace_back(_MyPlayers[index].MyPlayerId);
		return view;
	}
}
