#include <limits>
#include <stdexcept>
#include <stronghold/core/scheduler.hpp>

namespace Stronghold
{
	namespace
	{
		Scheduler::Time AddTime(Scheduler::Time _a, Scheduler::Time _b)
		{
			if (_b.count() > std::numeric_limits<Scheduler::Time::rep>::max() - _a.count())
				throw std::overflow_error("scheduler deadline overflow");
			return _a + _b;
		}
	} // namespace

	bool Scheduler::Later::operator()(const Entry& _a, const Entry& _b) const noexcept
	{
		return _a.MyAt != _b.MyAt ? _a.MyAt > _b.MyAt : _a.MySequence > _b.MySequence;
	}

	Scheduler::Handle Scheduler::Add(Time _delay, Time _interval, Callback _callback)
	{
		if (!_callback || _delay.count() < 0)
			throw std::invalid_argument("invalid scheduled task");
		const auto at = AddTime(_MyNow, _delay);
		if (_MyNextId == UINT64_MAX || _MySequence == UINT64_MAX)
			throw std::overflow_error("scheduler identity exhausted");
		const auto id = ++_MyNextId;
		_MyTasks.emplace(id, Task{.MyCallback = std::move(_callback), .MyInterval = _interval});
		_MyQueue.push({.MyAt = at, .MySequence = ++_MySequence, .MyId = id});
		return id;
	}

	Scheduler::Handle Scheduler::After(Time _delay, Callback _callback) { return Add(_delay, Time{}, std::move(_callback)); }

	Scheduler::Handle Scheduler::Every(Time _interval, Callback _callback)
	{
		if (_interval.count() <= 0)
			throw std::invalid_argument("interval must be positive");
		return Add(_interval, _interval, std::move(_callback));
	}

	bool Scheduler::Cancel(Handle _handle) { return _MyTasks.erase(_handle) != 0; }

	void Scheduler::Clear() noexcept
	{
		_MyTasks.clear();
		_MyQueue = {};
	}

	Scheduler::Progress Scheduler::AdvanceTo(Time _target, std::size_t _callbackBudget)
	{
		if (_target < _MyNow || _MyAdvancing)
			throw std::invalid_argument("non-monotonic or reentrant scheduler advance");

		struct Reset // NOLINT(cppcoreguidelines-special-member-functions)
		{
			bool& MyFlag; // Must reset the scheduler's flag, including early returns and exceptions.

			~Reset() { MyFlag = false; }
		} reset{_MyAdvancing};

		_MyAdvancing = true;
		std::size_t count = 0;
		while (!_MyQueue.empty())
		{
			const auto entry = _MyQueue.top();
			const auto it = _MyTasks.find(entry.MyId);
			if (it == _MyTasks.end())
			{
				_MyQueue.pop();
				continue;
			}
			if (entry.MyAt > _target)
				break;
			if (count == _callbackBudget)
				return {.MyExecuted = count, .MyCaughtUp = false};
			const auto interval = it->second.MyInterval;
			const auto nextAt = interval.count() > 0 ? AddTime(entry.MyAt, interval) : Time{};
			if (_MySequence == UINT64_MAX)
				throw std::overflow_error("scheduler sequence exhausted");
			_MyQueue.pop();
			_MyNow = entry.MyAt;
			if (interval.count() > 0)
				_MyQueue.push({.MyAt = nextAt, .MySequence = ++_MySequence, .MyId = entry.MyId});
			auto callback = std::move(it->second.MyCallback);
			if (interval.count() == 0)
				_MyTasks.erase(it);
			++count;
			const auto restore = [&]
			{
				// The invocation owns the callable while Cancel/Clear may erase its task.
				if (const auto pending = _MyTasks.find(entry.MyId); pending != _MyTasks.end())
					pending->second.MyCallback = std::move(callback);
			};
			try { callback(); }
			catch (...)
			{
				restore();
				throw;
			}
			restore(); // Preserve mutable captures; exceptions do not replay this occurrence.
		}
		_MyNow = _target;
		return {.MyExecuted = count, .MyCaughtUp = true};
	}
} // namespace Stronghold
