#ifndef STRONGHOLD_DOMAIN_STRATEGY_DRAFT_HPP
#define STRONGHOLD_DOMAIN_STRATEGY_DRAFT_HPP
#include <expected>
#include <stronghold/domain/pool.hpp>

namespace Stronghold
{
	struct StrategyOption { std::string MyId{}; std::int64_t MyStartingLife{30}; };
	struct StrategyDraftRules
	{
		bool MySolo{};
		bool MyUntimed{};
		unsigned MySkips{1};
		double MyTurnSeconds{30};
		std::string MyDefaultId{"band_bldsk"};
	};
	struct StrategyPick
	{
		std::string MyPlayerId{};
		std::string MyStrategyId{};
		std::string MyFocus{};
		std::int64_t MyStartingLife{};
		unsigned MySkipsLeft{};
	};
	struct StrategyDraftView
	{
		std::vector<StrategyPick> MyPlayers{}; // 座位顺序，已选内容不会因跳过而重新排列。
		std::vector<std::string> MyOrder{};
		std::size_t MyTurn{};
		double MyDeadline{};
		bool MyComplete{};
	};
	enum class DraftError { COMPLETE, UNKNOWN_PLAYER, ALREADY_PICKED, NOT_YOUR_TURN, BAD_STRATEGY, TAKEN, NO_SKIP, NOBODY_TO_PASS, SOLO_SKIP };

	// 构造时只合作模式消耗随机流。选项按模式的 sortId 顺序传入，并由此对象独立持有。
	// 时间是逻辑秒；宿主先 Advance 再处理该时刻的新命令，无 OS 计时器或网络副作用。
	class StrategyDraft final
	{
	public:
		StrategyDraft(StrategyDraftRules _rules, std::span<const Seat> _players, std::vector<StrategyOption> _options, Random& _random, double _now = 0);
		[[nodiscard]] std::expected<void, DraftError> Focus(std::string_view _player, std::string_view _strategy);
		[[nodiscard]] std::expected<void, DraftError> Pick(std::string_view _player, std::string_view _strategy);
		[[nodiscard]] std::expected<void, DraftError> Skip(std::string_view _player);
		// 每次推进最多完成一次超时选择；下一位的 30 秒从宿主本次推进时刻开始，避免补发陈旧超时。
		void Advance(double _now);
		// 离开者在原位获得自动选择；如改变当前回合，剩余玩家从当前逻辑时刻重新计时。
		[[nodiscard]] bool AssignDefault(std::string_view _player);
		void Finish(); // 按座位顺序为所有未选者补默认值。
		[[nodiscard]] StrategyDraftView View() const;
		[[nodiscard]] std::string_view CurrentPlayer() const noexcept;
		[[nodiscard]] bool Complete() const noexcept { return _MyTurn == _MyOrder.size(); }
		[[nodiscard]] bool Taken(std::string_view _strategy, std::string_view _player) const;
		[[nodiscard]] std::string_view TimeoutChoice(std::string_view _player) const;

	private:
		[[nodiscard]] const StrategyOption* Option(std::string_view _id) const;
		[[nodiscard]] std::string_view Default(std::string_view _player) const;
		[[nodiscard]] std::optional<std::size_t> Player(std::string_view _id) const;
		void Apply(std::size_t _player, std::string_view _strategy);
		void StartTurn();
		StrategyDraftRules _MyRules;
		std::vector<StrategyOption> _MyOptions;
		std::vector<StrategyPick> _MyPlayers;
		std::vector<std::size_t> _MyOrder;
		std::size_t _MyTurn{};
		double _MyNow{};
		double _MyDeadline{};
	};
}
#endif
