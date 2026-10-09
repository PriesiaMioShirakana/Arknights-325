#include <iomanip>
#include <iostream>
#include <sstream>
#include <stronghold/adapters/reference_bonds.hpp>
#include <stronghold/adapters/reference_catalog.hpp>
#include <stronghold/adapters/reference_choices.hpp>
#include <stronghold/adapters/reference_roster.hpp>
#include <stronghold/adapters/reference_match.hpp>
#include <stronghold/adapters/reference_wave_generation.hpp>
#include <stronghold/domain/preparation_content.hpp>

namespace
{
	using namespace Stronghold;

	void PrintPiece(const Piece& _piece)
	{
		std::cout << '[' << _piece.MyUid << ',' << std::quoted(_piece.MyId) << ',' << _piece.MyPoolCopies << ",[";
		for (std::size_t i = 0; i < _piece.MyItems.size(); ++i)
		{
			if (i) std::cout << ',';
			PrintPiece(_piece.MyItems[i]);
		}
		std::cout << "],";
		if (_piece.MyTemporaryDue) std::cout << *_piece.MyTemporaryDue; else std::cout << "null";
		std::cout << ']';
	}

	void PrintSlots(std::span<const std::optional<Piece>> _pieces)
	{
		std::cout << '[';
		for (std::size_t i = 0; i < _pieces.size(); ++i)
		{
			if (i) std::cout << ',';
			if (_pieces[i]) PrintPiece(*_pieces[i]); else std::cout << "null";
		}
		std::cout << ']';
	}

	void Snapshot(const EconomySession& _economy, const RoundLedger& _ledger, const PreparationContent& _content, const Random& _random,
		bool _ok, PieceUid _grant, bool _bands)
	{
		if (!_economy.PoolConservationHolds()) throw std::logic_error("item pool conservation");
		std::cout << '[' << _ok << ',' << _grant << ',' << _random.State() << ',' << static_cast<int>(_economy.Phase());
		for (const auto id : {"p", "q", "r"})
		{
			const auto view = *_economy.View(id);
			const auto content = *_content.View(id);
			std::cout << ",[" << view.MyAlive << ',' << view.MyReady << ',' << view.MyFunds << ',' << view.MyPendingFunds
				<< ',' << view.MyDeployCap << ',' << view.MyLevel << ',';
			PrintSlots(view.MyHand); std::cout << ','; PrintSlots(view.MyTemporary); std::cout << ','; PrintSlots(view.MyBoard);
			std::cout << ",[";
			for (std::size_t i = 0; i < view.MyShop.size(); ++i)
			{
				if (i) std::cout << ',';
				if (const auto& slot = view.MyShop[i])
					std::cout << '[' << std::quoted(slot->MyId) << ',' << (_bands ? *_content.PriceOf(id, *slot) : slot->MyPrice) << ',' << slot->MySold << ']';
				else std::cout << "null";
			}
			std::cout << "],[";
			for (std::size_t i = 0; i < view.MyOffers.size(); ++i)
			{
				if (i) std::cout << ',';
				std::cout << '[';
				for (std::size_t j = 0; j < view.MyOffers[i].size(); ++j)
				{
					if (j) std::cout << ',';
					std::cout << std::quoted(view.MyOffers[i][j].MyId);
				}
				std::cout << ']';
			}
			std::cout << "],{"; bool first = true;
			for (const auto& layer : content.MyLayers)
				if (layer.MyLayers > 0)
				{
					if (!first) std::cout << ',';
					first = false; std::cout << std::quoted(layer.MyId) << ':' << layer.MyLayers;
				}
			std::cout << "},["; first = true;
			const auto itemEffects = _content.ItemEffects(id);
			for (const auto& effect : *itemEffects)
			{
				if (!first) std::cout << ',';
				first = false;
				std::cout << '[' << (effect.MyKind == PreparationItemKind::BEACON) << ',' << effect.MySource << ','
					<< effect.MyCount << ',' << std::quoted(effect.MyRecipient) << ',' << std::quoted(effect.MyChess) << ",[";
				for (std::size_t i = 0; i < effect.MyBonds.size(); ++i)
				{
					if (i) std::cout << ',';
					std::cout << std::quoted(effect.MyBonds[i]);
				}
				std::cout << "]]";
			}
			const auto& stats = view.MyStatistics; const auto& round = view.MyRoundStatistics;
			std::cout << "],[" << stats.MySpent << ',' << stats.MyFundsGained << ',' << stats.MyRefreshes << ','
				<< stats.MyBuys << ',' << stats.MySells << ',' << stats.MyChessMerges << ',' << stats.MyItemMerges << ','
				<< stats.MyItemsEquipped << "],[" << round.MyRefreshes << ',' << round.MyBuys << ',' << round.MySells << ','
				<< round.MySpent << ',' << round.MyGainedChess << ',' << round.MyArts << "],[";
			first = true;
			if (view.MyAlive)
				for (const auto& bounty : std::ranges::find(_ledger.Players(), id, &MatchPlayerProgress::MyPlayerId)->MyBounties)
				{
					if (!first) std::cout << ',';
					first = false;
					std::cout << '[' << std::quoted(bounty.MyCard.MyId) << ',' << std::quoted(bounty.MyCard.MyEnemyId) << ','
						<< bounty.MyCard.MyCount << ',' << bounty.MyCard.MyCoins << ',' << bounty.MyCard.MyPerfect << ','
						<< bounty.MyRoundsLeft << ']';
				}
			std::cout << ']';
			if (_bands)
			{
				std::cout << ',' << view.MyUpgradePrice << ',' << view.MyFreeRefreshes << ",[";
				for (std::size_t i = 0; i < view.MyShop.size(); ++i)
				{
					if (i) std::cout << ',';
					std::cout << (view.MyShop[i] && view.MyShop[i]->MyFrozen);
				}
				std::cout << ']';
			}
			std::cout << ']';
		}
		std::cout << "]\n";
	}
}

