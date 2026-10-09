#include <cmath>
#include <stronghold/adapters/reference_choices.hpp>
#include <stronghold/adapters/reference_wave_generation.hpp>
#include <stronghold/runtime/local_match.hpp>

namespace Stronghold
{
	namespace
	{
		LocalMatchOptions NormalizeOptions(LocalMatchOptions _options)
		{
			if (static_cast<unsigned>(_options.MyRulePositionMode) > static_cast<unsigned>(RulePositionMode::INITIAL))
				throw std::invalid_argument("invalid rule position mode");
			if (!_options.MySeed) _options.MySeed = 1;
			return _options;
		}
	}

	LocalMatch::LocalMatch(LocalMatchOptions _options)
		: _MyOptions(NormalizeOptions(std::move(_options))), _MyCatalog(ReferenceCatalog()), _MyRules(ReferenceMatchMode(_MyOptions.MyMode)),
		_MyWaves(ReferenceWaveGeneration(), ReferenceWaveMode(_MyOptions.MyMode), ReferenceWaves()),
		_MyDraftRandom(DeriveSeed(_MyOptions.MySeed, u"draft")), _MyMetaRandom(DeriveSeed(_MyOptions.MySeed, u"meta"))
	{
		if (_MyOptions.MyPlayers.empty() || _MyOptions.MyPlayers.size() > (_MyOptions.MyCapacityExperiment ? 20U : 4U) ||
			(_MyRules.MySolo && _MyOptions.MyPlayers.size() != 1)) throw std::invalid_argument("invalid match seats");
		std::ranges::sort(_MyOptions.MyPlayers, {}, [](const MatchSeat& _seat) { return _seat.MySeat.MySeat; });
		_MyPlayers.reserve(_MyOptions.MyPlayers.size()); _MySeats.reserve(_MyOptions.MyPlayers.size());
		for (auto& seat : _MyOptions.MyPlayers)
		{
			_MySeats.push_back(seat.MySeat);
			_MyPlayers.emplace_back(Player{.MySeat = std::move(seat.MySeat), .MyBot = seat.MyBot,
				.MyInfoReady = seat.MyBot, .MyRoster = seat.MyBot ? PlayerRoster(true) : std::move(seat.MyRoster)});
		}
		(void)PoolGroups(_MySeats, _MyOptions.MyCapacityExperiment, _MyOptions.MyIndependentPools);
		if (_MyOptions.MyCustomRegistry && !_MyOptions.MyCustomRegistry->get().Sealed())
			throw std::invalid_argument("seal custom registry before starting a match");
		for (const auto& binding : _MyOptions.MyCustomBindings)
		{
			if (!_MyOptions.MyCustomRegistry || !FindPlayer(binding.MyPlayerId) ||
				(binding.MyContent.MyTag != ContentTag::CUSTOM_BOND && binding.MyContent.MyTag != ContentTag::CUSTOM_BOND_EFFECT))
				throw std::invalid_argument("invalid match custom binding");
			_MyOptions.MyCustomRegistry->get().Validate(binding.MyContent, binding.MyContent.MyTag);
		}
		_MyOptions.MyPlayers.clear();
		_MyUntimed = _MyRules.MySolo || std::ranges::count(_MyPlayers, false, &Player::MyBot) == 1;
		Random setup(DeriveSeed(_MyOptions.MySeed, u"setup"));
		_MySetup = _MyWaves.Setup(setup);
		_MyBans = DrawMatchBans(_MyRules, _MyCatalog, setup);
		ConfigurePlayers();
		for (const auto& bond : ReferenceBondRules())
		{
			if (std::ranges::contains(_MyRules.MyInactiveBonds, bond.MyId)) continue;
			for (const auto& id : _MyCatalog.VisibleChess())
				if (!_MyBans.MyChess.contains(id) && std::ranges::contains(ReferenceOperator(id).MyBonds, bond.MyId))
				{ _MyLiveBonds.push_back(bond.MyId); break; }
		}
		_MyFields.reserve(_MyPlayers.size()); _MyNormalResults.reserve(_MyPlayers.size());
		_MyBossPairs.reserve((_MyPlayers.size() + 1) / 2); _MyBossWaves.reserve(_MyBossPairs.capacity());
		_MyUniteRounds.reserve(2); _MyHistory.reserve(128);
		_MyLayerScratch.reserve(ReferenceBondRules().size());
	}

