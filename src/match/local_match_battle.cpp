#include <stronghold/adapters/reference_choices.hpp>
#include <stronghold/adapters/reference_map_characters.hpp>
#include <stronghold/adapters/reference_equipment.hpp>
#include <stronghold/adapters/reference_stage.hpp>
#include <stronghold/adapters/reference_summons.hpp>
#include <stronghold/adapters/reference_wave_generation.hpp>
#include <stronghold/runtime/local_match.hpp>

namespace Stronghold
{
	namespace
	{
		constexpr FieldRect NormalRect{9, 12, 0, 10};
		constexpr FieldRect UniteRect{9, 12, 0, 20};
		constexpr FieldRect BossRect{0, 5, 0, 20};

		std::uint32_t BattleSeed(std::uint32_t _seed, std::string_view _salt)
		{
			std::u16string salt; salt.reserve(_salt.size());
			for (const auto c : _salt) salt.push_back(static_cast<char16_t>(static_cast<unsigned char>(c)));
			return DeriveSeed(_seed, salt); // 盐仅包含宿主生成的 ASCII 阶段、回合与座位数字。
		}
	}

	std::span<const Battle> LocalMatch::Fields() const noexcept
	{ return _MyBossFields ? _MyBossFields->Fields() : std::span<const Battle>(_MyFields); }

	std::vector<BondState> LocalMatch::Bonds(const Player& _player)
	{
		const auto view = *_MyEconomy->View(_player.MySeat.MyPlayerId);
		const auto content = _MyContent->View(_player.MySeat.MyPlayerId);
		std::vector<BondNumber> layers; layers.reserve(content->MyLayers.size());
		for (const auto& layer : content->MyLayers) layers.push_back({layer.MyId, layer.MyLayers});
		const auto bonds = _MyBondCalculator.Compute(view, layers, {}, _MyRules.MyInactiveBonds, _player.MyBondRoster);
		return {bonds.MyEnabled.begin(), bonds.MyEnabled.end()};
	}

	std::vector<WaveBounty> LocalMatch::Bounties(std::string_view _player) const
	{
		std::vector<WaveBounty> result;
		const auto found = std::ranges::find(_MyLedger->Players(), _player, &MatchPlayerProgress::MyPlayerId);
		result.reserve(found->MyBounties.size());
		for (const auto& bounty : found->MyBounties) result.push_back(bounty.MyCard);
		return result;
	}

	void LocalMatch::UpdateLayouts()
	{
		const auto& stage = ReferenceBattleStage(_MySetup.MyStageId);
		for (const auto& seat : AliveSeats())
		{
			const auto group = std::ranges::find_if(_MyBossPairs, [&](const BossFieldGroup& _pair)
				{ return std::ranges::find(_pair.MyPlayers, seat.MyPlayerId) != _pair.MyPlayers.end(); });
			const bool boss = group != _MyBossPairs.end();
			const bool right = boss && group->MyCount == 2 && group->MyPlayers[1] == seat.MyPlayerId;
			const auto content = _MyContent->View(seat.MyPlayerId);
			BoardLayout layout;
			for (std::size_t i = 0; i < layout.MyTiles.size(); ++i)
			{
				const auto position = BoardPosition::FromIndex(i);
				const auto row = boss ? position.MyRow - 7 : position.MyRow;
				const auto column = right ? 20 - position.MyColumn : position.MyColumn;
				const auto& tile = stage.MyTiles[static_cast<std::size_t>(FieldGrid::Key(row, column))];
				if (tile.MyLow && (tile.MyBuild == FieldBuild::ALL || tile.MyBuild == FieldBuild::MELEE)) layout.MyTiles[i] = Terrain::GROUND;
				else if ((!tile.MyLow && tile.MyBuild == FieldBuild::ALL) || tile.MyBuild == FieldBuild::RANGED) layout.MyTiles[i] = Terrain::HIGH;
				for (const auto& device : stage.MyDevices)
				{
					if (device.MyPosition.MyRow != row || device.MyPosition.MyColumn != column) continue;
					const auto override = std::ranges::find(content->MyDevices, device.MyAlias, &ChoiceDeviceSetting::MyAlias);
					if (!(override == content->MyDevices.end() ? device.MyActive : override->MyActive)) continue;
					if (device.MyRole == "crate" || device.MyRole == "mound") layout.MyTiles[i] = Terrain::BLOCKED;
					else if (device.MyRole == "platform") layout.MyTiles[i] = Terrain::HIGH;
					else if (device.MyRole == "waterPlatform") layout.MyTiles[i] = Terrain::GROUND;
				}
			}
			if (!_MyEconomy->SetBoardLayout(seat.MyPlayerId, layout)) throw std::logic_error("cannot apply match layout");
		}
	}

