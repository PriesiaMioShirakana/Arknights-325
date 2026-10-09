#include <stronghold/domain/final_assault.hpp>

namespace Stronghold
{
	namespace
	{
		constexpr std::size_t MaximumExperimentalPlayers = 20;
		constexpr double MaximumExactInteger = 9007199254740991.0;
		constexpr std::array<double, 3> BossHitSteps{0.2, 0.5, 0.8};

		double Positive(std::optional<double> _mode, std::optional<double> _global, double _fallback)
		{
			for (const auto value : {_mode, _global})
				if (value && std::isfinite(*value) && std::isgreater(*value, 0)) return *value;
			return _fallback;
		}

		bool Enlarged(bool _solo, std::size_t _aliveCount, bool _experiment) noexcept
		{ return _experiment && !_solo && _aliveCount > 4 && _aliveCount <= MaximumExperimentalPlayers; }

		void ValidateRules(const FinalAssaultRules& _rules)
		{
			for (const auto value : {_rules.MyHiddenSoloLayers, _rules.MyHiddenCoopLayers, _rules.MyHiddenMinimumLife,
				_rules.MyGameSecondsPerRealSecond, _rules.MyOvertimeAfterRealSeconds, _rules.MyOvertimeDrainPerRealSecond})
				if (!std::isfinite(value)) throw std::invalid_argument("non-finite final assault rule");
			if (!std::isgreater(_rules.MyGameSecondsPerRealSecond, 0) || std::isless(_rules.MyOvertimeAfterRealSeconds, 0) ||
				std::isless(_rules.MyOvertimeDrainPerRealSecond, 0)) throw std::invalid_argument("invalid boss clock rule");
		}
	}

	double BossPoolShare(const BossHealthScale& _mode, const BossHealthScale& _global, bool _solo, std::optional<double> _aliveCount)
	{
		if (_solo) return Positive(_mode.MySolo, _global.MySolo, 1);
		const double full = std::max(1.0, std::floor(Positive(_mode.MyFullTeam, _global.MyFullTeam, 4)));
		const double alive = _aliveCount && std::isfinite(*_aliveCount) && std::isgreaterequal(*_aliveCount, 1) ? std::min(full, std::floor(*_aliveCount)) : full;
		const double coop = Positive(_mode.MyCoop, _global.MyCoop, 1);
		return coop * (_mode.MyPerPlayer.value_or(_global.MyPerPlayer.value_or(true)) ? alive :
			_mode.MyAliveScaling.value_or(_global.MyAliveScaling.value_or(false)) ? alive / full : 1);
	}

	double BossPoolHealth(double _base, double _share, double _tuning, bool _solo, std::size_t _aliveCount, bool _capacityExperiment)
	{
		const double share = Enlarged(_solo, _aliveCount, _capacityExperiment) ? static_cast<double>(_aliveCount) / 4 : _share;
		const double health = _base * share * _tuning;
		if (!std::isfinite(health)) throw std::invalid_argument("non-finite boss health");
		return std::max(1.0, std::floor(health + 0.5));
	}

	double BossOvertimeDue(const FinalAssaultRules& _rules, double _gameSeconds)
	{
		ValidateRules(_rules);
		if (!std::isfinite(_gameSeconds) || std::isless(_gameSeconds, 0)) throw std::invalid_argument("invalid boss clock");
		const double over = _gameSeconds / _rules.MyGameSecondsPerRealSecond - _rules.MyOvertimeAfterRealSeconds;
		const double due = std::isgreaterequal(over, 1) ? std::floor(over) * _rules.MyOvertimeDrainPerRealSecond : 0;
		if (!std::isfinite(due)) throw std::overflow_error("boss overtime overflow");
		return due;
	}

	bool HiddenCoreEligible(const FinalAssaultRules& _rules, double _layers, double _teamLife, std::size_t _aliveCount) noexcept
	{
		if (!_rules.MyHasHiddenRound || !_rules.MyHiddenDifficultyAllowed) return false;
		const double scale = Enlarged(_rules.MySolo, _aliveCount, _rules.MyCapacityExperiment) ? static_cast<double>(_aliveCount) / 4 : 1;
		return std::isgreater(_layers, _rules.MySolo ? _rules.MyHiddenSoloLayers : _rules.MyHiddenCoopLayers * scale) &&
			std::isgreater(_teamLife, _rules.MyHiddenMinimumLife);
	}