	LocalMatch::Player* LocalMatch::FindPlayer(std::string_view _id)
	{
		const auto found = std::ranges::find_if(_MyPlayers, [&](const Player& _player) { return _player.MySeat.MyPlayerId == _id; });
		return found == _MyPlayers.end() ? nullptr : &*found;
	}

	const LocalMatch::Player& LocalMatch::PlayerAt(std::string_view _id) const
	{
		const auto found = std::ranges::find_if(_MyPlayers, [&](const Player& _player) { return _player.MySeat.MyPlayerId == _id; });
		if (found == _MyPlayers.end()) throw std::logic_error("match player missing");
		return *found;
	}

	std::vector<Seat> LocalMatch::AliveSeats() const
	{
		std::vector<Seat> result; result.reserve(_MyPlayers.size());
		for (const auto& player : _MyLedger->Players())
			if (player.MyAlive) result.push_back(PlayerAt(player.MyPlayerId).MySeat);
		return result;
	}

	void LocalMatch::ConfigurePlayers()
	{
		// 配置只在首次收入前修改，先释放借用旧召唤目录的会话，再重建只读目录。
		_MyEconomy.reset();
		for (auto& player : _MyPlayers)
		{
			player.MySummons = player.MyRoster.MakeSummonCatalog();
			player.MyContentRoster = player.MyRoster.MakeContentPoolRoster();
			player.MyBondRoster.clear(); player.MyBondRoster.reserve(ReferenceOperators().size());
			for (const auto& record : ReferenceOperators())
			{
				const auto resolved = player.MyRoster.Resolve(record.MyId);
				player.MyBondRoster.emplace_back(BondRosterRecord{record.MyId, record.MyBaseId, record.MyGolden, resolved.MyBonds});
			}
		}
		_MyEconomy.emplace(_MyCatalog, _MyOptions.MyMode, _MySeats, _MyOptions.MySeed,
			_MyOptions.MyCapacityExperiment, _MyOptions.MyIndependentPools, _MyBans.MyChess,
			ReferenceStages().at(std::string(_MySetup.MyStageId)).MyNormal);
		for (const auto& player : _MyPlayers)
		{
			if (!ConfigurePreparationRoster(*_MyEconomy, player.MySeat.MyPlayerId, player.MyRoster, _MyBans.MyInactive))
				throw std::logic_error("cannot configure match roster");
			if (!_MyEconomy->ConfigureSummons(player.MySeat.MyPlayerId, player.MySummons)) throw std::logic_error("cannot configure match summons");
		}
	}

	void LocalMatch::SetPhase(MatchPhase _phase, std::optional<double> _seconds)
	{
		_MyPhase = _phase;
		_MyDeadline = _seconds ? std::optional(_MyNow + *_seconds) : std::nullopt;
		_MyHistory.push_back({_phase, _MyRound, _MyNow});
	}

	void LocalMatch::Start(double _now)
	{
		if (!std::isfinite(_now) || _now < _MyNow) throw std::invalid_argument("invalid match time");
		if (_MyPhase != MatchPhase::LOBBY) return;
		_MyNow = _now;
		SetPhase(MatchPhase::INFO_CHECK, _MyUntimed ? std::nullopt : std::optional(_MyRules.MyInformationSeconds));
	}

