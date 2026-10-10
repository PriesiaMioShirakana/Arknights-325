#include <limits>
#include <stronghold/domain/preparation_content.hpp>
#include "preparation_hooks.hpp"

namespace Stronghold
{
	const PreparationItemRule* PreparationContent::ItemRule(std::string_view _id) const
	{
		const auto found = std::ranges::find(_MyItems, _id, &PreparationItemRule::MyId);
		return found == _MyItems.end() ? nullptr : &*found;
	}

	std::optional<std::vector<PreparationItemState>> PreparationContent::ItemEffects(std::string_view _player) const
	{
		const auto found = std::ranges::find(_MyPlayers, _player, &Player::MyId);
		if (found == _MyPlayers.end()) return {};
		return found->MyItemEffects;
	}

	std::vector<std::string_view> PreparationContent::ItemBonds(const Player& _player, const Piece& _piece) const
	{
		const auto& definition = _MyEconomy._MyCatalog.At(_piece.MyId);
		const auto member = std::ranges::find(_player.MyRoster, definition.MyBaseId, &ContentPoolRoster::MyId);
		std::vector<std::string_view> bonds;
		if (member != _player.MyRoster.end()) bonds.assign(member->MyBonds.begin(), member->MyBonds.end());
		const bool canGive = _piece.MyItems.size() >= 2 && std::ranges::any_of(_piece.MyItems, [&](const Piece& _item)
		{
			const auto* rule = ItemRule(_item.MyId);
			return rule && rule->MyBond.MyCanGiveBond;
		});
		if (canGive)
			for (const auto& item : _piece.MyItems)
			{
				const auto* rule = ItemRule(item.MyId);
				if (rule && !rule->MyBond.MyCanGiveBond && !rule->MyBond.MyGrantedBond.empty() &&
					std::ranges::find(_MyBonds, rule->MyBond.MyGrantedBond, &BondRule::MyId) != _MyBonds.end() &&
					!std::ranges::contains(bonds, rule->MyBond.MyGrantedBond)) bonds.emplace_back(rule->MyBond.MyGrantedBond);
			}
		return bonds;
	}

	std::optional<std::size_t> PreparationContent::ItemRecipient(const EconomySession& _economy,
		std::span<const Player> _players, std::size_t _sender, std::span<const std::string_view> _bonds, Random& _random) const
	{
		std::vector<std::size_t> candidates;
		candidates.reserve(_players.size());
		std::int64_t best = -1;
		for (std::size_t i = 0; i < _players.size(); ++i)
		{
			const auto& view = _economy._MyPlayers[i].MyView;
			if (i == _sender || !view.MyAlive) continue;
			const auto counts = ItemBondStates(_economy, _players[i], i);
			std::int64_t score = 0;
			for (const auto bond : _bonds)
				if (const auto found = std::ranges::find(counts, bond, &BondState::MyId); found != counts.end())
					score += found->MyCount;
			if (score > best) { best = score; candidates.clear(); }
			if (score == best) candidates.emplace_back(i);
		}
		if (candidates.empty()) return {};
		return candidates[_random.Index(static_cast<std::uint32_t>(candidates.size()))];
	}

	bool PreparationContent::GrantItemChess(EconomySession& _economy, std::size_t _player,
		std::string_view _id, std::vector<EconomyEvent>& _events) const
	{
		auto& player = _economy._MyPlayers[_player];
		const auto& definition = _economy._MyCatalog.At(_id);
		if (const auto* stock = _economy.Stock(player, definition.MyBaseId); stock && stock->MyRemaining < 1) return false;
		const auto uid = _economy.Acquire(player, definition, _events);
		_economy.LiftOutOfRange(player); _economy.FillHand(player.MyView);
		if (!uid) return false;
		_events.emplace_back(EconomyEvent{.MyKind = EventKind::GRANTED,
			.MyDefinitionId = _economy.GrantDefinition(player.MyView, *uid, definition.MyId, _events), .MyUid = *uid});
		return true;
	}