	BattlePlayerInput LocalMatch::BattlePlayer(const Player& _player, bool _boss, bool _right)
	{
		const auto& id = _player.MySeat.MyPlayerId;
		const auto view = *_MyEconomy->View(id);
		BattlePlayerInput input{.MyPlayerId = id, .MyMirrorDeployment = _boss && _right, .MyRightHalf = _right};
		input.MyUnits.reserve(view.MyBoard.size());
		input.MyTokenTemplates.reserve(view.MyBoard.size());
		for (std::size_t i = 0; i < view.MyBoard.size(); ++i)
		{
			const auto& piece = view.MyBoard[i]; if (!piece) continue;
			const AllyRecord* body = nullptr;
			TokenSource tokenSource = TokenSource::NONE;
			bool genericTalents = false;
			if (!piece->IsToken())
			{
				const auto record = _player.MyRoster.Resolve(piece->MyId);
				body = &record.MyLoadout.MyBody;
				genericTalents = record.MyStandIn || record.MyDiySelected;
				for (const auto tokenId : body->MyTokens)
				{
					const auto& token = ReferenceToken(tokenId);
					const TokenVariantRecord* variant = nullptr;
					if (record.MyDiySelected)
					{
						const auto pick = std::ranges::find(_player.MyRoster.Diy(), record.MyIdentity.MyBaseId, &SelectedDiy::MySlot);
						variant = &token.DiyVariant(record.MyIdentity.Diy(pick->MyCharacter, pick->MySkill, pick->MyModule));
					}
					else variant = &token.Variant(record.MyTokenOwner, record.MyLoadout.MySkillIndex, record.MyLoadout.MyModuleId);
					input.MyTokenTemplates.push_back({.MyOwnerPieceUid = piece->MyUid, .MyDefinition = variant->MyBody.MakeKitDefinition()});
				}
			}
			else
			{
				const auto owner = std::ranges::find_if(view.MyBoard, [&](const auto& _piece) { return _piece && _piece->MyUid == piece->MyOwnerUid; });
				if (owner == view.MyBoard.end()) continue;
				const auto record = _player.MyRoster.Resolve((*owner)->MyId);
				const auto& token = ReferenceToken(piece->MyId);
				const TokenVariantRecord* variant = nullptr;
				if (record.MyDiySelected)
				{
					const auto pick = std::ranges::find(_player.MyRoster.Diy(), record.MyIdentity.MyBaseId, &SelectedDiy::MySlot);
					variant = &token.DiyVariant(record.MyIdentity.Diy(pick->MyCharacter, pick->MySkill, pick->MyModule));
				}
				else variant = &token.Variant(record.MyTokenOwner, record.MyLoadout.MySkillIndex, record.MyLoadout.MyModuleId);
				body = &variant->MyBody;
				const auto tokenOwner = record.MyTokenOwner;
				const bool ownVariant = variant->MyOwnerId == tokenOwner || (tokenOwner.ends_with("_b") && variant->MyOwnerId.ends_with("_a") &&
					variant->MyOwnerId.size() == tokenOwner.size() && variant->MyOwnerId.substr(0, tokenOwner.size() - 2) == tokenOwner.substr(0, tokenOwner.size() - 2));
				if (ownVariant && token.MyPlaceable && variant->MyHasSources && !variant->MySources.empty() && !std::ranges::contains(variant->MySources, "talent"))
					tokenSource = std::ranges::contains(variant->MySources, "skill") ? TokenSource::SKILL : TokenSource::UNAVAILABLE;
			}
			const auto position = BoardPosition::FromIndex(i);
			const auto row = _boss ? position.MyRow - 7 : position.MyRow;
			const auto column = _right ? (_boss ? 20 - position.MyColumn : position.MyColumn + 8) : position.MyColumn;
			auto facing = piece->MyFacing;
			if (_boss && _right && (facing == Facing::RIGHT || facing == Facing::LEFT)) facing = facing == Facing::RIGHT ? Facing::LEFT : Facing::RIGHT;
			auto definition = body->MakeKitDefinition(genericTalents);
			definition.MyIdentity.MyMeleePosition = body->MyPosition == "MELEE";
			if (!piece->IsToken())
			{
				auto& identity = definition.MyIdentity;
				const auto& catalog = _MyCatalog.At(piece->MyId);
				identity.MyBaseChess = catalog.MyBaseId; identity.MyTier = static_cast<unsigned>(catalog.MyTier); identity.MyGolden = catalog.MyGolden;
				const auto record = _player.MyRoster.Resolve(piece->MyId);
				identity.MyBonds.assign(record.MyBonds.begin(), record.MyBonds.end());
				if (!record.MyDiySelected) identity.MyGarrisons.assign(record.MyIdentity.MyGarrisons.begin(), record.MyIdentity.MyGarrisons.end());
				identity.MyItems.reserve(piece->MyItems.size());
				for (const auto& item : piece->MyItems) identity.MyItems.push_back(item.MyId);
				AppendEquipmentEffects(definition, identity.MyItems);
				const auto itemRules = ReferenceBondItems();
				const bool canGive = piece->MyItems.size() >= 2 && std::ranges::any_of(piece->MyItems, [&](const Piece& _item)
				{
					const auto rule = std::ranges::find(itemRules, _item.MyId, &BondItemRecord::MyId);
					return rule != itemRules.end() && rule->MyItem.MyCanGiveBond;
				});
				if (canGive) for (const auto& item : piece->MyItems)
				{
					const auto rule = std::ranges::find(itemRules, item.MyId, &BondItemRecord::MyId);
					if (rule != itemRules.end() && !rule->MyItem.MyCanGiveBond && !rule->MyItem.MyGrantedBond.empty() &&
						!std::ranges::contains(identity.MyBonds, rule->MyItem.MyGrantedBond)) identity.MyBonds.emplace_back(rule->MyItem.MyGrantedBond);
				}
			}
			input.MyUnits.emplace_back(AllyDeployment{.MyPieceUid = piece->MyUid, .MyDefinition = std::move(definition),
				.MyPosition = {static_cast<double>(column), static_cast<double>(row)}, .MyFacing = facing,
				.MyKind = piece->IsToken() ? UnitKind::TOKEN : UnitKind::OPERATOR, .MyOwnerPieceUid = piece->MyOwnerUid, .MyTokenSource = tokenSource});
		}
		const auto bonds = Bonds(_player); input.MyBonds.reserve(bonds.size());
		for (const auto& bond : bonds) input.MyBonds.emplace_back(BondLayer{std::string(bond.MyId), bond.MyLayers, bond.MyCount, bond.MyActive, bond.MyTier});
		for (const auto& effect : ReferenceAddonBondEffects())
			if (std::ranges::any_of(input.MyBonds, [&](const BondLayer& _bond) { return _bond.MyActive && _bond.MyId == AddonBondIds[static_cast<unsigned>(effect.MyKind)]; }))
				input.MyAddonBonds.push_back(effect);
		input.MyChoiceEffects = MakeChoiceBattleEffects(*_MyContent->View(id));
		const auto draft = _MyStrategy->View();
		const auto picked = std::ranges::find(draft.MyPlayers, id, &StrategyPick::MyPlayerId);
		if (picked != draft.MyPlayers.end()) input.MyBandEffects = MakeBandBattleEffects(picked->MyStrategyId);
		for (const auto& effect : ReferenceCoreBondEffects())
		{
			if (effect.MyKind == CoreBondKind::SARGON && effect.MyShareEquipment != (picked != draft.MyPlayers.end() && picked->MyStrategyId == "band_narant")) continue;
			if (std::ranges::any_of(input.MyBonds, [&](const BondLayer& _bond) { return _bond.MyActive && _bond.MyId == CoreBondIds[static_cast<unsigned>(effect.MyKind)]; }))
				input.MyCoreBonds.push_back(effect);
		}
		input.MyGainedChess = view.MyRoundStatistics.MyGainedChess;
		input.MyHandUnits = static_cast<std::size_t>(std::ranges::count_if(view.MyHand, [&](const auto& _piece)
			{ return _piece && !_piece->IsToken() && _MyCatalog.At(_piece->MyId).MyKind == PieceKind::CHESS; }));
		return input;
	}

