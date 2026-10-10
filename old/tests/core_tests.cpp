#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <stronghold/core/scheduler.hpp>
#include <stronghold/domain/combat_math.hpp>
#include <stronghold/domain/preparation.hpp>
#include <stronghold/domain/settlement.hpp>
#include <type_traits>

namespace
{
	using namespace Stronghold;

	using namespace std::chrono_literals;

	static_assert(
		[]
		{
			Random random(1);
			return random.NextU32() == 2693262067U;
		}());
	static_assert(
		std::
			is_constructible_v<EconomySession, const Catalog&, std::string_view, std::span<const Seat>, std::uint32_t>);
	static_assert(
		!std::is_constructible_v<EconomySession, Catalog&&, std::string_view, std::span<const Seat>, std::uint32_t>);

	void Check(bool _condition, const std::source_location _where = std::source_location::current())
	{
		if (!_condition)
			throw std::runtime_error(std::string(_where.file_name()) + ":" + std::to_string(_where.line()));
	}

	template <class _Callable>
	void Throws(_Callable _callable)
	{
		bool threw = false;
		try
		{
			_callable();
		}
		catch (const std::exception&)
		{
			threw = true;
		}
		Check(threw);
	}

	Catalog Fixture(int _mergeCount = 3, std::size_t _handSize = 10, int _copies = 12)
	{
		EconomyRules rules;
		rules.MyIncome = {0, 100, 100, 100};
		rules.MyHandSize = _handSize;
		return Catalog(
			std::vector<Definition>{
				{"a", "a", "a_gold", PieceKind::CHESS, 1, 2, 1, _copies, _mergeCount, false, true},
				{"a_gold", "a", "a_gold", PieceKind::CHESS, 1, 2, 1, _copies, 0, true, false},
				{"item", "item", "item_gold", PieceKind::ITEM, 1, 1, 0, 0, 2, false, true},
				{"item_gold", "item", "item_gold", PieceKind::ITEM, 1, 5, 0, 0, 0, true, false}},
			std::map<std::string, EconomyRules, std::less<>>{{"test", rules}});
	}

	EconomySession Session(const Catalog& _catalog)
	{
		const std::array<Seat, 2> Seats{{{0, "one"}, {1, "two"}}};
		return EconomySession(_catalog, "test", Seats, 1);
	}

	void RandomTests()
	{
		Random first(0), second(0x9e3779b9U);
		for (int i = 0; i < 1000; ++i)
			Check(first.NextU32() == second.NextU32());
		Random random(1);
		Check(random.NextU32() == 2693262067U);
		const auto state = random.State();
		Throws([&] { (void)random.Index(0); });
		Check(random.State() == state);
		std::array<int, 5> numbers{1, 2, 3, 4, 5};
		random.Shuffle(numbers.begin(), numbers.end());
		std::ranges::sort(numbers);
		Check((numbers == std::array{1, 2, 3, 4, 5}));
		Throws([&] { random.Shuffle(numbers.end(), numbers.begin()); });
	}

