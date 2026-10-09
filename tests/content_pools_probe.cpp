#include <iomanip>
#include <iostream>
#include <sstream>
#include <stronghold/adapters/reference_catalog.hpp>
#include <stronghold/adapters/reference_choices.hpp>
#include <stronghold/adapters/reference_roster.hpp>

int main(int _argc, char** _argv)
{
	using namespace Stronghold;
	if (_argc != 2) return 2;
	const auto catalog = ReferenceCatalog();
	const std::array seats{Seat{.MySeat = 0, .MyPlayerId = "p"}, Seat{.MySeat = 1, .MyPlayerId = "q"}};
	EconomySession economy(catalog, "mode_multi_normal", seats, 1);
	PlayerRoster roster;
	const std::array picks{DiyPick{.MySlot = "chess_char_5_diy1_a", .MyCharacter = "char_2013_cerber", .MySkill = 0, .MyModule = "none"}};
	const std::array<std::string_view, 1> kits{"char_2013_cerber"};
	if (!roster.SetDiy(picks, kits) || !ConfigurePreparationRoster(economy, "p", roster)) return 3;
	const auto members = roster.MakeContentPoolRoster();
	std::vector<ContentPoolRecord> pools(ReferenceContentPools().begin(), ReferenceContentPools().end());
	const std::array zeroItems{ContentPoolWeight{.MyId = "chess_item_1_01_e_a", .MyWeight = 0}, ContentPoolWeight{.MyId = "chess_item_1_02_e_a", .MyWeight = -2}};
	const std::array zeroChess{ContentPoolWeight{.MyId = "chess_char_1_01_a", .MyWeight = 0}, ContentPoolWeight{.MyId = "chess_char_1_02_a", .MyWeight = -2}};
	const std::array invalid{ContentPoolWeight{.MyId = "missing", .MyWeight = 1}};
	const std::array diyItems{std::string_view("chess_char_5_diy1_a")};
	const std::array duplicateTiers{2, 1, 2, 9};
	pools.emplace_back(ContentPoolRecord{.MyId = "zero_item", .MyKind = PieceKind::ITEM, .MyWeighted = zeroItems});
	pools.emplace_back(ContentPoolRecord{.MyId = "zero_chess", .MyKind = PieceKind::CHESS, .MyWeighted = zeroChess});
	pools.emplace_back(ContentPoolRecord{.MyId = "invalid_item", .MyKind = PieceKind::ITEM, .MyWeighted = invalid});
	pools.emplace_back(ContentPoolRecord{.MyId = "invalid_chess", .MyKind = PieceKind::CHESS, .MyWeighted = invalid});
	pools.emplace_back(ContentPoolRecord{.MyId = "diy_list", .MyKind = PieceKind::CHESS, .MyItems = diyItems});
	pools.emplace_back(ContentPoolRecord{.MyId = "diy_bond", .MyKind = PieceKind::CHESS, .MyBond = "emptyShip"});
	pools.emplace_back(ContentPoolRecord{.MyId = "duplicate_tiers", .MyKind = PieceKind::ITEM, .MyTiers = duplicateTiers});
	Random random(static_cast<std::uint32_t>(std::stoul(_argv[1])));
	for (std::string line; std::getline(std::cin, line);)
	{
		std::istringstream input(line); std::string operation; input >> operation;
		std::optional<ContentPoolResult> result;
		if (operation == "pool")
		{
			std::string player, id; int level = 0; input >> player >> id >> level;
			result = RollContentPool(catalog, economy, player, pools, members, random, id, level);
		}
		else if (operation == "item")
		{
			std::string pool; int tier = 0, max = 0, level = 0; input >> pool >> tier >> max >> level;
			auto id = RollContentItem(catalog, pools, random, ContentItemDraw{.MyPool = pool,
				.MyTier = tier == -1 ? std::nullopt : std::optional(tier), .MyMaxTier = max, .MyShopLevel = level});
			if (id) result = ContentPoolResult{.MyKind = PieceKind::ITEM, .MyId = std::move(*id)};
		}
		else if (operation == "grant")
		{
			std::string player, id; input >> player >> id;
			if (!economy.GrantPiece(player, id)) return 4;
		}
		else if (operation == "clear")
		{
			std::string player; input >> player;
			const auto view = *economy.View(player);
			for (const auto& slots : {view.MyHand, view.MyTemporary}) for (const auto& piece : slots)
				if (piece && !economy.ApplyInventoryEffect(player, RemoveOwnedPiece{.MyUid = piece->MyUid})) return 5;
		}
		else return 6;
		if (!economy.PoolConservationHolds()) return 7;
		std::cout << '[' << random.State() << ',';
		if (result) std::cout << '[' << (result->MyKind == PieceKind::ITEM) << ',' << std::quoted(result->MyId) << ',' << result->MyGolden << ']';
		else std::cout << "null";
		std::cout << "]\n";
	}
}
