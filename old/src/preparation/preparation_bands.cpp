#include <map>
#include "preparation_hooks.hpp"

namespace Stronghold
{
	bool PreparationContent::BandMember(const Player& _player, std::string_view _definition, std::string_view _bond) const
	{
		const auto* definition = _MyEconomy._MyCatalog.Find(_definition);
		if (!definition || definition->MyKind != PieceKind::CHESS) return false;
		const auto member = std::ranges::find(_player.MyRoster, definition->MyBaseId, &ContentPoolRoster::MyId);
		return member != _player.MyRoster.end() && std::ranges::contains(member->MyBonds, _bond);
	}

	std::int64_t PreparationContent::BandPrice(const EconomySession& _economy, const Player& _player, const ShopSlot& _slot) const
	{
		std::int64_t price = _slot.MyPrice;
		for (const auto id : Garrisons(_player, _slot.MyId))
			if (const auto* rule = GarrisonRule(id); rule && rule->MyEvent == GarrisonEvent::PRICE && rule->MyKind == GarrisonKind::CHESS_PRICE)
				price = std::max<std::int64_t>(0, price - static_cast<std::int64_t>(rule->MyPrice));
		if (_player.MyBand && _player.MyBandState.MyDiscountRound != _economy.Round())
			for (const auto& effect : _player.MyBand->MyEffects)
				if (effect.MyKind == PreparationBandKind::FIRST_DISCOUNT && BandMember(_player, _slot.MyId, effect.MyBond))
					price = std::min(price, effect.MyCount);
		if (_economy._MyCatalog.At(_slot.MyId).MyKind == PieceKind::CHESS)
			for (std::size_t i = 0; i < _MyBondEffects.size(); ++i)
			{
				const auto& effect = _MyBondEffects[i];
				const auto stage = _player.MyBondCounters[i];
				if (effect.MyKind == PreparationBondKind::DISCOUNT && stage > 0 &&
					(stage >= 2 || BandMember(_player, _slot.MyId, effect.MyDiscountBond)))
					price = std::max<std::int64_t>(0, price - effect.MyCount);
			}
		return std::max<std::int64_t>(0, price);
	}

	std::optional<std::int64_t> PreparationContent::PriceOf(std::string_view _player, const ShopSlot& _slot) const
	{
		const auto found = std::ranges::find(_MyPlayers, _player, &Player::MyId);
		if (found == _MyPlayers.end()) return {};
		return BandPrice(_MyEconomy, *found, _slot);
	}

	void PreparationContent::BandIncome(const EconomySession& _economy, const Player& _player, std::int64_t& _income) const
	{
		if (!_player.MyBand) return;
		const auto index = static_cast<std::size_t>(std::ranges::find(_economy._MyPlayers, _player.MyId,
			[](const auto& _value) -> const std::string& { return _value.MyView.MyPlayerId; }) - _economy._MyPlayers.begin());
		for (const auto& effect : _player.MyBand->MyEffects)
		{
			if (effect.MyKind == PreparationBandKind::INCOME && _economy.Round() == effect.MyRound) _income = effect.MyCount;
			if (effect.MyKind != PreparationBandKind::INTEREST || effect.MyThreshold <= 0 || effect.MyCount <= 0) continue;
			const auto capital = _economy._MyPlayers[index].MyView.MyFunds / effect.MyThreshold;
			const auto bonus = capital > effect.MyMaximum / effect.MyCount ? effect.MyMaximum : capital * effect.MyCount;
			if (_income > std::numeric_limits<std::int64_t>::max() - bonus) throw std::overflow_error("strategy income overflow");
			_income += bonus;
		}
	}

