#include <iostream>
#include <limits>
#include <source_location>
#include <stronghold/runtime/local_match.hpp>

namespace
{
	using namespace Stronghold;

	void Check(bool _value, std::source_location _where = std::source_location::current())
	{
		if (!_value) throw std::runtime_error(std::string(_where.file_name()) + ":" + std::to_string(_where.line()));
	}

	template<class Callable>
	void Throws(Callable _callable)
	{
		bool caught = false;
		try { _callable(); } catch (const std::exception&) { caught = true; }
		Check(caught);
	}

	LocalMatchOptions Options(std::string _mode = "mode_multi_normal", unsigned _players = 2)
	{
		LocalMatchOptions options{.MyMode = std::move(_mode), .MySeed = 42};
		for (unsigned i = 0; i < _players; ++i) options.MyPlayers.push_back({.MySeat = {static_cast<int>(i), "p" + std::to_string(i)}});
		return options;
	}

	void SelectStrategies(LocalMatch& _match, bool _battleOnly = false)
	{
		for (const auto& player : _match.Economy().PublicView()) Check(_match.InfoReady(player.MyPlayerId).has_value());
		_match.Advance(_match.Now());
		Check(_match.Phase() == MatchPhase::BAND_DRAFT);
		while (!_match.Strategies()->Complete())
		{
			const std::string player(_match.Strategies()->CurrentPlayer());
			const std::string pick(_battleOnly ? (player == "p0" ? "band_amiya" : "band_sarkazb") : _match.Strategies()->TimeoutChoice(player));
			Check(_match.PickStrategy(player, pick).has_value());
			Check(!_match.PickStrategy(player, pick));
		}
		_match.Advance(_match.Now());
		Check(_match.Phase() == MatchPhase::BATTLE_CHECK);
	}

	void SelectCards(LocalMatch& _match)
	{
		while (!_match.Choices()->Complete())
		{
			const std::string player(_match.Choices()->CurrentPlayer());
			const auto available = std::ranges::find_if(_match.Choices()->Taken(), [](const auto& _owner) { return !_owner; });
			Check(available != _match.Choices()->Taken().end());
			const auto index = static_cast<std::size_t>(available - _match.Choices()->Taken().begin());
			Check(_match.PickCard(player, index).has_value());
			Check(!_match.PickCard(player, index));
		}
		_match.Advance(_match.Now());
		Check(_match.Phase() == MatchPhase::PREP);
	}

	void ReachPreparation(LocalMatch& _match)
	{
		_match.Advance(*_match.Deadline());
		Check(_match.Phase() == MatchPhase::ROUND_START);
		_match.Advance(*_match.Deadline());
		if (_match.Phase() == MatchPhase::SP_DRAFT) SelectCards(_match);
		Check(_match.Phase() == MatchPhase::PREP);
	}

	void PhasesAndCommands()
	{
		LocalMatch match(Options());
		Check(!match.Execute({"p0", 0, Refresh{}}));
		match.Start(10); match.Start(10);
		Check(match.Deadline() == 35);
		Check(!match.InfoReady("missing"));
		Check(!match.PickStrategy("p0", "band_bldsk"));
		SelectStrategies(match);
		Check(match.Deadline() == 13);
		Check(!match.SetLoadout("p0", {}));
		ReachPreparation(match);
		Check(match.Round() == 1 && match.Economy().PoolConservationHolds());
		const auto revision = match.Economy().Revision();
		Check(!match.Execute({"p0", 0, Refresh{}}));
		Check(match.Economy().Revision() == revision);
		Check(match.Execute({"p0", 1, SetReady{true}}).has_value());
		Check(match.Execute({"p1", 1, SetReady{true}}).has_value());
		// 与原版的零延迟队列相同：确认可在下一次推进前撤销。
		Check(match.Execute({"p1", 1, SetReady{false}}).has_value());
		match.Advance(match.Now());
		Check(match.Phase() == MatchPhase::PREP);
		match.Advance(*match.Deadline());
		Check(match.Phase() == MatchPhase::COMBAT && match.Fields().size() == 2);
		Check(!match.Execute({"p0", 1, Refresh{}}));
		match.StepBattles(12000);
		Check(match.NormalResults().size() == 2 && match.Deadline().has_value());
		match.Advance(*match.Deadline());
		Check(match.Phase() == MatchPhase::SETTLE);
		Check(!match.Revive("p0", "p1", 1));
		const auto settled = match.Ledger()->Players()[0];
		const auto funds = match.Economy().View("p0")->MyFunds;
		const auto pending = settled.MyPendingFunds;
		match.Advance(*match.Deadline());
		Check(match.Round() == 2 && match.Phase() == MatchPhase::ROUND_START);
		const auto income = ReferenceCatalog().Rules("mode_multi_normal").IncomeAt(2);
		Check(match.Economy().View("p0")->MyFunds == funds + pending + income);
		Throws([&] { match.Advance(match.Now() - 1); });
		Throws([&] { match.Advance(std::numeric_limits<double>::infinity()); });
		Check(match.Economy().PoolConservationHolds());
	}

