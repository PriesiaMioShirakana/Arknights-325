#include <stronghold/domain/unite.hpp>

namespace Stronghold
{
	namespace
	{
		bool CountedLeak(const BattlePlayerState& _result)
		{ return std::ranges::any_of(_result.MyLeaks, &LeakedEnemy::MyCounted); }

		bool Perfect(const UniteParticipant& _player)
		{ return _player.MyResult && _player.MyResult->get().MyPerfect && !CountedLeak(_player.MyResult->get()); }

		bool KnownEnemy(std::span<const WaveEnemyParameters> _enemies, std::string_view _id)
		{ return std::ranges::any_of(_enemies, [&](const WaveEnemyParameters& _enemy) { return _enemy.MyId == _id; }); }

		void AddLoss(std::vector<UniteLoss>& _losses, std::string_view _id, std::size_t _count = 1)
		{
			const auto it = std::ranges::find(_losses, _id, &UniteLoss::MyPlayerId);
			if (it == _losses.end()) _losses.emplace_back(UniteLoss{.MyPlayerId = std::string(_id), .MyCount = _count});
			else it->MyCount += _count;
		}

		// 卡片仅奖励自身指定的本体；带 bountyId 的衍生敌人不能通过额外赏金回退重新获奖。
		std::int64_t BountyCoins(const UniteParticipant* _player, const LeakedEnemy& _leak)
		{
			if (!_leak.MyModifiers) return 0;
			const auto& mods = *_leak.MyModifiers;
			if (!mods.MyBountyId.empty())
			{
				if (!_player) return 0;
				const auto it = std::ranges::find(_player->MyBounties, mods.MyBountyId, &WaveBounty::MyId);
				return it != _player->MyBounties.end() && !it->MyPerfect && it->MyEnemyId == _leak.MyEnemyId ? std::max(std::int64_t{0}, it->MyCoins) : 0;
			}
			return std::max(std::int64_t{0}, mods.MyBountyCoins.value_or(0));
		}

		WaveLeak CopyLeak(const LeakedEnemy& _leak, std::string_view _owner, const UniteParticipant* _player)
		{
			return WaveLeak{.MyEnemyId = _leak.MyEnemyId, .MySourcePlayer = std::string(_owner), .MyModifiers = _leak.MyModifiers,
				.MyBounty = _leak.MyTag == EnemySpawnTag::BOUNTY, .MyBountyId = _leak.MyModifiers ? _leak.MyModifiers->MyBountyId : "",
				.MyCoins = BountyCoins(_player, _leak), .MyRewardOwner = std::string(_owner)};
		}
	}

	UniteMetrics MeasureUniteHelper(const UniteParticipant& _player)
	{
		UniteMetrics metrics{.MyUnits = _player.MyDeployCount, .MyStanding = _player.MyDeployCount};
		for (const auto& bond : _player.MyBonds)
		{
			if (!bond.MyActive) continue;
			metrics.MyActiveBond = true;
			// 原实现用非零持久化层数优先，否则读取当前盟约快照。新增层数先向下取整再封顶 999。
			const double stored = bond.MyStoredLayers && std::islessgreater(*bond.MyStoredLayers, 0) ? *bond.MyStoredLayers : bond.MyLayers;
			const double gain = std::isfinite(bond.MyPendingGain) && std::isgreater(bond.MyPendingGain, 0) ?
				std::min(std::max(0.0, 999 - stored), std::floor(bond.MyPendingGain)) : 0;
			metrics.MyLayers += stored + gain;
		}
		if (_player.MyResult && !_player.MyResult->get().MyUnitsEnd.empty())
		{
			metrics.MyStanding = 0;
			for (const auto& unit : _player.MyResult->get().MyUnitsEnd)
				if (unit.MyAlive && std::ranges::find(_player.MyOperatorUids, unit.MyPieceUid) != _player.MyOperatorUids.end()) ++metrics.MyStanding;
		}
		return metrics;
	}