	FinalAssault::FinalAssault(FinalAssaultRules _rules, std::span<const BossParticipant> _players, double _bossHealth)
		: _MyRules(_rules), _MyPool(_bossHealth)
	{
		ValidateRules(_rules);
		if (_players.empty()) throw std::invalid_argument("final assault needs living players");
		_MyPlayers.reserve(_players.size()); _MyRemainders.reserve(_players.size());
		_MyFields.reserve((_players.size() + 1) / 2); _MyHits.reserve(_players.size() * BossHitSteps.size());
		for (const auto& player : _players)
		{
			if (player.MyPlayerId.empty() || player.MyLife < 0 || !std::isfinite(player.MyActivatedLayers) || std::isless(player.MyActivatedLayers, 0) ||
				std::ranges::find(_MyPlayers, player.MyPlayerId, &BossPlayerProgress::MyPlayerId) != _MyPlayers.end()) throw std::invalid_argument("invalid boss participant");
			_MyPlayers.emplace_back(BossPlayerProgress{.MyPlayerId = player.MyPlayerId, .MySeat = player.MySeat, .MyInitialLife = player.MyLife, .MyLife = player.MyLife});
			_MyInitialLife += static_cast<double>(player.MyLife); _MyLayers += player.MyActivatedLayers;
			if (std::isgreater(_MyInitialLife, MaximumExactInteger) || !std::isfinite(_MyLayers)) throw std::overflow_error("boss participant total overflow");
			_MyPool.PreparePlayer(player.MyPlayerId);
		}
		std::ranges::stable_sort(_MyPlayers, {}, &BossPlayerProgress::MySeat);
		_MyTeamLife = _MyInitialLife;
		for (std::size_t i = 0; i < _MyPlayers.size(); i += 2)
		{
			const unsigned count = i + 1 < _MyPlayers.size() ? 2U : 1U;
			_MyFields.emplace_back(BossFieldGroup{.MyPlayers = {_MyPlayers[i].MyPlayerId, count == 2 ? _MyPlayers[i + 1].MyPlayerId : std::string{}},
				.MyCount = count, .MySoloTemplate = _rules.MySolo || count == 1});
		}
		Observe();
	}

	double FinalAssault::Damage(std::string_view _player, double _amount)
	{
		Observe();
		if (_MyOutcome != BossOutcome::ACTIVE) return 0;
		const double dealt = _MyPool.Damage(_player, _amount);
		Observe();
		return dealt;
	}

	void FinalAssault::Observe()
	{
		if (_MyOutcome != BossOutcome::ACTIVE) return;
		for (auto& player : _MyPlayers)
		{
			const double credited = _MyPool.Credits().at(player.MyPlayerId);
			player.MyTotalDamage += credited - player.MyRoundDamage;
			player.MyRoundDamage = credited;
			unsigned reached = player.MyHitStep;
			for (std::size_t i = reached; i < BossHitSteps.size(); ++i)
				if (std::isgreaterequal(credited / _MyPool.MaxHealth(), BossHitSteps[i])) reached = static_cast<unsigned>(i + 1);
			// 一次跳过多个阈值只播最高档；每关重新计数，整局伤害另行累计。
			if (reached > player.MyHitStep)
			{
				player.MyHitStep = reached;
				_MyHits.emplace_back(BossHit{.MyPlayerId = player.MyPlayerId, .MyThreshold = BossHitSteps[reached - 1]});
			}
		}
		if (!std::isgreater(_MyPool.Health(), 0)) _MyOutcome = BossOutcome::VICTORY;
		else if (!std::isgreater(_MyTeamLife, 0)) _MyOutcome = BossOutcome::DEFEAT;
		if (_MyOutcome != BossOutcome::ACTIVE) _MyPool.Seal();
	}

