#ifndef STRONGHOLD_DOMAIN_SUMMON_CATALOG_HPP
#define STRONGHOLD_DOMAIN_SUMMON_CATALOG_HPP
#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <stronghold/domain/board.hpp>
#include <stronghold/domain/range.hpp>
#include <vector>

namespace Stronghold
{
	struct SummonDefinition
	{
		std::string MyId;
		PlacementClass MyPlacement{};
		bool MyInsideOwnerRange{};
		bool MyOutsideOwnerRange{};
	};

	struct SummonCount
	{
		std::string MyId;
		unsigned MyCount{1};
	};

	struct SummonOwner
	{
		std::string MyId;
		std::vector<RangeOffset> MyRange{};
		std::vector<SummonCount> MyTokens{};
	};

	// 一位玩家开局时解析的只读准备规则。宿主持有目录，会话借用；不得在会话存活时移动或赋值目录。
	// 数据只在配置时复制一次，准备事务只复制一个引用，所有内置召唤物直接查表调用。
	class SummonCatalog final
	{
	public:
		SummonCatalog(std::vector<SummonDefinition> _tokens, std::vector<SummonOwner> _owners)
			: _MyTokens(std::move(_tokens)), _MyOwners(std::move(_owners))
		{
			std::ranges::sort(_MyTokens, {}, &SummonDefinition::MyId);
			std::ranges::sort(_MyOwners, {}, &SummonOwner::MyId);
			std::string_view previous;
			for (const auto& token : _MyTokens)
			{
				if (token.MyId.empty() || token.MyId == previous ||
					(token.MyPlacement != PlacementClass::ANY && token.MyPlacement != PlacementClass::MELEE && token.MyPlacement != PlacementClass::HIGH_ONLY))
					throw std::invalid_argument("invalid summon definition");
				previous = token.MyId;
			}
			previous = {};
			for (const auto& owner : _MyOwners)
			{
				if (owner.MyId.empty() || owner.MyId == previous) throw std::invalid_argument("duplicate or empty summon owner");
				previous = owner.MyId;
				for (std::size_t i = 0; i < owner.MyTokens.size(); ++i)
				{
					const auto& token = owner.MyTokens[i];
					if (!Token(token.MyId) || token.MyCount == 0) throw std::invalid_argument("invalid summon allowance");
					for (std::size_t j = 0; j < i; ++j)
						if (owner.MyTokens[j].MyId == token.MyId) throw std::invalid_argument("duplicate summon allowance");
				}
			}
		}

		[[nodiscard]] const SummonDefinition* Token(std::string_view _id) const noexcept
		{
			const auto found = std::ranges::lower_bound(_MyTokens, _id, {}, &SummonDefinition::MyId);
			return found != _MyTokens.end() && found->MyId == _id ? &*found : nullptr;
		}

		[[nodiscard]] const SummonOwner* Owner(std::string_view _id) const noexcept
		{
			const auto found = std::ranges::lower_bound(_MyOwners, _id, {}, &SummonOwner::MyId);
			return found != _MyOwners.end() && found->MyId == _id ? &*found : nullptr;
		}

		[[nodiscard]] std::span<const SummonOwner> Owners() const noexcept { return _MyOwners; }
		[[nodiscard]] std::span<const SummonDefinition> Tokens() const noexcept { return _MyTokens; }

	private:
		std::vector<SummonDefinition> _MyTokens;
		std::vector<SummonOwner> _MyOwners;
	};
}
#endif