	void SchedulerTests()
	{
		Scheduler scheduler;
		std::vector<int> order;
		const auto cancelled = scheduler.After(1ms, [&] { order.push_back(9); });
		Check(scheduler.Cancel(cancelled));
		(void)scheduler.After(
			2ms,
			[&]
			{
				order.push_back(1);
				(void)scheduler.After(0ms, [&] { order.push_back(3); });
			});
		(void)scheduler.After(2ms, [&] { order.push_back(2); });
		const auto progress = scheduler.AdvanceTo(5ms, 2);
		Check(!progress.MyCaughtUp && progress.MyExecuted == 2 && scheduler.Now() == 2ms);
		Check(scheduler.AdvanceTo(5ms).MyCaughtUp);
		Check((order == std::vector{1, 2, 3}));
		Scheduler::Handle interval{};
		interval = scheduler.Every(2ms, [&] { Check(scheduler.Cancel(interval)); });
		Check(scheduler.AdvanceTo(20ms).MyExecuted == 1);
		Check(scheduler.Pending() == 0);
		(void)scheduler.After(1ms, [&] { Throws([&] { (void)scheduler.AdvanceTo(30ms); }); });
		Check(scheduler.AdvanceTo(22ms).MyExecuted == 1);
		(void)scheduler.After(0ms, [] { throw std::runtime_error("callback"); });
		Throws([&] { (void)scheduler.AdvanceTo(23ms); });
		Check(scheduler.Pending() == 0 && scheduler.AdvanceTo(23ms).MyCaughtUp);
		Throws([&] { (void)scheduler.AdvanceTo(22ms); });
		Throws([&] { (void)scheduler.Every(0ms, [] {}); });
		(void)scheduler.After(0ms, [&] { scheduler.Clear(); });
		(void)scheduler.After(0ms, [] { throw std::runtime_error("must be cancelled"); });
		Check(scheduler.AdvanceTo(24ms).MyExecuted == 1);
		BattleClock clock;
		for (int i = 0; i < 30; ++i)
			clock.Step();
		Check(clock.Tick() == 30 && clock.Seconds() == 1);

		Scheduler stateful;
		std::vector<int> counts;
		(void)stateful.Every(1ms, [count = 0, &counts]() mutable { counts.emplace_back(++count); });
		Check(stateful.AdvanceTo(3ms).MyExecuted == 3);
		Check((counts == std::vector{1, 2, 3}));
		stateful.Clear();
		(void)stateful.Every(
			1ms,
			[count = 0, &counts]() mutable
			{
				counts.emplace_back(++count);
				if (count == 1) throw std::runtime_error("first invocation");
			}
		);
		Throws([&] { (void)stateful.AdvanceTo(5ms); });
		Check(stateful.AdvanceTo(5ms).MyExecuted == 1 && counts.back() == 2);
	}

	void PoolTests()
	{
		const auto catalog = Fixture();
		SharedPool pool(catalog, {}, 1.25);
		Check(pool.Find("a")->MyCapacity == 15);
		Check(pool.Take("a", 100) == 15 && pool.Take("a") == 0);
		Random random(123);
		const auto state = random.State();
		Check(!pool.Roll(random) && random.State() == state);
		Check(pool.RollItem(random, 6, catalog) == "item");
		Check(pool.Give("a", 100) == 15 && pool.Give("a") == 0);
		Throws([&] { (void)pool.Take("a", -1); });
		SharedPool banned(catalog, {"a"});
		Check(!banned.Find("a"));
		auto base = catalog.At("a");
		base.MyGoldenId.clear();
		base.MyMergeCount = 0;
		auto alias = base;
		alias.MyId = "alias";
		Throws([&] { Catalog invalid({base, alias}, {{"test", EconomyRules{}}}); });
		alias.MyShopEligible = false;
		const Catalog hiddenAlias({base, alias}, {{"test", EconomyRules{}}});
		Check(hiddenAlias.VisibleChess().size() == 1);
		std::vector<Seat> seats;
		for (int i = 0; i < 20; ++i)
			seats.push_back({i, std::to_string(i)});
		Check(PoolGroups(std::span(seats).first(6), true, false)[0].MyScale == 1.5);
		const auto seven = PoolGroups(std::span(seats).first(7), true, false);
		Check(seven.size() == 2 && seven[0].MyPlayers.size() == 4 && seven[1].MyPlayers.size() == 3);
		Check(PoolGroups(seats, true, false).size() == 5);
		Check(PoolGroups(seats, true, true).size() == 20);
		Throws([&] { (void)PoolGroups(seats, false, false); });
		seats[1].MyPlayerId = seats[0].MyPlayerId;
		Throws([&] { (void)PoolGroups(seats, true, false); });
	}

	void CombatTests()
	{
		Check(Mitigate(100, DamageType::PHYSICAL, {10000}) == 5);
		Check(Mitigate(100, DamageType::TRUE_DAMAGE, {10000}) == 100);
		Check(Mitigate(100, DamageType::ARTS, {0, 50}) == 50);
		Check(!LeaderHitCancelled(299999, true, true));
		Check(LeaderHitCancelled(299999.1, true, true));
		Check(!LeaderHitCancelled(300000, false, true));
		std::array<Shield, 3> shields{{{10, 0, 15}, {0, 1, 1}, {20, 0, 15}}};
		Check(AbsorbShields(100, DamageType::PHYSICAL, shields) == 0);
		Check(shields[0].MyHealth == 10 && shields[1].MyHits == 0);
		Check(AbsorbShields(25, DamageType::ARTS, shields) == 0 && shields[2].MyHealth == 5);
		Check(AbsorbShields(10, DamageType::ARTS, shields) == 5);
		Throws([&] { (void)AbsorbShields(1, static_cast<DamageType>(99), shields); });
		Throws([] { (void)Mitigate(std::numeric_limits<double>::infinity(), DamageType::PHYSICAL, {}); });
		SharedBossPool boss(100);
		Check(boss.Damage("one", 50.25) == 50.25);
		Check(boss.Damage("two", 49) == 49.75 && boss.Health() == 0);
		Check(boss.Damage("one", 1) == 0 && boss.Credits().at("two") == 49.75);
	}