	void PreparationContent::BeginRound(int _round, Random& _random)
	{
		auto economy = _MyEconomy;
		auto players = _MyPlayers;
		auto random = _random;
		PreparationHooks hooks(*this, economy, players, random);
		for (std::size_t i = 0; i < players.size(); ++i)
			if (players[i].MyBand && players[i].MyBand->MyKeepFunds) economy._MyPlayers[i].MyView.MyKeepRemainingFunds = true;
		economy.BeginRound(_round, false);
		_MyEconomy.Commit(std::move(economy)); _MyPlayers.swap(players); _random = random;
	}

	void PreparationContent::OnPreparationEnd(Random& _random)
	{
		if (_MyEconomy.Phase() != PreparationPhase::PREPARING || _MyBandEndRound >= _MyEconomy.Round())
			throw std::invalid_argument("strategy preparation end requires an unfinished preparation");
		auto economy = _MyEconomy;
		auto players = _MyPlayers;
		auto random = _random;
		PreparationHooks hooks(*this, economy, players, random);
		std::vector<EconomyEvent> events;
		for (std::size_t i = 0; i < players.size(); ++i)
			if (economy._MyPlayers[i].MyView.MyAlive)
			{
				PreparationHooks::Scope scope(hooks, true);
				players[i].MyPrepEndedRound = economy.Round();
				ApplyBand(economy, players, i, BandEvent::PREP_END, random, events);
				ApplyBonds(economy, players, i, BondEvent::PREP_END, random, events);
				RunGarrisons(economy, players, i, GarrisonEvent::PREP_END, random, events);
			}
		_MyEconomy.Commit(std::move(economy)); _MyPlayers.swap(players); _random = random;
		++_MyEconomy._MyRevision;
		_MyBandEndRound = _MyEconomy.Round();
		OnPreparationEnd();
	}

