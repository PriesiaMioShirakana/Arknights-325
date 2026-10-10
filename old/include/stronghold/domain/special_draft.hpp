#ifndef STRONGHOLD_DOMAIN_SPECIAL_DRAFT_HPP
#define STRONGHOLD_DOMAIN_SPECIAL_DRAFT_HPP
#include <expected>
#include <stronghold/domain/pool.hpp>
#include <stronghold/domain/wave_generation.hpp>

namespace Stronghold
{
	enum class ChoiceFamily { BOUNTY, SUPPLY, SHOP, TACTIC };
	enum class ChoiceCardKind { BOUNTY, ITEM, TACTIC };
	struct ChoiceCard
	{
		ChoiceCardKind MyKind{};
		std::string MyId{};
		std::string MyName{};
		std::string MyDescription{};
		std::string MyRichDescription{};
		std::optional<int> MyTier{};
		WaveBounty MyBounty{};
		unsigned MyBattles{1};
		bool MyTeam{};
		std::string MyTacticKind{};
	};

	struct SpecialDraftRules
	{
		bool MySolo{};
		bool MyUntimed{};
		double MyFirstTurnSeconds{30};
		double MyTurnSeconds{16};
	};

	struct ChoicePlayer
	{
		std::string MyPlayerId{};
		bool MyAlive{true};
		std::optional<std::size_t> MyCard{};
	};

	struct ChoiceAward { std::size_t MyPlayer{}; std::size_t MyCard{}; };
	enum class ChoiceDraftError { COMPLETE, UNKNOWN_PLAYER, ELIMINATED, ALREADY_PICKED, NOT_YOUR_TURN, BAD_CARD, TAKEN };

	// 只负责机变的顺序、选择和时间。卡片生成及奖励执行位于各自模块；同 ID 卡片按槽位独立占用。
	// 成功选择返回一次奖励凭据，由宿主执行内容效果；重复选择不再产生凭据。
	class SpecialDraft final
	{
	public:
		SpecialDraft(SpecialDraftRules _rules, std::span<const Seat> _players, std::vector<ChoiceCard> _cards, Random& _random, double _now = 0);
		[[nodiscard]] std::expected<ChoiceAward, ChoiceDraftError> Pick(std::string_view _player, std::size_t _card);
		[[nodiscard]] std::optional<ChoiceAward> Advance(double _now, Random& _random);
		// 淘汰者不再参与；已经获得的奖励仍然占用原卡槽，不返还给后续玩家。
		[[nodiscard]] bool Eliminate(std::string_view _player);
		void Finish() noexcept;
		[[nodiscard]] bool Complete() const noexcept { return _MyComplete; }
		[[nodiscard]] double Deadline() const noexcept { return _MyDeadline; }
		[[nodiscard]] std::size_t Turn() const noexcept { return _MyTurn; }
		[[nodiscard]] std::string_view CurrentPlayer() const noexcept;
		[[nodiscard]] std::span<const ChoiceCard> Cards() const noexcept { return _MyCards; }
		[[nodiscard]] std::span<const ChoicePlayer> Players() const noexcept { return _MyPlayers; }
		[[nodiscard]] std::span<const std::size_t> Order() const noexcept { return _MyOrder; }
		[[nodiscard]] std::span<const std::optional<std::size_t>> Taken() const noexcept { return _MyTaken; }

	private:
		[[nodiscard]] std::optional<std::size_t> Player(std::string_view _id) const;
		void StartTurn();
		[[nodiscard]] ChoiceAward Apply(std::size_t _player, std::size_t _card);
		SpecialDraftRules _MyRules;
		std::vector<ChoiceCard> _MyCards;
		std::vector<ChoicePlayer> _MyPlayers;
		std::vector<std::size_t> _MyOrder;
		std::vector<std::optional<std::size_t>> _MyTaken;
		std::vector<std::size_t> _MyAvailable;
		std::size_t _MyTurn{};
		double _MyNow{};
		double _MyDeadline{};
		bool _MyComplete{};
	};
}
#endif