	void UntimedAndTimeouts()
	{
		LocalMatch solo(Options("mode_single_funny", 1));
		solo.Start(); solo.Advance(1000);
		Check(solo.Untimed() && !solo.Deadline() && solo.Phase() == MatchPhase::INFO_CHECK);
		SelectStrategies(solo); ReachPreparation(solo);
		Check(!solo.Deadline());
		solo.Advance(solo.Now() + 1000); Check(solo.Phase() == MatchPhase::PREP);
		Check(!solo.SetPaused("p0", true));
		Check(solo.SetPaused("p0", false).has_value());
		Check(solo.Execute({"p0", 1, SetReady{true}}).has_value()); solo.Advance(solo.Now());
		solo.StepBattles(15); const auto battleTime = solo.Fields().front().Time();
		Check(solo.SetPaused("p0", true).has_value());
		solo.Advance(solo.Now() + 100); solo.StepBattles(600);
		Check(solo.Paused() && solo.Fields().front().Time() == battleTime);
		Check(solo.SetPaused("p0", false).has_value()); solo.StepBattles(15);
		Check(!solo.Paused() && solo.Fields().front().Time() == battleTime + 0.5);
		LocalMatch coop(Options()); coop.Start();
		Check(!coop.SetPaused("p0", false));
		coop.Advance(*coop.Deadline());
		const auto player = std::string(coop.Strategies()->CurrentPlayer());
		Check(coop.FocusStrategy(player, "band_cannot").has_value());
		coop.Advance(*coop.Deadline());
		const auto draft = coop.Strategies()->View();
		Check(std::ranges::find(draft.MyPlayers, player, &StrategyPick::MyPlayerId)->MyStrategyId == "band_cannot");
		coop.Advance(*coop.Deadline());
		Check(coop.Phase() == MatchPhase::BATTLE_CHECK);
		Check(coop.Economy().View(player)->MyKeepRemainingFunds);
		coop.Advance(*coop.Deadline()); coop.Advance(*coop.Deadline());
		while (coop.Phase() == MatchPhase::SP_DRAFT) coop.Advance(*coop.Deadline());
		Check(coop.Phase() == MatchPhase::PREP);
	}

	void ClearOverflow(LocalMatch& _match, std::string_view _player)
	{
		const auto catalog = ReferenceCatalog();
		for (unsigned attempt = 0; attempt < 1000; ++attempt)
		{
			const auto view = *_match.Economy().View(_player);
			if (std::ranges::none_of(view.MyTemporary, [](const auto& _piece) { return _piece.has_value(); })) return;
			bool changed = false;
			// Free an item slot first so selling an equipped operator can return its equipment.
			for (const auto& piece : view.MyTemporary)
				if (piece && !piece->IsToken() && catalog.At(piece->MyId).MyKind == PieceKind::ITEM)
					changed |= _match.Execute({std::string(_player), static_cast<int>(_match.Round()), DestroyItem{piece->MyUid}}).has_value();
			if (changed) continue;
			for (const auto& piece : view.MyTemporary)
				if (piece) changed |= _match.Execute({std::string(_player), static_cast<int>(_match.Round()),
					Sell{piece->IsToken() ? piece->MyOwnerUid : piece->MyUid}}).has_value();
			Check(changed);
		}
		throw std::runtime_error("overflow cleanup did not converge");
	}

