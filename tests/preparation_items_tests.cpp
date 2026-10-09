#include <iostream>
#include <limits>
#include <source_location>
#include <stronghold/domain/preparation_content.hpp>

namespace
{
	using namespace Stronghold;

	void Check(bool _condition, std::source_location _where = std::source_location::current())
	{
		if (!_condition) throw std::runtime_error(std::string(_where.file_name()) + ":" + std::to_string(_where.line()));
	}

	template <class Callable>
	void Throws(Callable _callable)
	{
		bool threw = false;
		try { _callable(); }
		catch (const std::exception&) { threw = true; }
		Check(threw);
	}

	Catalog TestCatalog()
	{
		return Catalog({
			{.MyId = "a", .MyBaseId = "a", .MyGoldenId = "gold", .MyPoolCopies = 20, .MyMergeCount = 3, .MyShopEligible = true},
			{.MyId = "gold", .MyBaseId = "a", .MyGoldenId = "gold", .MyPoolCopies = 20, .MyGolden = true},
			{.MyId = "gear", .MyBaseId = "gear", .MyGoldenId = "gear", .MyKind = PieceKind::ITEM},
			{.MyId = "persistent", .MyBaseId = "persistent", .MyGoldenId = "persistent", .MyKind = PieceKind::ITEM},
			{.MyId = "art", .MyBaseId = "art", .MyGoldenId = "art", .MyKind = PieceKind::ITEM, .MyItemUse = ItemUse::ART},
			{.MyId = "consume", .MyBaseId = "consume", .MyGoldenId = "consume", .MyKind = PieceKind::ITEM,
				.MyItemUse = ItemUse::CONSUME_ON_EQUIP}}, {{"test", EconomyRules{}}});
	}

	class Fixture
	{
	public:
		explicit Fixture(std::span<const PreparationItemEffect> _effects, ItemUse _use = ItemUse::CONSUME_ON_EQUIP,
			PreparationArtRules _arts = {})
			: MyEconomy(MyCatalog, "test", MySeats, 42), MyLedger({},
				{MatchPlayerProgress{.MyPlayerId = "p"}, MatchPlayerProgress{.MyPlayerId = "q"}}),
			MyRules{PreparationItemRule{.MyId = _use == ItemUse::ART ? "art" : _use == ItemUse::EQUIPMENT ? "persistent" : "consume",
				.MyUse = _use, .MyEffects = _effects}},
			MyContent(MyEconomy, MyLedger, {}, {}, MyBonds, MyPlayers, MyRules, _arts)
		{
			MyEconomy.BeginRound(1);
		}

		PieceUid Grant(std::string_view _id, std::string_view _player = "p")
		{
			return *MyEconomy.GrantPiece(_player, _id)->MyPiece;
		}

		std::expected<ChangeSet, CommandError> Equip(PieceUid _item, PieceUid _holder,
			std::string_view _player = "p", std::optional<PieceUid> _replace = {})
		{
			return MyContent.Execute({std::string(_player), MyEconomy.Round(), EquipItem{_item, _holder, _replace}}, MyRandom);
		}

		Catalog MyCatalog{TestCatalog()};
		std::array<Seat, 2> MySeats{Seat{0, "p"}, Seat{1, "q"}};
		std::array<double, 1> MyThresholds{1};
		std::array<BondRule, 1> MyBonds{BondRule{.MyId = "bond", .MyThresholds = MyThresholds}};
		std::array<std::string_view, 1> MyBondIds{"bond"};
		std::array<ContentPoolRoster, 1> MyRoster{ContentPoolRoster{"a", MyBondIds}};
		std::array<ChoiceRewardPlayerConfig, 2> MyPlayers{
			ChoiceRewardPlayerConfig{"p", MyRoster}, ChoiceRewardPlayerConfig{"q", MyRoster}};

		EconomySession MyEconomy;
		RoundLedger MyLedger;
		std::array<PreparationItemRule, 1> MyRules;
		PreparationContent MyContent;
		Random MyRandom{42};
	};

	bool Has(const PlayerView& _view, PieceUid _uid)
	{
		const auto match = [&](const auto& _piece) { return _piece && _piece->MyUid == _uid; };
		return std::ranges::any_of(_view.MyHand, match) || std::ranges::any_of(_view.MyTemporary, match) ||
			std::ranges::any_of(_view.MyBoard, match);
	}