	void LocalMatch::Advance(double _now)
	{
		if (!std::isfinite(_now) || _now < _MyNow) throw std::invalid_argument("match time must be finite and monotonic");
		_MyNow = _now;
		if (_MyPaused) return;
		const bool expired = _MyDeadline && _now >= *_MyDeadline;
		switch (_MyPhase)
		{
		case MatchPhase::INFO_CHECK:
			if (expired || std::ranges::all_of(_MyPlayers, &Player::MyInfoReady)) EnterStrategies();
			break;
		case MatchPhase::BAND_DRAFT:
			_MyStrategy->Advance(_now);
			if (_MyStrategy->Complete()) FinishStrategies();
			else _MyDeadline = _MyUntimed ? std::nullopt : std::optional(_MyStrategy->View().MyDeadline);
			break;
		case MatchPhase::BATTLE_CHECK:
			if (expired) StartRound(1);
			break;
		case MatchPhase::ROUND_START:
			if (expired) EnterChoices();
			break;
		case MatchPhase::SP_DRAFT:
			if (const auto award = _MySpecial->Advance(_now, _MyDraftRandom)) Award(*award);
			if (_MySpecial->Complete()) EnterPreparation();
			else _MyDeadline = _MyUntimed ? std::nullopt : std::optional(_MySpecial->Deadline());
			break;
		case MatchPhase::PREP:
		{
			const auto players = _MyEconomy->PublicView();
			if (expired || std::ranges::all_of(players, [](const auto& _player) { return !_player.MyAlive || _player.MyReady; })) EndPreparation(expired);
			break;
		}
		case MatchPhase::COMBAT:
			if (_MyCombatDone && expired)
			{
				if (auto plan = PlanNextUnite(false)) StartUnite(std::move(*plan));
				else Settle();
			}
			break;
		case MatchPhase::SETTLE:
			(void)_MyLedger->Advance(_now);
			if (expired) AfterSettlement();
			break;
		case MatchPhase::FINAL_ASSAULT:
		case MatchPhase::HIDDEN_CORE:
			if (_MyBossDone && expired)
			{
				if (!_MyAssault->Hidden() && _MyAssault->CanEnterHidden() && !_MySetup.MyHiddenBossId.empty()) StartRound(_MyRules.MyHiddenRound);
				else
				{
					const auto result = _MyAssault->Result();
					Finish(result.MyVictory, result.MyHiddenCleared, result.MyVictory ? "victory" : "defeat");
				}
			}
			break;
		default: break;
		}
	}

	std::expected<void, MatchError> LocalMatch::InfoReady(std::string_view _player)
	{
		if (_MyPhase != MatchPhase::INFO_CHECK) return std::unexpected(MatchFlowError::WRONG_PHASE);
		auto* player = FindPlayer(_player);
		if (!player) return std::unexpected(MatchFlowError::UNKNOWN_PLAYER);
		player->MyInfoReady = true;
		return {};
	}

	void LocalMatch::EnterStrategies()
	{
		_MyStrategy.emplace(_MyRules.StrategyRules(_MyUntimed), _MySeats, _MyRules.StrategyOptions(), _MyDraftRandom, _MyNow);
		SetPhase(MatchPhase::BAND_DRAFT);
		if (!_MyUntimed) _MyDeadline = _MyStrategy->View().MyDeadline;
	}

	std::expected<void, MatchError> LocalMatch::FocusStrategy(std::string_view _player, std::string_view _strategy)
	{
		if (_MyPhase != MatchPhase::BAND_DRAFT) return std::unexpected(MatchFlowError::WRONG_PHASE);
		if (const auto result = _MyStrategy->Focus(_player, _strategy); !result) return std::unexpected(result.error());
		return {};
	}

	std::expected<void, MatchError> LocalMatch::PickStrategy(std::string_view _player, std::string_view _strategy)
	{
		if (_MyPhase != MatchPhase::BAND_DRAFT) return std::unexpected(MatchFlowError::WRONG_PHASE);
		if (const auto result = _MyStrategy->Pick(_player, _strategy); !result) return std::unexpected(result.error());
		_MyDeadline = _MyUntimed || _MyStrategy->Complete() ? std::nullopt : std::optional(_MyStrategy->View().MyDeadline);
		return {};
	}

	std::expected<void, MatchError> LocalMatch::SkipStrategy(std::string_view _player)
	{
		if (_MyPhase != MatchPhase::BAND_DRAFT) return std::unexpected(MatchFlowError::WRONG_PHASE);
		if (const auto result = _MyStrategy->Skip(_player); !result) return std::unexpected(result.error());
		_MyDeadline = _MyUntimed ? std::nullopt : std::optional(_MyStrategy->View().MyDeadline);
		return {};
	}

