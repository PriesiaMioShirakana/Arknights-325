#include <iostream>
#include <limits>
#include <source_location>
#include <stronghold/domain/preparation_content.hpp>

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

	struct Fixture
	{
		explicit Fixture(std::span<const PreparationBandEffect> _effects, bool _keep = false)
			: MyRules{PreparationBandRule{"test", _effects, _keep}},
			MyEconomy(MyCatalog, "test", MySeats, 42), MyLedger({}, {MatchPlayerProgress{.MyPlayerId = "p"}}),
			MyContent(MyEconomy, MyLedger, {}, {}, MyBonds, MyPlayers, {}, {}, MyRules)
		{
		}

		void Begin()
		{
			MyContent.BeginRound(MyEconomy.Round() + 1, MyRandom);
			(void)MyContent.OnRoundStart(MyRandom); MyEconomy.BeginPreparation();
		}

		std::expected<ChangeSet, CommandError> Execute(PreparationCommand _command)
		{
			return MyContent.Execute({"p", MyEconomy.Round(), std::move(_command)}, MyRandom);
		}

		Catalog MyCatalog{{{.MyId = "a", .MyBaseId = "a", .MyGoldenId = "b", .MyPrice = 3,
			.MyPoolCopies = 20, .MyMergeCount = 3, .MyShopEligible = true},
			{.MyId = "b", .MyBaseId = "a", .MyGoldenId = "b", .MyPoolCopies = 20, .MyGolden = true}}, {{"test", EconomyRules{}}}};
		std::array<Seat, 1> MySeats{Seat{0, "p"}};
		std::array<double, 1> MyThresholds{1};
		std::array<BondRule, 1> MyBonds{BondRule{.MyId = "bond", .MyThresholds = MyThresholds}};
		std::array<std::string_view, 1> MyBondIds{"bond"};
		std::array<ContentPoolRoster, 1> MyRoster{ContentPoolRoster{"a", MyBondIds}};
		std::array<ChoiceRewardPlayerConfig, 1> MyPlayers{ChoiceRewardPlayerConfig{"p", MyRoster, {}, "test"}};
		std::array<PreparationBandRule, 1> MyRules;
		EconomySession MyEconomy;
		RoundLedger MyLedger;
		PreparationContent MyContent;
		Random MyRandom{42};
	};

	void DiscountsAndRejectedCommands()
	{
		const std::array effects{PreparationBandEffect{.MyKind = PreparationBandKind::FIRST_DISCOUNT, .MyCount = 1, .MyBond = "bond"}};
		Fixture fixture(effects); fixture.Begin();
		const ShopSlot price{.MyId = "a", .MyPrice = 3};
		Check(fixture.MyContent.PriceOf("p", price) == 1);
		Check(!fixture.MyContent.PriceOf("missing", price));
		const auto funds = fixture.MyEconomy.View("p")->MyFunds;
		Check(fixture.MyEconomy.ApplyEconomyEffect("p", AdjustFunds{-funds}).has_value());
		const auto revision = fixture.MyEconomy.Revision();
		const auto random = fixture.MyRandom.State();
		Check(!fixture.Execute(Buy{0}));
		Check(fixture.MyContent.PriceOf("p", price) == 1 && fixture.MyEconomy.Revision() == revision && fixture.MyRandom.State() == random);
		Check(fixture.MyEconomy.ApplyEconomyEffect("p", AdjustFunds{1}).has_value());
		Check(fixture.Execute(Buy{0}).has_value());
		Check(fixture.MyEconomy.View("p")->MyFunds == 0 && fixture.MyContent.PriceOf("p", price) == 3);
		fixture.MyContent.OnPreparationEnd(fixture.MyRandom); fixture.MyEconomy.EndPreparation(); fixture.Begin();
		Check(fixture.MyContent.PriceOf("p", price) == 1 && fixture.MyEconomy.PoolConservationHolds());
	}

	void IncomeRollbackAndEndOnce()
	{
		const std::array effects{PreparationBandEffect{.MyKind = PreparationBandKind::INTEREST, .MyCount = 1, .MyMaximum = 1, .MyThreshold = 5},
			PreparationBandEffect{.MyKind = PreparationBandKind::TIER_LAYERS, .MyCount = 2}};
		Fixture fixture(effects, true);
		Check(fixture.MyEconomy.ApplyEconomyEffect("p", AdjustFunds{std::numeric_limits<std::int64_t>::max()}).has_value());
		const auto revision = fixture.MyEconomy.Revision();
		Throws([&] { fixture.MyContent.BeginRound(1, fixture.MyRandom); });
		Check(fixture.MyEconomy.Round() == 0 && fixture.MyEconomy.Revision() == revision && fixture.MyRandom.State() == 42);
		Check(!fixture.MyEconomy.View("p")->MyKeepRemainingFunds);
		Check(fixture.MyEconomy.ApplyEconomyEffect("p", AdjustFunds{-std::numeric_limits<std::int64_t>::max()}).has_value());
		fixture.Begin();
		const auto piece = fixture.MyContent.GrantPiece("p", "a", fixture.MyRandom)->MyPiece;
		Check(piece.has_value()); Check(fixture.Execute(MoveToBoard{*piece, {12, 2}}).has_value());
		fixture.MyContent.OnPreparationEnd(fixture.MyRandom);
		Check(fixture.MyContent.View("p")->MyLayers.front().MyLayers == 2);
		const auto state = fixture.MyRandom.State();
		Throws([&] { fixture.MyContent.OnPreparationEnd(fixture.MyRandom); });
		Check(fixture.MyRandom.State() == state && fixture.MyContent.View("p")->MyLayers.front().MyLayers == 2);
		Check(fixture.MyEconomy.PoolConservationHolds());
	}
}

int main()
{
	try
	{
		DiscountsAndRejectedCommands(); IncomeRollbackAndEndOnce();
		const std::array invalid{PreparationBandEffect{.MyCount = -1}};
		Throws([&] { Fixture fixture(invalid); });
		std::cout << "Strategy prices, rollback, round resets and preparation-end deduplication passed\n";
	}
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
