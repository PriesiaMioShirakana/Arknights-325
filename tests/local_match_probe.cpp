#include <iomanip>
#include <iostream>
#include <stronghold/runtime/local_match.hpp>

namespace
{
	using namespace Stronghold;

	void Snapshot(const LocalMatch& _match)
	{
		constexpr std::array phases{"LOBBY", "INFO_CHECK", "BAND_DRAFT", "BATTLE_CHECK", "ROUND_START", "SP_DRAFT", "PREP", "COMBAT", "UNITE", "SETTLE", "FINAL_ASSAULT", "HIDDEN_CORE", "RESULT"};
		std::cout << '[' << std::quoted(phases[static_cast<std::size_t>(_match.Phase())]) << ',' << _match.Round() << ",[";
		bool comma = false;
		for (const auto& player : _match.Economy().PublicView())
		{
			if (comma) std::cout << ',';
			comma = true;
			const auto view = *_match.Economy().View(player.MyPlayerId);
			std::cout << '[' << std::quoted(player.MyPlayerId) << ',' << view.MyFunds << ',' << view.MyLevel << ',' << view.MyReady << ',';
			if (_match.Ledger()) std::cout << std::ranges::find(_match.Ledger()->Players(), player.MyPlayerId, &MatchPlayerProgress::MyPlayerId)->MyLife;
			else std::cout << "null";
			std::cout << ',';
			std::string strategy;
			if (_match.Strategies()) for (const auto& pick : _match.Strategies()->View().MyPlayers) if (pick.MyPlayerId == player.MyPlayerId) strategy = pick.MyStrategyId;
			std::cout << std::quoted(strategy) << ",[";
			bool entry = false;
			for (const auto& slot : view.MyShop)
			{
				if (entry) std::cout << ',';
				entry = true;
				if (slot) std::cout << '[' << std::quoted(slot->MyId) << ',' << slot->MyPrice << ',' << slot->MySold << ',' << slot->MyFrozen << ']';
				else std::cout << "null";
			}
			std::cout << "],["; entry = false;
			for (const auto& piece : view.MyHand)
			{
				if (entry) std::cout << ',';
				entry = true;
				if (piece) std::cout << '[' << piece->MyUid << ',' << std::quoted(piece->MyId) << ',' << piece->MyPoolCopies << ']';
				else std::cout << "null";
			}
			std::cout << "],["; entry = false;
			if (_match.Ledger())
				for (const auto& bounty : std::ranges::find(_match.Ledger()->Players(), player.MyPlayerId, &MatchPlayerProgress::MyPlayerId)->MyBounties)
				{
					if (entry) std::cout << ',';
					entry = true;
					std::cout << '[' << std::quoted(bounty.MyCard.MyId) << ',' << std::quoted(bounty.MyCard.MyEnemyId) << ',' << bounty.MyCard.MyCount
						<< ',' << bounty.MyCard.MyCoins << ',' << bounty.MyCard.MyPerfect << ',' << bounty.MyRoundsLeft << ']';
				}
			std::cout << "]]";
		}
		std::cout << "],[";
		if (_match.Phase() == MatchPhase::BAND_DRAFT)
		{
			const auto draft = _match.Strategies()->View();
			for (std::size_t i = 0; i < draft.MyOrder.size(); ++i) { if (i) std::cout << ','; std::cout << std::quoted(draft.MyOrder[i]); }
		}
		std::cout << "]," << std::quoted(_match.Phase() == MatchPhase::BAND_DRAFT ? _match.Strategies()->CurrentPlayer() : std::string_view{}) << ",[";
		if (_match.Choices())
			for (std::size_t i = 0; i < _match.Choices()->Cards().size(); ++i) { if (i) std::cout << ','; std::cout << std::quoted(_match.Choices()->Cards()[i].MyId); }
		std::cout << "],[";
		if (_match.Choices())
			for (std::size_t i = 0; i < _match.Choices()->Taken().size(); ++i)
			{
				if (i) std::cout << ',';
				if (const auto owner = _match.Choices()->Taken()[i]) std::cout << std::quoted(_match.Choices()->Players()[*owner].MyPlayerId);
				else std::cout << "null";
			}
		std::cout << "]," << std::quoted(_match.Choices() ? _match.Choices()->CurrentPlayer() : std::string_view{}) << "]\n";
	}
}

int main(int _argc, char** _argv)
{
	try
	{
		if (_argc != 3) return 2;
		const auto& rules = ReferenceMatchMode(_argv[1]);
		LocalMatchOptions options{.MyMode = _argv[1], .MySeed = static_cast<std::uint32_t>(std::stoul(_argv[2]))};
		for (int i = 0; i < (rules.MySolo ? 1 : 3); ++i) options.MyPlayers.push_back({.MySeat = {i, "p" + std::to_string(i)}});
		LocalMatch match(std::move(options)); match.Start(); Snapshot(match);
		for (const auto& player : match.Economy().PublicView()) if (!match.InfoReady(player.MyPlayerId)) return 3;
		match.Advance(0); Snapshot(match);
		while (!match.Strategies()->Complete())
		{
			const std::string player(match.Strategies()->CurrentPlayer());
			if (!match.PickStrategy(player, match.Strategies()->TimeoutChoice(player))) return 4;
			match.Advance(match.Now()); Snapshot(match);
			if (match.Phase() != MatchPhase::BAND_DRAFT) break;
		}
		match.Advance(*match.Deadline()); Snapshot(match);
		match.Advance(*match.Deadline()); Snapshot(match);
		while (match.Phase() == MatchPhase::SP_DRAFT)
		{
			const auto taken = match.Choices()->Taken();
			const auto free = std::ranges::find_if(taken, [](const auto& _value) { return !_value; });
			if (!match.PickCard(match.Choices()->CurrentPlayer(), static_cast<std::size_t>(free - taken.begin()))) return 5;
			match.Advance(match.Now()); Snapshot(match);
		}
		if (match.Phase() != MatchPhase::PREP) return 6;
		if (!match.Execute({"p0", 1, Freeze{}})) return 7;
		Snapshot(match);
	}
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
