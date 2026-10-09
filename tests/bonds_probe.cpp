#include <array>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stronghold/adapters/reference_preparation.hpp>
#include <stronghold/adapters/reference_match.hpp>

namespace
{
	using namespace Stronghold;

	void PrintStates(std::span<const BondState> _states)
	{
		std::cout << '[';
		for (std::size_t i = 0; i < _states.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& state = _states[i];
			std::cout << '[' << std::quoted(state.MyId) << ',' << state.MyCount << ',' << state.MyActive << ',' << state.MyTier << ',' << state.MyLayers << ',' << state.MyHarmony << ',' << state.MyOff << ']';
		}
		std::cout << ']';
	}
}

int main()
{
	using namespace Stronghold;
	const auto rules = ReferenceBondRules();
	const auto roster = ReferenceBondRoster();
	const auto items = ReferenceBondItems();
	const auto giver = std::ranges::find_if(items, [](const auto& _item) { return _item.MyItem.MyCanGiveBond; });
	std::vector<BondItemRecord> targets;
	for (const auto& item : items) if (!item.MyItem.MyGrantedBond.empty() && !item.MyItem.MyCanGiveBond) targets.emplace_back(item);
	const auto harmony = std::ranges::find_if(roster, [](const auto& _record) { return std::ranges::find(_record.MyBonds, "maniShip") != _record.MyBonds.end(); });
	if (giver == items.end() || targets.empty() || harmony == roster.end()) throw std::logic_error("missing bond test content");
	BondCalculator calculator(rules);
	PreparationBondCalculator preparation;
	PlayerView player; player.MyHand.resize(10); player.MyTemporary.resize(5);
	std::array<BondMember, 36> board{}; std::array<BondMember, 10> hand{};
	std::array<std::array<BondItem, 2>, 46> equipment{};
	std::vector<BondNumber> layers, gains; std::vector<BondCountBonus> bonuses;
	layers.reserve(rules.size()); gains.reserve(rules.size()); bonuses.reserve(rules.size());
	constexpr double Nan = std::numeric_limits<double>::quiet_NaN(), Inf = std::numeric_limits<double>::infinity();
	constexpr std::array LayerValues{-1.0, 0.0, 1.5, 998.5, 999.0, 1003.0, Nan, Inf};
	constexpr std::array GainValues{-1.0, 0.0, 0.25, 3.75, 1000.0, Inf, Nan};
	std::cout << std::setprecision(17) << "{\"rules\":[";
	for (std::size_t i = 0; i < rules.size(); ++i)
	{
		if (i) std::cout << ',';
		const auto& rule = rules[i];
		std::cout << '[' << std::quoted(rule.MyId) << ',' << static_cast<int>(rule.MyCountMode) << ',' << rule.MyCore << ',' << rule.MyDownward << ",[";
		for (std::size_t j = 0; j < rule.MyThresholds.size(); ++j) { if (j) std::cout << ','; std::cout << rule.MyThresholds[j]; }
		std::cout << "],"; if (rule.MyMaximum) std::cout << *rule.MyMaximum; else std::cout << "null"; std::cout << ']';
	}
	std::cout << "],\"room\":[";
	bool first = true;
	for (const auto before : LayerValues) for (const auto gain : GainValues) { if (!first) std::cout << ','; first = false; std::cout << LayerGainRoom(before, gain); }
	std::cout << "],\"scenes\":["; first = true;
	for (const auto& mode : ReferenceMatchModes()) for (std::size_t scene = 0; scene < 120; ++scene)
	{
		const auto build = [&](std::span<BondMember> _members, std::size_t _offset)
		{
			for (std::size_t i = 0; i < _members.size(); ++i)
			{
				const auto index = i + _offset;
				const auto& record = index == 0 && scene % 3 == 0 ? *harmony : roster[(index * 17 + scene * 11) % roster.size()];
				equipment[index] = {giver->MyItem, targets[(index + scene) % targets.size()].MyItem};
				const auto count = index % 4 == 0 ? 2U : index % 4 == 1 ? 1U : 0U;
				Piece piece{.MyUid = index + 1, .MyId = std::string(record.MyId)};
				if (count >= 1) piece.MyItems.emplace_back(Piece{.MyId = std::string(giver->MyId)});
				if (count >= 2) piece.MyItems.emplace_back(Piece{.MyId = std::string(targets[(index + scene) % targets.size()].MyId)});
				if (index < 36) player.MyBoard[index] = std::move(piece); else player.MyHand[index - 36] = std::move(piece);
				_members[i] = BondMember{.MyBaseId = record.MyBaseId, .MyGolden = record.MyGolden, .MyBonds = record.MyBonds, .MyItems = std::span<const BondItem>(equipment[index]).first(count)};
			}
		};
		build(board, 0); build(hand, 36);
		for (std::size_t i = scene % 37; i < 36; ++i) player.MyBoard[i].reset();
		player.MyTemporary[0] = Piece{.MyId = std::string(harmony->MyId)};
		layers.clear(); gains.clear(); bonuses.clear();
		for (std::size_t i = 0; i < rules.size(); ++i)
		{
			layers.emplace_back(BondNumber{.MyId = rules[i].MyId, .MyValue = LayerValues[(i + scene) % LayerValues.size()]});
			gains.emplace_back(BondNumber{.MyId = rules[i].MyId, .MyValue = GainValues[(i * 3 + scene) % GainValues.size()]});
			bonuses.emplace_back(BondCountBonus{.MyId = rules[i].MyId, .MyValue = static_cast<std::int64_t>((scene + i) % 7) - 3});
		}
		const auto computed = calculator.Compute(std::span<const BondMember>(board).first(scene % 37), hand, layers, bonuses, mode.MyInactiveBonds);
		const auto view = calculator.View();
		const auto withGains = calculator.View(gains);
		const auto projected = preparation.Compute(player, layers, bonuses, mode.MyInactiveBonds);
		std::vector<BondState> reached(computed.MyEnabled.begin(), computed.MyEnabled.end()); AddBondGains(reached, gains);
		if (!first) std::cout << ',';
		first = false;
		std::cout << '['; PrintStates(computed.MyEnabled); std::cout << ','; PrintStates(computed.MyDisabled); std::cout << ','; PrintStates(view); std::cout << ','; PrintStates(reached);
		std::cout << ',' << ActivatedLayers(computed.MyEnabled) << ',' << ActivatedLayers(reached) << ','; PrintStates(withGains); std::cout << ','; PrintStates(projected.MyEnabled); std::cout << ','; PrintStates(projected.MyDisabled); std::cout << ']';
	}
	std::cout << "]}";
}
