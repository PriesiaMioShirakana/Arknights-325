#ifndef STRONGHOLD_ADAPTERS_REFERENCE_MATCH_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_MATCH_HPP
#include <stronghold/domain/match_rules.hpp>

namespace Stronghold
{
	// 构建期表涵盖所有模式；返回值不分配、不解析 JSON，也不依赖原 JS 运行时。
	[[nodiscard]] std::span<const MatchRules> ReferenceMatchModes() noexcept;
	[[nodiscard]] const MatchRules& ReferenceMatchMode(std::string_view _id);
}
#endif
