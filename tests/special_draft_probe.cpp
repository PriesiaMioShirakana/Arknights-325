#include <iomanip>
#include <iostream>
#include <stronghold/domain/special_draft.hpp>

namespace
{
	using namespace Stronghold;
	void Snapshot(const SpecialDraft& _draft, const Random& _random)
	{
		std::cout << '[' << _random.State() << ',' << _draft.Turn() << ',' << _draft.Deadline() << ',' << _draft.Complete() << ",[";
		for (std::size_t i = 0; i < _draft.Order().size(); ++i)
		{
			if (i) std::cout << ',';
			std::cout << std::quoted(_draft.Players()[_draft.Order()[i]].MyPlayerId);
		}
		std::cout << "],[";
		for (std::size_t i = 0; i < _draft.Players().size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& p = _draft.Players()[i]; std::cout << '[' << std::quoted(p.MyPlayerId) << ',' << p.MyAlive << ',';
			if (p.MyCard) std::cout << *p.MyCard; else std::cout << "null";
			std::cout << ']';
		}
		std::cout << "],[";
		for (std::size_t i = 0; i < _draft.Taken().size(); ++i)
		{
			if (i) std::cout << ',';
			if (_draft.Taken()[i]) std::cout << std::quoted(_draft.Players()[*_draft.Taken()[i]].MyPlayerId); else std::cout << "null";
		}
		std::cout << "]]";
	}
}

int main()
{
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 36; ++scene)
	{
		if (scene) std::cout << ',';
		const unsigned count = scene % 3 == 0 ? 1U : scene % 3 == 1 ? 4U : 8U;
		std::vector<Seat> seats; seats.reserve(count);
		for (unsigned i = 0; i < count; ++i) seats.emplace_back(Seat{.MySeat = static_cast<int>(9 - i), .MyPlayerId = "p" + std::to_string(i)});
		std::vector<ChoiceCard> cards; cards.reserve(scene % 6 + 1);
		for (unsigned i = 0; i < scene % 6 + 1; ++i) cards.emplace_back(ChoiceCard{.MyKind = ChoiceCardKind::ITEM, .MyId = "item_" + std::to_string(i % 2)});
		Random random(scene + 31);
		SpecialDraft draft(SpecialDraftRules{.MySolo = scene % 4 == 0, .MyUntimed = scene % 5 == 0}, seats, std::move(cards), random, 10);
		std::cout << '['; Snapshot(draft, random); std::cout << ",[";
		for (unsigned step = 0; step < 16; ++step)
		{
			if (step) std::cout << ',';
			bool ok = true; const std::string current(draft.CurrentPlayer());
			switch (step)
			{
			case 0: ok = draft.Pick("p0", 0).has_value(); break;
			case 1: ok = draft.Pick(current, 999).has_value(); break;
			case 2: ok = draft.Pick(current, 0).has_value(); break;
			case 3: (void)draft.Advance(42, random); break;
			case 4: ok = draft.Eliminate("p" + std::to_string(count - 1)); break;
			case 5: ok = draft.Pick(current, 1).has_value(); break;
			case 6: (void)draft.Advance(100, random); break;
			case 7: ok = draft.Pick(current, 0).has_value(); break;
			case 8: (void)draft.Advance(116, random); break;
			case 9: (void)draft.Advance(132, random); break;
			case 10: (void)draft.Advance(148, random); break;
			case 11: ok = draft.Eliminate("p0"); break;
			case 12: (void)draft.Advance(1000, random); break;
			case 13: ok = draft.Pick(current, 2).has_value(); break;
			case 14: draft.Finish(); break;
			case 15: ok = draft.Pick("p0", 0).has_value(); break;
			}
			std::cout << '[' << ok << ','; Snapshot(draft, random); std::cout << ']';
		}
		std::cout << "]]";
	}
	std::cout << ']';
}