	BattleInput LocalMatch::BattleField(WavePlan _wave, std::vector<BattlePlayerInput> _players, bool _boss, bool _unite, std::string_view _salt)
	{
		const auto rect = _boss ? BossRect : _unite ? UniteRect : NormalRect;
		BattleInput input{.MyPlayers = std::move(_players), .MyTimeLimit = _wave.MyTimeLimit,
			.MyInitialDp = _MyRules.MyInitialDp, .MyDpPerSecond = _MyRules.MyDpPerSecond, .MyMaxDp = _MyRules.MyMaxDp,
			.MySeed = BattleSeed(_MyOptions.MySeed, _salt), .MyBossBattle = _boss, .MyLayerGainsEnabled = !_boss && !_unite};
		input.MyEquipmentTemplates = ReferenceEquipmentTemplates();
		input.MyYanyouDefinition = MakeYanyouDefinition();
		input.MyGarrisonRules = ReferenceBattleGarrisons();
		input.MyRulePositionMode = _MyOptions.MyRulePositionMode;
		input.MyGarrisonEffectsAfterExit = _MyOptions.MyGarrisonEffectsAfterExit;
		input.MyRetainGrantedGarrisonsAfterExit = _MyOptions.MyRetainGrantedGarrisonsAfterExit;
		input.MyContentRegistry = _MyOptions.MyCustomRegistry;
		input.MyContentBindings.reserve(_MyOptions.MyCustomBindings.size());
		for (const auto& binding : _MyOptions.MyCustomBindings)
			if (std::ranges::find(input.MyPlayers, binding.MyPlayerId, &BattlePlayerInput::MyPlayerId) != input.MyPlayers.end())
				input.MyContentBindings.push_back(binding);
		if (!_unite && _MyRound >= ReferenceDuckWave().MyFirstRound)
		{
			const auto picks = _MyStrategy->View();
			const auto alive = AliveSeats();
			const bool duck = std::ranges::any_of(picks.MyPlayers, [&](const StrategyPick& _pick)
				{ return _pick.MyStrategyId == ReferenceDuckWave().MyStrategy && std::ranges::find(alive, _pick.MyPlayerId, &Seat::MyPlayerId) != alive.end(); });
			if (duck)
				for (std::size_t i = 0; i < input.MyPlayers.size(); ++i)
					(void)_MyWaves.ReplaceDucks(_wave, _MyMetaRandom, ReferenceDuckWave(), input.MyPlayers[i].MyPlayerId,
						_boss && input.MyPlayers.size() > 1 ? (i ? WaveSide::RIGHT : WaveSide::LEFT) : WaveSide::ANY);
		}
		input.MySpawns = _MyWaves.MakeSpawns(_wave, input.MyPlayers, rect);
		input.MyGroundRoutes = _MyWaves.MakeGroundRoutes(_wave, rect);
		std::vector<DeviceOverride> devices;
		for (const auto& player : input.MyPlayers)
		{
			const auto content = _MyContent->View(player.MyPlayerId);
			for (const auto& device : content->MyDevices) devices.push_back({device.MyAlias, device.MyActive, player.MyPlayerId});
		}
		ReferenceBattleStage(_MySetup.MyStageId).Apply(input, rect, devices);
		const auto picks = _MyStrategy->View();
		const auto alive = AliveSeats();
		if (std::ranges::any_of(picks.MyPlayers, [&](const StrategyPick& _pick)
			{ return _pick.MyStrategyId == "band_amedic" && std::ranges::find(alive, _pick.MyPlayerId, &Seat::MyPlayerId) != alive.end(); }))
			input.MyMapCharacters = MakeMapCharacterVariants(ReferenceBattleStage(_MySetup.MyStageId));
		return input;
	}

