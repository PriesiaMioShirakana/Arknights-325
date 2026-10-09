#ifndef STRONGHOLD_DOMAIN_FINAL_ASSAULT_HPP
#define STRONGHOLD_DOMAIN_FINAL_ASSAULT_HPP
#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <stronghold/domain/combat_math.hpp>

namespace Stronghold
{
	// 模式覆盖全局配置；无效数值与缺省值遵循原 GameData 的回退顺序。
	struct BossHealthScale
	{
		std::optional<double> MySolo{};
		std::optional<double> MyCoop{};
		std::optional<double> MyFullTeam{};
		std::optional<bool> MyPerPlayer{};
		std::optional<bool> MyAliveScaling{};
	};

	[[nodiscard]] double BossPoolShare(const BossHealthScale& _mode, const BossHealthScale& _global, bool _solo, std::optional<double> _aliveCount = {});
	[[nodiscard]] double BossPoolHealth(double _base, double _share, double _tuning, bool _solo, std::size_t _aliveCount, bool _capacityExperiment);

	struct FinalAssaultRules
	{
		bool MySolo{};
		bool MyCapacityExperiment{};
		bool MyHasHiddenRound{};
		bool MyHiddenDifficultyAllowed{};
		double MyHiddenSoloLayers{350};
		double MyHiddenCoopLayers{1200};
		double MyHiddenMinimumLife{1};
		double MyGameSecondsPerRealSecond{2};
		double MyOvertimeAfterRealSeconds{150};
		double MyOvertimeDrainPerRealSecond{1};
	};

	[[nodiscard]] double BossOvertimeDue(const FinalAssaultRules& _rules, double _gameSeconds);
	[[nodiscard]] bool HiddenCoreEligible(const FinalAssaultRules& _rules, double _layers, double _teamLife, std::size_t _aliveCount) noexcept;

	struct BossParticipant
	{
		std::string MyPlayerId{};
		int MySeat{};
		std::int64_t MyLife{};
		double MyActivatedLayers{}; // 最终攻势备战结束时的快照；战中变化不改变隐藏关资格。
	};

	struct BossPlayerProgress
	{
		std::string MyPlayerId{};
		int MySeat{};
		std::int64_t MyInitialLife{};
		std::int64_t MyLife{};
		double MyRoundDamage{};
		double MyTotalDamage{};
		unsigned MyHitStep{};
	};

	struct BossFieldGroup
	{
		std::array<std::string, 2> MyPlayers{};
		unsigned MyCount{};
		bool MySoloTemplate{};
	};

	struct BossHit
	{
		std::string_view MyPlayerId{}; // 借用控制器的稳定玩家 ID；控制器销毁后失效。
		double MyThreshold{};
	};

	enum class BossOutcome { ACTIVE, VICTORY, DEFEAT };
	struct FinalAssaultResult { bool MyVictory{}; bool MyHiddenReached{}; bool MyHiddenCleared{}; };

	// 一次最终攻势及其可选隐藏关。调用者串行推进各战场；没有线程、系统时钟或网络依赖。
	// Battle 可借用 Pool()，因此控制器禁止复制/移动；进入隐藏关前必须销毁上一关的战场。
	class FinalAssault final
	{
	public:
		FinalAssault(FinalAssaultRules _rules, std::span<const BossParticipant> _players, double _bossHealth);
		FinalAssault(const FinalAssault&) = delete;
		FinalAssault& operator=(const FinalAssault&) = delete;
		FinalAssault(FinalAssault&&) = delete;
		FinalAssault& operator=(FinalAssault&&) = delete;
		[[nodiscard]] SharedBossPool& Pool() noexcept { return _MyPool; }
		[[nodiscard]] const SharedBossPool& Pool() const noexcept { return _MyPool; }
		[[nodiscard]] double Damage(std::string_view _player, double _amount);
		void LoseLife(double _amount);
		// 每个战场时间步及 LP 事件后调用；同步直接写入 Pool 的伤害并固定首次终局结果。
		void Observe();
		void Advance(double _gameSeconds);
		// 所有战场已结束或宿主主动中止时收口；尚未击败首领算失败，保留剩余团队 LP。
		void Conclude();
		[[nodiscard]] bool CanEnterHidden() const noexcept;
		void BeginHidden(double _bossHealth);
		[[nodiscard]] FinalAssaultResult Result() const;
		[[nodiscard]] BossOutcome Outcome() const noexcept { return _MyOutcome; }
		[[nodiscard]] bool Hidden() const noexcept { return _MyHidden; }
		[[nodiscard]] double TeamLife() const noexcept { return _MyTeamLife; }
		[[nodiscard]] double OvertimeApplied() const noexcept { return _MyOvertimeApplied; }
		[[nodiscard]] std::span<const BossPlayerProgress> Players() const noexcept { return _MyPlayers; }
		[[nodiscard]] std::span<const BossFieldGroup> Fields() const noexcept { return _MyFields; }
		[[nodiscard]] std::span<const BossHit> Hits() const noexcept { return _MyHits; }
		void ClearHits() noexcept { _MyHits.clear(); }

	private:
		void SyncLife();
		FinalAssaultRules _MyRules;
		SharedBossPool _MyPool;
		std::vector<BossPlayerProgress> _MyPlayers;
		std::vector<BossFieldGroup> _MyFields;
		std::vector<BossHit> _MyHits;
		struct Remainder { std::size_t MyPlayer{}; double MyFraction{}; };
		std::vector<Remainder> _MyRemainders;
		double _MyInitialLife{};
		double _MyTeamLife{};
		double _MyLayers{};
		double _MyGameSeconds{};
		double _MyOvertimeApplied{};
		BossOutcome _MyOutcome{};
		bool _MyHidden{};
	};
}
#endif
