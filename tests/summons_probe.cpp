#include <iomanip>
#include <iostream>
#include <sstream>
#include <stronghold/adapters/reference_catalog.hpp>
#include <stronghold/adapters/reference_roster.hpp>
#include <stronghold/domain/preparation.hpp>

namespace
{
	using namespace Stronghold;

	void PrintPiece(const Piece& _piece)
	{
		std::cout << '[' << _piece.MyUid << ',' << std::quoted(_piece.MyId) << ',' << _piece.MyPoolCopies << ',' << static_cast<int>(_piece.MyFacing) << ",[";
		for (std::size_t i = 0; i < _piece.MyItems.size(); ++i) { if (i) std::cout << ','; PrintPiece(_piece.MyItems[i]); }
		std::cout << "],";
		if (_piece.MyTemporaryDue) std::cout << *_piece.MyTemporaryDue; else std::cout << "null";
		std::cout << ',' << _piece.MyDeferredMerge << ',' << _piece.MyOwnerUid << ',' << _piece.MyCount << ']';
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

	void Snapshot(const EconomySession& _session, bool _ok, PieceUid _grant)
	{
		if (!_session.PoolConservationHolds()) throw std::logic_error("inventory violated shared pool conservation");
		const auto view = *_session.View("p");
		std::cout << '[' << _ok << ',' << _grant << ',' << view.MyFunds << ',' << view.MyReady << ',' << view.MyPreparationsEnded << ',';
		PrintSlots(view.MyHand); std::cout << ','; PrintSlots(view.MyTemporary); std::cout << ','; PrintSlots(view.MyBoard);
		std::cout << ",[";
		for (std::size_t i = 0; i < view.MyOffers.size(); ++i)
		{
			if (i) std::cout << ',';
			std::cout << '[';
			for (std::size_t j = 0; j < view.MyOffers[i].size(); ++j) { if (j) std::cout << ','; std::cout << std::quoted(view.MyOffers[i][j].MyId); }
			std::cout << ']';
		}
		std::cout << "]," << static_cast<unsigned>(_session.Phase()) << ",[";
		for (std::size_t i = 0; i < view.MyLayout.MyTiles.size(); ++i) { if (i) std::cout << ','; std::cout << static_cast<unsigned>(view.MyLayout.MyTiles[i]); }
		std::cout << "],[";
		for (std::size_t i = 0; i < view.MyBoardOrderSize; ++i) { if (i) std::cout << ','; std::cout << view.MyBoard[view.MyBoardOrder[i]]->MyUid; }
		std::cout << "]]\n";
	}
}

int main(int _argc, char** _argv)
{
	using namespace Stronghold;
	if (_argc != 7) return 2;
	const auto catalog = ReferenceCatalog();
	const std::array seats{Seat{.MySeat = 0, .MyPlayerId = "p"}};
	EconomySession session(catalog, "mode_multi_normal", seats, static_cast<std::uint32_t>(std::stoul(_argv[1])));

	PlayerRoster roster;
	const std::string_view owner = _argv[2], module = _argv[4], character = _argv[6];
	const auto skill = std::stoi(_argv[3]);
	if (character != "-")
	{
		const std::array picks{DiyPick{.MySlot = owner, .MyCharacter = character, .MySkill = skill, .MyModule = module}};
		const std::array kits{character};
		if (!roster.SetDiy(picks, kits) || roster.Diy().empty()) throw std::invalid_argument("invalid test DIY");
	}
	else
	{
		if (skill >= 0 || module != "-")
		{
			const std::array choices{LoadoutChoice{.MyId = owner, .MySkill = skill >= 0 ? std::optional(skill) : std::nullopt,
				.MyModule = module != "-" ? std::optional(module) : std::nullopt}};
			if (!roster.SetLoadout(choices)) throw std::invalid_argument("invalid test loadout");
		}
		if (std::string_view(_argv[5]) == "1") { const std::array ids{owner}; if (!roster.SetNotOwned(ids)) return 3; }
	}
	if (!ConfigurePreparationRoster(session, "p", roster)) return 4;
	const auto summons = roster.MakeSummonCatalog();
	if (!session.ConfigureSummons("p", summons)) return 5;

	// The original PlayerState oracle controls actions and live UIDs in this driver. lets the JS oracle choose actions from its live inventory.
	for (std::string line; std::getline(std::cin, line);)
	{
		std::istringstream input(line); std::string operation; input >> operation;
		bool ok = true; PieceUid grant = 0;
		if (operation == "begin") session.BeginRound(session.Round() + 1);
		else if (operation == "round_start") session.BeginRound(session.Round() + 1, false);
		else if (operation == "prep") session.BeginPreparation();
		else if (operation == "deadline") session.ApplyPreparationDeadline();
		else if (operation == "layout")
		{
			unsigned kind = 0; input >> kind;
			auto layout = BoardLayout::Fallback();
			if (kind) layout.MyTiles.fill(kind == 1 ? Terrain::HIGH : kind == 2 ? Terrain::BLOCKED : Terrain::GROUND);
			if (kind == 4) for (std::size_t i = 0; i < layout.MyTiles.size(); ++i) layout.MyTiles[i] = i % 2 ? Terrain::HIGH : Terrain::GROUND;
			ok = session.SetBoardLayout("p", layout).has_value();
		}
		else if (operation == "end") session.EndPreparation();
		else if (operation == "grant")
		{
			std::string id; bool pool = true, temporary = false, deferred = false; input >> id >> pool >> temporary >> deferred;
			const auto result = session.GrantPiece("p", id, GrantOptions{.MyFromPool = pool, .MyToTemporary = temporary, .MyDeferItemMerge = deferred});
			ok = result.has_value(); if (result) grant = result->MyPiece.value_or(0);
		}
		else if (operation == "promote" || operation == "upgrade" || operation == "remove" || operation == "transform" || operation == "attach")
		{
			PieceUid uid = 0; input >> uid;
			InventoryEffect effect;
			if (operation == "promote") effect = PromotePiece{.MyUid = uid};
			else if (operation == "upgrade") effect = UpgradeOwnedItem{.MyUid = uid};
			else if (operation == "remove") effect = RemoveOwnedPiece{.MyUid = uid};
			else if (operation == "attach") { PieceUid target = 0; input >> target; effect = AttachItemDirect{.MyItem = uid, .MyTarget = target}; }
			else { std::string id; input >> id; effect = TransformChess{.MyUid = uid, .MyDefinitionId = std::move(id)}; }
			const auto result = session.ApplyInventoryEffect("p", effect);
			ok = result.has_value(); if (result) grant = result->MyPiece.value_or(0);
		}
		else
		{
			PieceUid uid = 0; input >> uid;
			PreparationCommand command;
			if (operation == "equip")
			{
				PieceUid target = 0, replace = 0; input >> target >> replace;
				command = EquipItem{.MyItem = uid, .MyTarget = target, .MyReplace = replace ? std::optional<PieceUid>(replace) : std::nullopt};
			}
			else if (operation == "sell") command = Sell{.MyUid = uid};
			else if (operation == "destroy") command = DestroyItem{.MyUid = uid};
			else if (operation == "ready") command = SetReady{.MyReady = uid != 0};
			else if (operation == "buy") command = Buy{.MySlot = static_cast<std::size_t>(uid)};
			else if (operation == "reward") command = PickReward{.MySlot = static_cast<std::size_t>(uid)};
			else if (operation == "hand") { std::size_t slot = 0; input >> slot; command = MoveToHand{.MyUid = uid, .MySlot = slot}; }
			else if (operation == "board")
			{
				int row = 0, column = 0, direction = 0; input >> row >> column >> direction;
				command = MoveToBoard{.MyUid = uid, .MyPosition = {.MyRow = row, .MyColumn = column}, .MyFacing = static_cast<Facing>(direction)};
			}
			else throw std::invalid_argument("unknown test operation");
			ok = session.Execute(CommandEnvelope{.MyPlayerId = "p", .MyRound = session.Round(), .MyCommand = std::move(command)}).has_value();
		}
		Snapshot(session, ok, grant);
	}
}
