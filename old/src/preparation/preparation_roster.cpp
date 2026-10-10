#include <stronghold/domain/preparation.hpp>

namespace Stronghold
{
	std::expected<void, CommandError> EconomySession::ConfigureRoster(
		std::string_view _player,
		std::span<const PrivateStockSelection> _stock,
		std::span<const PlacementOverride> _placements
	)
	{
		const auto found = std::ranges::find_if(_MyPlayers, [&](const Player& _value) { return _value.MyView.MyPlayerId == _player; });
		if (found == _MyPlayers.end()) return std::unexpected(CommandError::UNKNOWN_PLAYER);
		if (_MyPhase != PreparationPhase::IDLE || _MyNextUid != 0) return std::unexpected(CommandError::WRONG_PHASE);
		std::vector<PrivatePoolEntry> stocks; stocks.reserve(_stock.size());
		for (const auto& selection : _stock)
		{
			const auto* definition = _MyCatalog.Find(selection.MyId);
			if (!definition || !definition->MyRequiresSelection || definition->MyGolden || definition->MyKind != PieceKind::CHESS ||
				definition->MyBaseId != definition->MyId || selection.MyShopLevel < 1 || selection.MyShopLevel > 6 ||
				std::ranges::contains(stocks, selection.MyId, &PrivatePoolEntry::MyId)) return std::unexpected(CommandError::BAD_TARGET);
			const auto copies = selection.MyEnabled ? definition->MyPoolCopies : 0;
			stocks.emplace_back(PrivatePoolEntry{.MyId = definition->MyId,
				.MyStock = {.MyCapacity = copies, .MyRemaining = copies, .MyTier = definition->MyTier},
				.MyShopLevel = selection.MyShopLevel});
		}
		std::vector<PlacementOverride> placements; placements.reserve(_placements.size());
		for (const auto& placement : _placements)
		{
			const auto* definition = _MyCatalog.Find(placement.MyId);
			if (!definition || definition->MyKind != PieceKind::CHESS ||
				(placement.MyPlacement != PlacementClass::ANY && placement.MyPlacement != PlacementClass::MELEE && placement.MyPlacement != PlacementClass::HIGH_ONLY) ||
				std::ranges::contains(placements, placement.MyId, &PlacementOverride::MyId)) return std::unexpected(CommandError::BAD_TARGET);
			placements.emplace_back(placement);
		}
		found->MyView.MyPrivateStock = std::move(stocks);
		found->MyView.MyPlacementOverrides = std::move(placements);
		++_MyRevision;
		return {};
	}

	const PoolEntry* EconomySession::Stock(const Player& _player, std::string_view _base) const
	{
		const auto& stocks = _player.MyView.MyPrivateStock;
		const auto found = std::ranges::find(stocks, _base, &PrivatePoolEntry::MyId);
		return found == stocks.end() ? _MyPools[_player.MyPool].Find(_base) : &found->MyStock;
	}

	int EconomySession::TakeCopies(Player& _player, std::string_view _base, int _count)
	{
		if (_count < 0) throw std::invalid_argument("negative pool withdrawal");
		auto& stocks = _player.MyView.MyPrivateStock;
		const auto found = std::ranges::find(stocks, _base, &PrivatePoolEntry::MyId);
		if (found == stocks.end()) return _MyPools[_player.MyPool].Take(_base, _count);
		const auto taken = std::min(_count, found->MyStock.MyRemaining);
		found->MyStock.MyRemaining -= taken;
		return taken;
	}

	int EconomySession::ReturnCopies(Player& _player, std::string_view _base, int _count)
	{
		if (_count < 0) throw std::invalid_argument("negative pool return");
		auto& stocks = _player.MyView.MyPrivateStock;
		const auto found = std::ranges::find(stocks, _base, &PrivatePoolEntry::MyId);
		if (found == stocks.end()) return _MyPools[_player.MyPool].Give(_base, _count);
		const auto returned = std::min(_count, found->MyStock.MyCapacity - found->MyStock.MyRemaining);
		found->MyStock.MyRemaining += returned;
		return returned;
	}

	PlacementClass EconomySession::PlacementOf(const PlayerView& _view, const Definition& _definition)
	{
		const auto found = std::ranges::find(_view.MyPlacementOverrides, _definition.MyId, &PlacementOverride::MyId);
		return found == _view.MyPlacementOverrides.end() ? _definition.MyPlacement : found->MyPlacement;
	}

	bool EconomySession::Selected(const PlayerView& _view, const Definition& _definition)
	{
		return !_definition.MyRequiresSelection || std::ranges::contains(_view.MyPrivateStock, _definition.MyBaseId, &PrivatePoolEntry::MyId);
	}
	std::optional<std::string> EconomySession::RollChess(std::string_view _player, Random& _random, RollOptions _options) const
	{
		const auto found = std::ranges::find_if(_MyPlayers, [&](const Player& _p) { return _p.MyView.MyPlayerId == _player; });
		if (found == _MyPlayers.end()) throw std::invalid_argument("unknown content-draw player");
		_options.MyExtra = found->MyView.MyPrivateStock;
		return _MyPools[found->MyPool].Roll(_random, _options);
	}

	std::optional<PoolEntry> EconomySession::CopyStock(std::string_view _player, std::string_view _baseId) const
	{
		const auto found = std::ranges::find_if(_MyPlayers, [&](const Player& _p) { return _p.MyView.MyPlayerId == _player; });
		if (found == _MyPlayers.end()) throw std::invalid_argument("unknown content-stock player");
		const auto* entry = Stock(*found, _baseId);
		return entry ? std::optional(*entry) : std::nullopt;
	}

}
