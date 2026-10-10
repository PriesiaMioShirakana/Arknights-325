#include <limits>
#include <stronghold/domain/preparation_content.hpp>
#include "preparation_hooks.hpp"

namespace Stronghold
{
	std::expected<std::size_t, CommandError> PreparationContent::ItemPlayer(const CommandEnvelope& _command) const
	{
		const auto found = std::ranges::find(_MyPlayers, _command.MyPlayerId, &Player::MyId);
		if (found == _MyPlayers.end()) return std::unexpected(CommandError::UNKNOWN_PLAYER);
		const auto index = static_cast<std::size_t>(found - _MyPlayers.begin());
		const auto& view = _MyEconomy._MyPlayers[index].MyView;
		if (!view.MyAlive) return std::unexpected(CommandError::ELIMINATED);
		if (_command.MyRound != _MyEconomy.Round()) return std::unexpected(CommandError::STALE_ROUND);
		if (_MyEconomy.Phase() != PreparationPhase::PREPARING) return std::unexpected(CommandError::WRONG_PHASE);
		if (view.MyReady) return std::unexpected(CommandError::READY);
		return index;
	}

	std::optional<CommandError> PreparationContent::ApplyArt(EconomySession& _economy, RoundLedger& _ledger,
		std::size_t _player, const UseArt& _command, const PreparationItemRule& _rule,
		Random& _random, std::vector<EconomyEvent>& _events) const
	{
		auto& owned = _economy._MyPlayers[_player];
		auto& view = owned.MyView;
		for (const auto& effect : _rule.MyEffects)
		{
			if (effect.MyKind == PreparationItemKind::DESTROY_PASS) continue;
			if (effect.MyKind == PreparationItemKind::COPY_ART)
			{
				std::optional<PieceUid> target;
				const auto select = [&](RangeOffset _offset)
				{
					const auto offset = RotateOffset(_offset, _command.MyFacing);
					const auto row = static_cast<std::int64_t>(_command.MyPosition.MyRow) + offset.MyRow;
					const auto column = static_cast<std::int64_t>(_command.MyPosition.MyColumn) + offset.MyColumn;
					if (target || row < 9 || row > 12 || column < 2 || column > 10) return;
					const auto& piece = view.MyBoard[BoardPosition{static_cast<int>(row), static_cast<int>(column)}.Index()];
					if (piece && !piece->IsToken() && _economy._MyCatalog.At(piece->MyId).MyKind == PieceKind::CHESS)
						target = piece->MyUid;
				};
				if (_rule.MyRange.empty()) select({});
				else for (const auto offset : _rule.MyRange) select(offset);
				if (!target) return CommandError::BAD_TARGET;
				const auto& definition = _economy._MyCatalog.At(_economy.Locate(view, *target)->Get(view).MyId);
				// requirePool=false 仅放宽库存下限，仍尽量扣取现有副本；并非 fromPool=false。
				const auto copy = _economy.Acquire(owned, definition, _events);
				_economy.LiftOutOfRange(owned); _economy.FillHand(view);
				if (!copy) return CommandError::HAND_FULL;
				_events.emplace_back(EconomyEvent{.MyKind = EventKind::GRANTED,
					.MyDefinitionId = _economy.GrantDefinition(view, *copy, definition.MyId, _events), .MyUid = *copy});
				// 复制干员本身已可能合成，原携带者的装备此时已经回收，不再额外复制。
				std::vector<std::string> items;
				if (const auto source = _economy.Locate(view, *target))
				{
					items.reserve(source->Get(view).MyItems.size());
					for (const auto& item : source->Get(view).MyItems) items.emplace_back(item.MyId);
				}
				for (const auto& id : items)
				{
					const auto got = _economy.Acquire(owned, _economy._MyCatalog.At(id), _events);
					_economy.LiftOutOfRange(owned); _economy.FillHand(view);
					if (!got) continue;
					const auto location = _economy.Locate(view, *got);
					_events.emplace_back(EconomyEvent{.MyKind = EventKind::GRANTED,
						.MyDefinitionId = _economy.GrantDefinition(view, *got, id, _events), .MyUid = *got});
					if (!location || location->Get(view).MyId != id || !_economy.Locate(view, *copy)) continue;
					GrantResult changes;
					if (const auto error = _economy.ApplyInventoryEffect(owned, AttachItemDirect{*got, *copy}, changes)) return error;
					for (auto& event : changes.MyChanges.MyEvents) _events.emplace_back(std::move(event));
					_economy.LiftOutOfRange(owned); _economy.FillHand(view);
				}
			}
			else if (effect.MyKind == PreparationItemKind::TRAINING_BOUNTY || effect.MyKind == PreparationItemKind::BAND_BOUNTY)
			{
				const auto& family = effect.MyKind == PreparationItemKind::TRAINING_BOUNTY ? _MyTrainingBounties : _MyBandBounties;
				const ChoiceCardRecord* card = nullptr;
				if (!family.empty())
				{
					auto offer = family;
					_random.Shuffle(offer.begin(), offer.end());
					card = offer[_random.Index(static_cast<std::uint32_t>(std::min<std::size_t>(3, offer.size())))];
				}
				else
				{
					const auto& fallback = _MyFallbackBounties[_rule.MyFallbackPerfect ? 1 : 0];
					if (fallback.empty()) return CommandError::BAD_TARGET;
					card = fallback[_random.Index(static_cast<std::uint32_t>(fallback.size()))];
				}
				if (_economy._MyNextUid == std::numeric_limits<PieceUid>::max()) throw std::overflow_error("bounty UID overflow");
				ActiveBounty bounty{.MyCard = WaveBounty{.MyId = "bounty:" + std::to_string(++_economy._MyNextUid),
					.MyEnemyId = card->MyEnemyId, .MyCount = card->MyCount, .MyCoins = card->MyCoins,
					.MyPerfect = card->MyPerfect}, .MyRoundsLeft = card->MyBattles};
				if (!_ledger.AddBounty(view.MyPlayerId, std::move(bounty))) return CommandError::BAD_TARGET;
			}
			else return CommandError::BAD_TARGET;
		}
		return {};
	}