	std::optional<CommandError> PreparationContent::ApplyItem(EconomySession& _economy, std::vector<Player>& _players,
		std::size_t _player, const PreparationItemEffect& _effect, const EquipItem& _command,
		Random& _random, std::vector<EconomyEvent>& _events, bool& _keep) const
	{
		auto& player = _players[_player];
		auto& owned = _economy._MyPlayers[_player];
		auto& view = owned.MyView;
		const auto location = _economy.Locate(view, _command.MyTarget);
		if (!location) return CommandError::BAD_TARGET;
		// 发放可能合成或销毁携带者；只保存定义和盟约的快照，不跨发放持有 Piece 引用。
		const auto& definition = _economy._MyCatalog.At(location->Get(view).MyId);
		const auto bonds = ItemBonds(player, location->Get(view));
		const auto grant = [&](std::string_view _id) { return GrantItemChess(_economy, _player, _id, _events); };
		const auto inventory = [&](const InventoryEffect& _action)
		{
			GrantResult result;
			const auto error = _economy.ApplyInventoryEffect(owned, _action, result);
			for (auto& event : result.MyChanges.MyEvents) _events.emplace_back(std::move(event));
			_economy.LiftOutOfRange(owned); _economy.FillHand(view);
			return error;
		};
		const auto offer = [&](const std::vector<std::string>& _ids)
		{
			if (_ids.empty()) return;
			std::vector<ShopSlot> slots;
			slots.reserve(_ids.size());
			for (const auto& id : _ids) slots.emplace_back(ShopSlot{.MyId = id, .MyPrice = _economy._MyRules.MyRewardPrice});
			view.MyOffers.emplace_back(std::move(slots));
		};
		const auto sameBond = [&]
		{
			std::vector<std::string_view> ids;
			ids.reserve(player.MyRoster.size());
			for (const auto& member : player.MyRoster)
				if (std::ranges::any_of(member.MyBonds, [&](auto _bond) { return std::ranges::contains(bonds, _bond); }))
					ids.emplace_back(member.MyId);
			return ids;
		};
		switch (_effect.MyKind)
		{
		case PreparationItemKind::COINS:
			EconomySession::AddFunds(view, _effect.MyMinimum + _random.Index(
				static_cast<std::uint32_t>(_effect.MyMaximum - _effect.MyMinimum + 1)));
			break;
		case PreparationItemKind::LAYERS:
			for (const auto bond : bonds) (void)AddContentLayers(_economy, _players, _player, bond, static_cast<double>(_effect.MyCount), _random, _events);
			break;
		case PreparationItemKind::SHOP_CHESS:
		{
			std::vector<bool> tried(view.MyShop.size());
			for (std::int64_t n = 0; n < std::max<std::int64_t>(1, _effect.MyCount);)
			{
				std::vector<std::size_t> slots;
				for (std::size_t i = 0; i < view.MyShop.size(); ++i)
					if (const auto& slot = view.MyShop[i]; !tried[i] && slot && !slot->MySold &&
						_economy._MyCatalog.At(slot->MyId).MyKind == PieceKind::CHESS) slots.emplace_back(i);
				if (slots.empty()) break;
				const auto index = slots[_random.Index(static_cast<std::uint32_t>(slots.size()))];
				tried[index] = true;
				const auto id = view.MyShop[index]->MyId;
				if (grant(id)) { view.MyShop[index].reset(); ++n; }
			}
			break;
		}
		case PreparationItemKind::ROUND_COINS:
			player.MyItemEffects.emplace_back(PreparationItemState{.MyKind = _effect.MyKind,
				.MySource = _command.MyItem, .MyCount = _effect.MyCount});
			break;
		case PreparationItemKind::NEXT_ROUND_COINS:
			if (view.MyPendingFunds > std::numeric_limits<std::int64_t>::max() - _effect.MyCount)
				throw std::overflow_error("item pending funds overflow");
			view.MyPendingFunds += _effect.MyCount;
			break;
		case PreparationItemKind::DEPLOY_CAP:
			view.MyDeployCap = std::max(view.MyDeployCap, static_cast<std::size_t>(std::min<std::int64_t>(12, _effect.MyCount)));
			break;
		case PreparationItemKind::PROMOTE:
		case PreparationItemKind::PROMOTE_NEXT_ROUND:
			if (definition.MyGolden) return CommandError::BAD_TARGET;
			if (_effect.MyKind == PreparationItemKind::PROMOTE_NEXT_ROUND) _keep = true;
			else (void)inventory(PromotePiece{.MyUid = _command.MyTarget});
			break;
		case PreparationItemKind::MIMIC:
		case PreparationItemKind::SAME_BOND_CHESS:
		case PreparationItemKind::OFFER_SAME_BOND:
		{
			if (_effect.MyKind == PreparationItemKind::MIMIC)
			{
				std::size_t count = 0;
				const auto countOwned = [&](const auto& _slots)
				{
					for (const auto& piece : _slots)
						if (piece && !piece->IsToken() && piece->MyId == definition.MyBaseId) ++count;
				};
				countOwned(view.MyBoard); countOwned(view.MyHand); countOwned(view.MyTemporary);
				if (count >= 2) { (void)grant(definition.MyBaseId); break; }
			}
			const auto members = sameBond();
			RollOptions options;
			options.MyIncluded = members;
			options.MyMaxTier = _effect.MyKind == PreparationItemKind::MIMIC ? 6 : view.MyLevel;
			const auto count = _effect.MyKind == PreparationItemKind::MIMIC ? 1 : std::max<std::int64_t>(1, _effect.MyCount);
			std::vector<std::string> ids;
			for (std::int64_t n = 0; n < count; ++n)
			{
				const auto id = _economy.RollChess(player.MyId, _random, options);
				if (!id) continue;
				if (_effect.MyKind == PreparationItemKind::OFFER_SAME_BOND)
				{
					options.MyExcluded.emplace(*id); ids.emplace_back(*id);
				}
				else (void)grant(*id);
			}
			offer(ids);
			break;
		}
		case PreparationItemKind::BEACON:
		{
			(void)inventory(RemoveOwnedPiece{.MyUid = _command.MyTarget});
			RollOptions options;
			std::vector<std::string> ids;
			for (std::int64_t n = 0; n < std::max<std::int64_t>(1, _effect.MyCount); ++n)
			{
				std::optional<std::string> id;
				for (int tier = definition.MyTier; tier >= 1 && !id; --tier)
				{
					options.MyExactTier = tier;
					id = _economy.RollChess(player.MyId, _random, options);
				}
				if (id) { options.MyExcluded.emplace(*id); ids.emplace_back(*id); }
			}
			offer(ids);
			if (!definition.MyRequiresSelection)
				if (const auto recipient = ItemRecipient(_economy, _players, _player, bonds, _random))
					player.MyItemEffects.emplace_back(PreparationItemState{.MyKind = _effect.MyKind,
						.MySource = _command.MyItem, .MyRecipient = _players[*recipient].MyId,
						.MyChess = definition.MyId, .MyBonds = bonds});
			break;
		}
		default: return CommandError::BAD_TARGET;
		}
		return {};
	}