	void LocalMatch::StartCombat()
	{
		_MyFields.clear();
		for (const auto& seat : AliveSeats())
		{
			const auto bounties = Bounties(seat.MyPlayerId);
			auto wave = _MyWaves.WithBounties(*_MyNormalWave, _MyRound, bounties, seat.MyPlayerId);
			std::vector<BattlePlayerInput> players; players.push_back(BattlePlayer(PlayerAt(seat.MyPlayerId), false, false));
			_MyFields.emplace_back(BattleField(std::move(wave), std::move(players), false, false,
				"n:" + std::to_string(_MyRound) + ":" + std::to_string(seat.MySeat)));
		}
		SetPhase(MatchPhase::COMBAT);
	}

	void LocalMatch::SyncLayers(std::span<const BattlePlayerState> _players)
	{
		for (const auto& player : _players)
		{
			if (player.MyLayerGains.empty()) continue;
			_MyLayerScratch.clear(); _MyLayerScratch.reserve(player.MyLayerGains.size());
			for (const auto& gain : player.MyLayerGains) _MyLayerScratch.push_back({gain.MyId, gain.MyLayers});
			if (!_MyContent->SynchronizeLayers(player.MyPlayerId, static_cast<int>(_MyRound), _MyLayerScratch, _MyMetaRandom))
				throw std::logic_error("cannot synchronize normal battle layers");
		}
	}

