#ifndef STRONGHOLD_SIMULATION_CONTENT_TAGS_HPP
#define STRONGHOLD_SIMULATION_CONTENT_TAGS_HPP
#include <cstdint>

namespace Stronghold
{
	// 内置规则由引擎直接调用。只有这些 CUSTOM 标签允许进入注册的扩展处理器。
	// 普通业务枚举（伤害类型、状态类型等）不承担扩展标签的职责。
	enum class ContentTag
	{
		BUILTIN,
		CUSTOM_OPERATOR,
		CUSTOM_ENEMY,
		CUSTOM_BUFF,
		CUSTOM_BOND,
		CUSTOM_BOND_EFFECT
	};

	struct ContentReference
	{
		ContentTag MyTag{ContentTag::BUILTIN};
		std::uint32_t MyRegistration{}; // 注册表中的稳定编号；0 专用于内置路径。
	};

	[[nodiscard]] constexpr bool IsCustom(ContentTag _tag) noexcept
	{ return _tag >= ContentTag::CUSTOM_OPERATOR && _tag <= ContentTag::CUSTOM_BOND_EFFECT; }
}
#endif