	std::expected<ChangeSet, CommandError> PreparationContent::Execute(const CommandEnvelope& _command, Random& _random)
	{
		if (std::holds_alternative<UseArt>(_command.MyCommand)) return ExecuteArt(_command, _random);
		if (std::holds_alternative<DestroyItem>(_command.MyCommand)) return ExecuteDestroy(_command, _random);
		const auto* equip = std::get_if<EquipItem>(&_command.MyCommand);
		if (!equip) return ExecuteEconomy(_command, _random);
		const auto player = ItemPlayer(_command);
		if (!player) return std::unexpected(player.error());
		const auto index = *player;
		auto& original = _MyEconomy._MyPlayers[index].MyView;
		const auto source = _MyEconomy.Locate(original, equip->MyItem);
		if (!source || source->Get(original).IsToken()) return std::unexpected(CommandError::BAD_TARGET);
		const auto& definition = _MyEconomy._MyCatalog.At(source->Get(original).MyId);
		if (definition.MyItemUse != ItemUse::CONSUME_ON_EQUIP) return ExecuteEconomy(_command, _random);
		const auto target = _MyEconomy.Locate(original, equip->MyTarget);
		const auto* rule = ItemRule(definition.MyId);
		if (!rule || rule->MyEffects.empty() || !target || source->MyOnBoard || source->MyEquippedIndex ||
			target->MyEquippedIndex || target->Get(original).IsToken() ||
			_MyEconomy._MyCatalog.At(target->Get(original).MyId).MyKind != PieceKind::CHESS)
			return std::unexpected(CommandError::BAD_TARGET);
		const auto& originalItems = target->Get(original).MyItems;
		if (equip->MyReplace && std::ranges::find(originalItems, *equip->MyReplace, &Piece::MyUid) == originalItems.end())
			return std::unexpected(CommandError::BAD_TARGET);

		auto economy = _MyEconomy;
		auto players = _MyPlayers;
		auto random = _random;
		PreparationHooks hooks(*this, economy, players, random);
		auto& owned = economy._MyPlayers[index];
		auto& view = owned.MyView;
		auto& items = target->Get(view).MyItems;
		ChangeSet result;
		if (items.size() >= economy._MyRules.MyEquipmentPerChess)
		{
			const auto off = equip->MyReplace ? std::ranges::find(items, *equip->MyReplace, &Piece::MyUid) : items.begin();
			result.MyEvents.emplace_back(EconomyEvent{.MyKind = EventKind::DESTROYED,
				.MyDefinitionId = off->MyId, .MyUid = off->MyUid});
			items.erase(off);
		}
		// 消耗品在效果结算期间仍占用原库存槽；不能提前腾空以使本应失败的发放成功。
		bool keep = false;
		for (const auto& effect : rule->MyEffects)
			if (const auto error = ApplyItem(economy, players, index, effect, *equip, random, result.MyEvents, keep))
				return std::unexpected(*error);
		if (const auto again = economy.Locate(view, equip->MyItem); again && !again->MyEquippedIndex)
		{
			auto item = economy.Detach(view, *again);
			if (keep)
				if (const auto holder = economy.Locate(view, equip->MyTarget)) holder->Get(view).MyItems.emplace_back(std::move(item));
		}
		result.MyEvents.emplace_back(EconomyEvent{.MyKind = EventKind::EQUIPPED,
			.MyDefinitionId = definition.MyId, .MyUid = equip->MyItem});
		++view.MyStatistics.MyItemsEquipped;
		economy.CheckItemMerges(owned, result.MyEvents); economy.LiftOutOfRange(owned); economy.FillHand(view);
		result.MyRevision = ++economy._MyRevision;
		_MyEconomy.Commit(std::move(economy)); _MyPlayers.swap(players); _random = random;
		return result;
	}

