#include <limits>
#include <cmath>
#include <stronghold/domain/preparation_content.hpp>
#include "preparation_hooks.hpp"

namespace Stronghold
{
	std::expected<bool, CommandError> PreparationContent::SynchronizeLayers(std::string_view _player, int _round,
		std::span<const BondNumber> _gains)
	{
		if (!_MyBondEffects.empty()) throw std::invalid_argument("bond rewards require the host random stream");
		const auto found = std::ranges::find(_MyPlayers, _player, &Player::MyId);
		if (found == _MyPlayers.end()) return std::unexpected(CommandError::UNKNOWN_PLAYER);
		if (_round <= 0 || _round != _MyEconomy.Round() || _round != _MyItemRound) return std::unexpected(CommandError::STALE_ROUND);
		if (_MyEconomy.Phase() != PreparationPhase::CLOSED) return std::unexpected(CommandError::WRONG_PHASE);
		const auto index = static_cast<std::size_t>(found - _MyPlayers.begin());
		if (!_MyEconomy._MyPlayers[index].MyView.MyAlive) return std::unexpected(CommandError::ELIMINATED);
		return found->MyLayers.Synchronize(static_cast<unsigned>(_round), _gains);
	}

	namespace
	{
		std::size_t BenchCount(const PlayerView& _view)
		{
			const auto present = [](const auto& _piece) { return _piece.has_value(); };
			return static_cast<std::size_t>(std::ranges::count_if(_view.MyHand, present) + std::ranges::count_if(_view.MyTemporary, present));
		}

		std::optional<bool> BenchGate(ChoiceGate _gate, std::size_t _count)
		{
			if (_gate.MyKind == ChoiceGateKind::BENCH_AT_LEAST) return _count >= _gate.MyCount;
			if (_gate.MyKind == ChoiceGateKind::BENCH_AT_MOST) return _count <= _gate.MyCount;
			return {};
		}
	}