int main(int _argc, char** _argv)
{
	using namespace Stronghold;
	if (_argc < 2 || _argc > 4) return 2;
	const auto seed = static_cast<std::uint32_t>(std::stoul(_argv[1]));
	const std::string_view mode = _argc > 2 ? _argv[2] : "mode_multi_normal";
	const std::string_view variant = _argc > 3 ? _argv[3] : "normal";
	const bool bonds = variant.starts_with("bonds");
	const bool garrisons = variant.starts_with("garrisons") || variant == "bonds_all";
	const bool bands = variant.starts_with("band_");
	const auto catalog = ReferenceCatalog();
	const std::array seats{Seat{.MySeat = 0, .MyPlayerId = "p"}, Seat{.MySeat = 1, .MyPlayerId = "q"},
		Seat{.MySeat = 2, .MyPlayerId = "r"}};
	EconomySession economy(catalog, mode, seats, seed);
	PlayerRoster roster;
	const std::array picks{DiyPick{.MySlot = "chess_char_5_diy1_a", .MyCharacter = "char_2013_cerber",
		.MySkill = 0, .MyModule = "none"}};
	const std::array<std::string_view, 1> kits{"char_2013_cerber"};
	if (!roster.SetDiy(picks, kits) || !ConfigurePreparationRoster(economy, "p", roster)) return 3;
	const auto members = roster.MakeContentPoolRoster();
	const auto defaults = PlayerRoster{}.MakeContentPoolRoster();
	const auto inactive = ReferenceMatchMode(mode).MyInactiveBonds;
	const std::array players{ChoiceRewardPlayerConfig{.MyPlayerId = "p", .MyRoster = members, .MyInactiveBonds = inactive, .MyStrategy = bands ? variant : variant == "garrisons_lisa" ? "band_lisa" : ""},
		ChoiceRewardPlayerConfig{.MyPlayerId = "q", .MyRoster = defaults, .MyInactiveBonds = inactive},
		ChoiceRewardPlayerConfig{.MyPlayerId = "r", .MyRoster = defaults, .MyInactiveBonds = inactive}};
	RoundLedger ledger({}, {MatchPlayerProgress{.MyPlayerId = "p"}, MatchPlayerProgress{.MyPlayerId = "q"},
		MatchPlayerProgress{.MyPlayerId = "r"}});
	std::vector<ChoiceCardRecord> artCards;
	if (variant != "empty")
		for (const auto& card : ReferenceChoices().MyCards)
			if (card.MyKind == ChoiceCardKind::BOUNTY && (variant != "fallback" ||
				(!card.MyPerfect && !card.MyId.starts_with("enemyeffect_b_")))) artCards.emplace_back(card);
	PreparationContent content(economy, ledger, ReferenceChoiceRewards(), ReferenceContentPools(), ReferenceBondRules(),
		players, ReferencePreparationItems(), {artCards, ReferenceWaveMode(mode).MyInactiveEnemies}, ReferencePreparationBands(), garrisons ? ReferencePreparationGarrisons() : PreparationGarrisonRules{}, bonds ? ReferencePreparationBonds() : std::span<const PreparationBondEffect>{});
	Random random(seed);
	for (std::string line; std::getline(std::cin, line);)
	{
		std::istringstream input(line); std::string operation, player; input >> operation >> player;
		bool ok = true; PieceUid grant = 0;
		if (operation == "begin" || operation == "round_start")
		{
			content.BeginRound(economy.Round() + 1, random);
			if (operation == "begin") { (void)content.OnRoundStart(random); content.BeginPreparation(random); }
		}
		else if (operation == "hooks") (void)content.OnRoundStart(random);
		else if (operation == "battle_result") ok = content.OnBattleResult(player, economy.Round(), random).has_value();
		else if (operation == "prep") content.BeginPreparation(random);
		else if (operation == "end") { if (bands || garrisons || bonds) content.OnPreparationEnd(random); economy.EndPreparation(); }
		else if (operation == "settle")
		{
			unsigned mask = 0; input >> mask;
			std::vector<EconomySettlement> results;
			for (std::size_t i = 0; i < seats.size(); ++i)
				results.emplace_back(EconomySettlement{.MyPlayerId = seats[i].MyPlayerId,
					.MyEliminated = !economy.View(seats[i].MyPlayerId)->MyAlive || (mask & (1U << i)) != 0});
			economy.ApplySettlement(economy.Round(), results);
		}
		else if (operation == "grant")
		{
			std::string id; bool pool = true, temporary = false; input >> id >> pool >> temporary;
			const auto result = content.GrantPiece(player, id, random, {.MyFromPool = pool, .MyToTemporary = temporary});
			ok = result.has_value(); if (result) grant = result->MyPiece.value_or(0);
		}
		else if (operation == "remove" || operation == "promote")
		{
			PieceUid uid = 0; input >> uid;
			const InventoryEffect effect = operation == "remove" ? InventoryEffect(RemoveOwnedPiece{uid}) : PromotePiece{uid};
			ok = content.ApplyInventoryEffect(player, effect, random).has_value();
		}
		else if (operation == "cap")
		{
			std::size_t cap = 0; input >> cap;
			ok = economy.ApplyEconomyEffect(player, RaiseDeployCap{cap}).has_value();
		}
		else if (operation == "free")
		{
			std::uint64_t amount = 0; input >> amount;
			ok = economy.ApplyEconomyEffect(player, GrantFreeRefreshes{amount}).has_value();
		}
		else if (operation == "funds")
		{
			std::int64_t amount = 0; input >> amount;
			ok = economy.ApplyEconomyEffect(player, AdjustFunds{amount}).has_value();
		}
		else if (operation == "sync")
		{
			std::vector<std::string> names;
			std::vector<double> counts;
			for (std::string bond; input >> bond;) { double n = 0; input >> n; names.emplace_back(std::move(bond)); counts.push_back(n); }
			std::vector<BondNumber> gains; gains.reserve(names.size());
			for (std::size_t i = 0; i < names.size(); ++i) gains.push_back({names[i], counts[i]});
			ok = content.SynchronizeLayers(player, economy.Round(), gains, random).has_value();
		}
		else if (operation == "hand")
		{
			PieceUid uid = 0; input >> uid;
			ok = content.Execute({player, economy.Round(), MoveToHand{uid}}, random).has_value();
		}
		else if (operation == "layers")
		{
			std::string bond; double count = 0; input >> bond >> count;
			(void)content.AddLayers(player, bond, count, random);
		}
		else
		{
			PieceUid uid = 0; input >> uid;
			PreparationCommand command;
			if (operation == "equip")
			{
				PieceUid target = 0, replace = 0; input >> target >> replace;
				command = EquipItem{uid, target, replace ? std::optional(replace) : std::nullopt};
			}
			else if (operation == "board")
			{
				int row = 0, column = 0; input >> row >> column;
				command = MoveToBoard{.MyUid = uid, .MyPosition = {row, column}};
			}
			else if (operation == "art")
			{
				int row = 0, column = 0, facing = 1; input >> row >> column >> facing;
				command = UseArt{uid, {row, column}, static_cast<Facing>(facing)};
			}
			else if (operation == "sell") command = Sell{uid};
			else if (operation == "destroy") command = DestroyItem{uid};
			else if (operation == "ready") command = SetReady{uid != 0};
			else if (operation == "reward") command = PickReward{static_cast<std::size_t>(uid)};
			else if (operation == "buy") command = Buy{static_cast<std::size_t>(uid)};
			else if (operation == "refresh") command = Refresh{};
			else if (operation == "level") command = LevelUp{};
			else return 4;
			ok = content.Execute({player, economy.Round(), command}, random).has_value();
		}
		Snapshot(economy, ledger, content, random, ok, grant, bands || garrisons || bonds);
	}
}