	void EconomyTests()
	{
		for (const int count : {2, 3})
		{
			const auto catalog = Fixture(count);
			auto session = Session(catalog);
			session.BeginRound(1);
			for (int i = 0; i < count; ++i)
				Check(bool(session.Execute({"one", 1, Buy{static_cast<std::size_t>(i)}})));
			const auto view = *session.View("one");
			Check(view.MyHand.back()->MyId == "a_gold" && view.MyHand.back()->MyPoolCopies == count);
			Check(view.MyOffers.size() == 1 && view.MyOffers.front().size() == 1);
			Check(session.PoolConservationHolds());
			Check(bool(session.Execute({"one", 1, Sell{view.MyHand.back()->MyUid}})));
			Check(session.PoolConservationHolds());
			Check(bool(session.Execute({"one", 1, PickReward{0}})));
			Check(session.View("one")->MyOffers.empty());
		}
		const auto catalog = Fixture(3, 2);
		auto session = Session(catalog);
		session.BeginRound(1);
		Check(bool(session.Execute({"one", 1, Buy{0}})));
		Check(bool(session.Execute({"one", 1, Buy{1}})));
		const auto revision = session.Revision();
		Check(session.Execute({"one", 1, Buy{2}}).error() == CommandError::HAND_FULL);
		Check(session.Revision() == revision && session.PoolConservationHolds());
		Check(session.Execute({"unknown", 1, Refresh{}}).error() == CommandError::UNKNOWN_PLAYER);
		Check(session.Execute({"one", 0, Refresh{}}).error() == CommandError::STALE_ROUND);
		Check(bool(session.Execute({"two", 1, Freeze{}})));
		const auto frozen = session.View("two")->MyShop.front()->MyId;
		Check(bool(session.Execute({"two", 1, SetReady{true}})));
		Check(session.Execute({"two", 1, Refresh{}}).error() == CommandError::READY);
		Check(bool(session.Execute({"two", 1, SetReady{false}})));
		session.EndPreparation();
		Check(session.View("one")->MyFunds == 0 && !session.View("one")->MyShop[0]);
		Check(session.Execute({"one", 1, Refresh{}}).error() == CommandError::WRONG_PHASE);
		session.BeginRound(2);
		Check(session.View("two")->MyShop[0]->MyId == frozen && !session.View("two")->MyFrozen);
		Check(session.View("two")->MyUpgradePrice == 4);
		Check(bool(session.Execute({"two", 2, LevelUp{}})));
		Check(session.View("two")->MyLevel == 2 && session.View("two")->MyShop.size() == 5);
		auto copy = *session.View("two");
		copy.MyFunds = 9999;
		Check(session.View("two")->MyFunds != 9999);
		Throws([&] { session.BeginRound(3); });
		const auto limitedCatalog = Fixture(3, 10, 1);
		auto exhausted = Session(limitedCatalog);
		exhausted.BeginRound(1);
		Check(bool(exhausted.Execute({"one", 1, Buy{0}})));
		Check(exhausted.Execute({"two", 1, Buy{0}}).error() == CommandError::SOLD_OUT);
		Check(exhausted.PoolConservationHolds());
	}

