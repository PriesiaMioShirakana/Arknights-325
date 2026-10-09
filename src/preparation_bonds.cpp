#include <cmath>
#include <limits>
#include "preparation_hooks.hpp"

namespace Stronghold
{
	void PreparationContent::BeginPreparation(Random& _random)
	{
		auto economy = _MyEconomy;
		auto players = _MyPlayers;
		auto random = _random;
		PreparationHooks hooks(*this, economy, players, random);
		economy.BeginPreparation();
		_MyEconomy.Commit(std::move(economy)); _MyPlayers.swap(players); _random = random;
	}

	std::optional<BondLayerChange> PreparationContent::AddContentLayers(EconomySession& _economy,
		std::vector<Player>& _players, std::size_t _index, std::string_view _bond, double _count,
		Random& _random, std::vector<EconomyEvent>& _events) const
	{
		const auto change = _players[_index].MyLayers.Add(_bond, _count);
		if (change)
		{
			_economy._MyHooks->RefreshBonds(_economy, _index);
			PreparationHooks::Scope scope(*_economy._MyHooks);
			if (scope.MyEntered) ApplyBonds(_economy, _players, _index, BondEvent::LAYERS, _random, _events, _bond);
		}
		return change;
	}

	std::optional<BondLayerChange> PreparationContent::AddLayers(std::string_view _player, std::string_view _bond,
		double _count, Random& _random)
	{
		const auto found = std::ranges::find(_MyPlayers, _player, &Player::MyId);
		if (found == _MyPlayers.end()) return {};
		const auto index = static_cast<std::size_t>(found - _MyPlayers.begin());
		auto economy = _MyEconomy;
		auto players = _MyPlayers;
		auto random = _random;
		PreparationHooks hooks(*this, economy, players, random);
		std::vector<EconomyEvent> events;
		const auto result = AddContentLayers(economy, players, index, _bond, _count, random, events);
		if (result) ++economy._MyRevision;
		_MyEconomy.Commit(std::move(economy)); _MyPlayers.swap(players); _random = random;
		return result;
	}

	std::expected<bool, CommandError> PreparationContent::SynchronizeLayers(std::string_view _player, int _round,
		std::span<const BondNumber> _gains, Random& _random)
	{
		const auto found = std::ranges::find(_MyPlayers, _player, &Player::MyId);
		if (found == _MyPlayers.end()) return std::unexpected(CommandError::UNKNOWN_PLAYER);
		if (_round <= 0 || _round != _MyEconomy.Round() || _round != _MyItemRound) return std::unexpected(CommandError::STALE_ROUND);
		if (_MyEconomy.Phase() != PreparationPhase::CLOSED) return std::unexpected(CommandError::WRONG_PHASE);
		const auto index = static_cast<std::size_t>(found - _MyPlayers.begin());
		if (!_MyEconomy._MyPlayers[index].MyView.MyAlive) return std::unexpected(CommandError::ELIMINATED);
		const auto entries = found->MyLayers.Entries();
		const bool pending = std::ranges::any_of(_gains, [&](const BondNumber& _gain)
		{
			const auto entry = std::ranges::find(entries, _gain.MyId, &BondLayerProgress::MyId);
			return entry != entries.end() && std::isfinite(_gain.MyValue) && std::floor(_gain.MyValue) > entry->MyCredited;
		});
		if (!pending) return false;
		auto economy = _MyEconomy;
		auto players = _MyPlayers;
		auto random = _random;
		PreparationHooks hooks(*this, economy, players, random);
		std::vector<EconomyEvent> events;
		const auto result = players[index].MyLayers.Synchronize(static_cast<unsigned>(_round), _gains, [&](const BondLayerChange& _change)
		{
			PreparationHooks::Scope scope(hooks);
			if (scope.MyEntered) ApplyBonds(economy, players, index, BondEvent::LAYERS, random, events, _change.MyId);
		});
		++economy._MyRevision;
		_MyEconomy.Commit(std::move(economy)); _MyPlayers.swap(players); _random = random;
		return result;
	}

