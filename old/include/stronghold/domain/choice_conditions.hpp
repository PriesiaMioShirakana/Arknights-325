#ifndef STRONGHOLD_DOMAIN_CHOICE_CONDITIONS_HPP
#define STRONGHOLD_DOMAIN_CHOICE_CONDITIONS_HPP
#include <cstddef>

namespace Stronghold
{
	enum class ChoiceGateKind { ALWAYS, BENCH_AT_LEAST, BENCH_AT_MOST, SAME_ROW };
	struct ChoiceGate { ChoiceGateKind MyKind{}; std::size_t MyCount{}; };
}
#endif