	void SettlementBridgeTests()
	{
		const auto catalog = Fixture();
		for (const bool rescue : {false, true})
		{
			auto session = Session(catalog); session.BeginRound(1);
			for (std::size_t i = 0; i < 3; ++i) Check(session.Execute(CommandEnvelope{.MyPlayerId = "one", .MyRound = 1, .MyCommand = Buy{.MySlot = i}}).has_value());
			const auto uid = session.View("one")->MyHand.back()->MyUid;
			Check(session.Execute(CommandEnvelope{.MyPlayerId = "one", .MyRound = 1,
				.MyCommand = MoveToBoard{.MyUid = uid, .MyPosition = BoardPosition{.MyRow = 12, .MyColumn = 2}}}).has_value());
			session.EndPreparation();
			RoundLedger ledger(SettlementRules{.MyRevivalEnabled = rescue}, {
				MatchPlayerProgress{.MyPlayerId = "one", .MyLife = 1},
				MatchPlayerProgress{.MyPlayerId = "two", .MyBounties = {ActiveBounty{.MyCard = WaveBounty{.MyCoins = 5, .MyPerfect = true}}}}});
			const std::array normal{
				BattlePlayerState{.MyPlayerId = "one", .MyPerfect = false, .MyLeaks = {LeakedEnemy{.MyCounted = true}}},
				BattlePlayerState{.MyPlayerId = "two", .MyCoins = 7.8}};
			const UnitePlan plan{.MyHelpers = {"two"}, .MyLeakers = {"one"}};
			const BattleResult unite{.MyReason = BattleEndReason::TIMEOUT, .MyPlayers = {BattlePlayerState{.MyPlayerId = "two",
				.MyLeaks = {LeakedEnemy{.MySourcePlayer = "one", .MyCounted = true}}}}};
			const std::array stages{UniteSettlementStage{.MyPlan = std::cref(plan), .MyResult = std::cref(unite)}};
			(void)ledger.Settle(RoundSettlementInput{.MyRound = 1, .MyNow = 10, .MyNormalResults = normal,
				.MyUniteStages = rescue ? std::span<const UniteSettlementStage>(stages) : std::span<const UniteSettlementStage>{}});
			if (rescue)
			{
				Throws([&] { ledger.CommitToPreparation(session); });
				Check(session.View("one")->DeployCount() == 1 && session.PoolConservationHolds());
				Check(ledger.Revive("two", "one", 1, 11).has_value());
				Check(ledger.CloseRevival().empty());
			}
			ledger.CommitToPreparation(session);
			Throws([&] { ledger.CommitToPreparation(session); });
			Check(session.PoolConservationHolds() && session.View("one")->MyAlive == rescue);
			Check(session.View("one")->DeployCount() == (rescue ? 1U : 0U));
			Check(session.View("two")->MyFunds == 0 && session.View("two")->MyPendingFunds == 12);
			Check(ledger.Players()[1].MyPendingFunds == 0);
			session.BeginRound(2);
			Check(session.View("two")->MyFunds == 112 && session.View("two")->MyPendingFunds == 0);
			if (!rescue)
			{
				Check(session.View("one")->MyFunds == 0 && session.View("one")->MyShop.empty());
				Check(session.Execute(CommandEnvelope{.MyPlayerId = "one", .MyRound = 2, .MyCommand = Refresh{}}).error() == CommandError::ELIMINATED);
			}
			session.EndPreparation();
			const auto revision = session.Revision();
			const std::array duplicates{EconomySettlement{.MyPlayerId = "two", .MyFunds = 8}, EconomySettlement{.MyPlayerId = "two", .MyFunds = 9}};
			Throws([&] { session.ApplySettlement(2, duplicates); });
			Check(session.Revision() == revision && session.View("two")->MyPendingFunds == 0 && session.PoolConservationHolds());
			const std::array bad{BattlePlayerState{.MyPlayerId = "two", .MyCoins = std::numeric_limits<double>::quiet_NaN()}};
			const auto perfects = ledger.Players()[1].MyStatistics.MyPerfectRounds;
			Throws([&] { (void)ledger.Settle(RoundSettlementInput{.MyRound = 2, .MyNow = 30, .MyNormalResults = bad}); });
			Check(ledger.Players()[1].MyStatistics.MyPerfectRounds == perfects && ledger.Players()[1].MyPendingFunds == 0);
		}
	}

