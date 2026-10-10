#include <iomanip>
#include <iostream>
#include <sstream>
#include <stronghold/adapters/reference_catalog.hpp>
#include <stronghold/adapters/reference_roster.hpp>

namespace
{
	using namespace Stronghold;
	void PieceValue(const std::optional<Piece>& _piece)
	{
		if (!_piece) { std::cout << "null"; return; }
		std::cout << '[' << _piece->MyUid << ',' << std::quoted(_piece->MyId) << ',' << _piece->MyPoolCopies << ']';
	}

	void Snapshot(const EconomySession& _session, bool _ok, PieceUid _uid)
	{
		if (!_session.PoolConservationHolds()) throw std::logic_error("DIY pool conservation failed");
		std::cout << '[' << _ok << ',' << _uid;
		for (const auto id : {"p", "q"})
		{
			const auto view = *_session.View(id);
			std::cout << ",[" << view.MyFunds << ',' << view.MyLevel << ',' << view.MyAlive << ",[";
			for (std::size_t i = 0; i < view.MyPrivateStock.size(); ++i)
			{
				if (i) std::cout << ',';
				const auto& entry = view.MyPrivateStock[i];
				std::cout << '[' << std::quoted(entry.MyId) << ',' << entry.MyStock.MyCapacity << ',' << entry.MyStock.MyRemaining << ']';
			}
			std::cout << ']';
			for (const auto pieces : {std::span<const std::optional<Piece>>(view.MyHand), std::span<const std::optional<Piece>>(view.MyTemporary), std::span<const std::optional<Piece>>(view.MyBoard)})
			{
				std::cout << ",[";
				for (std::size_t i = 0; i < pieces.size(); ++i) { if (i) std::cout << ','; PieceValue(pieces[i]); }
				std::cout << ']';
			}
			std::cout << ",[";
			for (std::size_t i = 0; i < view.MyShop.size(); ++i)
			{
				if (i) std::cout << ',';
				if (view.MyShop[i]) std::cout << '[' << std::quoted(view.MyShop[i]->MyId) << ',' << view.MyShop[i]->MySold << ']';
				else std::cout << "null";
			}
			std::cout << "],[";
			for (std::size_t i = 0; i < view.MyOffers.size(); ++i)
			{
				if (i) std::cout << ',';
				std::cout << '[';
				for (std::size_t j = 0; j < view.MyOffers[i].size(); ++j) { if (j) std::cout << ','; std::cout << std::quoted(view.MyOffers[i][j].MyId); }
				std::cout << ']';
			}
			std::cout << "]]";
		}
		std::cout << "]\n";
	}
}

int main(int _argc, char** _argv)
{
	using namespace Stronghold;
	if (_argc != 2) return 2;
	const auto catalog = ReferenceCatalog();
	const std::array seats{Seat{.MySeat = 0, .MyPlayerId = "p"}, Seat{.MySeat = 1, .MyPlayerId = "q"}};
	EconomySession session(catalog, "mode_multi_normal", seats, static_cast<std::uint32_t>(std::stoul(_argv[1])));
	try
	{
		for (std::string line; std::getline(std::cin, line);)
		{
			std::istringstream in(line); std::string op, player; in >> op >> player;
			bool ok = true; PieceUid uid = 0;
			if (op == "config")
			{
				std::size_t count; in >> count; std::vector<std::array<std::string, 3>> storage(count);
				std::vector<DiyPick> picks; std::vector<std::string_view> kits; picks.reserve(count); kits.reserve(count);
				for (auto& s : storage)
				{
					int skill; in >> s[0] >> s[1] >> skill >> s[2];
					picks.emplace_back(DiyPick{.MySlot = s[0], .MyCharacter = s[1], .MySkill = skill, .MyModule = s[2]}); kits.emplace_back(s[1]);
				}
				std::size_t n; in >> n; std::vector<std::string> off(n); std::vector<std::string_view> offViews; offViews.reserve(n);
				for (auto& id : off) { in >> id; offViews.emplace_back(id); }
				PlayerRoster roster; if (!roster.SetDiy(picks, kits)) throw std::runtime_error("bad test DIY selection");
				ok = ConfigurePreparationRoster(session, player, roster, offViews).has_value();
			}
			else if (op == "begin") session.BeginRound(session.Round() + 1);
			else if (op == "end") session.EndPreparation();
			else if (op == "settle")
			{
				bool eliminate; in >> eliminate;
				const std::array results{EconomySettlement{.MyPlayerId = "p", .MyFunds = 70, .MyEliminated = false},
					EconomySettlement{.MyPlayerId = "q", .MyFunds = 0, .MyEliminated = eliminate}};
				session.ApplySettlement(session.Round(), results);
			}
			else if (op == "grant")
			{
				std::string id; in >> id; const auto result = session.GrantPiece(player, id);
				// Oracle acquireChess returns null for an unfilled DIY slot and for overflow alike.
				if (result) uid = result->MyPiece.value_or(0);
			}
			else if (op == "promote" || op == "remove" || op == "transform")
			{
				PieceUid target; in >> target; InventoryEffect effect;
				if (op == "promote") effect = PromotePiece{.MyUid = target};
				else if (op == "remove") effect = RemoveOwnedPiece{.MyUid = target};
				else { std::string id; in >> id; effect = TransformChess{.MyUid = target, .MyDefinitionId = std::move(id)}; }
				const auto result = session.ApplyInventoryEffect(player, effect); ok = result.has_value(); if (result) uid = result->MyPiece.value_or(0);
			}
			else
			{
				PreparationCommand command;
				if (op == "level") command = LevelUp{};
				else if (op == "refresh") command = Refresh{};
				else
				{
					std::size_t target; in >> target;
					if (op == "buy") command = Buy{.MySlot = target};
					else if (op == "reward") command = PickReward{.MySlot = target};
					else if (op == "sell") command = Sell{.MyUid = target};
					else if (op == "board") { int row, column; in >> row >> column; command = MoveToBoard{.MyUid = target, .MyPosition = {.MyRow = row, .MyColumn = column}}; }
					else throw std::invalid_argument("unknown test command");
				}
				ok = session.Execute(CommandEnvelope{.MyPlayerId = player, .MyRound = session.Round(), .MyCommand = std::move(command)}).has_value();
			}
			Snapshot(session, ok, uid);
		}
	}
	catch (const std::exception& error) { std::cerr << error.what(); return 1; }
}
