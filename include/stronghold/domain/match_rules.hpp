#ifndef STRONGHOLD_DOMAIN_MATCH_RULES_HPP
#define STRONGHOLD_DOMAIN_MATCH_RULES_HPP
#include <stronghold/domain/final_assault.hpp>
#include <stronghold/domain/strategy_draft.hpp>
#include <stronghold/domain/special_draft.hpp>

namespace Stronghold
{
	struct StrategyRecord
	{
		std::string_view MyId{};
		std::int64_t MyStartingLife{30};
		std::string_view MyEffectId{};
		std::span<const std::string_view> MyBonds{};
	};

	struct MatchRoundRules
	{
		unsigned MyRound{};
		std::optional<double> MyPreparationSeconds{}; // 空值表示没有备战倒计时。
		double MyCombatGameSeconds{120};
		std::optional<double> MyBossCountdownSeconds{}; // UI 的实秒倒计时，不是首领战时长上限。
	};

	struct BossHealthRecord { std::string_view MyId{}; double MyBaseHealth{500000}; };

	// 不可变数据视图；可使用构建期表或由调用方提供的配置，所有 span/string_view 生命周期须覆盖对局。
	struct MatchRules
	{
		std::string_view MyId{};
		bool MySolo{};
		std::string_view MyDifficulty{};
		unsigned MyLastRound{14};
		unsigned MyBossRound{14};
		unsigned MyHiddenRound{};
		std::span<const unsigned> MySpecialRounds{};
		std::span<const MatchRoundRules> MyRounds{};
		std::span<const StrategyRecord> MyStrategies{};
		std::span<const BossHealthRecord> MyBosses{};
		std::span<const std::string_view> MyActiveBonds{};
		std::span<const std::string_view> MyInactiveBonds{};
		BossHealthScale MyBossScale{};
		BossHealthScale MyGlobalBossScale{};
		FinalAssaultRules MyFinalAssault{};
		SpecialDraftRules MySpecialDraft{};
		double MyInformationSeconds{25};
		double MyBattleCheckSeconds{3};
		double MyStrategyTurnSeconds{30};
		unsigned MyStrategySkips{1};
		std::string_view MyDefaultStrategy{"band_bldsk"};
		unsigned MyLifeCapPerRound{10};

		[[nodiscard]] MatchRoundRules Round(unsigned _round) const
		{
			const auto found = std::ranges::lower_bound(MyRounds, _round, {}, &MatchRoundRules::MyRound);
			return found != MyRounds.end() && found->MyRound == _round ? *found : MatchRoundRules{.MyRound = _round,
				.MyPreparationSeconds = MySolo ? std::nullopt : std::optional(90.0), .MyCombatGameSeconds = 60 * MyFinalAssault.MyGameSecondsPerRealSecond};
		}

		[[nodiscard]] StrategyDraftRules StrategyRules(bool _untimed = false) const
		{ return StrategyDraftRules{.MySolo = MySolo, .MyUntimed = _untimed, .MySkips = MyStrategySkips, .MyTurnSeconds = MyStrategyTurnSeconds, .MyDefaultId = std::string(MyDefaultStrategy)}; }

		[[nodiscard]] std::vector<StrategyOption> StrategyOptions() const
		{
			std::vector<StrategyOption> options; options.reserve(MyStrategies.size());
			for (const auto& strategy : MyStrategies) options.emplace_back(StrategyOption{.MyId = std::string(strategy.MyId), .MyStartingLife = strategy.MyStartingLife});
			return options;
		}

		[[nodiscard]] double BossHealth(std::string_view _boss, std::optional<std::size_t> _alive = {}, bool _capacityExperiment = false) const
		{
			const auto found = std::ranges::lower_bound(MyBosses, _boss, {}, &BossHealthRecord::MyId);
			const double base = found != MyBosses.end() && found->MyId == _boss ? found->MyBaseHealth : 500000;
			const auto count = _alive ? std::optional(static_cast<double>(*_alive)) : std::nullopt;
			return BossPoolHealth(base, BossPoolShare(MyBossScale, MyGlobalBossScale, MySolo, count), 1, MySolo, _alive.value_or(0), _capacityExperiment);
		}
	};
}
#endif