	PreparationContent::PreparationContent(EconomySession& _economy, RoundLedger& _ledger, std::span<const ChoiceRewardRule> _rules,
		std::span<const ContentPoolRecord> _pools, std::span<const BondRule> _bonds,
		std::span<const ChoiceRewardPlayerConfig> _players, std::span<const PreparationItemRule> _items, PreparationArtRules _arts,
		std::span<const PreparationBandRule> _bands, PreparationGarrisonRules _garrisons, std::span<const PreparationBondEffect> _bondEffects)
		: _MyEconomy(_economy), _MyLedger(_ledger), _MyRules(_rules), _MyPools(_pools), _MyItems(_items), _MyBonds(_bonds), _MyGarrisons(_garrisons), _MyBondEffects(_bondEffects)
	{
		if (_players.size() != _economy._MyPlayers.size() || _players.size() != _ledger.Players().size())
			throw std::invalid_argument("choice reward player roster mismatch");
		for (const auto& effect : _bondEffects)
		{
			if (std::ranges::find(_bonds, effect.MyBond, &BondRule::MyId) == _bonds.end() ||
				effect.MyKind < PreparationBondKind::PREP_LAYERS || effect.MyKind > PreparationBondKind::ITEM_MILESTONE ||
				effect.MyStep < 0 || effect.MyCount < 0 || effect.MyCount > 10000 || effect.MyHighCount < 0 || effect.MyHighCount > 10000 ||
				!std::isfinite(effect.MyBaseChance) || !std::isfinite(effect.MyChancePerLayer))
				throw std::invalid_argument("invalid preparation bond effect");
			if ((effect.MyKind == PreparationBondKind::COIN_MILESTONE || effect.MyKind == PreparationBondKind::ITEM_MILESTONE ||
				effect.MyKind == PreparationBondKind::DISCOUNT) && effect.MyStep == 0)
				throw std::invalid_argument("invalid preparation bond milestone");
			if (effect.MyKind == PreparationBondKind::DISCOUNT && effect.MyHighStep < effect.MyStep)
				throw std::invalid_argument("unordered preparation bond discount milestones");
		}
		if (_garrisons.MyInvestRepeat < 1 || _garrisons.MyInvestRepeat > 100 ||
			!std::isfinite(_garrisons.MyInvestLayer) || _garrisons.MyInvestLayer <= 0)
			throw std::invalid_argument("invalid preparation garrison multiplier");
		for (std::size_t i = 0; i < _garrisons.MyEffects.size(); ++i)
		{
			const auto& rule = _garrisons.MyEffects[i];
			if (rule.MyId.empty() || rule.MyEvent < GarrisonEvent::GAIN || rule.MyEvent > GarrisonEvent::REFRESH ||
				rule.MyTrigger < GarrisonEvent::GAIN || rule.MyTrigger > GarrisonEvent::REFRESH ||
				rule.MyKind < GarrisonKind::ADD_BOND || rule.MyKind > GarrisonKind::FRONT_SAME_EFFECT_PREP_START ||
				rule.MyMethod < GarrisonMethod::NONE || rule.MyMethod > GarrisonMethod::BOND_TIERS)
				throw std::invalid_argument("invalid preparation garrison");
			for (std::size_t j = 0; j < i; ++j)
				if (_garrisons.MyEffects[j].MyId == rule.MyId) throw std::invalid_argument("duplicate preparation garrison");
			for (const auto value : {rule.MyCount, rule.MyMultiplier, rule.MyLayer, rule.MyMaximum, rule.MyCheckCount, rule.MyPrice, rule.MyRefreshCount})
				if (!std::isfinite(value) || value < 0 || value > 10000) throw std::invalid_argument("invalid preparation garrison number");
			for (const auto count : rule.MyBondCounts)
				if (!std::isfinite(count) || count < 0 || count > 10000) throw std::invalid_argument("invalid garrison bond award");
			for (const auto& weight : rule.MyGoldenWeights)
				if (!std::isfinite(weight.MyWeight) || weight.MyWeight <= 0 || weight.MyWeight > 10000)
					throw std::invalid_argument("invalid garrison equip weight");
		}
		for (std::size_t i = 0; i < _bands.size(); ++i)
		{
			const auto& band = _bands[i];
			if (band.MyId.empty()) throw std::invalid_argument("empty preparation strategy ID");
			for (std::size_t j = 0; j < i; ++j)
				if (_bands[j].MyId == band.MyId) throw std::invalid_argument("duplicate preparation strategy ID");
			for (const auto& effect : band.MyEffects)
			{
				if (effect.MyKind < PreparationBandKind::TIER_LAYERS || effect.MyKind > PreparationBandKind::TRIGGER_GAIN ||
					effect.MyCount < 0 || effect.MyCount > 10000 || effect.MyMaximum < 0 || effect.MyThreshold < 0 ||
					effect.MyAlternate < 0 || effect.MyAlternate > 10000 || effect.MyRound < 0 || effect.MyRound > 10000 || effect.MyPeriod < 1)
					throw std::invalid_argument("invalid preparation strategy parameters");
				for (const auto level : effect.MyLevels)
					if (level < 1 || level > _economy._MyRules.MyMaxLevel) throw std::invalid_argument("invalid strategy shop level");
			}
		}
		_MyPlayers.reserve(_players.size());
		for (const auto& player : _economy._MyPlayers)
		{
			const auto& id = player.MyView.MyPlayerId;
			const auto found = std::ranges::find(_players, id, &ChoiceRewardPlayerConfig::MyPlayerId);
			if (found == _players.end() || std::ranges::find(_ledger.Players(), id, &MatchPlayerProgress::MyPlayerId) == _ledger.Players().end())
				throw std::invalid_argument("choice reward player missing");
			const auto band = std::ranges::find(_bands, found->MyStrategy, &PreparationBandRule::MyId);
			if (!found->MyStrategy.empty() && band == _bands.end()) throw std::invalid_argument("unknown preparation strategy");
			_MyPlayers.emplace_back(Player{.MyId = id, .MyRoster = {found->MyRoster.begin(), found->MyRoster.end()},
				.MyLayers = BondLayerLedger(_bonds), .MyInactiveBonds = {found->MyInactiveBonds.begin(), found->MyInactiveBonds.end()},
				.MyBand = band == _bands.end() ? nullptr : &*band, .MyBondCounters = std::vector<std::int64_t>(_bondEffects.size())});
		}
		for (std::size_t i = 0; i < _rules.size(); ++i)
		{
			const auto& rule = _rules[i];
			if (rule.MyId.empty()) throw std::invalid_argument("empty choice reward ID");
			for (std::size_t j = 0; j < i; ++j) if (_rules[j].MyId == rule.MyId) throw std::invalid_argument("duplicate choice reward ID");
			for (const auto& action : rule.MyActions)
				if (action.MyCount < 0 || action.MyKind < ChoiceRewardKind::ITEM_POOL || action.MyKind > ChoiceRewardKind::BOND_CHESS)
					throw std::invalid_argument("invalid choice reward action");
		}
		for (std::size_t i = 0; i < _items.size(); ++i)
		{
			const auto& item = _items[i];
			const auto* definition = _economy._MyCatalog.Find(item.MyId);
			if (!definition || definition->MyKind != PieceKind::ITEM || definition->MyItemUse != item.MyUse)
				throw std::invalid_argument("preparation item does not match catalog");
			for (std::size_t j = 0; j < i; ++j)
				if (_items[j].MyId == item.MyId) throw std::invalid_argument("duplicate preparation item ID");
			for (const auto& effect : item.MyEffects)
			{
				if (effect.MyCount < 0 || effect.MyMinimum < 0 || effect.MyMaximum < effect.MyMinimum ||
					effect.MyMaximum - effect.MyMinimum >= UINT32_MAX)
					throw std::invalid_argument("invalid preparation item parameters");
			}
			for (const auto offset : item.MyRange)
				if (offset.MyRow == std::numeric_limits<int>::min() || offset.MyColumn == std::numeric_limits<int>::min())
					throw std::invalid_argument("preparation item range cannot be rotated");
		}
		_MyTrainingBounties.reserve(_arts.MyBounties.size()); _MyBandBounties.reserve(_arts.MyBounties.size());
		for (const auto& card : _arts.MyBounties)
		{
			if (card.MyKind != ChoiceCardKind::BOUNTY || card.MyEnemyId.empty() ||
				std::ranges::contains(_arts.MyInactiveEnemies, card.MyEnemyId)) continue;
			if (card.MyCount < 1 || card.MyCount > 20 || card.MyCoins < 0 || card.MyBattles < 1 || card.MyBattles > 99)
				throw std::invalid_argument("invalid art bounty");
			if (card.MyPerfect) _MyTrainingBounties.emplace_back(&card);
			constexpr std::string_view Prefix = "enemyeffect_b_";
			if (card.MyId.starts_with(Prefix) && card.MyId.size() > Prefix.size() &&
				std::ranges::all_of(card.MyId.substr(Prefix.size()), [](char _c) { return _c >= '0' && _c <= '9'; }))
				_MyBandBounties.emplace_back(&card);
			if (card.MyTier && *card.MyTier <= 2) _MyFallbackBounties[card.MyPerfect ? 1 : 0].emplace_back(&card);
		}
	}

