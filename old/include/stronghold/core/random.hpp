#ifndef STRONGHOLD_CORE_RANDOM_HPP
#define STRONGHOLD_CORE_RANDOM_HPP
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace Stronghold
{
	class Random final
	{
	public:
		explicit constexpr Random(std::uint32_t _seed = 1) noexcept : _MyState(_seed ? _seed : 0x9e3779b9U) {}

		// Exact mulberry32 stream; unsigned 32-bit arithmetic deliberately wraps.
		[[nodiscard]] constexpr std::uint32_t NextU32() noexcept
		{
			_MyState += 0x6d2b79f5U;
			auto t = (_MyState ^ (_MyState >> 15U)) * (_MyState | 1U);
			t ^= t + ((t ^ (t >> 7U)) * (t | 61U));
			return t ^ (t >> 14U);
		}

		[[nodiscard]] constexpr double Next() noexcept { return static_cast<double>(NextU32()) / 4294967296.0; }

		[[nodiscard]] constexpr std::uint32_t Index(std::uint32_t _count)
		{
			if (_count == 0)
				throw std::invalid_argument("random index requires a nonempty range");
			return static_cast<std::uint32_t>(Next() * static_cast<double>(_count));
		}

		[[nodiscard]] constexpr std::uint32_t State() const noexcept { return _MyState; }

		template <std::random_access_iterator _Iterator>
		constexpr void Shuffle(_Iterator _begin, _Iterator _end)
		{
			const auto count = _end - _begin;
			if (count < 0 || static_cast<std::uint64_t>(count) > UINT32_MAX)
				throw std::invalid_argument("shuffle range out of bounds");
			for (auto n = count; n > 1; --n)
			{
				const auto j = Index(static_cast<std::uint32_t>(n));
				std::iter_swap(_begin + n - 1, _begin + j);
			}
		}

	private:
		std::uint32_t _MyState;
	};

	// JS charCodeAt hashes UTF-16 code units, including both halves of surrogate pairs.
	[[nodiscard]] constexpr std::uint32_t DeriveSeed(std::uint32_t _seed, std::u16string_view _salt) noexcept
	{
		auto h = _seed ^ 0x85ebca6bU;
		for (const auto c : _salt)
		{
			h = (h ^ static_cast<std::uint32_t>(c)) * 0x9e3779b1U;
			h ^= h >> 13U;
		}
		return h;
	}
} // namespace Stronghold
#endif