	std::vector<std::string> SelectUniteHelpers(std::span<const UniteParticipant> _players, std::size_t _limit)
	{
		struct Candidate { std::size_t MyIndex{}; UniteMetrics MyMetrics{}; };
		std::vector<Candidate> candidates; candidates.reserve(_players.size());
		for (std::size_t i = 0; i < _players.size(); ++i) candidates.emplace_back(Candidate{.MyIndex = i, .MyMetrics = MeasureUniteHelper(_players[i])});
		const auto before = [&](const Candidate& _left, const Candidate& _right, bool _layers)
		{
			const auto& left = _left.MyMetrics; const auto& right = _right.MyMetrics;
			if (left.MyUnits != right.MyUnits) return left.MyUnits > right.MyUnits;
			if (left.MyActiveBond != right.MyActiveBond) return left.MyActiveBond;
			if (_layers && std::islessgreater(left.MyLayers, right.MyLayers)) return std::isgreater(left.MyLayers, right.MyLayers);
			if (left.MyStanding != right.MyStanding) return left.MyStanding > right.MyStanding;
			return _players[_left.MyIndex].MySeat < _players[_right.MyIndex].MySeat;
		};
		std::ranges::stable_sort(candidates, [&](const Candidate& _a, const Candidate& _b) { return before(_a, _b, false); });
		candidates.resize(std::min(_limit, candidates.size()));
		std::ranges::stable_sort(candidates, [&](const Candidate& _a, const Candidate& _b) { return before(_a, _b, true); });
		std::vector<std::string> helpers; helpers.reserve(candidates.size());
		for (const auto& candidate : candidates) helpers.emplace_back(_players[candidate.MyIndex].MyPlayerId);
		return helpers;
	}

	std::optional<UnitePlan> PlanUnite(const UniteRules& _rules, std::span<const UniteParticipant> _players, std::span<const WaveEnemyParameters> _enemies)
	{
		if (_rules.MySolo) return std::nullopt;
		if (!_rules.MyMaxHelpers || _rules.MyMaxHelpers > 2) throw std::invalid_argument("unite requires one or two helpers");
		UnitePlan plan;
		std::vector<UniteParticipant> perfects; perfects.reserve(_players.size());
		std::size_t alive = 0;
		for (const auto& player : _players)
		{
			if (!player.MyAlive) continue;
			++alive;
			if (!player.MyResult) continue;
			const auto& result = player.MyResult->get();
			if (!CountedLeak(result)) { if (result.MyPerfect) perfects.emplace_back(player); continue; }
			plan.MyLeakers.emplace_back(player.MyPlayerId);
			for (const auto& leak : result.MyLeaks)
			{
				if (!leak.MyCounted) continue;
				if (!KnownEnemy(_enemies, leak.MyEnemyId)) { AddLoss(plan.MyNotReentered, player.MyPlayerId); continue; }
				plan.MyLeaks.emplace_back(CopyLeak(leak, player.MyPlayerId, &player));
			}
		}
		if (plan.MyLeakers.empty() || perfects.empty()) return std::nullopt;
		plan.MyHelpers = SelectUniteHelpers(perfects, _rules.MyMaxHelpers);
		if (_rules.MyCapacityExperiment && _rules.MyNormalAliveCount >= 8 && alive >= 8)
		{
			plan.MyRelayRound = 1;
			for (const auto& player : perfects)
				if (!player.MySynthetic && std::ranges::find(plan.MyHelpers, player.MyPlayerId) == plan.MyHelpers.end()) plan.MyRelayCandidates.emplace_back(player.MyPlayerId);
		}
		return plan;
	}

