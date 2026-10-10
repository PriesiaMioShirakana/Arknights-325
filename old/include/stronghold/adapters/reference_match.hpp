#ifndef STRONGHOLD_ADAPTERS_REFERENCE_MATCH_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_MATCH_HPP
#include <stronghold/domain/match_rules.hpp>
#include <set>

namespace Stronghold
{
	// 构建期表涵盖所有模式；返回值不分配、不解析 JSON，也不依赖原 JS 运行时。
	[[nodiscard]] std::span<const MatchRules> ReferenceMatchModes() noexcept;
	[[nodiscard]] const MatchRules& ReferenceMatchMode(std::string_view _id);

	struct MatchBans
	{
		std::vector<std::string_view> MyDrawn{};
		std::vector<std::string_view> MyInactive{};
		std::set<std::string, std::less<>> MyChess{};
	};

	// 在 WaveGenerator::Setup 后继续消费 setup 流；即使禁用数量为零也保留完整洗牌。
	[[nodiscard]] MatchBans DrawMatchBans(const MatchRules& _rules, const Catalog& _catalog, Random& _random);
}
#endif
