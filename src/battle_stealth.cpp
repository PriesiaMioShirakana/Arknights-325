#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	bool Battle::EnemyStealthed(const CombatUnit& _enemy) const noexcept
	{
		if (!_enemy.MyStatuses.Has(CombatStatus::STEALTH) || _enemy.MyStatuses.Has(CombatStatus::REVEAL) || _enemy.MyBlockedBy) return false;
		if (std::isgreater(_enemy.MyStatuses.MyRemaining[static_cast<std::size_t>(CombatStatus::STEALTH)], 0) &&
			std::isgreaterequal(Time() + 1e-9, _enemy.MyStatuses.MyStealthOffUntil)) return true;
		// 每个来源有自己的关停窗口；新增隐匿源不应被另一来源残留的关停 Buff 压制。
		// 用 string_view 匹配既有键，索敌热路径不拼接字符串或分配内存。
		constexpr std::string_view prefix = "stealthOff:";
		for (const auto& source : _enemy.MyBuffs)
		{
			if (!source.MyDefinition.MyFlags || !(*source.MyDefinition.MyFlags)[static_cast<std::size_t>(CombatStatus::STEALTH)]) continue;
			const bool off = std::ranges::any_of(_enemy.MyBuffs, [&](const CombatBuff& _buff)
			{
				const std::string_view key = _buff.MyDefinition.MyKey;
				return key.starts_with(prefix) && key.substr(prefix.size()) == source.MyDefinition.MyKey;
			});
			if (!off) return true;
		}
		return false;
	}

	void Battle::SwitchStealth(CombatUnit& _enemy)
	{
		if (!_enemy.MyAlive || !_enemy.MyStatuses.Has(CombatStatus::STEALTH)) return;
		if (std::isgreater(_enemy.MyStatuses.MyRemaining[static_cast<std::size_t>(CombatStatus::STEALTH)], 0))
			_enemy.MyStatuses.MyStealthOffUntil = Time() + 3;
		// 添加关停 Buff 会使 Buff 容器重分配，也可能进入扩展回调；只持有稳定 ID 快照。
		auto& scratch = AcquireAttackScratch();
		struct ScratchGuard { std::size_t& MyDepth; ~ScratchGuard() { --MyDepth; } } guard{.MyDepth = _MyAttackDepth};
		for (const auto& buff : _enemy.MyBuffs)
			if (buff.MyDefinition.MyFlags && (*buff.MyDefinition.MyFlags)[static_cast<std::size_t>(CombatStatus::STEALTH)]) scratch.MyBuffIds.emplace_back(buff.MyId);
		for (const auto id : scratch.MyBuffIds)
		{
			const auto found = std::ranges::find(_enemy.MyBuffs, id, &CombatBuff::MyId);
			if (found == _enemy.MyBuffs.end()) continue;
			const auto seconds = found->MyDefinition.MyStealthRestore.value_or(3);
			if (!std::isgreater(seconds, 0)) continue;
			(void)AddBuff(_enemy.MyId, BuffDefinition{.MyKey = "stealthOff:" + found->MyDefinition.MyKey, .MyDuration = seconds});
		}
	}
}