	void LocalMatch::StepBattles(std::uint64_t _ticks)
	{
		if (_MyPaused) return;
		while (_ticks-- > 0)
		{
			if ((_MyPhase == MatchPhase::FINAL_ASSAULT || _MyPhase == MatchPhase::HIDDEN_CORE) && !_MyBossDone)
			{
				_MyBossFields->Step();
				if (_MyBossFields->Finished()) { FinishBoss(); return; }
			}
			else if ((_MyPhase == MatchPhase::COMBAT && !_MyCombatDone) || _MyPhase == MatchPhase::UNITE)
			{
				for (auto& field : _MyFields)
				{
					field.Step();
					if (_MyPhase == MatchPhase::COMBAT) SyncLayers(field.Players());
				}
				if (std::ranges::all_of(_MyFields, &Battle::Finished))
				{
					if (_MyPhase == MatchPhase::COMBAT) FinishCombat(); else FinishUnite();
					return;
				}
			}
			else return;
		}
	}

	void LocalMatch::FinishCombat()
	{
		_MyNormalResults.clear();
		for (const auto& field : _MyFields)
		{
			auto result = field.Result(); SyncLayers(result.MyPlayers);
			for (auto& player : result.MyPlayers) _MyNormalResults.push_back(std::move(player));
		}
		_MyCombatDone = true; _MyDeadline = _MyNow + 1.5;
	}