	std::expected<ChangeSet, CommandError> PreparationContent::ExecuteArt(const CommandEnvelope& _command, Random& _random)
	{
		const auto player = ItemPlayer(_command);
		if (!player) return std::unexpected(player.error());
		const auto& command = std::get<UseArt>(_command.MyCommand);
		if (command.MyFacing < Facing::UP || command.MyFacing > Facing::LEFT) return std::unexpected(CommandError::BAD_TARGET);
		auto& original = _MyEconomy._MyPlayers[*player].MyView;
		const auto source = _MyEconomy.Locate(original, command.MyItem);
		if (!source || source->MyOnBoard || source->MyEquippedIndex || source->Get(original).IsToken())
			return std::unexpected(CommandError::BAD_TARGET);
		const auto& definition = _MyEconomy._MyCatalog.At(source->Get(original).MyId);
		if (definition.MyKind != PieceKind::ITEM || definition.MyItemUse != ItemUse::ART)
			return std::unexpected(CommandError::BAD_TARGET);
		if (!command.MyPosition.InField()) return std::unexpected(CommandError::BAD_TILE);
		if (original.MyRoundStatistics.MyArts >= _MyEconomy._MyRules.MyMaxArtsPerRound)
			return std::unexpected(CommandError::BAD_TARGET);
		const auto* rule = ItemRule(definition.MyId);
		if (!rule || !std::ranges::any_of(rule->MyEffects, [](const auto& _effect)
			{ return _effect.MyKind >= PreparationItemKind::COPY_ART && _effect.MyKind <= PreparationItemKind::BAND_BOUNTY; }))
			return std::unexpected(CommandError::BAD_TARGET);
		auto economy = _MyEconomy;
		auto ledger = _MyLedger;
		auto players = _MyPlayers;
		auto random = _random;
		PreparationHooks hooks(*this, economy, players, random);
		ChangeSet changes;
		if (const auto error = ApplyArt(economy, ledger, *player, command, *rule, random, changes.MyEvents))
			return std::unexpected(*error);
		auto& owned = economy._MyPlayers[*player];
		if (const auto again = economy.Locate(owned.MyView, command.MyItem)) (void)economy.Detach(owned.MyView, *again);
		++owned.MyView.MyRoundStatistics.MyArts;
		changes.MyEvents.emplace_back(EconomyEvent{.MyKind = EventKind::ART_USED, .MyDefinitionId = definition.MyId, .MyUid = command.MyItem});
		economy.LiftOutOfRange(owned); economy.FillHand(owned.MyView);
		changes.MyRevision = ++economy._MyRevision;
		_MyEconomy.Commit(std::move(economy)); _MyLedger = std::move(ledger); _MyPlayers.swap(players); _random = random;
		return changes;
	}