	void LocalMatch::FinishStrategies()
	{
		const auto draft = _MyStrategy->View();
		std::vector<MatchPlayerProgress> players; players.reserve(_MyPlayers.size());
		std::vector<ChoiceRewardPlayerConfig> configs; configs.reserve(_MyPlayers.size());
		for (const auto& player : _MyPlayers)
		{
			const auto& id = player.MySeat.MyPlayerId;
			const auto picked = std::ranges::find(draft.MyPlayers, id, &StrategyPick::MyPlayerId);
			players.emplace_back(MatchPlayerProgress{.MyPlayerId = id, .MyLife = picked->MyStartingLife, .MyBot = player.MyBot});
			configs.emplace_back(ChoiceRewardPlayerConfig{id, player.MyContentRoster, _MyRules.MyInactiveBonds, picked->MyStrategyId});
			if (picked->MyStrategyId == "band_cannot")
				if (!_MyEconomy->ApplyEconomyEffect(id, KeepRemainingFunds{true})) throw std::logic_error("cannot configure strategy funds");
		}
		_MyLedger.emplace(SettlementRules{_MyRules.MyLifeCapPerRound, _MyOptions.MyRevivalEnabled}, std::move(players));
		_MyContent = std::make_unique<PreparationContent>(*_MyEconomy, *_MyLedger, ReferenceChoiceRewards(), ReferenceContentPools(),
			ReferenceBondRules(), configs, ReferencePreparationItems(), PreparationArtRules{ReferenceChoices().MyCards,
				ReferenceWaveMode(_MyOptions.MyMode).MyInactiveEnemies}, ReferencePreparationBands(), ReferencePreparationGarrisons(), ReferencePreparationBonds());
		SetPhase(MatchPhase::BATTLE_CHECK, _MyRules.MyBattleCheckSeconds);
	}

	std::expected<void, MatchError> LocalMatch::SetLoadout(std::string_view _player, std::span<const LoadoutChoice> _choices)
	{
		if (_MyPhase != MatchPhase::INFO_CHECK) return std::unexpected(MatchFlowError::WRONG_PHASE);
		auto* player = FindPlayer(_player);
		if (!player) return std::unexpected(MatchFlowError::UNKNOWN_PLAYER);
		if (const auto result = player->MyRoster.SetLoadout(_choices); !result) return std::unexpected(result.error());
		ConfigurePlayers();
		return {};
	}

	void LocalMatch::StartRound(unsigned _round)
	{
		_MyBossFields.reset(); _MyFields.clear(); _MyNormalResults.clear(); _MyUniteRounds.clear();
		_MyNormalWave.reset(); _MySpecial.reset(); _MyBossPairs.clear(); _MyBossWaves.clear();
		_MyCombatDone = false; _MyBossDone = false;
		_MyRound = _round;
		const auto alive = AliveSeats();
		if (alive.empty()) { Finish(false, false, "eliminated"); return; }
		const bool boss = _round == _MyRules.MyBossRound || _round == _MyRules.MyHiddenRound;
		if (boss)
		{
			for (std::size_t i = 0; i < alive.size(); i += 2)
			{
				BossFieldGroup pair{.MyCount = static_cast<unsigned>(std::min<std::size_t>(2, alive.size() - i))};
				pair.MySoloTemplate = _MyRules.MySolo || pair.MyCount == 1;
				for (unsigned j = 0; j < pair.MyCount; ++j) pair.MyPlayers[j] = alive[i + j].MyPlayerId;
				_MyBossWaves.push_back(_MyWaves.BuildBoss(_MySetup, _round,
					_round == _MyRules.MyBossRound ? _MySetup.MyBossId : _MySetup.MyHiddenBossId, pair.MySoloTemplate));
				_MyBossPairs.push_back(std::move(pair));
			}
		}
		else _MyNormalWave = _MyWaves.BuildNormal(_MySetup, _round);
		UpdateLayouts();
		_MyContent->BeginRound(static_cast<int>(_round), _MyMetaRandom);
		(void)_MyContent->OnRoundStart(_MyMetaRandom);
		SetPhase(MatchPhase::ROUND_START, 2);
	}

	void LocalMatch::EnterChoices()
	{
		if (!std::ranges::contains(_MyRules.MySpecialRounds, _MyRound)) { EnterPreparation(); return; }
		const auto alive = AliveSeats();
		auto draft = GenerateChoices(ReferenceChoices(), ChoiceGenerationInput{.MyMode = _MyOptions.MyMode, .MyRound = _MyRound,
			.MySolo = _MyRules.MySolo, .MyStage = _MySetup.MyStageId, .MyLiveBonds = std::span<const std::string_view>(_MyLiveBonds),
			.MyPlayerCount = alive.size(), .MyCapacityExperiment = _MyOptions.MyCapacityExperiment}, _MyDraftRandom);
		if (!draft) { EnterPreparation(); return; }
		auto rules = _MyRules.MySpecialDraft; rules.MyUntimed = _MyUntimed;
		_MySpecial.emplace(rules, alive, std::move(draft->MyCards), _MyDraftRandom, _MyNow);
		SetPhase(MatchPhase::SP_DRAFT);
		if (!_MyUntimed) _MyDeadline = _MySpecial->Deadline();
	}

