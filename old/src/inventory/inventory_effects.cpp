#include <stronghold/domain/preparation.hpp>
#include <type_traits>

namespace Stronghold
{
	std::expected<GrantResult, CommandError> EconomySession::ApplyInventoryEffect(std::string_view _player, const InventoryEffect& _effect)
	{
		const auto found = std::ranges::find_if(_MyPlayers, [&](const Player& _value) { return _value.MyView.MyPlayerId == _player; });
		if (found == _MyPlayers.end()) return std::unexpected(CommandError::UNKNOWN_PLAYER);
		if (!found->MyView.MyAlive) return std::unexpected(CommandError::ELIMINATED);
		auto next = *this;
		auto& player = next._MyPlayers[static_cast<std::size_t>(found - _MyPlayers.begin())];
		GrantResult result;
		if (const auto error = next.ApplyInventoryEffect(player, _effect, result)) return std::unexpected(*error);
		next.LiftOutOfRange(player); next.FillHand(player.MyView);
		result.MyChanges.MyRevision = ++next._MyRevision;
		Commit(std::move(next));
		return result;
	}

	std::optional<CommandError> EconomySession::ApplyInventoryEffect(Player& _player, const InventoryEffect& _effect, GrantResult& _result)
	{
		auto& view = _player.MyView;
		auto& events = _result.MyChanges.MyEvents;
		return std::visit([&](const auto& _action) -> std::optional<CommandError>
		{
			using Action = std::decay_t<decltype(_action)>;
			if constexpr (std::is_same_v<Action, AttachItemDirect>)
			{
				const auto source = Locate(view, _action.MyItem), holder = Locate(view, _action.MyTarget);
				if (!source || !holder || source->MyEquippedIndex || holder->MyEquippedIndex || source->Get(view).IsToken() || holder->Get(view).IsToken() ||
					_MyCatalog.At(source->Get(view).MyId).MyKind != PieceKind::ITEM ||
					_MyCatalog.At(holder->Get(view).MyId).MyKind != PieceKind::CHESS) return CommandError::BAD_TARGET;
				auto item = Detach(view, *source);
				auto& items = holder->Get(view).MyItems;
				if (items.size() >= _MyRules.MyEquipmentPerChess)
				{
					events.emplace_back(EconomyEvent{.MyKind = EventKind::DESTROYED, .MyDefinitionId = items.front().MyId, .MyUid = items.front().MyUid});
					items.erase(items.begin());
				}
				items.reserve(_MyRules.MyEquipmentPerChess);
				events.emplace_back(EconomyEvent{.MyKind = EventKind::EQUIPPED, .MyDefinitionId = item.MyId, .MyUid = item.MyUid});
				items.emplace_back(std::move(item));
				CheckItemMerges(_player, events);
				// Direct attachment may immediately merge the item; do not return a UID that no longer exists.
				if (Locate(view, _action.MyItem)) _result.MyPiece = _action.MyItem;
			}
			else
			{
				const auto location = Locate(view, _action.MyUid);
				if (!location) return CommandError::BAD_TARGET;
				if (location->Get(view).IsToken())
				{
					if constexpr (!std::is_same_v<Action, RemoveOwnedPiece>) return CommandError::BAD_TARGET;
					else
					{
						auto token = Detach(view, *location);
						events.emplace_back(EconomyEvent{.MyKind = EventKind::DESTROYED, .MyDefinitionId = token.MyId, .MyUid = token.MyUid});
						return {};
					}
				}
				const auto& definition = _MyCatalog.At(location->Get(view).MyId);
				if constexpr (std::is_same_v<Action, PromotePiece> || std::is_same_v<Action, UpgradeOwnedItem>)
				{
					constexpr auto Kind = std::is_same_v<Action, PromotePiece> ? PieceKind::CHESS : PieceKind::ITEM;
					const auto* golden = _MyCatalog.Find(definition.MyGoldenId);
					if (definition.MyKind != Kind || definition.MyGolden || !golden || golden->MyKind != Kind) return CommandError::BAD_TARGET;
					auto& piece = location->Get(view);
					if constexpr (Kind == PieceKind::CHESS)
					{
						const auto extra = std::max(0, _MyRules.MyGoldenCopies - piece.MyPoolCopies);
						piece.MyPoolCopies += TakeCopies(_player, definition.MyBaseId, extra);
					}
					piece.MyId = definition.MyGoldenId;
					const auto promotedUid = piece.MyUid;
					if constexpr (Kind == PieceKind::CHESS)
					{
						LiftOutOfRange(_player); FillHand(view);
						if (location->MyOnBoard) GrantTokens(_player, piece);
					}
					_result.MyPiece = promotedUid;
					constexpr auto Event = Kind == PieceKind::CHESS ? EventKind::PROMOTED : EventKind::ITEM_UPGRADED;
					events.emplace_back(EconomyEvent{.MyKind = Event, .MyDefinitionId = definition.MyGoldenId, .MyUid = promotedUid});
				}
				else
				{
					if constexpr (std::is_same_v<Action, TransformChess>)
					{
						const auto* replacement = _MyCatalog.Find(_action.MyDefinitionId);
						if (definition.MyKind != PieceKind::CHESS || !replacement || replacement->MyKind != PieceKind::CHESS) return CommandError::BAD_TARGET;
					}
					auto old = Detach(view, *location); RemoveTokens(view, old.MyUid);
					events.emplace_back(EconomyEvent{.MyKind = EventKind::DESTROYED, .MyDefinitionId = old.MyId, .MyUid = old.MyUid});
					if (old.MyPoolCopies) (void)ReturnCopies(_player, definition.MyBaseId, old.MyPoolCopies);
					// Compact only the items that could not yet be returned. A subsequent merge can free space.
					std::size_t left = 0;
					for (std::size_t i = 0; i < old.MyItems.size(); ++i)
						if (!Stow(view, old.MyItems[i]))
						{
							if (left != i) old.MyItems[left] = std::move(old.MyItems[i]);
							++left;
						}
					old.MyItems.resize(left);
					if (definition.MyKind == PieceKind::CHESS) CheckItemMerges(_player, events);
					if constexpr (std::is_same_v<Action, TransformChess>)
					{
						_result.MyPiece = Acquire(_player, _MyCatalog.At(_action.MyDefinitionId), events);
						FillHand(view); // acquireChess recomputes before transformChess retries the overflow equipment.
					}
					for (auto& item : old.MyItems)
					{
						if constexpr (std::is_same_v<Action, TransformChess>)
						{
							if (Stow(view, item)) continue;
							if (_result.MyPiece)
							{
								auto& items = Locate(view, *_result.MyPiece)->Get(view).MyItems;
								if (items.size() < _MyRules.MyEquipmentPerChess) { items.emplace_back(std::move(item)); continue; }
							}
						}
						events.emplace_back(EconomyEvent{.MyKind = EventKind::DESTROYED, .MyDefinitionId = item.MyId, .MyUid = item.MyUid});
					}
					if constexpr (std::is_same_v<Action, TransformChess>)
						if (left) CheckItemMerges(_player, events);
				}
			}
			return {};
		}, _effect);
	}
}