	std::expected<ChangeSet, CommandError> PreparationContent::ExecuteDestroy(const CommandEnvelope& _command, Random& _random)
	{
		const auto player = ItemPlayer(_command);
		if (!player) return std::unexpected(player.error());
		auto& original = _MyEconomy._MyPlayers[*player].MyView;
		const auto source = _MyEconomy.Locate(original, std::get<DestroyItem>(_command.MyCommand).MyUid);
		if (!source || source->MyEquippedIndex || source->Get(original).IsToken())
			return _MyEconomy.Execute(_command);
		const auto* rule = ItemRule(source->Get(original).MyId);
		if (!rule || !std::ranges::any_of(rule->MyEffects,
			[](const auto& _effect) { return _effect.MyKind == PreparationItemKind::DESTROY_PASS; }))
			return _MyEconomy.Execute(_command);
		auto economy = _MyEconomy;
		auto players = _MyPlayers;
		auto random = _random;
		PreparationHooks hooks(*this, economy, players, random);
		auto& owned = economy._MyPlayers[*player];
		ChangeSet changes;
		if (const auto error = economy.Apply(owned, _command.MyCommand, changes.MyEvents)) return std::unexpected(*error);
		for (const auto& effect : rule->MyEffects)
		{
			if (effect.MyKind != PreparationItemKind::DESTROY_PASS) continue;
			if (effect.MyCount > 0) EconomySession::AddFunds(owned.MyView, effect.MyCount);
			std::optional<std::size_t> recipient;
			int distance = 65;
			for (std::size_t i = 0; i < economy._MyPlayers.size(); ++i)
			{
				const auto& candidate = economy._MyPlayers[i];
				if (i == *player || !candidate.MyView.MyAlive) continue;
				const auto next = (candidate.MySeat - owned.MySeat + 64) % 64;
				if (next < distance) { distance = next; recipient = i; }
			}
			if (recipient)
			{
				auto& target = economy._MyPlayers[*recipient];
				const auto first = changes.MyEvents.size();
				const auto uid = economy.Acquire(target, economy._MyCatalog.At(rule->MyId), changes.MyEvents);
				economy.LiftOutOfRange(target); economy.FillHand(target.MyView);
				if (uid) changes.MyEvents.emplace_back(EconomyEvent{.MyKind = EventKind::GRANTED,
					.MyDefinitionId = economy.GrantDefinition(target.MyView, *uid, rule->MyId, changes.MyEvents), .MyUid = *uid});
				for (auto i = first; i < changes.MyEvents.size(); ++i) changes.MyEvents[i].MyPlayerId = target.MyView.MyPlayerId;
			}
		}
		economy.LiftOutOfRange(owned); economy.FillHand(owned.MyView);
		changes.MyRevision = ++economy._MyRevision;
		_MyEconomy.Commit(std::move(economy)); _MyPlayers.swap(players); _random = random;
		return changes;
	}
}