	void AtomicRejection()
	{
		const std::array effects{
			PreparationItemEffect{.MyKind = PreparationItemKind::COINS, .MyMinimum = 1, .MyMaximum = 6},
			PreparationItemEffect{.MyKind = PreparationItemKind::LAYERS, .MyCount = 3},
			PreparationItemEffect{.MyKind = PreparationItemKind::ROUND_COINS, .MyCount = 2},
			PreparationItemEffect{.MyKind = PreparationItemKind::SHOP_CHESS, .MyCount = 1},
			PreparationItemEffect{.MyKind = PreparationItemKind::PROMOTE}};
		Fixture test(effects);
		const auto holder = test.Grant("gold"), first = test.Grant("gear"), second = test.Grant("gear");
		Check(test.Equip(first, holder).has_value()); Check(test.Equip(second, holder).has_value());
		const auto item = test.Grant("consume");
		const auto before = *test.MyEconomy.View("p");
		const auto revision = test.MyEconomy.Revision(), state = static_cast<std::uint64_t>(test.MyRandom.State());
		const auto stock = test.MyEconomy.CopyStock("p", "a")->MyRemaining;
		const auto result = test.Equip(item, holder, "p", second);
		Check(!result && result.error() == CommandError::BAD_TARGET);
		const auto after = *test.MyEconomy.View("p");
		Check(after.MyFunds == before.MyFunds && after.MyStatistics.MyFundsGained == before.MyStatistics.MyFundsGained);
		Check(after.MyRoundStatistics.MyGainedChess == before.MyRoundStatistics.MyGainedChess && Has(after, item));
		Check(test.MyEconomy.Revision() == revision && test.MyRandom.State() == state);
		Check(test.MyEconomy.CopyStock("p", "a")->MyRemaining == stock && test.MyEconomy.PoolConservationHolds());
		for (const auto& piece : after.MyHand)
			if (piece && piece->MyUid == holder)
				Check(piece->MyItems.size() == 2 && piece->MyItems[0].MyUid == first && piece->MyItems[1].MyUid == second);
		Check(test.MyContent.View("p")->MyLayers.front().MyLayers == 0);
		Check(test.MyContent.ItemEffects("p")->empty());
		Check(test.Grant("gear") == item + 1); // Failed random acquisition must not consume the shared UID stream.
	}

	void OverflowAndGates()
	{
		const std::array effects{PreparationItemEffect{.MyKind = PreparationItemKind::COINS, .MyMinimum = 1, .MyMaximum = 1}};
		Fixture test(effects);
		const auto holder = test.Grant("a"), item = test.Grant("consume");
		const auto before = test.MyRandom.State();
		Check(test.MyContent.Execute({"p", 0, EquipItem{item, holder}}, test.MyRandom).error() == CommandError::STALE_ROUND);
		Check(test.Equip(item, holder, "missing").error() == CommandError::UNKNOWN_PLAYER);
		Check(test.Equip(item, item).error() == CommandError::BAD_TARGET);
		Check(test.MyRandom.State() == before);
		const auto amount = std::numeric_limits<std::int64_t>::max() - test.MyEconomy.View("p")->MyFunds;
		Check(test.MyEconomy.ApplyEconomyEffect("p", AdjustFunds{amount}).has_value());
		const auto revision = test.MyEconomy.Revision();
		Throws([&] { (void)test.Equip(item, holder); });
		Check(test.MyRandom.State() == before && test.MyEconomy.Revision() == revision && Has(*test.MyEconomy.View("p"), item));
		Check(test.MyEconomy.ApplyEconomyEffect("p", AdjustFunds{-10}).has_value());
		Check(test.Equip(item, holder).has_value());
		Random expected(before); (void)expected.Index(1);
		Check(test.MyRandom.State() == expected.State()); // A fixed one-coin award still advances Mulberry32.
	}

