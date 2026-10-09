#ifndef STRONGHOLD_SIMULATION_BOSS_BATTLE_GROUP_HPP
#define STRONGHOLD_SIMULATION_BOSS_BATTLE_GROUP_HPP
#include <stronghold/domain/final_assault.hpp>
#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	// 多个首领场按座位配对顺序，以同一 30 Hz 逻辑时钟推进。调用一次 Step 不读墙上时间。
	// 借用控制器、持有战场；进入隐藏关前销毁本组，再以新的输入构造下一组。
	class BossBattleGroup final
	{
	public:
		BossBattleGroup(FinalAssault& _assault, std::vector<BattleInput> _fields);
		BossBattleGroup(const BossBattleGroup&) = delete;
		BossBattleGroup& operator=(const BossBattleGroup&) = delete;
		void Step();
		void Advance(std::uint64_t _ticks);
		void ForceEnd();
		[[nodiscard]] bool Finished() const noexcept { return _MyFinished; }
		[[nodiscard]] double Time() const noexcept { return _MyClock.Seconds(); }
		[[nodiscard]] std::span<const Battle> Fields() const noexcept { return _MyFields; }
		[[nodiscard]] Battle& Field(std::size_t _index) { return _MyFields.at(_index); }
		[[nodiscard]] std::vector<BattleResult> Results() const;

	private:
		void Finish(BattleEndReason _reason);
		FinalAssault& _MyAssault;
		std::vector<Battle> _MyFields;
		BattleClock _MyClock;
		bool _MyFinished{};
	};
}
#endif