	void PreparationContent::ApplyBonds(EconomySession& _economy, std::vector<Player>& _players, std::size_t _index,
		BondEvent _event, Random& _random, std::vector<EconomyEvent>& _events, std::string_view _bond) const
	{
		if (_MyBondEffects.empty()) return;
		auto& player = _players[_index];
		auto& owned = _economy._MyPlayers[_index];
		auto& view = owned.MyView;
		for (std::size_t i = 0; i < _MyBondEffects.size(); ++i)
		{
			const auto& effect = _MyBondEffects[i];
			if (_event == BondEvent::LAYERS && effect.MyBond != _bond) continue;
			const auto states = ItemBondStates(_economy, player, _index);
			const auto state = std::ranges::find(states, effect.MyBond, &BondState::MyId);
			if (state == states.end() || !state->MyActive) continue;
			const auto entry = std::ranges::find(player.MyLayers.Entries(), effect.MyBond, &BondLayerProgress::MyId);
			const auto layers = entry == player.MyLayers.Entries().end() ? 0 : entry->MyLayers;
			auto& paid = player.MyBondCounters[i]; // 固定大小；嵌套赠送前推进水位，回调后重新读取。
			if (effect.MyKind == PreparationBondKind::REFRESH_CHANCE)
			{
				if (_event == BondEvent::REFRESH && view.MyFreeRefreshes == 0 &&
					_random.Next() < std::clamp(effect.MyBaseChance + effect.MyChancePerLayer * layers, 0.0, 1.0))
					++view.MyFreeRefreshes;
				continue;
			}
			if (_event == BondEvent::REFRESH) continue;
			switch (effect.MyKind)
			{
			case PreparationBondKind::PREP_LAYERS:
				if (_event == BondEvent::PREP_END)
					for (const auto& bond : states)
						if (bond.MyActive) (void)AddContentLayers(_economy, _players, _index, bond.MyId,
							static_cast<double>(state->MyTier >= 2 ? effect.MyHighCount : effect.MyCount), _random, _events);
				break;
			case PreparationBondKind::DISCOUNT:
				paid = std::max<std::int64_t>(paid, layers >= static_cast<double>(effect.MyHighStep) ? 2 : layers >= static_cast<double>(effect.MyStep) ? 1 : 0);
				break;
			case PreparationBondKind::COIN_MILESTONE:
			{
				const auto due = static_cast<std::int64_t>(std::floor(layers / static_cast<double>(effect.MyStep)));
				if (due <= paid) break;
				if (effect.MyCount && due - paid > std::numeric_limits<std::int64_t>::max() / effect.MyCount)
					throw std::overflow_error("bond funds overflow");
				const auto gain = (due - paid) * effect.MyCount; paid = due;
				if (_economy.Phase() == PreparationPhase::PREPARING && player.MyPrepEndedRound == _economy.Round())
				{
					if (view.MyPendingFunds > std::numeric_limits<std::int64_t>::max() - gain) throw std::overflow_error("bond pending funds overflow");
					view.MyPendingFunds += gain;
				}
				else EconomySession::AddFunds(view, gain);
				break;
			}
			case PreparationBondKind::ITEM_MILESTONE:
			{
				const auto due = static_cast<std::int64_t>(std::floor(layers / static_cast<double>(effect.MyStep)));
				for (unsigned guard = 0; guard < 100 && paid < due; ++guard)
				{
					++paid;
					for (std::int64_t j = 0; j < effect.MyCount; ++j)
					{
						const auto id = RollContentItem(_economy._MyCatalog, _MyPools, _random, {.MyPool = effect.MyPool});
						if (!id) continue;
						const auto& definition = _economy._MyCatalog.At(*id);
						const auto uid = _economy.Acquire(owned, definition, _events);
						_economy.LiftOutOfRange(owned); _economy.FillHand(view);
						if (uid) _events.emplace_back(EconomyEvent{.MyKind = EventKind::GRANTED,
							.MyDefinitionId = _economy.GrantDefinition(view, *uid, definition.MyId, _events), .MyUid = *uid});
					}
				}
				break;
			}
			case PreparationBondKind::REFRESH_CHANCE: break;
			}
		}
	}
}