	void FinalAssault::SyncLife()
	{
		const auto total = static_cast<std::int64_t>(std::floor(_MyTeamLife + 0.5));
		std::int64_t left = total;
		_MyRemainders.clear();
		for (std::size_t i = 0; i < _MyPlayers.size(); ++i)
		{
			auto& player = _MyPlayers[i];
			const double exact = std::isgreater(_MyInitialLife, 0) ? static_cast<double>(total) * static_cast<double>(player.MyInitialLife) / _MyInitialLife :
				static_cast<double>(total) / static_cast<double>(_MyPlayers.size());
			player.MyLife = static_cast<std::int64_t>(std::floor(exact + 1e-9));
			left -= player.MyLife;
			_MyRemainders.emplace_back(Remainder{.MyPlayer = i, .MyFraction = exact - static_cast<double>(player.MyLife)});
		}
		std::ranges::sort(_MyRemainders, [](const Remainder& _a, const Remainder& _b)
		{
			if (std::islessgreater(_a.MyFraction, _b.MyFraction)) return std::isgreater(_a.MyFraction, _b.MyFraction);
			return _a.MyPlayer < _b.MyPlayer; // 玩家已按座位稳定排序，同余数时先低座位。
		});
		for (const auto& row : _MyRemainders)
		{
			if (left <= 0) break;
			++_MyPlayers[row.MyPlayer].MyLife; --left;
		}
	}

	void FinalAssault::LoseLife(double _amount)
	{
		Observe();
		if (_MyOutcome != BossOutcome::ACTIVE || !std::isfinite(_amount) || !std::isgreater(_amount, 0)) return;
		_MyTeamLife = std::max(0.0, _MyTeamLife - _amount);
		SyncLife(); Observe();
	}

	void FinalAssault::Advance(double _gameSeconds)
	{
		if (!std::isfinite(_gameSeconds) || std::isless(_gameSeconds, _MyGameSeconds)) throw std::invalid_argument("boss clock must be monotonic");
		const double due = BossOvertimeDue(_MyRules, _gameSeconds);
		_MyGameSeconds = _gameSeconds;
		Observe();
		if (_MyOutcome != BossOutcome::ACTIVE || !std::isgreater(due, _MyOvertimeApplied)) return;
		const double loss = due - _MyOvertimeApplied;
		_MyOvertimeApplied = due; LoseLife(loss);
	}

	bool FinalAssault::CanEnterHidden() const noexcept
	{ return !_MyHidden && _MyOutcome == BossOutcome::VICTORY && HiddenCoreEligible(_MyRules, _MyLayers, _MyTeamLife, _MyPlayers.size()); }

	void FinalAssault::Conclude()
	{
		Observe();
		if (_MyOutcome == BossOutcome::ACTIVE) { _MyOutcome = BossOutcome::DEFEAT; _MyPool.Seal(); }
	}

	void FinalAssault::BeginHidden(double _bossHealth)
	{
		if (!CanEnterHidden()) throw std::logic_error("hidden core is not eligible");
		SharedBossPool next(_bossHealth);
		for (const auto& player : _MyPlayers) next.PreparePlayer(player.MyPlayerId);
		_MyPool = std::move(next);
		for (auto& player : _MyPlayers) { player.MyRoundDamage = 0; player.MyHitStep = 0; }
		_MyHits.clear(); _MyHidden = true; _MyOutcome = BossOutcome::ACTIVE; _MyGameSeconds = 0; _MyOvertimeApplied = 0;
	}

	FinalAssaultResult FinalAssault::Result() const
	{
		if (_MyOutcome == BossOutcome::ACTIVE) throw std::logic_error("boss fight is still active");
		// 隐藏关失败不撤销已经完成最终攻势的整局胜利。
		return FinalAssaultResult{.MyVictory = _MyHidden || _MyOutcome == BossOutcome::VICTORY,
			.MyHiddenReached = _MyHidden, .MyHiddenCleared = _MyHidden && _MyOutcome == BossOutcome::VICTORY};
	}
}
