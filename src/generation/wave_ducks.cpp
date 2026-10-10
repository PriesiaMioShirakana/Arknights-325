#include <stronghold/domain/wave_generation.hpp>

namespace Stronghold
{
	std::size_t WaveGenerator::ReplaceDucks(WavePlan& _plan, Random& _random, const DuckWaveRules& _rules,
		std::string_view _owner, WaveSide _side) const
	{
		if (_rules.MyMaximum < _rules.MyMinimum || _rules.MyMaximum == UINT32_MAX || _rules.MyCoins < 0 ||
			!std::isfinite(_rules.MyStartFraction) || !std::isfinite(_rules.MyEndFraction))
			throw std::invalid_argument("invalid duck wave rules");
		std::vector<std::string_view> ducks; ducks.reserve(_rules.MyEnemies.size());
		for (const auto id : _rules.MyEnemies) if (Enemy(id)) ducks.push_back(id);
		if (ducks.empty()) return 0;
		const auto want = _rules.MyMinimum + _random.Index(_rules.MyMaximum - _rules.MyMinimum + 1);
		if (!want) return 0;
		const auto* wave = Template(_plan.MyTemplateId);
		struct Candidate
		{
			std::size_t MySpawn{};
			unsigned MyCopy{};
			double MyTime{};
		};

		std::vector<Candidate> candidates;
		std::size_t capacity = 0;
		for (const auto& spawn : _plan.MySpawns) capacity += std::max(1U, spawn.MyCount);
		candidates.reserve(capacity);
		for (std::size_t i = 0; i < _plan.MySpawns.size(); ++i)
		{
			const auto& spawn = _plan.MySpawns[i];
			if (_side != WaveSide::ANY)
			{
				if (!wave || spawn.MyRoute >= wave->MyRoutes.size()) continue;
				const auto goal = wave->MyRoutes[spawn.MyRoute].MyEnd.MyX;
				if (_side == WaveSide::LEFT ? !(goal < 10) : !(goal > 10)) continue;
			}
			for (unsigned k = 0; k < std::max(1U, spawn.MyCount); ++k)
				candidates.push_back({i, k, spawn.MyTime + k * spawn.MyInterval});
		}
		std::ranges::stable_sort(candidates, {}, &Candidate::MyTime);
		const auto total = candidates.size();
		std::size_t index = 0;
		std::erase_if(candidates, [&](const Candidate& _candidate)
		{
			const auto fraction = total > 1 ? static_cast<double>(index++) / static_cast<double>(total - 1) : 0;
			const auto& spawn = _plan.MySpawns[_candidate.MySpawn];
			const auto* enemy = Enemy(spawn.MyEnemyId);
			return spawn.MyTag != EnemySpawnTag::NONE || !spawn.MyCounted || !enemy || enemy->MyRank == EnemyRank::BOSS ||
				enemy->MyNotCounted || enemy->MyFlying || fraction < _rules.MyStartFraction - 1e-9 || fraction > _rules.MyEndFraction + 1e-9;
		});
		_random.Shuffle(candidates.begin(), candidates.end());
		if (candidates.size() > want) candidates.resize(want);
		struct Selection
		{
			std::size_t MySpawn{};
			std::vector<unsigned> MyCopies;
		};

		std::vector<Selection> selected; selected.reserve(candidates.size());
		for (const auto& candidate : candidates)
		{
			auto found = std::ranges::find(selected, candidate.MySpawn, &Selection::MySpawn);
			if (found == selected.end()) { selected.push_back({candidate.MySpawn, {}}); found = std::prev(selected.end()); }
			found->MyCopies.push_back(candidate.MyCopy);
		}
		std::vector<WavePlannedSpawn> added; added.reserve(capacity);
		for (const auto& selection : selected)
		{
			const auto& source = _plan.MySpawns[selection.MySpawn];
			for (const auto k : selection.MyCopies)
			{
				auto duck = source;
				duck.MyEnemyId = ducks[_random.Index(static_cast<std::uint32_t>(ducks.size()))];
				duck.MyCount = 1; duck.MyInterval = 0; duck.MyTime = source.MyTime + k * source.MyInterval;
				duck.MyTag = EnemySpawnTag::DUCK; duck.MyCoins = _rules.MyCoins; duck.MyRewardOwner = _owner; duck.MySlot = WaveSlot::NONE;
				if (duck.MyOriginalModifiers) duck.MyOriginalModifiers->MySlot.clear();
				added.push_back(std::move(duck));
			}
			for (unsigned k = 0; k < std::max(1U, source.MyCount); ++k)
				if (!std::ranges::contains(selection.MyCopies, k))
				{
					auto keep = source; keep.MyCount = 1; keep.MyInterval = 0; keep.MyTime = source.MyTime + k * source.MyInterval;
					added.push_back(std::move(keep));
				}
		}
		index = 0;
		std::erase_if(_plan.MySpawns, [&](const WavePlannedSpawn&)
			{ return std::ranges::find(selected, index++, &Selection::MySpawn) != selected.end(); });
		_plan.MySpawns.reserve(_plan.MySpawns.size() + added.size());
		for (auto& spawn : added) _plan.MySpawns.push_back(std::move(spawn));
		return candidates.size();
	}
}
