#ifndef STRONGHOLD_CORE_SCHEDULER_HPP
#define STRONGHOLD_CORE_SCHEDULER_HPP
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <queue>
#include <vector>

namespace Stronghold
{
	// Single-owner logical timer: external monotonic time, FIFO ties, no OS or threads.
	class Scheduler final
	{
	public:
		using Time = std::chrono::milliseconds;
		using Handle = std::uint64_t;

		using Callback = std::function<void()>;

		struct Progress
		{
			std::size_t MyExecuted{};
			bool MyCaughtUp{};
		};

		[[nodiscard]] Handle After(Time _delay, Callback _callback);

		[[nodiscard]] Handle Every(Time _interval, Callback _callback);

		bool Cancel(Handle _handle);

		[[nodiscard]] Progress AdvanceTo(Time _target, std::size_t _callbackBudget = 10000);

		[[nodiscard]] Time Now() const noexcept { return _MyNow; }

		[[nodiscard]] std::size_t Pending() const noexcept { return _MyTasks.size(); }

		void Clear() noexcept;

	private:
		struct Task
		{
			Callback MyCallback;
			Time MyInterval;
		};

		struct Entry
		{
			Time MyAt;
			std::uint64_t MySequence;
			Handle MyId;
		};

		struct Later
		{
			bool operator()(const Entry& _a, const Entry& _b) const noexcept;
		};

		[[nodiscard]] Handle Add(Time _delay, Time _interval, Callback _callback);
		std::map<Handle, Task> _MyTasks;
		std::priority_queue<Entry, std::vector<Entry>, Later> _MyQueue;
		Time _MyNow{};
		Handle _MyNextId{};
		std::uint64_t _MySequence{};
		bool _MyAdvancing{};
	};

	class BattleClock final
	{
	public:
		static constexpr double StepSeconds = 1.0 / 30.0;

		void Step() noexcept { ++_MyTick; }

		[[nodiscard]] std::uint64_t Tick() const noexcept { return _MyTick; }

		[[nodiscard]] double Seconds() const noexcept { return static_cast<double>(_MyTick) * StepSeconds; }

	private:
		std::uint64_t _MyTick{};
	};
} // namespace Stronghold
#endif
