#include <iomanip>
#include <iostream>
#include <stronghold/domain/strategy_draft.hpp>

namespace
{
	using namespace Stronghold;
	void Snapshot(const StrategyDraft& _draft)
	{
		const auto v = _draft.View();
		std::cout << '[' << v.MyTurn << ',' << v.MyDeadline << ',' << v.MyComplete << ",[";
		for (std::size_t i = 0; i < v.MyOrder.size(); ++i) { if (i) std::cout << ','; std::cout << std::quoted(v.MyOrder[i]); }
		std::cout << "],[";
		for (std::size_t i = 0; i < v.MyPlayers.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& p = v.MyPlayers[i];
			std::cout << '[' << std::quoted(p.MyPlayerId) << ',' << std::quoted(p.MyStrategyId) << ',' << std::quoted(p.MyFocus) << ','
				<< p.MyStartingLife << ',' << p.MySkipsLeft << ']';
		}
		std::cout << "]]";
	}
}

int main()
{
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 30; ++scene)
	{
		const unsigned count = scene % 3 == 0 ? 1U : scene % 3 == 1 ? 4U : 8U;
		std::vector<Seat> seats; seats.reserve(count);
		for (unsigned i = 0; i < count; ++i) seats.emplace_back(Seat{.MySeat = static_cast<int>(9 - i), .MyPlayerId = "p" + std::to_string(i)});
		Random random(scene + 1);
		StrategyDraft draft(StrategyDraftRules{.MySolo = scene % 4 == 0, .MyUntimed = scene % 5 == 0, .MySkips = scene % 2 + 1}, seats,
			{StrategyOption{.MyId = "band_bldsk", .MyStartingLife = 40}, StrategyOption{.MyId = "a", .MyStartingLife = 20},
				StrategyOption{.MyId = "b", .MyStartingLife = 30}, StrategyOption{.MyId = "c", .MyStartingLife = 50}}, random, 10);
		if (scene) std::cout << ',';
		std::cout << '[' << random.State() << ','; Snapshot(draft); std::cout << ",[";
		for (unsigned step = 0; step < 16; ++step)
		{
			if (step) std::cout << ',';
			bool ok = true;
			const std::string current(draft.CurrentPlayer());
			switch (step)
			{
			case 0: for (const auto& seat : seats) (void)draft.Focus(seat.MyPlayerId, "b"); break;
			case 1: ok = draft.Skip(current).has_value(); break;
			case 2: ok = draft.Pick("p0", "a").has_value(); break;
			case 3: ok = draft.Pick(current, "band_bldsk").has_value(); break;
			case 4: ok = draft.Pick(current, "b").has_value(); break;
			case 5: ok = draft.AssignDefault("p" + std::to_string(count - 1)); break;
			case 6: draft.Advance(100); break;
			case 7: ok = draft.Focus(current, "invalid").has_value(); break;
			case 8: ok = draft.Focus(current, "").has_value(); break;
			case 9: ok = draft.Skip(current).has_value(); break;
			case 10: draft.Advance(130); break;
			case 11: ok = draft.Pick(current, "band_bldsk").has_value(); break;
			case 12: draft.Advance(160); break;
			case 13: draft.Advance(1000); break;
			case 14: draft.Finish(); break;
			case 15: ok = draft.Pick("p0", "a").has_value(); break;
			}
			std::cout << '[' << ok << ','; Snapshot(draft); std::cout << ']';
		}
		std::cout << "]]";
	}
	std::cout << ']';
}
