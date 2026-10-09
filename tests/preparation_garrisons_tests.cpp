#include <iostream>
#include <source_location>
#include <stronghold/domain/preparation_content.hpp>

namespace
{
	using namespace Stronghold;

	void Check(bool _value, std::source_location _where = std::source_location::current())
	{
		if (!_value) throw std::runtime_error(std::string(_where.file_name()) + ":" + std::to_string(_where.line()));
	}

	void RecursiveGift()
	{
		const Catalog catalog({
			{.MyId = "a", .MyBaseId = "a", .MyGoldenId = "b", .MyPrice = 1, .MyPoolCopies = 99, .MyMergeCount = 3, .MyShopEligible = true},
			{.MyId = "b", .MyBaseId = "a", .MyGoldenId = "b", .MyPoolCopies = 99, .MyGolden = true}}, {{"test", EconomyRules{}}});
		const std::array seats{Seat{0, "p"}};
		const std::array<std::string_view, 1> traits{"gift"}, ids{"a"};
		const std::array roster{ContentPoolRoster{"a", {}, traits}, ContentPoolRoster{"b", {}, traits}};
		const std::array players{ChoiceRewardPlayerConfig{"p", roster}};
		const std::array pools{ContentPoolRecord{.MyId = "self", .MyKind = PieceKind::CHESS, .MyItems = ids}};
		const std::array traitsRules{PreparationGarrisonRule{.MyId = "gift", .MyEvent = GarrisonEvent::GAIN,
			.MyKind = GarrisonKind::POOL_CHAR, .MyPool = "self"}};
		EconomySession economy(catalog, "test", seats, 42);
		RoundLedger ledger({}, {MatchPlayerProgress{.MyPlayerId = "p"}});
		PreparationContent content(economy, ledger, {}, pools, {}, players, {}, {}, {}, {traitsRules});
		Random random(42);
		content.BeginRound(1, random); (void)content.OnRoundStart(random); economy.BeginPreparation();
		const auto result = content.GrantPiece("p", "a", random);
		Check(result && result->MyPiece && *result->MyPiece == 1);
		const auto view = *economy.View("p");
		Check(view.MyRoundStatistics.MyGainedChess > 1 && view.MyRoundStatistics.MyGainedChess <= 10);
		Check(std::ranges::none_of(view.MyHand, [&](const auto& _piece) { return _piece && _piece->MyUid == *result->MyPiece; }));
		Check(!result->MyChanges.MyEvents.empty() && result->MyChanges.MyEvents.back().MyDefinitionId == "a");
		Check(content.Execute({"p", 1, Buy{0}}, random).has_value());
		Check(economy.PoolConservationHolds());
	}

	void DeferredItemMerge()
	{
		const Catalog catalog({
			{.MyId = "a", .MyBaseId = "a", .MyGoldenId = "a", .MyPoolCopies = 20},
			{.MyId = "item", .MyBaseId = "item", .MyGoldenId = "elite", .MyKind = PieceKind::ITEM, .MyMergeCount = 3},
			{.MyId = "elite", .MyBaseId = "item", .MyGoldenId = "elite", .MyKind = PieceKind::ITEM, .MyGolden = true}}, {{"test", EconomyRules{}}});
		const std::array seats{Seat{0, "p"}};
		const std::array<std::string_view, 1> traits{"gear"};
		const std::array roster{ContentPoolRoster{"a", {}, traits}};
		const std::array players{ChoiceRewardPlayerConfig{"p", roster}};
		const std::array rules{PreparationGarrisonRule{.MyId = "gear", .MyEvent = GarrisonEvent::PREP_END,
			.MyKind = GarrisonKind::GAIN_EQUIP, .MyCount = 3, .MyChess = "item"}};
		EconomySession economy(catalog, "test", seats, 42);
		RoundLedger ledger({}, {MatchPlayerProgress{.MyPlayerId = "p"}});
		PreparationContent content(economy, ledger, {}, {}, {}, players, {}, {}, {}, {rules});
		Random random(42);
		content.BeginRound(1, random); (void)content.OnRoundStart(random); economy.BeginPreparation();
		Check(content.GrantPiece("p", "a", random).has_value());
		content.OnPreparationEnd(random);
		const auto before = *economy.View("p");
		Check(std::ranges::count_if(before.MyHand, [](const auto& _piece) { return _piece && _piece->MyId == "item"; }) == 3);
		economy.EndPreparation(); content.BeginRound(2, random); (void)content.OnRoundStart(random);
		Check(economy.View("p")->MyStatistics.MyItemMerges == 0);
		economy.BeginPreparation();
		Check(economy.View("p")->MyStatistics.MyItemMerges == 1 && economy.PoolConservationHolds());
	}

	void RefreshSnapshot()
	{
		const Catalog catalog({
			{.MyId = "a", .MyBaseId = "a", .MyGoldenId = "b", .MyPoolCopies = 99, .MyMergeCount = 3, .MyShopEligible = true},
			{.MyId = "b", .MyBaseId = "a", .MyGoldenId = "b", .MyPoolCopies = 99, .MyGolden = true}}, {{"test", EconomyRules{}}});
		const std::array seats{Seat{0, "p"}};
		const std::array<std::string_view, 1> traits{"refresh"}, ids{"bond"};
		const std::array roster{ContentPoolRoster{"a", ids, traits}, ContentPoolRoster{"b", ids, traits}};
		const std::array players{ChoiceRewardPlayerConfig{"p", roster, {}, "gift"}};
		const std::array<double, 1> thresholds{1};
		const std::array bonds{BondRule{.MyId = "bond", .MyThresholds = thresholds}};
		const std::array rules{PreparationGarrisonRule{.MyId = "refresh", .MyEvent = GarrisonEvent::REFRESH,
			.MyKind = GarrisonKind::GAIN_BOND_LAYER_BY_REFRESH_CNT, .MyLayer = 4, .MyBonds = ids}};
		const std::array effects{PreparationBandEffect{.MyKind = PreparationBandKind::REFRESH_GIFT, .MyThreshold = 1, .MyBond = "bond"}};
		const std::array bands{PreparationBandRule{"gift", effects}};
		EconomySession economy(catalog, "test", seats, 42);
		RoundLedger ledger({}, {MatchPlayerProgress{.MyPlayerId = "p"}});
		PreparationContent content(economy, ledger, {}, {}, bonds, players, {}, {}, bands, {rules});
		Random random(42);
		content.BeginRound(1, random); (void)content.OnRoundStart(random); economy.BeginPreparation();
		Check(content.GrantPiece("p", "a", random).has_value());
		for (const auto expected : {4, 4, 8})
		{
			Check(content.Execute({"p", 1, Refresh{}}, random).has_value());
			Check(content.View("p")->MyLayers.front().MyLayers == expected);
		}
		Check(economy.PoolConservationHolds());
	}
}

int main()
{
	try { RecursiveGift(); DeferredItemMerge(); RefreshSnapshot(); std::cout << "Garrison reentry, refresh snapshot, source lifetime and deferred item merge passed\n"; }
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
