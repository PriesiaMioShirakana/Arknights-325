#include <iomanip>
#include <iostream>
#include <sstream>
#include <stronghold/adapters/reference_roster.hpp>

namespace
{
	using namespace Stronghold;
	void Strings(std::span<const std::string_view> _values)
	{
		std::cout << '[';
		for (std::size_t i = 0; i < _values.size(); ++i) { if (i) std::cout << ','; std::cout << std::quoted(std::string(_values[i])); }
		std::cout << ']';
	}

	void Snapshot(const PlayerRoster& _roster, int _error, std::size_t _dropped = 0)
	{
		std::cout << '[' << _error << ',' << _dropped << ",[";
		bool first = true;
		for (const auto& selected : _roster.Loadouts())
		{
			if (!first) std::cout << ',';
			first = false;
			std::cout << '[' << std::quoted(std::string(selected.MyId)) << ',' << selected.MySkill << ','
				<< std::quoted(std::string(selected.MyModule)) << ']';
		}
		std::cout << "],"; Strings(_roster.NotOwned()); std::cout << ",[";
		first = true;
		for (const auto& selected : _roster.Diy())
		{
			if (!first) std::cout << ',';
			first = false;
			std::cout << '[' << std::quoted(std::string(selected.MySlot)) << ',' << std::quoted(std::string(selected.MyCharacter))
				<< ',' << selected.MySkill << ',' << std::quoted(std::string(selected.MyModule)) << ']';
		}
		std::cout << "]]\n";
	}
}

int main()
{
	using namespace Stronghold;
	try
	{
		PlayerRoster roster;
		std::vector<std::string> kits;
		std::vector<std::string_view> kitViews;
		std::vector<TokenAllowance> tokens;
		std::cout << std::setprecision(17);
		for (std::string line; std::getline(std::cin, line);)
		{
			std::istringstream in(line); std::string op; in >> op;
			if (op == "kits")
			{
				std::size_t count; in >> count; kits.resize(count); kitViews.clear(); kitViews.reserve(count);
				for (auto& id : kits) { in >> std::quoted(id); kitViews.emplace_back(id); }
				continue;
			}
			if (op == "reset") { bool bot; in >> bot; roster = PlayerRoster(bot); Snapshot(roster, 0); }
			else if (op == "loadout")
			{
				std::size_t count; in >> count;
				std::vector<std::array<std::string, 2>> storage(count);
				std::vector<LoadoutChoice> choices; choices.reserve(count);
				for (auto& values : storage)
				{
					int skill; in >> std::quoted(values[0]) >> skill >> std::quoted(values[1]);
					choices.emplace_back(LoadoutChoice{.MyId = values[0], .MySkill = skill == -99 ? std::nullopt : std::optional(skill),
						.MyModule = values[1] == "-" ? std::nullopt : std::optional<std::string_view>(values[1])});
				}
				const auto result = roster.SetLoadout(choices); Snapshot(roster, result ? 0 : static_cast<int>(result.error()) + 1);
			}
			else if (op == "own")
			{
				std::size_t count; in >> count; std::vector<std::string> ids(count); std::vector<std::string_view> views; views.reserve(count);
				for (auto& id : ids) { in >> std::quoted(id); views.emplace_back(id); }
				const auto result = roster.SetNotOwned(views); Snapshot(roster, result ? 0 : static_cast<int>(result.error()) + 1, result ? *result : 0);
			}
			else if (op == "diy")
			{
				std::size_t count; in >> count;
				std::vector<std::array<std::string, 3>> storage(count);
				std::vector<DiyPick> picks; picks.reserve(count);
				for (auto& values : storage)
				{
					int skill; in >> std::quoted(values[0]) >> std::quoted(values[1]) >> skill >> std::quoted(values[2]);
					picks.emplace_back(DiyPick{.MySlot = values[0], .MyCharacter = values[1] == "-" ? "" : std::string_view(values[1]),
						.MySkill = skill == -99 ? std::nullopt : std::optional(skill),
						.MyModule = values[2] == "-" ? std::nullopt : std::optional<std::string_view>(values[2])});
				}
				const auto result = roster.SetDiy(picks, kitViews); Snapshot(roster, result ? 0 : static_cast<int>(result.error()) + 1, result ? *result : 0);
			}
			else if (op == "query")
			{
				std::string id; in >> std::quoted(id); const auto selected = roster.Resolve(id); const auto& body = selected.MyLoadout.MyBody;
				roster.PlaceableTokens(id, tokens);
				std::cout << '[' << std::quoted(std::string(body.MyCharacterId)) << ',' << selected.MyLoadout.MySkillIndex << ','
					<< std::quoted(std::string(selected.ModuleId())) << ',' << selected.MyStandIn << ',' << selected.MyDiySelected << ',';
				Strings(selected.MyBonds); std::cout << ",[";
				for (std::size_t i = 0; i < tokens.size(); ++i)
				{
					if (i) std::cout << ',';
					std::cout << '[' << std::quoted(std::string(tokens[i].MyId)) << ',' << tokens[i].MyCount << ',' << tokens[i].MyOwnerRange << ',' << tokens[i].MyOwnerRangeOutside << ']';
				}
				std::cout << "]," << std::quoted(std::string(body.MyPosition)) << ',' << body.MyStats.MyAttack << ',' << body.MyStats.MyMaxHealth
					<< ',' << static_cast<int>(body.MyPreparationPlacement) << ",[";
				for (std::size_t i = 0; i < body.MyPreparationRange.size(); ++i)
				{
					if (i) std::cout << ',';
					std::cout << '[' << body.MyPreparationRange[i].MyRow << ',' << body.MyPreparationRange[i].MyColumn << ']';
				}
				std::cout << "]]\n";
			}
			else throw std::invalid_argument("unknown roster probe operation");
			if (!in) throw std::invalid_argument("malformed roster probe input");
		}
	}
	catch (const std::exception& error) { std::cerr << error.what(); return 1; }
}
