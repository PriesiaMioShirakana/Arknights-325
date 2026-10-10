#ifndef STRONGHOLD_DOMAIN_COMBAT_MATH_HPP
#define STRONGHOLD_DOMAIN_COMBAT_MATH_HPP
#include <algorithm>
#include <cmath>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Stronghold
{
	enum class DamageType
	{
		PHYSICAL,
		ARTS,
		TRUE_DAMAGE,
		ELEMENTAL
	};

	struct Mitigation
	{
		double MyDefense{};
		double MyResistance{};
		double MyDefenseIgnorePercent{};
		double MyDefenseIgnoreFlat{};
		double MyResistanceIgnorePercent{};
		double MyResistanceIgnoreFlat{};
		double MyElementalResistance{};
	};

	// Pure numeric stages only. Skills, hooks, status modifiers and elemental gauges are separate migration work.
	[[nodiscard]] inline double Mitigate(double _amount, DamageType _type, const Mitigation& _target)
	{
		if (!std::isfinite(_amount) || _amount < 0)
			throw std::invalid_argument("invalid damage amount");
		for (const auto value :
			 {_target.MyDefense,
			  _target.MyResistance,
			  _target.MyDefenseIgnorePercent,
			  _target.MyDefenseIgnoreFlat,
			  _target.MyResistanceIgnorePercent,
			  _target.MyResistanceIgnoreFlat,
			  _target.MyElementalResistance})
			if (!std::isfinite(value))
				throw std::invalid_argument("non-finite mitigation input");
		constexpr double MinimumDamageRatio = 0.05;
		const auto floor = MinimumDamageRatio * _amount;
		if (_type == DamageType::PHYSICAL)
		{
			const auto defense = std::max(
				0.0,
				_target.MyDefense * (1 - std::clamp(_target.MyDefenseIgnorePercent, 0.0, 1.0)) -
					_target.MyDefenseIgnoreFlat);
			return std::max(_amount - defense, floor);
		}
		if (_type == DamageType::ARTS)
		{
			const auto resistance = std::max(
				0.0,
				_target.MyResistance * (1 - std::clamp(_target.MyResistanceIgnorePercent, 0.0, 1.0)) -
					_target.MyResistanceIgnoreFlat);
			return std::max(_amount * (1 - std::min(100.0, resistance) / 100), floor);
		}
		if (_type == DamageType::ELEMENTAL)
			return std::max(_amount * (1 - std::clamp(_target.MyElementalResistance, 0.0, 100.0) / 100), floor);
		return _amount;
	}

	[[nodiscard]] inline bool LeaderHitCancelled(double _amount, bool _bossBattle, bool _leader) noexcept
	{ return _bossBattle && _leader && std::ceil(_amount) >= 300000; }

	struct Shield
	{
		double MyHealth{};
		int MyHits{};
		unsigned MyTypeMask{15}; // PHYSICAL=1, ARTS=2, TRUE_DAMAGE=4, ELEMENTAL=8
	};

	// All hit barriers precede all HP shields; within each pass, oldest first.
	[[nodiscard]] inline double AbsorbShields(double _amount, DamageType _type, std::span<Shield> _shields)
	{
		if (static_cast<unsigned>(_type) > static_cast<unsigned>(DamageType::ELEMENTAL))
			throw std::invalid_argument("invalid shield damage type");
		if (!std::isfinite(_amount) || _amount < 0)
			throw std::invalid_argument("invalid shield damage");
		for (const auto& shield : _shields)
			if (!std::isfinite(shield.MyHealth) || shield.MyHealth < 0 || shield.MyHits < 0)
				throw std::invalid_argument("invalid shield");
		if (_amount == 0)
			return 0;
		const auto mask = 1U << static_cast<unsigned>(_type);
		for (auto& shield : _shields)
			if ((shield.MyTypeMask & mask) && shield.MyHits > 0)
			{
				--shield.MyHits;
				return 0;
			}
		auto remaining = _amount;
		for (auto& shield : _shields)
			if (shield.MyTypeMask & mask)
			{
				const auto take = std::min(remaining, shield.MyHealth);
				shield.MyHealth -= take;
				remaining -= take;
			}
		return remaining;
	}

	// One owner per boss phase, shared by all of that phase's fields. No internal threads or callbacks.
	class SharedBossPool final
	{
	public:
		explicit SharedBossPool(double _health) : _MyHealth(_health), _MyMaxHealth(_health)
		{
			if (!std::isfinite(_health) || _health < 1)
				throw std::invalid_argument("boss pool HP must be finite and >= 1");
		}

		// 已知玩家可在开战前登记，命中热路径使用异构查找，不反复分配临时 string。
		void PreparePlayer(std::string_view _playerId)
		{
			if (!_playerId.empty()) _MyCredits.try_emplace(std::string(_playerId), 0);
		}

		[[nodiscard]] double Damage(std::string_view _playerId, double _amount)
		{
			if (_MySealed || !std::isfinite(_amount) || !std::isgreater(_amount, 0) || !std::isgreater(_MyHealth, 0)) return 0;
			const auto dealt = std::isless(_MyHealth - _amount, 1) ? _MyHealth : _amount;
			_MyHealth -= dealt;
			if (!_playerId.empty())
			{
				const auto found = _MyCredits.find(_playerId);
				if (found != _MyCredits.end()) found->second += dealt;
				else _MyCredits.try_emplace(std::string(_playerId), dealt);
			}
			return dealt;
		}

		[[nodiscard]] double Health() const noexcept { return _MyHealth; }

		[[nodiscard]] double MaxHealth() const noexcept { return _MyMaxHealth; }

		[[nodiscard]] const auto& Credits() const noexcept { return _MyCredits; }

		// 团队 LP 先耗尽时封存仍有 HP 的池，迟到战场不能继续记伤害或改写胜负。
		void Seal() noexcept { _MySealed = true; }

	private:
		double _MyHealth;
		double _MyMaxHealth;
		std::map<std::string, double, std::less<>> _MyCredits;
		bool _MySealed{};
	};
} // namespace Stronghold
#endif
