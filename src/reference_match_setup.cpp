#include <stronghold/adapters/reference_bonds.hpp>
#include <stronghold/adapters/reference_match.hpp>

namespace Stronghold
{
	MatchBans DrawMatchBans(const MatchRules& _rules, const Catalog& _catalog, Random& _random)
	{
		MatchBans bans;
		const auto bonds = ReferenceBondRules();
		std::array<std::vector<std::string_view>, 2> eligible;
		for (auto& group : eligible) group.reserve(bonds.size());
		bans.MyInactive.reserve(bonds.size());
		for (const auto& bond : bonds)
		{
			if (std::ranges::contains(_rules.MyInactiveBonds, bond.MyId)) bans.MyInactive.push_back(bond.MyId);
			else if (bond.MyCanDisable) eligible[bond.MyCore ? 0 : 1].push_back(bond.MyId);
		}
		const std::array counts{_rules.MyDisabledCoreBonds, _rules.MyDisabledAddonBonds};
		for (std::size_t i = 0; i < eligible.size(); ++i)
		{
			auto& group = eligible[i];
			_random.Shuffle(group.begin(), group.end());
			const auto count = std::min<std::size_t>(counts[i], group.size());
			bans.MyDrawn.insert(bans.MyDrawn.end(), group.begin(), group.begin() + static_cast<std::ptrdiff_t>(count));
		}
		std::ranges::sort(bans.MyDrawn);
		bans.MyInactive.insert(bans.MyInactive.end(), bans.MyDrawn.begin(), bans.MyDrawn.end());
		std::ranges::sort(bans.MyInactive);
		const auto roster = ReferenceBondRoster();
		for (const auto& id : _catalog.VisibleChess())
		{
			const auto found = std::ranges::lower_bound(roster, id, {}, &BondRosterRecord::MyId);
			if (found != roster.end() && found->MyId == id && !found->MyBonds.empty() &&
				std::ranges::all_of(found->MyBonds, [&](std::string_view _bond) { return std::ranges::contains(bans.MyInactive, _bond); }))
				bans.MyChess.insert(id);
		}
		return bans;
	}
}