	void RoundTransaction()
	{
		const std::array effects{PreparationItemEffect{.MyKind = PreparationItemKind::ROUND_COINS, .MyCount = 10}};
		Fixture test(effects);
		for (const auto player : {"p", "q"})
		{
			const auto holder = test.Grant("a", player), item = test.Grant("consume", player);
			Check(test.Equip(item, holder, player).has_value());
		}
		Throws([&] { (void)test.MyContent.OnRoundStart(test.MyRandom); });
		test.MyEconomy.EndPreparation(); test.MyEconomy.BeginRound(2, false);
		const auto amount = std::numeric_limits<std::int64_t>::max() - test.MyEconomy.View("q")->MyFunds;
		Check(test.MyEconomy.ApplyEconomyEffect("q", AdjustFunds{amount}).has_value());
		const auto funds = test.MyEconomy.View("p")->MyFunds;
		const auto revision = test.MyEconomy.Revision();
		Throws([&] { (void)test.MyContent.OnRoundStart(test.MyRandom); });
		Check(test.MyEconomy.View("p")->MyFunds == funds && test.MyEconomy.Revision() == revision);
		Check(test.MyEconomy.ApplyEconomyEffect("q", AdjustFunds{-20}).has_value());
		const auto result = test.MyContent.OnRoundStart(test.MyRandom);
		Check(result.MyRecipients.size() == 2 && test.MyEconomy.View("p")->MyFunds == funds + 10);
		Throws([&] { (void)test.MyContent.OnRoundStart(test.MyRandom); });
		Check(test.MyEconomy.View("p")->MyFunds == funds + 10);
	}

	void UnsupportedRules()
	{
		const std::array effects{PreparationItemEffect{.MyKind = PreparationItemKind::CAULDRON}};
		Fixture test(effects);
		const auto holder = test.Grant("a"), item = test.Grant("consume");
		Check(test.Equip(item, holder).error() == CommandError::BAD_TARGET && Has(*test.MyEconomy.View("p"), item));
		const std::array invalid{PreparationItemEffect{.MyKind = PreparationItemKind::COINS, .MyMinimum = 2, .MyMaximum = 1}};
		Throws([&] { Fixture bad(invalid); });
	}

	void ArtTransactions()
	{
		const std::array effects{PreparationItemEffect{.MyKind = PreparationItemKind::COPY_ART},
			PreparationItemEffect{.MyKind = PreparationItemKind::TRAINING_BOUNTY}};
		Fixture test(effects, ItemUse::ART);
		const auto holder = test.Grant("a"), item = test.Grant("art");
		Check(test.MyEconomy.Execute({"p", 1, MoveToBoard{holder, {12, 2}}}).has_value());
		const CommandEnvelope command{"p", 1, UseArt{item, {12, 2}}};
		// 经济底层拒绝绕过内容执行器；复制成功之后悬赏池为空，也必须回滚整条法术。
		Check(test.MyEconomy.Execute(command).error() == CommandError::BAD_TARGET);
		const auto revision = test.MyEconomy.Revision();
		const auto state = test.MyRandom.State();
		const auto before = *test.MyEconomy.View("p");
		const auto stock = test.MyEconomy.CopyStock("p", "a")->MyRemaining;
		Check(test.MyContent.Execute(command, test.MyRandom).error() == CommandError::BAD_TARGET);
		const auto after = *test.MyEconomy.View("p");
		Check(test.MyEconomy.Revision() == revision && test.MyRandom.State() == state);
		Check(after.MyRoundStatistics.MyGainedChess == before.MyRoundStatistics.MyGainedChess);
		Check(after.MyRoundStatistics.MyArts == 0 && Has(after, item) && test.MyLedger.Players().front().MyBounties.empty());
		Check(test.MyEconomy.CopyStock("p", "a")->MyRemaining == stock && test.MyEconomy.PoolConservationHolds());
		Check(test.Grant("gear") == item + 1);

		const std::array destroy{PreparationItemEffect{.MyKind = PreparationItemKind::DESTROY_PASS, .MyCount = 1}};
		Fixture passing(destroy, ItemUse::ART);
		const auto pass = passing.Grant("art");
		const auto funds = passing.MyEconomy.View("p")->MyFunds;
		Check(passing.MyEconomy.ApplyEconomyEffect("p", AdjustFunds{std::numeric_limits<std::int64_t>::max() - funds}).has_value());
		const CommandEnvelope destroyCommand{"p", 1, DestroyItem{pass}};
		const auto version = passing.MyEconomy.Revision();
		Throws([&] { (void)passing.MyContent.Execute(destroyCommand, passing.MyRandom); });
		Check(passing.MyEconomy.Revision() == version && Has(*passing.MyEconomy.View("p"), pass));
		Check(passing.MyEconomy.ApplyEconomyEffect("p", AdjustFunds{-10}).has_value());
		const auto result = passing.MyContent.Execute(destroyCommand, passing.MyRandom);
		Check(result.has_value());
		Check(std::ranges::any_of(result->MyEvents, [](const auto& _event)
			{ return _event.MyKind == EventKind::GRANTED && _event.MyPlayerId == "q"; }));
	}