	void PreparationContent::ApplyBand(EconomySession& _economy, std::vector<Player>& _players, std::size_t _index,
		BandEvent _event, Random& _random, std::vector<EconomyEvent>& _events, std::string_view _definition, std::int64_t* _amount) const
	{
		auto& player = _players[_index];
		if (!player.MyBand) return;
		auto& owned = _economy._MyPlayers[_index];
		auto& view = owned.MyView;
		auto& state = player.MyBandState;
		const auto roundCount = [&]() -> std::int64_t&
		{
			if (state.MyRound != _economy.Round()) { state.MyRound = _economy.Round(); state.MyRoundCount = 0; }
			return state.MyRoundCount;
		};
		const auto grant = [&](std::string_view _id, bool _requirePool = true)
		{
			const auto* definition = _economy._MyCatalog.Find(_id);
			if (!definition || !EconomySession::Selected(view, *definition)) return false;
			if (_requirePool && definition->MyKind == PieceKind::CHESS)
				if (const auto* stock = _economy.Stock(owned, definition->MyBaseId); stock && stock->MyRemaining < 1) return false;
			const auto uid = _economy.Acquire(owned, *definition, _events);
			_economy.LiftOutOfRange(owned); _economy.FillHand(view);
			if (uid) _events.emplace_back(EconomyEvent{.MyKind = EventKind::GRANTED,
				.MyDefinitionId = _economy.GrantDefinition(view, *uid, definition->MyId, _events), .MyUid = *uid});
			return uid.has_value();
		};
		const auto roll = [&](std::string_view _bond, int _tier)
		{
			RollOptions options; options.MyMaxTier = _tier;
			std::vector<std::string_view> members;
			if (!_bond.empty())
			{
				members.reserve(player.MyRoster.size());
				for (const auto& member : player.MyRoster)
					if (std::ranges::contains(member.MyBonds, _bond)) members.emplace_back(member.MyId);
				options.MyIncluded = members;
			}
			return _economy.RollChess(player.MyId, _random, options);
		};
		const auto pool = [&](std::string_view _pool)
		{
			return RollContentPool(_economy._MyCatalog, _economy, player.MyId, _MyPools, player.MyRoster, _random, _pool, view.MyLevel);
		};
		const auto chessSlots = [&]
		{
			std::vector<std::size_t> indices; indices.reserve(view.MyShop.size());
			for (std::size_t i = 0; i < view.MyShop.size(); ++i)
				if (const auto& slot = view.MyShop[i]; slot && !slot->MySold && _economy._MyCatalog.At(slot->MyId).MyKind == PieceKind::CHESS)
					indices.emplace_back(i);
			return indices;
		};
		const auto setSlot = [&](std::size_t _slot, std::string_view _id, bool _frozen = false)
		{
			const auto& definition = _economy._MyCatalog.At(_id);
			view.MyShop[_slot] = ShopSlot{.MyId = definition.MyId, .MyPrice = definition.MyPrice, .MyFrozen = _frozen};
		};
		const auto offer = [&](const PreparationBandEffect& _effect, std::int64_t _times)
		{
			const auto count = std::max<std::int64_t>(1, _effect.MyCount);
			std::vector<ShopSlot> slots; slots.reserve(static_cast<std::size_t>(std::min<std::int64_t>(count, 6)));
			for (std::int64_t i = 0; i < count * 6 && static_cast<std::int64_t>(slots.size()) < count; ++i)
			{
				const auto choice = pool(_effect.MyPool);
				if (!choice || choice->MyKind != PieceKind::ITEM) break;
				if (std::ranges::find(slots, choice->MyId, &ShopSlot::MyId) == slots.end())
					slots.emplace_back(ShopSlot{.MyId = choice->MyId, .MyPrice = _economy._MyRules.MyRewardPrice});
			}
			if (slots.size() > 6) slots.resize(6);
			if (!slots.empty()) for (std::int64_t i = 0; i < _times; ++i) view.MyOffers.emplace_back(slots);
		};
		for (const auto& effect : player.MyBand->MyEffects)
		{
			using Kind = PreparationBandKind;
			switch (effect.MyKind)
			{
			case Kind::TIER_LAYERS:
				if (_event == BandEvent::PREP_END && effect.MyCount > 0)
				{
					std::map<int, std::vector<const Piece*>> tiers;
					for (const auto& piece : view.MyBoard)
						if (piece && !piece->IsToken() && _economy._MyCatalog.At(piece->MyId).MyKind == PieceKind::CHESS)
							tiers[_economy._MyCatalog.At(piece->MyId).MyTier].emplace_back(&*piece);
					for (const auto& [tier, pieces] : tiers)
					{
						(void)tier;
						const auto& chosen = *pieces[_random.Index(static_cast<std::uint32_t>(pieces.size()))];
						for (const auto bond : ItemBonds(player, chosen)) (void)AddContentLayers(_economy, _players, _index, bond, static_cast<double>(effect.MyCount), _random, _events);
					}
				}
				break;
			case Kind::REFRESH_BOND:
				if (_event == BandEvent::REFRESH && view.MyRoundStatistics.MyRefreshes <= 2)
				{
					auto indices = chessSlots();
					const auto total = indices.size();
					std::erase_if(indices, [&](auto _i) { return BandMember(player, view.MyShop[_i]->MyId, effect.MyBond); });
					auto have = static_cast<std::int64_t>(total - indices.size());
					_random.Shuffle(indices.begin(), indices.end());
					for (const auto i : indices)
					{
						if (have >= std::max<std::int64_t>(1, effect.MyCount)) break;
						const auto id = roll(effect.MyBond, view.MyLevel);
						if (!id) break;
						setSlot(i, *id); ++have;
					}
				}
				break;
			case Kind::COPY_SHOP:
				if (_event == BandEvent::REFRESH)
				{
					auto indices = chessSlots();
					if (indices.size() < 2) break;
					int best = -1;
					for (const auto i : indices) best = std::max(best, _economy._MyCatalog.At(view.MyShop[i]->MyId).MyTier);
					auto candidates = indices;
					std::erase_if(candidates, [&](auto _i) { return _economy._MyCatalog.At(view.MyShop[_i]->MyId).MyTier != best; });
					const auto source = candidates[_random.Index(static_cast<std::uint32_t>(candidates.size()))];
					std::erase(indices, source);
					const auto destination = indices[_random.Index(static_cast<std::uint32_t>(indices.size()))];
					setSlot(destination, view.MyShop[source]->MyId, true);
				}
				break;
			case Kind::ROUND_GIFT:
			case Kind::PERIODIC_GIFT:
				if (_event == BandEvent::ROUND_START && (effect.MyKind == Kind::ROUND_GIFT ? _economy.Round() == effect.MyRound :
					effect.MyRound > 0 && _economy.Round() % effect.MyRound == 0))
					for (std::int64_t i = 0; i < std::max<std::int64_t>(1, effect.MyCount); ++i)
						(void)grant(effect.MyChess, effect.MyKind != Kind::ROUND_GIFT);
				break;
			case Kind::ROUND_POOL:
				if (_event == BandEvent::ROUND_START && _economy.Round() == effect.MyRound)
					for (std::int64_t i = 0; i < std::max<std::int64_t>(1, effect.MyCount); ++i)
						if (const auto choice = pool(effect.MyPool)) (void)grant(choice->MyId);
				break;
			case Kind::SPEND_CHESS:
			case Kind::SPEND_POOL:
				if (_event == BandEvent::SPEND && _amount && *_amount > 0 && !state.MyComplete)
				{
					if (state.MySpent > std::numeric_limits<std::int64_t>::max() - *_amount) throw std::overflow_error("strategy spending overflow");
					state.MySpent += *_amount;
					if (effect.MyKind == Kind::SPEND_POOL)
					{
						if (state.MySpent >= effect.MyThreshold)
							if (const auto choice = pool(effect.MyPool); choice && grant(choice->MyId)) state.MyComplete = true;
					}
					else if (effect.MyThreshold > 0)
						while (state.MySpent >= effect.MyThreshold)
						{
							state.MySpent -= effect.MyThreshold;
							for (std::int64_t i = 0; i < std::max<std::int64_t>(1, effect.MyCount); ++i)
								if (const auto id = roll({}, view.MyLevel)) (void)grant(*id);
						}
				}
				break;
			case Kind::SPECIAL_REFRESH:
				if (_event == BandEvent::LEVEL_UP && std::ranges::contains(effect.MyLevels, view.MyLevel)) ++state.MyPendingRefreshes;
				if (_event == BandEvent::REFRESH && state.MyPendingRefreshes > 0)
				{
					--state.MyPendingRefreshes;
					for (const auto i : chessSlots())
					{
						if (BandMember(player, view.MyShop[i]->MyId, effect.MyBond)) continue;
						const auto id = roll(effect.MyBond, view.MyLevel);
						if (!id) break;
						setSlot(i, *id);
					}
				}
				break;
			case Kind::PERIODIC_BOND:
				if (_event == BandEvent::ROUND_START && _economy.Round() >= effect.MyRound &&
					(_economy.Round() - effect.MyRound) % std::max(1, effect.MyPeriod) == 0)
					for (std::int64_t i = 0; i < std::max<std::int64_t>(1, effect.MyCount); ++i)
					{
						auto id = roll(effect.MyBond, view.MyLevel);
						if (!id) id = roll(effect.MyBond, 6);
						if (id) (void)grant(*id);
					}
				break;
			case Kind::FIRST_DISCOUNT:
				if (_event == BandEvent::BUY && BandMember(player, _definition, effect.MyBond)) state.MyDiscountRound = _economy.Round();
				break;
			case Kind::ACTIVE_LAYERS:
				if (_event == BandEvent::ROUND_START && _economy.Round() == effect.MyRound)
				{
					const auto states = ItemBondStates(_economy, player, _index);
					const auto count = std::ranges::count_if(states, &BondState::MyActive);
					const auto gain = count == effect.MyThreshold ? effect.MyCount : effect.MyAlternate;
					if (gain > 0) for (const auto& bond : states) if (bond.MyActive) (void)AddContentLayers(_economy, _players, _index, bond.MyId, static_cast<double>(gain), _random, _events);
				}
				break;
			case Kind::LEVEL_OFFER:
				if (_event == BandEvent::LEVEL_UP) offer(effect, 1);
				break;
			case Kind::SELL_EXCHANGE:
				if (_event == BandEvent::SOLD && _amount && !_economy._MyCatalog.At(_definition).MyGolden && roundCount() < std::max<std::int64_t>(1, effect.MyCount))
				{
					const auto indices = chessSlots();
					if (indices.empty()) break;
					const auto i = indices[_random.Index(static_cast<std::uint32_t>(indices.size()))];
					if (grant(view.MyShop[i]->MyId)) { setSlot(i, _definition); *_amount = 0; ++roundCount(); }
				}
				break;
			case Kind::SHOP_GIFT:
				if (_event == BandEvent::ROUND_START && effect.MyRound > 0 && _economy.Round() % effect.MyRound == 0)
					for (std::int64_t k = 0; k < std::max<std::int64_t>(1, effect.MyCount); ++k)
					{
						const auto indices = chessSlots();
						if (indices.empty()) break;
						const auto i = indices[_random.Index(static_cast<std::uint32_t>(indices.size()))];
						if (grant(view.MyShop[i]->MyId)) view.MyShop[i].reset();
					}
				break;
			case Kind::BUY_PENDING:
				if (_event == BandEvent::BUY && BandMember(player, _definition, effect.MyBond))
				{
					const auto gain = std::min(effect.MyCount, effect.MyMaximum - roundCount());
					if (gain <= 0) break;
					if (view.MyPendingFunds > std::numeric_limits<std::int64_t>::max() - gain) throw std::overflow_error("strategy pending funds overflow");
					view.MyPendingFunds += gain; roundCount() += gain;
				}
				break;
			case Kind::REFRESH_GIFT:
				if (_event == BandEvent::REFRESH && effect.MyThreshold > 0 && ++state.MyRefreshes % static_cast<std::uint64_t>(effect.MyThreshold) == 0 &&
					roundCount() < effect.MyMaximum)
					if (const auto id = roll(effect.MyBond, view.MyLevel); id && grant(*id)) ++roundCount();
				break;
			case Kind::PERIODIC_OFFER:
				if (_event == BandEvent::ROUND_START && effect.MyRound > 0 && _economy.Round() % effect.MyRound == 0)
					offer(effect, std::max<std::int64_t>(1, effect.MyAlternate));
				break;
			case Kind::TRIGGER_GAIN:
				if (_event == BandEvent::ROUND_START)
				{
					std::vector<Piece> chosen; chosen.reserve(view.MyBoard.size());
					for (int col = 10; col >= 2; --col)
						for (int row = 9; row <= 12; ++row)
						{
							const auto& piece = view.MyBoard[BoardPosition{row, col}.Index()];
							if (piece && !piece->IsToken() && std::ranges::any_of(Garrisons(player, piece->MyId), [&](auto _id)
								{ const auto* rule = GarrisonRule(_id); return rule && rule->MyEvent == GarrisonEvent::GAIN; })) chosen.push_back(*piece);
						}
					const auto count = std::min(chosen.size(), static_cast<std::size_t>(std::max<std::int64_t>(1, effect.MyCount)));
					for (std::size_t i = 0; i < count; ++i)
						if (const auto loc = _economy.Locate(view, chosen[i].MyUid))
						{
							const auto piece = loc->Get(view);
							RunPieceGarrisons(_economy, _players, _index, GarrisonEvent::GAIN, piece, piece,
								!loc->MyOnBoard && !loc->MyInTemporary, true, _random, _events);
						}
				}
				break;
			case Kind::INCOME:
			case Kind::INTEREST:
				break;
			}
		}
	}
}