	void CompleteEmptyMatch(std::string _mode, unsigned _players)
	{
		LocalMatch match(Options(std::move(_mode), _players));
		match.Start(); SelectStrategies(match);
		for (unsigned steps = 0; !match.Result() && steps < 200; ++steps)
		{
			if (match.Phase() == MatchPhase::SP_DRAFT) SelectCards(match);
			else if (match.Phase() == MatchPhase::PREP)
			{
				for (const auto& player : match.Economy().PublicView())
					if (player.MyAlive)
					{
						ClearOverflow(match, player.MyPlayerId);
						Check(match.Execute({player.MyPlayerId, static_cast<int>(match.Round()), SetReady{true}}).has_value());
					}
				match.Advance(match.Now());
			}
			else if (match.Phase() == MatchPhase::COMBAT || match.Phase() == MatchPhase::UNITE ||
				match.Phase() == MatchPhase::FINAL_ASSAULT || match.Phase() == MatchPhase::HIDDEN_CORE)
			{
				match.StepBattles(120000);
				if (match.Deadline()) match.Advance(*match.Deadline());
			}
			else if (match.Deadline()) match.Advance(*match.Deadline());
			else throw std::logic_error("stalled match phase");
			Check(match.Economy().PoolConservationHolds());
		}
		Check(match.Result() && !match.Result()->MyVictory);
		const auto count = match.PhaseChanges().size();
		match.Advance(match.Now() + 1000); match.StepBattles(100);
		Check(match.PhaseChanges().size() == count);
		for (const auto& player : match.Result()->MyPlayers) Check(!player.MyAlive);
	}

	void Validation()
	{
		Check(LocalMatchOptions{}.MyRulePositionMode == RulePositionMode::CURRENT);
		Check(!LocalMatchOptions{}.MyGarrisonEffectsAfterExit);
		Check(LocalMatchOptions{}.MyRetainGrantedGarrisonsAfterExit);
		auto invalidPosition = Options(); invalidPosition.MyRulePositionMode = static_cast<RulePositionMode>(-1);
		Throws([&] { LocalMatch match(invalidPosition); });
		Throws([] { LocalMatch match(Options("mode_single_normal", 2)); });
		Throws([] { LocalMatch match(Options("mode_multi_normal", 5)); });
		auto options = Options(); options.MyPlayers[1].MySeat.MySeat = 0;
		Throws([&] { LocalMatch match(options); });
	}

	// 显式 CUSTOM 场景只控制输赢；阶段、真实波次、召唤、伤害、层数和结算仍由宿主执行。
	struct ClearField final : CustomBondEffect<ClearField>
	{
		void OnBattleStart(Battle& _battle, ContentEvent& _event)
		{
			const auto& player = _battle.Players()[_event.MyHandlerOwner];
			const auto bonds = player.MyBonds;
			for (const auto& bond : bonds) (void)_battle.AddBondLayers(player.MyPlayerId, bond.MyId, 999);
		}

		void OnTick(Battle& _battle, ContentEvent&)
		{
			for (const auto& unit : _battle.Units())
				if (unit.MySide == UnitSide::ENEMY && unit.MyAlive)
					(void)_battle.LoseHealth(0, unit.MyId, 10000);
		}
	};

	void BuyAndDeploy(LocalMatch& _match)
	{
		const auto catalog = ReferenceCatalog();
		for (const auto& player : _match.Economy().PublicView())
		{
			if (!player.MyAlive) continue;
			const auto command = [&](PreparationCommand _command)
			{ return _match.Execute({player.MyPlayerId, static_cast<int>(_match.Round()), std::move(_command)}).has_value(); };
			while (command(LevelUp{})) { }
			const auto shop = _match.Economy().View(player.MyPlayerId)->MyShop;
			for (std::size_t i = 0; i < shop.size(); ++i)
				if (shop[i] && catalog.At(shop[i]->MyId).MyKind == PieceKind::CHESS) (void)command(Buy{i});
			const auto view = *_match.Economy().View(player.MyPlayerId);
			for (const auto& piece : view.MyHand)
			{
				if (!piece || piece->IsToken() || catalog.At(piece->MyId).MyKind != PieceKind::CHESS) continue;
				const auto board = _match.Economy().View(player.MyPlayerId)->MyBoard;
				for (std::size_t i = 0; i < board.size(); ++i)
					if (!board[i] && command(MoveToBoard{piece->MyUid, BoardPosition::FromIndex(i), Facing::RIGHT})) break;
			}
		}
	}