	void ArtBountyFiltering()
	{
		const std::array cards{
			ChoiceCardRecord{.MyKind = ChoiceCardKind::BOUNTY, .MyId = "training", .MyTier = 1,
				.MyEnemyId = "enemy", .MyPerfect = true},
			ChoiceCardRecord{.MyKind = ChoiceCardKind::BOUNTY, .MyId = "fallback", .MyTier = 2, .MyEnemyId = "other"}};
		const std::array blocked{std::string_view("enemy")};
		const std::array effect{PreparationItemEffect{.MyKind = PreparationItemKind::TRAINING_BOUNTY}};
		Fixture test(effect, ItemUse::ART, {cards, blocked});
		const auto art = test.Grant("art");
		Check(test.MyContent.Execute({"p", 1, UseArt{art, {12, 2}}}, test.MyRandom).has_value());
		const auto& bounty = test.MyLedger.Players().front().MyBounties.front();
		Check(bounty.MyCard.MyEnemyId == "other" && !bounty.MyCard.MyPerfect);
		Random expected(42); (void)expected.Index(1);
		Check(test.MyRandom.State() == expected.State());
		Check(test.MyEconomy.View("p")->MyRoundStatistics.MyArts == 1);
	}

	void OwnedItemTransactions()
	{
		const std::array companions{std::string_view("gear")};
		const std::array effects{PreparationItemEffect{.MyKind = PreparationItemKind::CAULDRON, .MyCount = 2,
			.MyMaximum = 1, .MyBond = "bond", .MyOtherItems = companions}};
		Fixture test(effects, ItemUse::EQUIPMENT);
		const auto holder = test.Grant("a"), gear = test.Grant("gear"), cauldron = test.Grant("persistent");
		Check(test.Equip(gear, holder).has_value()); Check(test.Equip(cauldron, holder).has_value());
		const auto funds = test.MyEconomy.View("p")->MyFunds;
		Check(test.MyEconomy.ApplyEconomyEffect("p", AdjustFunds{std::numeric_limits<std::int64_t>::max() - funds}).has_value());
		const auto revision = test.MyEconomy.Revision();
		const auto before = test.MyEconomy.CopyStock("p", "a")->MyRemaining;
		Throws([&] { (void)test.MyContent.GrantPiece("p", "gold", test.MyRandom); });
		Check(test.MyEconomy.Revision() == revision && test.MyEconomy.CopyStock("p", "a")->MyRemaining == before);
		Check(test.MyEconomy.ApplyEconomyEffect("p", AdjustFunds{-10}).has_value());
		const auto recovered = test.MyContent.GrantPiece("p", "gold", test.MyRandom);
		Check(recovered && recovered->MyPiece == cauldron + 1);
		Check(test.MyEconomy.View("p")->MyFunds == std::numeric_limits<std::int64_t>::max() - 8);
		Check(test.MyContent.GrantPiece("p", "gold", test.MyRandom).has_value());
		Check(test.MyEconomy.View("p")->MyFunds == std::numeric_limits<std::int64_t>::max() - 8);
		Check(test.MyContent.OnBattleResult("p", 1, test.MyRandom).error() == CommandError::WRONG_PHASE);
		test.MyEconomy.EndPreparation();
		Check(test.MyContent.OnBattleResult("p", 1, test.MyRandom).has_value());
		Check(test.MyContent.OnBattleResult("p", 1, test.MyRandom).error() == CommandError::STALE_ROUND);
		Check(test.MyEconomy.PoolConservationHolds());
	}
}

int main()
{
	AtomicRejection(); OverflowAndGates(); RoundTransaction(); UnsupportedRules();
	ArtTransactions(); ArtBountyFiltering(); OwnedItemTransactions();
	std::cout << "preparation item transactions passed\n";
}