	void LocalMatch::Award(const ChoiceAward& _award)
	{
		const auto& id = _MySpecial->Players()[_award.MyPlayer].MyPlayerId;
		if (!_MyContent->Apply(id, _MySpecial->Cards()[_award.MyCard], _MyMetaRandom)) throw std::logic_error("invalid generated choice reward");
		UpdateLayouts();
	}

	std::expected<void, MatchError> LocalMatch::PickCard(std::string_view _player, std::size_t _index)
	{
		if (_MyPhase != MatchPhase::SP_DRAFT) return std::unexpected(MatchFlowError::WRONG_PHASE);
		auto next = *_MySpecial;
		const auto award = next.Pick(_player, _index);
		if (!award) return std::unexpected(award.error());
		Award(*award);
		*_MySpecial = std::move(next);
		_MyDeadline = _MyUntimed || _MySpecial->Complete() ? std::nullopt : std::optional(_MySpecial->Deadline());
		return {};
	}

	void LocalMatch::EnterPreparation()
	{
		_MySpecial.reset();
		_MyContent->BeginPreparation(_MyMetaRandom);
		SetPhase(MatchPhase::PREP, _MyUntimed ? std::nullopt : _MyRules.Round(_MyRound).MyPreparationSeconds);
	}

	std::expected<ChangeSet, MatchError> LocalMatch::Execute(const CommandEnvelope& _command)
	{
		if (_MyPhase != MatchPhase::PREP) return std::unexpected(MatchFlowError::WRONG_PHASE);
		auto result = _MyContent->Execute(_command, _MyMetaRandom);
		if (!result) return std::unexpected(result.error());
		return std::move(*result);
	}

	void LocalMatch::EndPreparation(bool _deadline)
	{
		if (_deadline) _MyEconomy->ApplyPreparationDeadline();
		_MyContent->OnPreparationEnd(_MyMetaRandom);
		_MyEconomy->EndPreparation();
		if (!_MyBossPairs.empty()) StartBoss();
		else StartCombat();
	}

	std::expected<void, MatchError> LocalMatch::Revive(std::string_view _donor, std::string_view _target, unsigned _round)
	{
		if (_MyPhase != MatchPhase::SETTLE) return std::unexpected(MatchFlowError::WRONG_PHASE);
		if (const auto result = _MyLedger->Revive(_donor, _target, _round, _MyNow); !result) return std::unexpected(result.error());
		return {};
	}

	std::expected<void, MatchError> LocalMatch::SetPaused(std::string_view _player, bool _paused)
	{
		if (!FindPlayer(_player)) return std::unexpected(MatchFlowError::UNKNOWN_PLAYER);
		if (!_MyRules.MySolo) return std::unexpected(MatchFlowError::WRONG_PHASE);
		if (!_paused)
		{
			if (_MyPaused && _MyDeadline) *_MyDeadline += _MyNow - _MyPausedAt;
			_MyPaused = false; _MyPausedAt = 0;
			return {};
		}
		if (_MyPaused) return {};
		if (!((_MyPhase == MatchPhase::COMBAT && !_MyCombatDone) ||
			((_MyPhase == MatchPhase::FINAL_ASSAULT || _MyPhase == MatchPhase::HIDDEN_CORE) && !_MyBossDone)))
			return std::unexpected(MatchFlowError::WRONG_PHASE);
		if (std::ranges::none_of(Fields(), [](const Battle& _field) { return !_field.Finished(); }))
			return std::unexpected(MatchFlowError::WRONG_PHASE);
		_MyPaused = true; _MyPausedAt = _MyNow;
		return {};
	}

	void LocalMatch::AfterSettlement()
	{
		(void)_MyLedger->CloseRevival();
		_MyLedger->CommitToPreparation(*_MyEconomy);
		if (AliveSeats().empty()) Finish(false, false, "eliminated");
		else StartRound(_MyRound + 1);
	}

	void LocalMatch::Finish(bool _victory, bool _hiddenCleared, std::string_view _reason)
	{
		if (_MyResult) return;
		_MyResult.emplace(LocalMatchResult{.MyVictory = _victory, .MyHiddenReached = _MyAssault && _MyAssault->Hidden(),
			.MyHiddenCleared = _hiddenCleared, .MyRound = _MyRound, .MyReason = std::string(_reason),
			.MyPlayers = std::vector<MatchPlayerProgress>(_MyLedger->Players().begin(), _MyLedger->Players().end())});
		SetPhase(MatchPhase::RESULT);
	}
}