	void PlacementTests()
	{
		for (std::size_t i = 0; i < 36; ++i)
			Check(BoardPosition::FromIndex(i).Index() == i);
		Throws([] { (void)BoardPosition{8, 2}.Index(); });
		Throws([] { (void)BoardPosition::FromIndex(36); });
		BoardLayout layout = BoardLayout::Fallback();
		layout.MyTiles[BoardPosition{12, 2}.Index()] = Terrain::HIGH;
		Check(!layout.CanPlace(PlacementClass::MELEE, {12, 2}));
		Check(layout.CanPlace(PlacementClass::ANY, {12, 2}));
		Check(layout.CanPlace(PlacementClass::HIGH_ONLY, {12, 2}));
		Check(!layout.CanPlace(PlacementClass::HIGH_ONLY, {12, 3}));
		Check(!layout.CanPlace(PlacementClass::ANY, {12, 9}));
		Check(!layout.CanPlace(PlacementClass::ANY, {std::numeric_limits<int>::max(), 2}));
		EconomyRules rules;
		rules.MyIncome = {0, 1000};
		rules.MyHandSize = 2;
		rules.MyDeployCap = 2;
		rules.MyLayouts[0] = {8, 1};
		const Catalog catalog(
			{{"melee", "melee", {}, PieceKind::CHESS, 1, 0, 1, 40, 0, false, true, PlacementClass::MELEE},
			 {"ranged", "ranged", {}, PieceKind::CHESS, 1, 0, 1, 40, 0, false, true},
			 {"item", "item", {}, PieceKind::ITEM, 1, 0, 0, 0, 0, false, true}},
			{{"test", rules}});
		const std::array<Seat, 1> Seats{{{0, "one"}}};
		EconomySession session(catalog, "test", Seats, 7, false, false, {}, layout);
		session.BeginRound(1);
		const auto execute = [&](PreparationCommand _command)
		{
			auto result = session.Execute({"one", 1, _command});
			Check(session.PoolConservationHolds());
			return result;
		};
		const auto buy = [&](std::string_view _id)
		{
			for (int attempt = 0; attempt < 50; ++attempt)
			{
				const auto view = *session.View("one");
				for (std::size_t i = 0; i < view.MyShop.size(); ++i)
					if (view.MyShop[i] && !view.MyShop[i]->MySold && view.MyShop[i]->MyId == _id)
					{
						const auto result = execute(Buy{i});
						Check(bool(result));
						return result->MyEvents.back().MyUid;
					}
				Check(bool(execute(Refresh{})));
			}
			throw std::runtime_error("fixture draw failed");
		};
		const auto ranged = buy("ranged");
		Check(bool(execute(MoveToBoard{ranged, {12, 2}, Facing::LEFT})));
		const auto melee = buy("melee");
		Check(execute(MoveToBoard{melee, {12, 2}}).error() == CommandError::BAD_TILE);
		Check(bool(execute(MoveToBoard{melee, {12, 3}, Facing::DOWN})));
		const auto revision = session.Revision();
		Check(execute(MoveToBoard{ranged, {12, 3}}).error() == CommandError::BAD_TILE);
		Check(session.Revision() == revision);
		Check(session.View("one")->MyBoard[0]->MyFacing == Facing::LEFT);
		const auto replacement = buy("ranged");
		Check(execute(MoveToBoard{replacement, {12, 4}}).error() == CommandError::BOARD_FULL);
		Check(bool(execute(MoveToBoard{replacement, {12, 3}, Facing::UP})));
		Check(session.View("one")->DeployCount() == 2);
		Check(session.View("one")->MyHand[1]->MyUid == melee);
		Check(execute(MoveToHand{ranged, 1}).error() == CommandError::BAD_TILE);
		Check(bool(execute(MoveToHand{replacement, 1})));
		Check(session.View("one")->MyBoard[1]->MyUid == melee);
		Check(session.View("one")->MyBoard[1]->MyFacing == Facing::UP);
		const auto item = buy("item");
		Check(execute(MoveToHand{ranged, 0}).error() == CommandError::HAND_FULL);
		Check(execute(MoveToBoard{item, {12, 4}}).error() == CommandError::BAD_TARGET);
		Check(bool(execute(Sell{replacement})));
		Check(bool(execute(MoveToHand{ranged, 0})));
		Check(session.View("one")->MyHand[1]->MyUid == ranged);
		Check(session.View("one")->MyHand[0]->MyUid == item);
		Check(bool(execute(MoveToHand{ranged, 0})));
		Check(session.View("one")->MyHand[0]->MyUid == ranged);
		Check(bool(execute(MoveToBoard{ranged, {12, 2}, Facing::LEFT})));
		Check(bool(execute(MoveToBoard{ranged, {12, 2}, Facing::DOWN})));
		Check(session.View("one")->MyBoard[0]->MyFacing == Facing::DOWN);
		Check(execute(MoveToBoard{ranged, {12, 2}, static_cast<Facing>(99)}).error() == CommandError::BAD_TARGET);
		Check(execute(MoveToBoard{ranged, {8, 2}}).error() == CommandError::BAD_TILE);
		Check(execute(MoveToHand{ranged, 2}).error() == CommandError::BAD_TARGET);
		Check(execute(MoveToHand{999, 0}).error() == CommandError::BAD_TARGET);
		Check(bool(execute(SetReady{true})));
		Check(execute(MoveToHand{ranged, 0}).error() == CommandError::READY);
		Check(bool(execute(SetReady{false})));
		Check(bool(execute(Sell{ranged})));
		Check(session.PublicView().front().MyBoard[1]->MyUid == melee);
		session.EndPreparation();
		session.BeginRound(2);
		Check(session.View("one")->MyBoard[1]->MyUid == melee);

		const auto mergeCatalog = Fixture();
		auto merging = Session(mergeCatalog);
		merging.BeginRound(1);
		Check(bool(merging.Execute({"one", 1, Buy{0}})));
		Check(bool(merging.Execute({"one", 1, MoveToBoard{1, {12, 8}, Facing::UP}})));
		Check(bool(merging.Execute({"one", 1, Buy{1}})));
		Check(bool(merging.Execute({"one", 1, MoveToBoard{2, {9, 2}, Facing::LEFT}})));
		Check(bool(merging.Execute({"one", 1, Buy{2}})));
		const auto merged = *merging.View("one");
		const auto elite = *merged.MyBoard[BoardPosition{9, 2}.Index()];
		Check(merged.DeployCount() == 1 && elite.MyUid == 4 && elite.MyId == "a_gold");
		Check(elite.MyFacing == Facing::LEFT && elite.MyPoolCopies == 3 && merged.MyOffers.size() == 1);
		Check(merging.PoolConservationHolds());
	}