	const ChoiceRewardRule* PreparationContent::Rule(std::string_view _id) const
	{
		const auto found = std::ranges::find(_MyRules, _id, &ChoiceRewardRule::MyId);
		return found == _MyRules.end() ? nullptr : &*found;
	}

	std::expected<ChoiceRewardResult, ChoiceRewardError> PreparationContent::Apply(std::string_view _picker, const ChoiceCard& _card, Random& _random)
	{
		const auto picker = std::ranges::find(_MyPlayers, _picker, &Player::MyId);
		if (picker == _MyPlayers.end()) return std::unexpected(ChoiceRewardError::UNKNOWN_PLAYER);
		const auto index = static_cast<std::size_t>(picker - _MyPlayers.begin());
		if (!_MyEconomy._MyPlayers[index].MyView.MyAlive) return std::unexpected(ChoiceRewardError::ELIMINATED);
		const auto* rule = Rule(_card.MyId);
		if (_card.MyKind == ChoiceCardKind::ITEM)
		{
			const auto* item = _MyEconomy._MyCatalog.Find(_card.MyId);
			if (!item || item->MyKind != PieceKind::ITEM) return std::unexpected(ChoiceRewardError::BAD_CARD);
		}
		else if (!rule || rule->MyKind != _card.MyKind) return std::unexpected(ChoiceRewardError::BAD_CARD);

		auto economy = _MyEconomy;
		auto ledger = _MyLedger;
		auto players = _MyPlayers;
		auto random = _random;
		PreparationHooks hooks(*this, economy, players, random);
		ChoiceRewardResult result;
		result.MyRecipients.reserve(_card.MyKind == ChoiceCardKind::TACTIC && _card.MyTeam ? players.size() : 1);
		const auto apply = [&](std::size_t _index)
		{
			auto& player = players[_index];
			auto& owned = economy._MyPlayers[_index];
			auto& view = owned.MyView;
			auto& receipt = result.MyRecipients.emplace_back(ChoiceRecipient{.MyPlayerId = player.MyId});
			const auto grant = [&](std::string_view _id)
			{
				const auto& definition = economy._MyCatalog.At(_id);
				if (definition.MyKind == PieceKind::CHESS)
					if (const auto* stock = economy.Stock(owned, definition.MyBaseId); stock && stock->MyRemaining < 1) return;
				const auto uid = economy.Acquire(owned, definition, receipt.MyEvents);
				economy.LiftOutOfRange(owned); economy.FillHand(view);
				if (uid) receipt.MyEvents.emplace_back(EconomyEvent{.MyKind = EventKind::GRANTED, .MyDefinitionId = economy.GrantDefinition(view, *uid, definition.MyId, receipt.MyEvents), .MyUid = *uid});
			};
			const auto nextReference = [&]
			{
				if (player.MySequence == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("choice reference sequence overflow");
				return "choice:" + std::string(rule->MyId) + "#" + std::to_string(++player.MySequence);
			};
			if (_card.MyKind == ChoiceCardKind::ITEM) grant(_card.MyId);
			else if (_card.MyKind == ChoiceCardKind::BOUNTY)
			{
				if (!rule->MyEnemyId.empty())
				{
					ActiveBounty bounty{.MyCard = WaveBounty{.MyId = "bounty:" + std::to_string(++economy._MyNextUid), .MyEnemyId = rule->MyEnemyId,
						.MyCount = rule->MyEnemyCount, .MyCoins = rule->MyCoins, .MyPerfect = rule->MyPerfect}, .MyRoundsLeft = rule->MyBattles};
					if (!ledger.AddBounty(player.MyId, std::move(bounty))) throw std::logic_error("choice bounty rejected by settlement ledger");
				}
			}
			else
			{
				for (const auto& action : rule->MyActions)
				{
					switch (action.MyKind)
					{
					case ChoiceRewardKind::FUNDS: EconomySession::AddFunds(view, action.MyCount); break;
					case ChoiceRewardKind::FREE_REFRESH:
						if (view.MyFreeRefreshes > std::numeric_limits<std::uint64_t>::max() - static_cast<std::uint64_t>(action.MyCount)) throw std::overflow_error("choice refresh overflow");
						view.MyFreeRefreshes += static_cast<std::uint64_t>(action.MyCount); break;
					case ChoiceRewardKind::UPGRADE_ITEM:
					case ChoiceRewardKind::UPGRADE_CHESS:
						if (action.MyCount) view.MyPurchaseUpgrades.emplace_back(PurchaseUpgrade{.MyKind = action.MyKind == ChoiceRewardKind::UPGRADE_ITEM ? PieceKind::ITEM : PieceKind::CHESS,
							.MyRemaining = static_cast<std::uint64_t>(action.MyCount), .MySource = nextReference()});
						break;
					case ChoiceRewardKind::LAYERS:
						for (const auto bond : action.MyBonds)
							if (const auto change = AddContentLayers(economy, players, _index, bond, static_cast<double>(action.MyCount), random, receipt.MyEvents)) receipt.MyLayers.emplace_back(*change);
						break;
					case ChoiceRewardKind::ITEM_POOL:
						for (std::int64_t i = 0; i < action.MyCount; ++i)
						{
							auto draw = RollContentPool(economy._MyCatalog, economy, player.MyId, _MyPools, player.MyRoster, random, action.MyPool, view.MyLevel);
							auto id = draw && draw->MyKind == PieceKind::ITEM ? std::optional(std::move(draw->MyId))
								: RollContentItem(economy._MyCatalog, _MyPools, random, ContentItemDraw{.MyTier = 1});
							if (id) grant(*id);
						}
						break;
					case ChoiceRewardKind::BOND_CHESS:
					{
						std::vector<std::string_view> members; members.reserve(player.MyRoster.size());
						for (const auto& member : player.MyRoster)
							if (std::ranges::any_of(action.MyBonds, [&](std::string_view _bond) { return std::ranges::find(member.MyBonds, _bond) != member.MyBonds.end(); })) members.emplace_back(member.MyId);
						RollOptions options; options.MyIncluded = members;
						for (std::int64_t i = 0; i < action.MyCount; ++i)
						{
							options.MyMaxTier = std::max(1, view.MyLevel);
							auto id = economy.RollChess(player.MyId, random, options);
							if (!id) { options.MyMaxTier = 6; id = economy.RollChess(player.MyId, random, options); }
							if (id) grant(*id);
						}
						break;
					}
					}
				}
				for (const auto& setting : rule->MyDevices)
				{
					const auto existing = std::ranges::find(player.MyDevices, setting.MyAlias, &ChoiceDeviceSetting::MyAlias);
					if (existing == player.MyDevices.end()) player.MyDevices.emplace_back(setting);
					else existing->MyActive = setting.MyActive;
				}
				if (rule->MyBattleEffect || !rule->MyDevices.empty())
				{
					auto id = nextReference();
					player.MyEffects.emplace_back(ChoiceEffectState{.MyId = std::move(id), .MyRule = rule->MyId, .MySequence = player.MySequence,
						.MyRound = static_cast<unsigned>(economy.Round()), .MyPreparationPassed = BenchGate(rule->MyGate, BenchCount(view))});
				}
			}
			economy.LiftOutOfRange(owned); economy.FillHand(view);
		};
		apply(index);
		if (_card.MyKind == ChoiceCardKind::TACTIC && _card.MyTeam)
			for (std::size_t i = 0; i < players.size(); ++i) if (i != index && economy._MyPlayers[i].MyView.MyAlive) apply(i);
		++economy._MyRevision;
		_MyEconomy.Commit(std::move(economy)); _MyLedger = std::move(ledger); _MyPlayers.swap(players); _random = random;
		return result;
	}

	void PreparationContent::OnPreparationEnd()
	{
		for (std::size_t i = 0; i < _MyPlayers.size(); ++i)
		{
			const auto& view = _MyEconomy._MyPlayers[i].MyView;
			if (!view.MyAlive) continue;
			for (auto& effect : _MyPlayers[i].MyEffects)
				if (const auto* rule = Rule(effect.MyRule))
				{
					const auto count = BenchCount(view);
					if (const auto gate = BenchGate(rule->MyGate, count)) { effect.MyPreparationPassed = gate; effect.MyBenchCount = count; }
				}
		}
	}

	std::optional<ChoiceRewardView> PreparationContent::View(std::string_view _player) const
	{
		const auto found = std::ranges::find(_MyPlayers, _player, &Player::MyId);
		if (found == _MyPlayers.end()) return {};
		return ChoiceRewardView{.MyPlayerId = found->MyId, .MyLayers = {found->MyLayers.Entries().begin(), found->MyLayers.Entries().end()},
			.MyBattleEffects = found->MyEffects, .MyDevices = found->MyDevices, .MySequence = found->MySequence};
	}

	std::optional<BondLayerChange> PreparationContent::AddLayers(std::string_view _player, std::string_view _bond, double _count)
	{
		if (!_MyBondEffects.empty()) throw std::invalid_argument("bond rewards require the host random stream");
		const auto found = std::ranges::find(_MyPlayers, _player, &Player::MyId);
		return found == _MyPlayers.end() ? std::nullopt : found->MyLayers.Add(_bond, _count);
	}
}
