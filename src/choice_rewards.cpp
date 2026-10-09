#include <limits>
#include <stronghold/domain/preparation_content.hpp>

namespace Stronghold
{
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
		std::span<const ContentPoolRecord> _pools, std::span<const BondRule> _bonds, std::span<const ChoiceRewardPlayerConfig> _players)
		: _MyEconomy(_economy), _MyLedger(_ledger), _MyRules(_rules), _MyPools(_pools)
	{
		if (_players.size() != _economy._MyPlayers.size() || _players.size() != _ledger.Players().size())
			throw std::invalid_argument("choice reward player roster mismatch");
		_MyPlayers.reserve(_players.size());
		for (const auto& player : _economy._MyPlayers)
		{
			const auto& id = player.MyView.MyPlayerId;
			const auto found = std::ranges::find(_players, id, &ChoiceRewardPlayerConfig::MyPlayerId);
			if (found == _players.end() || std::ranges::find(_ledger.Players(), id, &MatchPlayerProgress::MyPlayerId) == _ledger.Players().end())
				throw std::invalid_argument("choice reward player missing");
			_MyPlayers.emplace_back(Player{.MyId = id, .MyRoster = {found->MyRoster.begin(), found->MyRoster.end()}, .MyLayers = BondLayerLedger(_bonds)});
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
				if (uid) receipt.MyEvents.emplace_back(EconomyEvent{.MyKind = EventKind::GRANTED, .MyDefinitionId = economy.Locate(view, *uid)->Get(view).MyId, .MyUid = *uid});
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
							if (const auto change = player.MyLayers.Add(bond, static_cast<double>(action.MyCount))) receipt.MyLayers.emplace_back(*change);
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
		const auto found = std::ranges::find(_MyPlayers, _player, &Player::MyId);
		return found == _MyPlayers.end() ? std::nullopt : found->MyLayers.Add(_bond, _count);
	}
}