	void PreparationContentTests()
	{
		const auto catalog = Fixture();
		auto session = Session(catalog);
		session.BeginRound(1);
		const auto effect = [&](EconomyEffect _effect) { return session.ApplyEconomyEffect("one", _effect).has_value(); };
		Check(effect(AdjustFunds{.MyAmount = std::numeric_limits<std::int64_t>::min()}));
		Check(session.View("one")->MyFunds == 0);
		Check(effect(AdjustFunds{.MyAmount = std::numeric_limits<std::int64_t>::max()}));
		const auto revision = session.Revision();
		Throws([&] { (void)effect(AdjustFunds{.MyAmount = 1}); });
		Check(session.Revision() == revision && session.View("one")->MyFunds == std::numeric_limits<std::int64_t>::max());
		Check(effect(GrantFreeRefreshes{.MyCount = std::numeric_limits<std::uint64_t>::max()}));
		Throws([&] { (void)effect(GrantFreeRefreshes{.MyCount = 1}); });
		Check(session.View("one")->MyFreeRefreshes == std::numeric_limits<std::uint64_t>::max());
		Check(!effect(AddPendingFunds{.MyAmount = -1}));
		Check(!effect(GrantPurchaseUpgrade{.MyUpgrade = PurchaseUpgrade{.MyKind = static_cast<PieceKind>(99), .MyRemaining = 1}}));
		Check(!session.ApplyEconomyEffect("missing", AdjustFunds{}));

		// 奖励可指定不占卡池的普通单位；未完成配置的 DIY 仍须在扣款或消耗选项前拒绝。
		const Catalog offered({Definition{.MyId = "ordinary", .MyBaseId = "ordinary", .MyGoldenId = {}, .MyPoolCopies = 10, .MyShopEligible = true},
			Definition{.MyId = "off-pool", .MyBaseId = "off-pool", .MyGoldenId = {}},
			Definition{.MyId = "diy", .MyBaseId = "diy", .MyGoldenId = {}, .MyRequiresSelection = true}}, {{"test", EconomyRules{}}});
		auto offers = Session(offered); offers.BeginRound(1);
		const std::array<std::string_view, 1> OffPool{"off-pool"}, Diy{"diy"};
		Check(offers.OfferPieces("one", PieceKind::CHESS, OffPool).has_value());
		Check(offers.Execute(CommandEnvelope{.MyPlayerId = "one", .MyRound = 1, .MyCommand = PickReward{.MySlot = 0}}).has_value());
		Check(offers.View("one")->MyHand.back()->MyPoolCopies == 0);
		Check(offers.OfferPieces("one", PieceKind::CHESS, Diy).has_value());
		const auto offeredRevision = offers.Revision();
		Check(offers.Execute(CommandEnvelope{.MyPlayerId = "one", .MyRound = 1, .MyCommand = PickReward{.MySlot = 0}}).error() == CommandError::BAD_TARGET);
		Check(offers.Revision() == offeredRevision && offers.View("one")->MyOffers.size() == 1 && offers.PoolConservationHolds());

		// 目录非法配置应尽早拒绝；配置目录被引用，临时对象不能建立该借用。
		Throws([] { (void)SummonCatalog({SummonDefinition{.MyId = "token"}, SummonDefinition{.MyId = "token"}}, {}); });
		Throws([] { (void)SummonCatalog({}, {SummonOwner{.MyId = "a", .MyTokens = {SummonCount{.MyId = "missing"}}}}); });
		Throws([] { (void)SummonCatalog({SummonDefinition{.MyId = "token"}}, {SummonOwner{.MyId = "a", .MyTokens = {SummonCount{.MyId = "token", .MyCount = 0}}}}); });
		const SummonCatalog summons({SummonDefinition{.MyId = "token"}}, {SummonOwner{.MyId = "a", .MyTokens = {SummonCount{.MyId = "token"}}}});
		auto configured = Session(catalog);
		Check(configured.ConfigureSummons("one", summons).has_value());
		Check(!configured.ConfigureSummons("missing", summons));
		configured.BeginRound(1);
		Check(!configured.ConfigureSummons("one", summons));
		Check(configured.PoolConservationHolds());
	}

