#include "battle_core.hpp"
#include <stronghold/simulation/component_registry.hpp>
#include <tuple>

namespace Stronghold
{
	void EffectExecutor::Execute(Battle& _battle, const EffectContext& _context, const EffectProgram& _program)
	{
		auto& core = *_battle._MyView;
		if (core._MyEffectDepth >= 32) throw std::runtime_error("effect recursion limit");
		const auto depth = core._MyEffectDepth++;
		struct Guard
		{
			std::size_t& MyDepth;
			~Guard() { --MyDepth; }
		} guard{core._MyEffectDepth};
		if (core._MyEffectScratch.size() <= depth) core._MyEffectScratch.emplace_back();
		auto& targets = core._MyEffectScratch[depth];
		for (const auto& step : _program.MySteps)
		{
			Select(_battle, _context, step.MySelector, targets);
			for (const auto& target : targets)
				for (const auto& operation : step.MyOperations)
				{
					if (_battle.Finished()) return;
					Apply(_battle, _context, target, operation);
				}
		}
	}
}
