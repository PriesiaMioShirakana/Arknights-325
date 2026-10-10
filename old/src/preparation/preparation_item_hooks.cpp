#include <limits>
#include "preparation_hooks.hpp"

namespace Stronghold
{
	std::vector<std::pair<PieceUid, PieceUid>> PreparationContent::EquippedItems(
		const EconomySession& _economy, std::size_t _player) const
	{
		const auto& view = _economy._MyPlayers[_player].MyView;
		std::vector<std::pair<PieceUid, PieceUid>> result;
		std::size_t count = 0;
		const auto countItems = [&](const auto& _slots)
		{
			for (const auto& piece : _slots) if (piece && !piece->IsToken()) count += piece->MyItems.size();
		};
		countItems(view.MyBoard); countItems(view.MyHand); countItems(view.MyTemporary);
		result.reserve(count);
		const auto add = [&](const auto& _slots)
		{
			for (const auto& piece : _slots)
				if (piece && !piece->IsToken())
					for (const auto& item : piece->MyItems) result.emplace_back(item.MyUid, piece->MyUid);
		};
		add(view.MyBoard); add(view.MyHand); add(view.MyTemporary);
		return result;
	}

	std::vector<BondState> PreparationContent::ItemBondStates(const EconomySession& _economy,
		const Player& _player, std::size_t _index) const
	{
		if (_economy._MyHooks && !_economy._MyHooks->MyBondStates.empty()) return _economy._MyHooks->MyBondStates[_index];
		return ComputeItemBondStates(_economy, _player, _index);
	}

	std::vector<BondState> PreparationContent::ComputeItemBondStates(const EconomySession& _economy,
		const Player& _player, std::size_t _index) const
	{
		const auto& view = _economy._MyPlayers[_index].MyView;
		std::vector<std::vector<std::string_view>> bonds;
		bonds.reserve(view.MyBoard.size() + view.MyHand.size());
		std::vector<BondMember> board, hand;
		board.reserve(view.MyBoard.size()); hand.reserve(view.MyHand.size());
		const auto add = [&](const auto& _slots, std::vector<BondMember>& _members)
		{
			for (const auto& piece : _slots)
			{
				if (!piece || piece->IsToken()) continue;
				const auto& definition = _economy._MyCatalog.At(piece->MyId);
				if (definition.MyKind != PieceKind::CHESS) continue;
				bonds.emplace_back(ItemBonds(_player, *piece));
				_members.emplace_back(BondMember{.MyBaseId = definition.MyBaseId,
					.MyGolden = definition.MyGolden, .MyBonds = bonds.back()});
			}
		};
		add(view.MyBoard, board); add(view.MyHand, hand);
		std::vector<BondNumber> layers;
		layers.reserve(_player.MyLayers.Entries().size());
		for (const auto& layer : _player.MyLayers.Entries()) layers.emplace_back(BondNumber{layer.MyId, layer.MyLayers});
		BondCalculator calculator(_MyBonds);
		const auto result = calculator.Compute(board, hand, layers, {}, _player.MyInactiveBonds);
		return {result.MyEnabled.begin(), result.MyEnabled.end()};
	}

	void PreparationContent::OnItemGain(EconomySession& _economy, std::vector<Player>& _players,
		std::string_view _player, PieceUid _uid, std::vector<EconomyEvent>& _events) const
	{
		if (_MyItems.empty()) return;
		const auto found = std::ranges::find(_players, _player, &Player::MyId);
		const auto index = static_cast<std::size_t>(found - _players.begin());
		auto& player = _players[index];
		auto& view = _economy._MyPlayers[index].MyView;
		if (!_economy.Locate(view, _uid)) return;
		const auto pairs = EquippedItems(_economy, index);
		std::optional<std::vector<BondState>> states;
		for (const auto& [item, holder] : pairs)
		{
			const auto location = _economy.Locate(view, item);
			if (!location || !location->MyEquippedIndex || location->Slot(view)->MyUid != holder) continue;
			const auto* rule = ItemRule(location->Get(view).MyId);
			if (!rule) continue;
			for (const auto& effect : rule->MyEffects)
			{
				if (effect.MyKind != PreparationItemKind::CAULDRON) continue;
				const auto& owner = *location->Slot(view);
				const bool companion = std::ranges::any_of(owner.MyItems, [&](const Piece& _item)
				{
					if (_item.MyUid == item) return false;
					auto id = std::string_view(_item.MyId);
					if (id.ends_with("_a") || id.ends_with("_b")) id.remove_suffix(2);
					return std::ranges::contains(effect.MyOtherItems, id);
				});
				if (!companion) continue;
				const auto bonds = ItemBonds(player, owner);
				bool member = std::ranges::contains(bonds, effect.MyBond);
				if (!member && std::ranges::contains(bonds, std::string_view("maniShip")))
				{
					const auto bond = std::ranges::find(_MyBonds, effect.MyBond, &BondRule::MyId);
					if (bond != _MyBonds.end() && bond->MyCore)
					{
						if (!states) states = ItemBondStates(_economy, player, index);
						const auto active = [&](std::string_view _bond)
						{
							const auto state = std::ranges::find(*states, _bond, &BondState::MyId);
							return state != states->end() && state->MyActive;
						};
						member = active(effect.MyBond) && active("maniShip");
					}
				}
				if (!member) continue;
				auto counter = std::ranges::find(player.MyItemCounters, item, &ItemCounter::MyUid);
				if (counter == player.MyItemCounters.end()) counter = player.MyItemCounters.insert(counter, ItemCounter{.MyUid = item});
				if (counter->MyGainRound != _economy.Round()) { counter->MyGainRound = _economy.Round(); counter->MyGains = 0; }
				if (counter->MyGains >= static_cast<std::uint64_t>(effect.MyMaximum)) continue;
				++counter->MyGains;
				EconomySession::AddFunds(view, effect.MyCount);
				_events.emplace_back(EconomyEvent{.MyKind = EventKind::ECONOMY_EFFECT,
					.MyDefinitionId = std::string(rule->MyId), .MyUid = item, .MyAmount = effect.MyCount});
			}
		}
	}