	void ConservationTrace()
	{
		const auto catalog = Fixture();
		auto session = Session(catalog);
		Random random(91);
		for (int round = 1; round <= 30; ++round)
		{
			session.BeginRound(round);
			for (int step = 0; step < 100; ++step)
			{
				const auto id = random.Index(2) ? "one" : "two";
				const auto view = *session.View(id);
				PreparationCommand command = Buy{random.Index(static_cast<std::uint32_t>(view.MyShop.size()))};
				if (step % 3 == 0)
					command = Refresh{};
				if (step % 5 == 0)
					command = PickReward{0};
				if (step % 7 == 0)
				{
					const auto it = std::ranges::find_if(view.MyHand, [](const auto& _piece) { return bool(_piece); });
					if (it != view.MyHand.end())
					{
						if ((*it)->MyId.starts_with("item"))
							command = DestroyItem{(*it)->MyUid};
						else
							command = Sell{(*it)->MyUid};
					}
				}
				if (step % 11 == 0)
				{
					const auto it = std::ranges::find_if(view.MyHand, [](const auto& _piece) { return bool(_piece); });
					if (it != view.MyHand.end())
						command = MoveToBoard{(*it)->MyUid, BoardPosition::FromIndex(random.Index(36))};
				}
				if (step % 13 == 0)
				{
					const auto it = std::ranges::find_if(view.MyBoard, [](const auto& _piece) { return bool(_piece); });
					if (it != view.MyBoard.end())
						command = MoveToHand{(*it)->MyUid, random.Index(10)};
				}
				(void)session.Execute({id, round, command});
				Check(session.PoolConservationHolds());
			}
			session.EndPreparation();
		}
	}
} // namespace

int main()
{
	try
	{
		RandomTests();
		SchedulerTests();
		PoolTests();
		CombatTests();
		EconomyTests();
		SettlementBridgeTests();
		PlacementTests();
		PreparationContentTests();
		ConservationTrace();
		std::cout << "9 test groups passed\n";
	}

	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