	std::optional<UnitePlan> PlanUniteRelay(const UnitePlan& _previous, const BattleResult& _result, bool _synthetic,
		std::span<const UniteParticipant> _players, std::span<const EnemySpawn> _fieldSpawns, std::span<const WaveEnemyParameters> _enemies, std::size_t _maxHelpers)
	{
		if (_previous.MyRelayRound != 1 || _synthetic) return std::nullopt;
		std::vector<UniteParticipant> candidates; candidates.reserve(_previous.MyRelayCandidates.size());
		for (const auto& id : _previous.MyRelayCandidates)
		{
			const auto it = std::ranges::find(_players, id, &UniteParticipant::MyPlayerId);
			if (it != _players.end() && it->MyAlive && !it->MyLeft && !it->MySynthetic && Perfect(*it) &&
				std::ranges::find(_previous.MyHelpers, id) == _previous.MyHelpers.end()) candidates.emplace_back(*it);
		}
		if (candidates.empty()) return std::nullopt;
		UnitePlan plan{.MyHelpers = SelectUniteHelpers(candidates, std::min(std::size_t{2}, _maxHelpers)), .MyLeakers = _previous.MyLeakers,
			.MyNotReentered = _previous.MyNotReentered, .MyRelayRound = 2};
		std::vector<bool> consumed(_fieldSpawns.size());
		const auto add = [&](LeakedEnemy _leak, const std::optional<BountyReward>& _reward)
		{
			if (!_leak.MyCounted || std::ranges::find(plan.MyLeakers, _leak.MySourcePlayer) == plan.MyLeakers.end()) return;
			if (!KnownEnemy(_enemies, _leak.MyEnemyId)) { AddLoss(plan.MyNotReentered, _leak.MySourcePlayer); return; }
			const auto player = std::ranges::find(_players, _leak.MySourcePlayer, &UniteParticipant::MyPlayerId);
			auto copy = CopyLeak(_leak, _leak.MySourcePlayer, player != _players.end() ? &*player : nullptr);
			if (_reward)
			{
				if (!std::isfinite(_reward->MyCoins) || !std::isless(_reward->MyCoins, static_cast<double>(std::numeric_limits<std::int64_t>::max())) || std::isless(_reward->MyCoins, 0))
					throw std::invalid_argument("unite bounty exceeds integer currency range");
				copy.MyCoins = static_cast<std::int64_t>(std::trunc(_reward->MyCoins));
				copy.MyRewardOwner = _reward->MyOwnerId;
			}
			plan.MyLeaks.emplace_back(std::move(copy));
		};
		for (const auto& player : _result.MyPlayers) for (const auto& leak : player.MyLeaks) add(leak, std::nullopt);
		for (const auto& pending : _result.MyPendingEnemies)
		{
			std::optional<std::size_t> fallback, exact;
			for (std::size_t i = 0; i < _fieldSpawns.size(); ++i)
			{
				const auto& spawn = _fieldSpawns[i];
				if (consumed[i] || spawn.MyDefinition.MyId != pending.MyEnemyId || spawn.MySourcePlayer != pending.MySourcePlayer) continue;
				if (!fallback) fallback = i;
				if (std::isless(std::abs(spawn.MyTime - pending.MyTime), 1e-6)) { exact = i; break; }
			}
			const auto found = exact ? exact : fallback;
			const auto* source = found ? &_fieldSpawns[*found] : nullptr;
			if (found) consumed[*found] = true;
			add(LeakedEnemy{.MyEnemyId = pending.MyEnemyId, .MyModifiers = source ? source->MyModifiers : std::nullopt,
				.MySourcePlayer = pending.MySourcePlayer, .MyTag = pending.MyTag, .MyCounted = true}, source ? source->MyBounty : std::nullopt);
		}
		if (plan.MyLeaks.empty()) return std::nullopt;
		return plan;
	}

	std::vector<UniteLoss> UniteSurvivors(const UnitePlan& _plan, const BattleResult& _result)
	{
		auto output = _plan.MyNotReentered;
		for (const auto& player : _result.MyPlayers)
			for (const auto& leak : player.MyLeaks)
				if (leak.MyCounted && !leak.MySourcePlayer.empty()) AddLoss(output, leak.MySourcePlayer);
		// 未出生也算联防未击倒：不能像本场战斗的 total 那样直接扣掉。
		for (const auto& pending : _result.MyPendingEnemies)
			if (!pending.MySourcePlayer.empty()) AddLoss(output, pending.MySourcePlayer);
		return output;
	}

	void ApplyUniteCarry(BattlePlayerInput& _input, const BattlePlayerState& _previous)
	{
		for (auto& unit : _input.MyUnits)
		{
			unit.MyCarry.reset();
			const auto it = std::ranges::find(_previous.MyUnitsEnd, unit.MyPieceUid, &UnitEndState::MyPieceUid);
			if (it == _previous.MyUnitsEnd.end()) continue;
			const double sp = std::isfinite(it->MySp) ? std::max(0.0, it->MySp) : 0;
			if (unit.MyKind == UnitKind::TOKEN) { if (it->MyAlive) unit.MyCarry = CarryState{.MySp = sp}; continue; }
			if (!it->MyAlive) { unit.MyCarry = CarryState{.MyDown = true}; continue; }
			unit.MyCarry = CarryState{.MyHealthRatio = std::isfinite(it->MyHealthRatio) ? std::clamp(it->MyHealthRatio, 0.01, 1.0) : 1, .MySp = sp};
		}
	}
}
