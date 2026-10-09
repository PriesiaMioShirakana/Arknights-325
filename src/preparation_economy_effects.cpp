#include <limits>
#include <stronghold/domain/preparation.hpp>
#include <type_traits>

namespace Stronghold
{
	void EconomySession::AddFunds(PlayerView& _view, std::int64_t _amount)
	{
		if (_amount > 0)
		{
			const auto gain = static_cast<std::uint64_t>(_amount);
			if (_view.MyFunds > std::numeric_limits<std::int64_t>::max() - _amount ||
				_view.MyStatistics.MyFundsGained > std::numeric_limits<std::uint64_t>::max() - gain)
				throw std::overflow_error("funds or funds-gained counter overflow");
			_view.MyStatistics.MyFundsGained += gain;
		}
		// 不对 INT64_MIN 取负；资金保持非负，使这个加法仍在 int64_t 范围内。
		_view.MyFunds = std::max<std::int64_t>(0, _view.MyFunds + _amount);
	}

	bool EconomySession::Spend(PlayerView& _view, std::int64_t _amount)
	{
		if (_amount < 0) throw std::invalid_argument("negative spending");
		if (_amount > _view.MyFunds) return false;
		const auto value = static_cast<std::uint64_t>(_amount);
		if (_view.MyStatistics.MySpent > std::numeric_limits<std::uint64_t>::max() - value ||
			_view.MyRoundStatistics.MySpent > std::numeric_limits<std::uint64_t>::max() - value)
			throw std::overflow_error("spending counter overflow");
		_view.MyFunds -= _amount;
		_view.MyStatistics.MySpent += value;
		_view.MyRoundStatistics.MySpent += value;
		return true;
	}

	std::expected<ChangeSet, CommandError> EconomySession::ApplyEconomyEffect(std::string_view _player, const EconomyEffect& _effect)
	{
		const auto found = std::ranges::find_if(_MyPlayers, [&](const Player& _p) { return _p.MyView.MyPlayerId == _player; });
		if (found == _MyPlayers.end()) return std::unexpected(CommandError::UNKNOWN_PLAYER);
		if (!found->MyView.MyAlive) return std::unexpected(CommandError::ELIMINATED);
		auto next = *this;
		auto& view = next._MyPlayers[static_cast<std::size_t>(found - _MyPlayers.begin())].MyView;
		const auto error = std::visit([&](const auto& _action) -> std::optional<CommandError>
		{
			using Action = std::decay_t<decltype(_action)>;
			if constexpr (std::is_same_v<Action, AdjustFunds>) AddFunds(view, _action.MyAmount);
			else if constexpr (std::is_same_v<Action, AddPendingFunds>)
			{
				if (_action.MyAmount < 0) return CommandError::BAD_TARGET;
				if (view.MyPendingFunds > std::numeric_limits<std::int64_t>::max() - _action.MyAmount)
					throw std::overflow_error("pending funds overflow");
				view.MyPendingFunds += _action.MyAmount;
			}
			else if constexpr (std::is_same_v<Action, GrantFreeRefreshes>)
			{
				if (view.MyFreeRefreshes > std::numeric_limits<std::uint64_t>::max() - _action.MyCount)
					throw std::overflow_error("free refresh counter overflow");
				view.MyFreeRefreshes += _action.MyCount;
			}
			else if constexpr (std::is_same_v<Action, GrantPurchaseUpgrade>)
			{
				if (_action.MyUpgrade.MyKind != PieceKind::CHESS && _action.MyUpgrade.MyKind != PieceKind::ITEM)
					return CommandError::BAD_TARGET;
				if (_action.MyUpgrade.MyRemaining) view.MyPurchaseUpgrades.emplace_back(_action.MyUpgrade);
			}
			else if constexpr (std::is_same_v<Action, KeepRemainingFunds>) view.MyKeepRemainingFunds = _action.MyEnabled;
			else if constexpr (std::is_same_v<Action, RaiseDeployCap>) view.MyDeployCap = std::max(view.MyDeployCap, _action.MyMinimum);
			return {};
		}, _effect);
		if (error) return std::unexpected(*error);
		ChangeSet changes{.MyRevision = ++next._MyRevision, .MyEvents = {EconomyEvent{.MyKind = EventKind::ECONOMY_EFFECT, .MyDefinitionId = {}}}};
		Commit(std::move(next));
		return changes;
	}

	void EconomySession::ApplyPurchaseUpgrade(Player& _player, PieceUid _piece, std::vector<EconomyEvent>& _events)
	{
		auto& view = _player.MyView;
		const auto location = Locate(view, _piece);
		if (!location) return;
		const auto& definition = _MyCatalog.At(location->Get(view).MyId);
		// 合成已先执行；买到精锐/进阶品时保留计数，也不在奖励领取或直接赠送时消耗。
		if (definition.MyGolden) return;
		for (auto it = view.MyPurchaseUpgrades.begin(); it != view.MyPurchaseUpgrades.end(); ++it)
		{
			if (it->MyKind != definition.MyKind || !it->MyRemaining) continue;
			const InventoryEffect effect = definition.MyKind == PieceKind::CHESS
				? InventoryEffect(PromotePiece{.MyUid = _piece}) : InventoryEffect(UpgradeOwnedItem{.MyUid = _piece});
			GrantResult result;
			if (ApplyInventoryEffect(_player, effect, result)) continue;
			for (auto& event : result.MyChanges.MyEvents) _events.emplace_back(std::move(event));
			if (--it->MyRemaining == 0) view.MyPurchaseUpgrades.erase(it);
			return; // 晋升后的同一棋子不再消耗后续叠加的奖励。
		}
	}

	std::expected<ChangeSet, CommandError> EconomySession::OfferPieces(std::string_view _player, PieceKind _kind, std::span<const std::string_view> _ids)
	{
		const auto found = std::ranges::find_if(_MyPlayers, [&](const Player& _p) { return _p.MyView.MyPlayerId == _player; });
		if (found == _MyPlayers.end()) return std::unexpected(CommandError::UNKNOWN_PLAYER);
		if (!found->MyView.MyAlive) return std::unexpected(CommandError::ELIMINATED);
		if (_kind != PieceKind::CHESS && _kind != PieceKind::ITEM) return std::unexpected(CommandError::BAD_TARGET);
		std::vector<ShopSlot> slots; slots.reserve(std::min<std::size_t>(6, _ids.size()));
		for (const auto id : _ids)
		{
			const auto* definition = _MyCatalog.Find(id);
			if (!definition || definition->MyKind != _kind || std::ranges::find(slots, id, &ShopSlot::MyId) != slots.end()) continue;
			slots.emplace_back(ShopSlot{.MyId = definition->MyId, .MyPrice = _kind == PieceKind::CHESS ? _MyRules.MyRewardPrice : 0});
			if (slots.size() == 6) break;
		}
		if (slots.empty()) return ChangeSet{.MyRevision = _MyRevision, .MyEvents = {}};
		// 构建好完整选项再入队；vector 的强异常保证避免半份奖励可见。
		found->MyView.MyOffers.emplace_back(std::move(slots));
		return ChangeSet{.MyRevision = ++_MyRevision, .MyEvents = {}};
	}
}