	void PreparationContent::OnItemSold(EconomySession& _economy, std::vector<Player>& _players,
		std::string_view _player, Random& _random, std::vector<EconomyEvent>& _events) const
	{
		if (_MyItems.empty()) return;
		const auto found = std::ranges::find(_players, _player, &Player::MyId);
		const auto index = static_cast<std::size_t>(found - _players.begin());
		auto& player = _players[index];
		auto& view = _economy._MyPlayers[index].MyView;
		const auto pairs = EquippedItems(_economy, index);
		for (const auto& [item, holder] : pairs)
		{
			const auto location = _economy.Locate(view, item);
			if (!location || !location->MyEquippedIndex || location->Slot(view)->MyUid != holder) continue;
			const auto* rule = ItemRule(location->Get(view).MyId);
			if (!rule) continue;
			for (const auto& effect : rule->MyEffects)
			{
				if (effect.MyKind != PreparationItemKind::SELL_BONUS) continue;
				auto counter = std::ranges::find(player.MyItemCounters, item, &ItemCounter::MyUid);
				if (counter == player.MyItemCounters.end()) counter = player.MyItemCounters.insert(counter, ItemCounter{.MyUid = item});
				if (++counter->MySells < static_cast<std::uint64_t>(std::max<std::int64_t>(1, effect.MyCount))) continue;
				counter->MySells = 0;
				const auto bonds = ItemBonds(player, *location->Slot(view));
				std::vector<std::string_view> members;
				members.reserve(player.MyRoster.size());
				for (const auto& member : player.MyRoster)
					if (std::ranges::any_of(member.MyBonds, [&](auto _bond) { return std::ranges::contains(bonds, _bond); }))
						members.emplace_back(member.MyId);
				RollOptions options;
				options.MyIncluded = members; options.MyMaxTier = view.MyLevel;
				if (const auto id = _economy.RollChess(player.MyId, _random, options)) (void)GrantItemChess(_economy, index, *id, _events);
			}
		}
	}

	std::expected<ChangeSet, CommandError> PreparationContent::ExecuteEconomy(const CommandEnvelope& _command, Random& _random)
	{
		if (!std::holds_alternative<Buy>(_command.MyCommand) && !std::holds_alternative<PickReward>(_command.MyCommand) &&
			!std::holds_alternative<Sell>(_command.MyCommand) && !std::holds_alternative<Refresh>(_command.MyCommand) &&
			!std::holds_alternative<LevelUp>(_command.MyCommand)) return _MyEconomy.Execute(_command);
		const auto player = ItemPlayer(_command);
		if (!player) return std::unexpected(player.error());
		auto economy = _MyEconomy;
		auto players = _MyPlayers;
		auto random = _random;
		PreparationHooks hooks(*this, economy, players, random);
		auto& owned = economy._MyPlayers[*player];
		ChangeSet changes;
		if (const auto error = economy.Apply(owned, _command.MyCommand, changes.MyEvents)) return std::unexpected(*error);
		economy.LiftOutOfRange(owned); economy.FillHand(owned.MyView);
		changes.MyRevision = ++economy._MyRevision;
		_MyEconomy.Commit(std::move(economy)); _MyPlayers.swap(players); _random = random;
		return changes;
	}