	ChoiceRewardResult PreparationContent::OnRoundStart(Random& _random)
	{
		if (_MyEconomy.Phase() != PreparationPhase::ROUND_START || _MyItemRound >= _MyEconomy.Round())
			throw std::invalid_argument("item round start requires a new unopened preparation");
		auto economy = _MyEconomy;
		auto players = _MyPlayers;
		auto random = _random;
		PreparationHooks hooks(*this, economy, players, random);
		ChoiceRewardResult result;
		result.MyRecipients.reserve(players.size());
		for (auto& player : players)
		{
			player.MyLayers.BeginRound(static_cast<unsigned>(economy.Round()));
			result.MyRecipients.emplace_back(ChoiceRecipient{.MyPlayerId = player.MyId});
		}
		const auto apply = [&](std::size_t _index)
		{
			auto& owned = economy._MyPlayers[_index];
			auto& view = owned.MyView;
			auto& events = result.MyRecipients[_index].MyEvents;
			std::erase_if(players[_index].MyItemCounters, [&](const auto& _counter)
				{ return !economy.Locate(view, _counter.MyUid); });
			std::erase_if(players[_index].MyGarrisonCounters, [&](const auto& _counter)
				{ return !economy.Locate(view, _counter.MyUid); });
			PreparationHooks::Scope scope(hooks);
			if (view.MyAlive)
			{
				ApplyBand(economy, players, _index, BandEvent::ROUND_START, random, events);
				ApplyBonds(economy, players, _index, BondEvent::ROUND_START, random, events);
				RunGarrisons(economy, players, _index, GarrisonEvent::ROUND_START, random, events);
				std::vector<std::pair<PieceUid, PieceUid>> promotions;
				const auto collect = [&](const std::optional<Piece>& _piece)
				{
					if (!_piece || _piece->IsToken()) return;
					for (const auto& item : _piece->MyItems)
						if (const auto* rule = ItemRule(item.MyId); rule && std::ranges::any_of(rule->MyEffects,
							[](const auto& _effect) { return _effect.MyKind == PreparationItemKind::PROMOTE_NEXT_ROUND; }))
							promotions.emplace_back(item.MyUid, _piece->MyUid);
				};
				for (const auto& piece : view.MyBoard) collect(piece);
				for (const auto& piece : view.MyHand) collect(piece);
				for (const auto& piece : view.MyTemporary) collect(piece);
				for (const auto& [item, holder] : promotions)
				{
					const auto location = economy.Locate(view, item);
					if (!location || !location->MyEquippedIndex || location->Slot(view)->MyUid != holder) continue;
					GrantResult changes;
					(void)economy.ApplyInventoryEffect(owned, RemoveOwnedPiece{.MyUid = item}, changes);
					economy.LiftOutOfRange(owned); economy.FillHand(view);
					(void)economy.ApplyInventoryEffect(owned, PromotePiece{.MyUid = holder}, changes);
					economy.LiftOutOfRange(owned); economy.FillHand(view);
					for (auto& event : changes.MyChanges.MyEvents) events.emplace_back(std::move(event));
				}
			}
			auto& effects = players[_index].MyItemEffects;
			for (auto it = effects.begin(); it != effects.end();)
			{
				if (it->MyKind == PreparationItemKind::ROUND_COINS)
				{
					if (view.MyAlive && it->MyCount > 0)
					{
						EconomySession::AddFunds(view, it->MyCount);
						events.emplace_back(EconomyEvent{.MyKind = EventKind::ECONOMY_EFFECT,
							.MyDefinitionId = {}, .MyUid = it->MySource, .MyAmount = it->MyCount});
					}
					++it;
					continue;
				}
				const auto found = std::ranges::find(players, it->MyRecipient, &Player::MyId);
				std::optional<std::size_t> recipient;
				if (found != players.end())
				{
					const auto index = static_cast<std::size_t>(found - players.begin());
					if (economy._MyPlayers[index].MyView.MyAlive) recipient = index;
				}
				if (!recipient) recipient = ItemRecipient(economy, players, _index, it->MyBonds, random);
				if (!recipient || GrantItemChess(economy, *recipient, it->MyChess, result.MyRecipients[*recipient].MyEvents))
					it = effects.erase(it);
				else ++it;
			}
		};
		for (const bool alive : {true, false})
			for (std::size_t i = 0; i < players.size(); ++i)
				if (economy._MyPlayers[i].MyView.MyAlive == alive) apply(i);
		++economy._MyRevision;
		_MyEconomy.Commit(std::move(economy)); _MyPlayers.swap(players); _random = random;
		_MyItemRound = _MyEconomy.Round();
		return result;
	}
}
