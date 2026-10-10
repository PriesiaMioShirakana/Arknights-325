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
		explicit Fixture(std::span<const PreparationBondEffect> _effects)
			: MyEconomy(MyCatalog, "test", MySeats, 42), MyLedger({}, {MatchPlayerProgress{.MyPlayerId = "p"}}),
			MyContent(MyEconomy, MyLedger, {}, {}, MyBonds, MyPlayers, {}, {}, {}, {}, _effects)
		{
			MyContent.BeginRound(1, MyRandom); (void)MyContent.OnRoundStart(MyRandom); MyContent.BeginPreparation(MyRandom);
			MyUid = *MyContent.GrantPiece("p", "a", MyRandom)->MyPiece;
			Check(MyContent.Execute({"p", 1, MoveToBoard{MyUid, {12, 2}}}, MyRandom).has_value());
		}

		Catalog MyCatalog{{{.MyId = "a", .MyBaseId = "a", .MyGoldenId = "b", .MyPrice = 3,
			.MyPoolCopies = 20, .MyMergeCount = 3, .MyShopEligible = true},
			{.MyId = "b", .MyBaseId = "a", .MyGoldenId = "b", .MyPoolCopies = 20, .MyGolden = true}}, {{"test", EconomyRules{}}}};
		std::array<Seat, 1> MySeats{Seat{0, "p"}};
		std::array<double, 1> MyThresholds{1};
		std::array<BondRule, 1> MyBonds{BondRule{.MyId = "bond", .MyThresholds = MyThresholds}};
		std::array<std::string_view, 1> MyBondIds{"bond"};
		std::array<ContentPoolRoster, 1> MyRoster{ContentPoolRoster{"a", MyBondIds}};
		std::array<ChoiceRewardPlayerConfig, 1> MyPlayers{ChoiceRewardPlayerConfig{"p", MyRoster}};
		EconomySession MyEconomy;
		RoundLedger MyLedger;
		PreparationContent MyContent;
		Random MyRandom{42};
		PieceUid MyUid{};
	};

	void RollbackAndAuthority()
	{
		const std::array effects{PreparationBondEffect{"bond", PreparationBondKind::COIN_MILESTONE, 10, 2}};
		Fixture fixture(effects);
		const auto funds = fixture.MyEconomy.View("p")->MyFunds;
		Check(fixture.MyEconomy.ApplyEconomyEffect("p", AdjustFunds{std::numeric_limits<std::int64_t>::max() - funds}).has_value());
		const auto revision = fixture.MyEconomy.Revision();
		Throws([&] { (void)fixture.MyContent.AddLayers("p", "bond", 10, fixture.MyRandom); });
		Check(fixture.MyEconomy.Revision() == revision && fixture.MyRandom.State() == 42);
		Check(fixture.MyContent.View("p")->MyLayers.front().MyLayers == 0);
		Throws([&] { (void)fixture.MyContent.AddLayers("p", "bond", 10); });
		Check(fixture.MyEconomy.ApplyEconomyEffect("p", AdjustFunds{-std::numeric_limits<std::int64_t>::max()}).has_value());
		const std::array gains{BondNumber{"bond", 25}};
		Check(fixture.MyContent.SynchronizeLayers("p", 1, gains, fixture.MyRandom).error() == CommandError::WRONG_PHASE);
		fixture.MyEconomy.EndPreparation();
		Check(fixture.MyContent.SynchronizeLayers("p", 2, gains, fixture.MyRandom).error() == CommandError::STALE_ROUND);
		Check(fixture.MyEconomy.ApplyEconomyEffect("p", AdjustFunds{std::numeric_limits<std::int64_t>::max()}).has_value());
		// A rejected battle report must not consume its deduplication watermark.
		Throws([&] { (void)fixture.MyContent.SynchronizeLayers("p", 1, gains, fixture.MyRandom); });
		Check(fixture.MyContent.View("p")->MyLayers.front().MyCredited == 0);
	}

	void MilestonesAndDiscounts()
	{
		const std::array effects{PreparationBondEffect{"bond", PreparationBondKind::COIN_MILESTONE, 10, 2},
			PreparationBondEffect{.MyBond = "bond", .MyKind = PreparationBondKind::DISCOUNT, .MyStep = 80,
				.MyCount = 1, .MyHighStep = 150, .MyDiscountBond = "bond"},
			PreparationBondEffect{.MyBond = "bond", .MyKind = PreparationBondKind::PREP_LAYERS, .MyCount = 2, .MyHighCount = 4}};
		Fixture fixture(effects);
		Check(fixture.MyContent.AddLayers("p", "bond", 79, fixture.MyRandom).has_value());
		const ShopSlot slot{.MyId = "a", .MyPrice = 1};
		Check(fixture.MyContent.PriceOf("p", slot) == 1);
		fixture.MyContent.OnPreparationEnd(fixture.MyRandom);
		Check(fixture.MyContent.View("p")->MyLayers.front().MyLayers == 81);
		Check(fixture.MyEconomy.View("p")->MyPendingFunds == 2);
		Check(fixture.MyContent.PriceOf("p", slot) == 0);
		fixture.MyEconomy.EndPreparation();
		const std::array gains{BondNumber{"bond", 20}};
		Check(*fixture.MyContent.SynchronizeLayers("p", 1, gains, fixture.MyRandom));
		Check(fixture.MyEconomy.View("p")->MyFunds == 4);
		Check(!*fixture.MyContent.SynchronizeLayers("p", 1, gains, fixture.MyRandom));
		const std::array atCap{BondNumber{"bond", 10000}};
		Check(*fixture.MyContent.SynchronizeLayers("p", 1, atCap, fixture.MyRandom));
		const auto funds = fixture.MyEconomy.View("p")->MyFunds;
		Check(!*fixture.MyContent.SynchronizeLayers("p", 1, atCap, fixture.MyRandom));
		Check(fixture.MyContent.View("p")->MyLayers.front().MyLayers == 999 && fixture.MyEconomy.View("p")->MyFunds == funds);
		fixture.MyContent.BeginRound(2, fixture.MyRandom); (void)fixture.MyContent.OnRoundStart(fixture.MyRandom); fixture.MyContent.BeginPreparation(fixture.MyRandom);
		Check(fixture.MyContent.Execute({"p", 2, Sell{fixture.MyUid}}, fixture.MyRandom).has_value());
		const auto state = fixture.MyRandom.State();
		Check(fixture.MyContent.PriceOf("p", slot) == 0 && fixture.MyContent.PriceOf("p", slot) == 0);
		Check(fixture.MyRandom.State() == state && fixture.MyEconomy.PoolConservationHolds());
	}
}

int main()
{
	try
	{
		RollbackAndAuthority(); MilestonesAndDiscounts();
		const std::array invalid{PreparationBondEffect{"bond", PreparationBondKind::COIN_MILESTONE, 0, 2}};
		Throws([&] { Fixture fixture(invalid); });
		std::cout << "Bond reward rollback, pending funds, permanent discounts and cumulative gain deduplication passed\n";
	}
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