	std::expected<GrantResult, CommandError> PreparationContent::GrantPiece(std::string_view _player,
		std::string_view _definition, Random& _random, GrantOptions _options)
	{
		const auto found = std::ranges::find(_MyPlayers, _player, &Player::MyId);
		if (found == _MyPlayers.end()) return std::unexpected(CommandError::UNKNOWN_PLAYER);
		const auto index = static_cast<std::size_t>(found - _MyPlayers.begin());
		const auto& view = _MyEconomy._MyPlayers[index].MyView;
		if (!view.MyAlive) return std::unexpected(CommandError::ELIMINATED);
		const auto* definition = _MyEconomy._MyCatalog.Find(_definition);
		if (!definition || !EconomySession::Selected(view, *definition)) return std::unexpected(CommandError::BAD_TARGET);
		auto economy = _MyEconomy;
		auto players = _MyPlayers;
		auto random = _random;
		PreparationHooks hooks(*this, economy, players, random);
		auto& owned = economy._MyPlayers[index];
		GrantResult result;
		result.MyPiece = economy.Acquire(owned, *definition, result.MyChanges.MyEvents, _options);
		economy.LiftOutOfRange(owned); economy.FillHand(owned.MyView);
		if (result.MyPiece)
			result.MyChanges.MyEvents.emplace_back(EconomyEvent{.MyKind = EventKind::GRANTED,
				.MyDefinitionId = economy.GrantDefinition(owned.MyView, *result.MyPiece, definition->MyId, result.MyChanges.MyEvents), .MyUid = *result.MyPiece});
		result.MyChanges.MyRevision = ++economy._MyRevision;
		_MyEconomy.Commit(std::move(economy)); _MyPlayers.swap(players); _random = random;
		return result;
	}

	std::expected<GrantResult, CommandError> PreparationContent::ApplyInventoryEffect(std::string_view _player,
		const InventoryEffect& _effect, Random& _random)
	{
		const auto found = std::ranges::find(_MyPlayers, _player, &Player::MyId);
		if (found == _MyPlayers.end()) return std::unexpected(CommandError::UNKNOWN_PLAYER);
		const auto index = static_cast<std::size_t>(found - _MyPlayers.begin());
		if (!_MyEconomy._MyPlayers[index].MyView.MyAlive) return std::unexpected(CommandError::ELIMINATED);
		auto economy = _MyEconomy;
		auto players = _MyPlayers;
		auto random = _random;
		PreparationHooks hooks(*this, economy, players, random);
		auto& owned = economy._MyPlayers[index];
		GrantResult result;
		if (const auto error = economy.ApplyInventoryEffect(owned, _effect, result)) return std::unexpected(*error);
		economy.LiftOutOfRange(owned); economy.FillHand(owned.MyView);
		result.MyChanges.MyRevision = ++economy._MyRevision;
		_MyEconomy.Commit(std::move(economy)); _MyPlayers.swap(players); _random = random;
		return result;
	}

	std::expected<ChangeSet, CommandError> PreparationContent::OnBattleResult(std::string_view _player, int _round, Random& _random)
	{
		const auto found = std::ranges::find(_MyPlayers, _player, &Player::MyId);
		if (found == _MyPlayers.end()) return std::unexpected(CommandError::UNKNOWN_PLAYER);
		const auto index = static_cast<std::size_t>(found - _MyPlayers.begin());
		if (!_MyEconomy._MyPlayers[index].MyView.MyAlive) return std::unexpected(CommandError::ELIMINATED);
		if (_round != _MyEconomy.Round() || _round <= found->MyBattleResultRound) return std::unexpected(CommandError::STALE_ROUND);
		if (_MyEconomy.Phase() != PreparationPhase::CLOSED) return std::unexpected(CommandError::WRONG_PHASE);
		auto economy = _MyEconomy;
		auto players = _MyPlayers;
		auto random = _random;
		PreparationHooks hooks(*this, economy, players, random);
		auto& owned = economy._MyPlayers[index];
		auto& view = owned.MyView;
		ChangeSet changes;
		const auto pairs = EquippedItems(economy, index);
		for (const auto& [item, holder] : pairs)
		{
			const auto location = economy.Locate(view, item);
			if (!location || !location->MyEquippedIndex || location->Slot(view)->MyUid != holder) continue;
			const auto* rule = ItemRule(location->Get(view).MyId);
			if (!rule) continue;
			for (const auto& effect : rule->MyEffects)
			{
				if (effect.MyKind != PreparationItemKind::TRANSFORM) continue;
				const auto tier = economy._MyCatalog.At(location->Slot(view)->MyId).MyTier;
				RollOptions options; options.MyExactTier = std::min(6, tier + 1);
				if (const auto id = economy.RollChess(_player, random, options))
				{
					GrantResult result;
					if (const auto error = economy.ApplyInventoryEffect(owned, TransformChess{holder, *id}, result))
						return std::unexpected(*error);
					for (auto& event : result.MyChanges.MyEvents) changes.MyEvents.emplace_back(std::move(event));
					economy.LiftOutOfRange(owned); economy.FillHand(view);
				}
			}
		}
		players[index].MyBattleResultRound = _round;
		changes.MyRevision = ++economy._MyRevision;
		_MyEconomy.Commit(std::move(economy)); _MyPlayers.swap(players); _random = random;
		return changes;
	}
}
