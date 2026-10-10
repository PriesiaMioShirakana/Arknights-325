#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_wave_generation.hpp>
#include <stronghold/runtime/local_match.hpp>

int main()
{
	using namespace Stronghold;
	const auto catalog = ReferenceCatalog();
	std::cout << '['; bool first = true;
	for (const auto& mode : ReferenceMatchModes())
		for (const auto seed : {0U, 1U, 42U, 65535U, 0xffffffffU})
		{
			if (!first) std::cout << ',';
			first = false;
			Random random(DeriveSeed(seed, u"setup"));
			WaveGenerator generator(ReferenceWaveGeneration(), ReferenceWaveMode(mode.MyId), ReferenceWaves());
			const auto setup = generator.Setup(random);
			const auto bans = DrawMatchBans(mode, catalog, random);
			std::cout << '[' << std::quoted(mode.MyId) << ',' << seed << ',' << random.State() << ',' << std::quoted(setup.MyStageId)
				<< ',' << std::quoted(setup.MyBossId) << ',' << std::quoted(setup.MyHiddenBossId) << ",[";
			bool comma = false;
			for (const auto id : bans.MyDrawn) { if (comma) std::cout << ','; comma = true; std::cout << std::quoted(id); }
			std::cout << "],["; comma = false;
			for (const auto id : bans.MyInactive) { if (comma) std::cout << ','; comma = true; std::cout << std::quoted(id); }
			std::cout << "],["; comma = false;
			for (const auto& id : bans.MyChess) { if (comma) std::cout << ','; comma = true; std::cout << std::quoted(id); }
			std::cout << "]]";
		}
	std::cout << "]\n";
}