	std::optional<UnitePlan> LocalMatch::PlanNextUnite(bool _relay)
	{
		const auto alive = AliveSeats();
		struct Owned
		{
			std::vector<std::uint64_t> MyUids;
			std::vector<UniteBond> MyBonds;
			std::vector<WaveBounty> MyBounties;
		};

		std::vector<Owned> storage(alive.size());
		std::vector<UniteParticipant> players; players.reserve(alive.size());
		for (std::size_t i = 0; i < alive.size(); ++i)
		{
			const auto& seat = alive[i]; auto& owned = storage[i];
			const auto view = *_MyEconomy->View(seat.MyPlayerId);
			for (const auto& piece : view.MyBoard) if (piece && !piece->IsToken()) owned.MyUids.push_back(piece->MyUid);
			for (const auto& bond : Bonds(PlayerAt(seat.MyPlayerId))) owned.MyBonds.push_back({.MyActive = bond.MyActive, .MyLayers = bond.MyLayers});
			owned.MyBounties = Bounties(seat.MyPlayerId);
			const auto result = std::ranges::find(_MyNormalResults, seat.MyPlayerId, &BattlePlayerState::MyPlayerId);
			players.push_back(UniteParticipant{.MyPlayerId = seat.MyPlayerId, .MySeat = seat.MySeat,
				.MyDeployCount = owned.MyUids.size(), .MyOperatorUids = owned.MyUids, .MyBonds = owned.MyBonds,
				.MyBounties = owned.MyBounties, .MyResult = std::cref(*result)});
		}
		if (!_relay) return PlanUnite(UniteRules{_MyRules.MySolo, _MyOptions.MyCapacityExperiment, _MyNormalResults.size(),
			_MyRules.MyMaxUniteHelpers}, players, ReferenceWaveGeneration().MyEnemies);
		const auto& previous = _MyUniteRounds.back();
		return PlanUniteRelay(previous.MyPlan, previous.MyResult, false, players, previous.MySpawns,
			ReferenceWaveGeneration().MyEnemies, _MyRules.MyMaxUniteHelpers);
	}

	void LocalMatch::StartUnite(UnitePlan _plan)
	{
		auto wave = _MyWaves.BuildUnite(_plan.MyLeaks, static_cast<unsigned>(_plan.MyHelpers.size()));
		wave.MyTimeLimit = _MyRules.Round(_MyRound).MyCombatGameSeconds;
		std::vector<BattlePlayerInput> players; players.reserve(_plan.MyHelpers.size());
		for (std::size_t i = 0; i < _plan.MyHelpers.size(); ++i)
		{
			const auto& id = _plan.MyHelpers[i];
			auto input = BattlePlayer(PlayerAt(id), false, _plan.MyHelpers.size() > 1 && i == 0);
			ApplyUniteCarry(input, *std::ranges::find(_MyNormalResults, id, &BattlePlayerState::MyPlayerId));
			players.push_back(std::move(input));
		}
		auto input = BattleField(std::move(wave), std::move(players), false, true,
			"u:" + std::to_string(_MyRound) + (_plan.MyRelayRound == 2 ? ":2" : ""));
		_MyUniteRounds.emplace_back(UniteRound{.MyPlan = std::move(_plan), .MySpawns = input.MySpawns});
		_MyFields.clear(); _MyFields.emplace_back(std::move(input));
		SetPhase(MatchPhase::UNITE);
	}

	void LocalMatch::FinishUnite()
	{
		_MyUniteRounds.back().MyResult = _MyFields.front().Result();
		if (auto plan = PlanNextUnite(true)) StartUnite(std::move(*plan));
		else Settle();
	}

