#ifndef STRONGHOLD_DOMAIN_UNITE_HPP
#define STRONGHOLD_DOMAIN_UNITE_HPP
#include <stronghold/domain/wave_generation.hpp>

namespace Stronghold
{
	struct UniteBond
	{
		bool MyActive{};
		double MyLayers{};
		std::optional<double> MyStoredLayers{};
		double MyPendingGain{};
	};

	// 短期只读视图。所有 span、ID 和战报引用在本次调用结束前有效；计划不保留这些引用。
	struct UniteParticipant
	{
		std::string_view MyPlayerId{};
		int MySeat{};
		bool MyAlive{true};
		bool MyLeft{};
		std::size_t MyDeployCount{};
		std::span<const std::uint64_t> MyOperatorUids{};
		std::span<const UniteBond> MyBonds{};
		std::span<const WaveBounty> MyBounties{};
		std::optional<std::reference_wrapper<const BattlePlayerState>> MyResult{};
		bool MySynthetic{};
	};

	struct UniteMetrics
	{
		std::size_t MyUnits{};
		bool MyActiveBond{};
		double MyLayers{};
		std::size_t MyStanding{};
	};

	struct UniteRules
	{
		bool MySolo{};
		bool MyCapacityExperiment{};
		std::size_t MyNormalAliveCount{};
		std::size_t MyMaxHelpers{2};
	};

	struct UniteLoss { std::string MyPlayerId{}; std::size_t MyCount{}; };

	struct UnitePlan
	{
		std::vector<std::string> MyHelpers{}; // 两人时首位在右半场迎敌，第二位在左半场。
		std::vector<std::string> MyLeakers{};
		std::vector<WaveLeak> MyLeaks{};
		std::vector<UniteLoss> MyNotReentered{};
		unsigned MyRelayRound{}; // 0 为普通单场联防；实验规则最多接力到第 2 场。
		std::vector<std::string> MyRelayCandidates{};
	};

	[[nodiscard]] UniteMetrics MeasureUniteHelper(const UniteParticipant& _player);
	// 先按部署数、激活盟约、存活数、座位选人；只在选中的人之间加入层数决定迎敌顺序。
	[[nodiscard]] std::vector<std::string> SelectUniteHelpers(std::span<const UniteParticipant> _players, std::size_t _limit = 2);
	[[nodiscard]] std::optional<UnitePlan> PlanUnite(const UniteRules& _rules, std::span<const UniteParticipant> _players,
		std::span<const WaveEnemyParameters> _enemies);
	// _fieldSpawns 必须是实际投入第一场联防、已展开 count 的队列。未出生同名敌人按时间和来源逐个取回倍率。
	[[nodiscard]] std::optional<UnitePlan> PlanUniteRelay(const UnitePlan& _previous, const BattleResult& _result, bool _synthetic,
		std::span<const UniteParticipant> _players, std::span<const EnemySpawn> _fieldSpawns, std::span<const WaveEnemyParameters> _enemies, std::size_t _maxHelpers = 2);
	[[nodiscard]] std::vector<UniteLoss> UniteSurvivors(const UnitePlan& _plan, const BattleResult& _result);
	// 不带入 Buff 或技能激活状态。干员继承 HP/SP 或倒地，棋盘召唤物仅在存活时继承 SP。
	void ApplyUniteCarry(BattlePlayerInput& _input, const BattlePlayerState& _previous);
}
#endif