	void CompleteVictory(bool _solo)
	{
		ContentRegistry registry;
		const auto content = registry.Register<ClearField>("flow-test"); registry.Seal();
		auto options = Options(_solo ? "mode_single_normal" : "mode_multi_normal", _solo ? 1U : 2U);
		options.MyCustomRegistry = std::cref(registry);
		options.MyCustomBindings.push_back({content, "p0"});
		const auto positionMode = _solo ? RulePositionMode::CURRENT : RulePositionMode::INITIAL;
		options.MyRulePositionMode = positionMode;
		options.MyGarrisonEffectsAfterExit = !_solo;
		options.MyRetainGrantedGarrisonsAfterExit = _solo;
		LocalMatch match(std::move(options)); match.Start(); SelectStrategies(match, true);
		Check(match.PositionMode() == positionMode);
		Check(match.GarrisonEffectsAfterExit() == !_solo);
		Check(match.RetainGrantedGarrisonsAfterExit() == _solo);
		bool unite = false, boss = false, hidden = false, deployed = false, genericSkill = false;
		for (unsigned transitions = 0; !match.Result() && transitions < 300; ++transitions)
		{
			if (match.Phase() == MatchPhase::SP_DRAFT) SelectCards(match);
			else if (match.Phase() == MatchPhase::PREP)
			{
				BuyAndDeploy(match);
				for (const auto& player : match.Economy().PublicView())
					if (player.MyAlive)
					{
						ClearOverflow(match, player.MyPlayerId);
						Check(match.Execute({player.MyPlayerId, static_cast<int>(match.Round()), SetReady{true}}).has_value());
					}
				match.Advance(match.Now());
			}
			else if (match.Phase() == MatchPhase::COMBAT || match.Phase() == MatchPhase::UNITE ||
				match.Phase() == MatchPhase::FINAL_ASSAULT || match.Phase() == MatchPhase::HIDDEN_CORE)
			{
				unite |= match.Phase() == MatchPhase::UNITE;
				boss |= match.Phase() == MatchPhase::FINAL_ASSAULT;
				hidden |= match.Phase() == MatchPhase::HIDDEN_CORE;
				// Deployment is delayed by DP; sample during the battle, not only on its first tick.
				const auto phase = match.Phase();
				for (unsigned i = 0; i < 400 && match.Phase() == phase; ++i)
				{
					match.StepBattles(300);
					for (const auto& field : match.Fields())
					{
						Check(field.PositionMode() == positionMode);
						Check(field.GarrisonEffectsAfterExit() == !_solo);
						Check(field.RetainGrantedGarrisonsAfterExit() == _solo);
						for (const auto& unit : field.Units())
							if (unit.MyKind == UnitKind::OPERATOR)
							{
								deployed = true;
								genericSkill |= unit.MyDefinition.MyGenericSkill && unit.MyDefinition.MySkill.MyKind != SkillKind::NONE;
							}
						Check(field.ContentErrors().empty());
					}
					if (match.Deadline()) break;
				}
				for (const auto& field : match.Fields()) Check(field.ContentErrors().empty());
				if (match.Deadline()) match.Advance(*match.Deadline());
			}
			else if (match.Deadline()) match.Advance(*match.Deadline());
			else throw std::logic_error("stalled custom match");
			Check(match.Economy().PoolConservationHolds());
		}
		if (!match.Result() || !match.Result()->MyVictory)
			throw std::runtime_error("custom match did not win: solo=" + std::to_string(_solo) +
				" round=" + std::to_string(match.Round()) + " phase=" + std::to_string(static_cast<int>(match.Phase())) +
				" reason=" + (match.Result() ? match.Result()->MyReason : "running"));
		if (!boss || !hidden || !deployed) throw std::runtime_error("missing victory phase or deployment: solo=" + std::to_string(_solo) +
			" boss=" + std::to_string(boss) + " hidden=" + std::to_string(hidden) + " deployed=" + std::to_string(deployed));
		Check(match.Result()->MyHiddenReached && match.Result()->MyHiddenCleared);
		Check(_solo || unite);
		Check(genericSkill);
		Check(match.Round() == 15);
	}
}

int main()
{
	try
	{
		Validation(); PhasesAndCommands(); UntimedAndTimeouts();
		CompleteEmptyMatch("mode_single_normal", 1); CompleteEmptyMatch("mode_multi_normal", 2);
		CompleteVictory(true); CompleteVictory(false);
		std::cout << "Local match flow, deadlines, rewards, combat, settlement and elimination passed\n";
	}
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