	void LocalMatch::Settle()
	{
		for (const auto& seat : AliveSeats())
			if (!_MyContent->OnBattleResult(seat.MyPlayerId, static_cast<int>(_MyRound), _MyMetaRandom))
				throw std::logic_error("cannot execute battle result contents");
		std::vector<UniteSettlementStage> stages; stages.reserve(_MyUniteRounds.size());
		for (const auto& stage : _MyUniteRounds) stages.push_back({std::cref(stage.MyPlan), std::cref(stage.MyResult), false});
		(void)_MyLedger->Settle(RoundSettlementInput{.MyRound = _MyRound, .MyNow = _MyNow,
			.MyNormalResults = _MyNormalResults, .MyUniteStages = stages});
		_MyFields.clear();
		SetPhase(MatchPhase::SETTLE, _MyLedger->RevivalOpen(_MyNow) ? _MyLedger->RevivalDeadline() - _MyNow : 3);
	}

	void LocalMatch::StartBoss()
	{
		const bool hidden = _MyRound != _MyRules.MyBossRound;
		const auto alive = AliveSeats();
		const auto health = _MyRules.BossHealth(hidden ? _MySetup.MyHiddenBossId : _MySetup.MyBossId, alive.size(), _MyOptions.MyCapacityExperiment);
		if (hidden) _MyAssault->BeginHidden(health);
		else
		{
			std::vector<BossParticipant> players; players.reserve(alive.size());
			for (const auto& seat : alive)
			{
				const auto progress = std::ranges::find(_MyLedger->Players(), seat.MyPlayerId, &MatchPlayerProgress::MyPlayerId);
				players.push_back({seat.MyPlayerId, seat.MySeat, progress->MyLife, ActivatedLayers(Bonds(PlayerAt(seat.MyPlayerId)))});
			}
			auto rules = _MyRules.MyFinalAssault; rules.MyCapacityExperiment = _MyOptions.MyCapacityExperiment;
			_MyAssault = std::make_unique<FinalAssault>(rules, players, health);
		}
		std::vector<BattleInput> inputs; inputs.reserve(_MyBossPairs.size());
		for (std::size_t i = 0; i < _MyBossPairs.size(); ++i)
		{
			const auto& pair = _MyBossPairs[i]; auto wave = _MyBossWaves[i];
			std::vector<BattlePlayerInput> players; players.reserve(pair.MyCount);
			for (unsigned j = 0; j < pair.MyCount; ++j)
			{
				const auto& id = pair.MyPlayers[j];
				wave = _MyWaves.WithBounties(wave, _MyRound, Bounties(id), id, _MyRules.MySolo ? WaveSide::ANY : j ? WaveSide::RIGHT : WaveSide::LEFT, false);
				players.push_back(BattlePlayer(PlayerAt(id), true, j == 1));
			}
			inputs.push_back(BattleField(std::move(wave), std::move(players), true, false,
				"b" + std::to_string(i + 1) + ":" + std::to_string(_MyRound)));
		}
		_MyBossFields = std::make_unique<BossBattleGroup>(*_MyAssault, std::move(inputs));
		SetPhase(hidden ? MatchPhase::HIDDEN_CORE : MatchPhase::FINAL_ASSAULT);
	}

	void LocalMatch::FinishBoss()
	{
		std::vector<BattlePlayerState> players; players.reserve(_MyPlayers.size());
		for (auto& result : _MyBossFields->Results())
			for (auto& player : result.MyPlayers)
			{
				if (!_MyContent->OnBattleResult(player.MyPlayerId, static_cast<int>(_MyRound), _MyMetaRandom))
					throw std::logic_error("cannot execute boss result contents");
				players.push_back(std::move(player));
			}
		_MyLedger->SettleBoss(_MyRound, _MyNow, _MyAssault->Players(), players);
		_MyLedger->CommitToPreparation(*_MyEconomy);
		_MyBossDone = true; _MyDeadline = _MyNow + 3;
	}
}
