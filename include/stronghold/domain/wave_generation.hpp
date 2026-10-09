#ifndef STRONGHOLD_DOMAIN_WAVE_GENERATION_HPP
#define STRONGHOLD_DOMAIN_WAVE_GENERATION_HPP
#include <stronghold/adapters/reference_wave.hpp>
#include <stronghold/core/random.hpp>

namespace Stronghold
{
	// 波次生成使用独立的数据视图；所有 string_view / span 由调用方持有，至少活到计划消费完毕。
	enum class FactionType { SPECIAL, FLY, TIMES, ELEMENT, DOT, INVISIBLE, REFLECTION };
	enum class WaveSlot { NONE, N, E, S, NF, EF, SF, T, TF };

	struct WeightedId { std::string_view MyId{}; double MyWeight{1}; };
	struct BossTemplate { std::string_view MyBossId{}; std::string_view MyTemplateId{}; };
	struct FactionDefinition
	{
		FactionType MyType{};
		bool MyRandom{};
		unsigned MyCount{3};
		int MySort{};
	};

	struct FactionEntry
	{
		std::string_view MyEnemyId{};
		FactionType MyType{};
		double MyWeight{1};
		bool MyFirstHalf{};
		bool MyFlying{};
		std::span<const std::string_view> MyNormal{};
		std::span<const std::string_view> MyElite{};
	};

	struct WaveEnemyParameters
	{
		std::string_view MyId{};
		double MyPower{};
		double MyFactor{1};
		bool MyFlying{};
		EnemyRank MyRank{};
		bool MyTokenOnly{};
		bool MyNotCounted{};
	};

	struct WavePlaceholder { std::string_view MyEnemyId{}; WaveSlot MySlot{}; };
	struct WaveScale
	{
		double MyHealth{1};
		double MyAttack{1};
		double MySpeed{1};
		double MyDefense{1};
		double MyResistance{1};
		std::optional<double> MySupplyHealth{}; // 频次敌人的内容层需要扣除补给线的血量倍率。
	};

	struct WaveRoundRules
	{
		unsigned MyRound{};
		std::string_view MyTemplateId{};
		std::span<const BossTemplate> MyBossTemplates{}; // 原 JSON 顺序决定未知 boss ID 的回退。
		double MyTimeLimit{120}; // 显式游戏秒；数据适配器负责实秒到游戏秒的转换。
		WaveScale MyScale{};
	};

	struct WaveModeRules
	{
		std::string_view MyId{};
		std::span<const WeightedId> MyStages{};
		std::string_view MyFallbackStage{};
		std::span<const WeightedId> MyBosses{};
		std::span<const WeightedId> MyHiddenBosses{};
		unsigned MyBossRound{14};
		unsigned MyHiddenRound{};
		std::span<const std::string_view> MyInactiveEnemies{};
		std::span<const WaveRoundRules> MyRounds{};
	};

	struct WaveGenerationRules
	{
		std::span<const FactionDefinition> MyFactions{}; // 字典序（DOT, ELEMENT, …），用于相同种子的洗牌。
		std::span<const FactionEntry> MyEntries{}; // 敌人 ID 字典序。
		std::span<const WaveEnemyParameters> MyEnemies{}; // 敌人 ID 字典序。
		std::span<const WavePlaceholder> MyPlaceholders{};
		unsigned MyFactionCount{3};
		unsigned MyRoundCount{15};
		unsigned MyFirstHalfLastRound{7};
		unsigned MyMinReplacement{1};
		unsigned MyMaxReplacement{5};
		FactionType MyFillType{FactionType::SPECIAL};
		std::span<const WavePlaceholder> MyTemplateSlots{};
		std::array<std::string_view, 2> MyUniteTemplates{};
	};

	struct WavePick
	{
		unsigned MyRound{};
		FactionType MyType{};
		std::string_view MySpecial{};
		std::string_view MyNormal{};
		std::string_view MyElite{};
		bool MyFlying{};
		bool MyFirstHalf{};
	};

	struct WaveSetup
	{
		std::string_view MyStageId{};
		std::vector<FactionType> MyFactions{}; // 显示顺序，独立于下方打乱前的选中顺序。
		std::string_view MyBossId{};
		std::string_view MyHiddenBossId{};
		std::vector<FactionType> MyTypeSlots{};
		std::vector<std::optional<WavePick>> MyPicks{}; // 第 0 项为空，round 可直接索引。
	};

	struct WavePreview
	{
		bool MyUpper{};
		std::optional<WorldPoint> MyStart{};
		bool MyFlying{};
		bool MyElite{};
		bool MyBoss{};
	};

	enum class WaveSide { ANY, LEFT, RIGHT };

	struct DuckWaveRules
	{
		std::string_view MyStrategy{};
		unsigned MyFirstRound{1};
		unsigned MyMinimum{};
		unsigned MyMaximum{};
		double MyStartFraction{};
		double MyEndFraction{1};
		std::int64_t MyCoins{1};
		std::span<const std::string_view> MyEnemies{};
	};

	struct WaveBounty
	{
		std::string MyId{};
		std::string_view MyEnemyId{};
		unsigned MyCount{1};
		std::int64_t MyCoins{};
		bool MyPerfect{}; // 完美悬赏由比赛结算支付，不附在敌人的击杀奖励上。
	};

	struct WaveLeak
	{
		std::string MyEnemyId{};
		std::string MySourcePlayer{};
		std::optional<EnemySpawnModifiers> MyModifiers{};
		bool MyToken{};
		bool MyBounty{};
		std::string MyBountyId{};
		std::int64_t MyCoins{};
		std::string MyRewardOwner{};
	};

	struct WavePlannedSpawn
	{
		double MyTime{};
		std::string_view MyEnemyId{};
		std::size_t MyRoute{};
		unsigned MyCount{1};
		double MyInterval{};
		WaveScale MyScale{};
		WaveSlot MySlot{};
		EnemySpawnTag MyTag{};
		std::size_t MyActionIndex{};
		bool MyCounted{true};
		WavePreview MyPreview{};
		bool MyBounty{};
		std::string MyBountyId{};
		std::string MyOwnerPlayer{};
		std::string MySourcePlayer{};
		std::int64_t MyCoins{};
		std::string MyRewardOwner{};
		std::optional<EnemySpawnModifiers> MyOriginalModifiers{}; // 联防原样携带首次出生倍率，避免重算回合强化。
	};

	struct WavePlannedAction
	{
		std::size_t MyIndex{};
		std::string_view MyEnemyId{}; // 无效运动类别的动作保留元数据，敌人 ID 为空。
		std::string_view MyTemplateEnemy{};
		WaveSlot MySlot{};
		double MyTime{};
		unsigned MyCount{};
		unsigned MyTemplateCount{};
		double MyWindow{};
		std::size_t MyRoute{};
		bool MyValid{};
		bool MyServer{};
	};

	struct WavePlan
	{
		std::string_view MyTemplateId{};
		double MyTimeLimit{std::numeric_limits<double>::infinity()};
		std::optional<WavePick> MyPick{};
		std::vector<WavePlannedSpawn> MySpawns{};
		std::vector<WavePlannedAction> MyActions{};
	};

	// 显式传入确定性随机流；阵营排程派生自己的流，不额外消耗主对局流。
	class WaveGenerator final
	{
	public:
		WaveGenerator(const WaveGenerationRules& _rules, const WaveModeRules& _mode, std::span<const WaveRecord> _waves);
		[[nodiscard]] WaveSetup Setup(Random& _random) const;
		[[nodiscard]] std::optional<WavePick> PickRound(Random& _random, FactionType _type, unsigned _round) const;
		[[nodiscard]] unsigned ReplacedCount(std::string_view _templateEnemy, unsigned _count, std::string_view _enemy) const;
		[[nodiscard]] WavePlan BuildNormal(const WaveSetup& _setup, unsigned _round) const;
		[[nodiscard]] WavePlan BuildBoss(const WaveSetup& _setup, unsigned _round, std::string_view _boss, bool _solo) const;
		// 模板指针仅在本次查询内使用；返回计划拥有所有可变容器，只借用已验证配置中的文字。
		// 悬赏插在其原模板宿主动作的单位之间，并重排该动作自身的时间；不修改输入计划。
		// 首领场沿用原 MatchBoss 的追加规则，传 false 保留宿主原时间，只追加悬赏单位。
		[[nodiscard]] WavePlan WithBounties(const WavePlan& _plan, unsigned _round, std::span<const WaveBounty> _bounties,
			std::string_view _player, WaveSide _side = WaveSide::ANY, bool _retimeOwn = true) const;
		[[nodiscard]] WavePlan BuildUnite(std::span<const WaveLeak> _leaks, unsigned _helpers) const;
		// 对选中半场按出生顺序取区间、洗牌，再原位拆分多单位动作；返回实际替换数。
		[[nodiscard]] std::size_t ReplaceDucks(WavePlan& _plan, Random& _random, const DuckWaveRules& _rules,
			std::string_view _owner, WaveSide _side = WaveSide::ANY) const;
		[[nodiscard]] const WaveRecord* Template(std::string_view _id) const noexcept;
		[[nodiscard]] std::vector<EnemySpawn> MakeSpawns(const WavePlan& _plan, std::span<const BattlePlayerInput> _players, FieldRect _rect) const;
		[[nodiscard]] std::vector<CombatRoute> MakeGroundRoutes(const WavePlan& _plan, FieldRect _rect) const;

	private:
		[[nodiscard]] const WaveEnemyParameters* Enemy(std::string_view _id) const noexcept;
		[[nodiscard]] const WaveRoundRules* Round(unsigned _round) const noexcept;
		[[nodiscard]] WavePlan Build(std::string_view _id, const WaveSetup& _setup, unsigned _round, double _timeLimit) const;
		[[nodiscard]] std::size_t RouteByMotion(const WaveRecord& _wave, bool _flying, WaveSide _side) const;
		[[nodiscard]] WavePreview Preview(const WaveRecord* _wave, std::string_view _enemy, std::size_t _route, std::optional<bool> _leader = {}) const;
		[[nodiscard]] std::optional<std::size_t> Host(const WaveRecord* _wave, bool _flying, bool _token = false, bool _firstSpawnFallback = true) const;
		WaveGenerationRules _MyRules;
		WaveModeRules _MyMode;
		std::span<const WaveRecord> _MyWaves;
	};
}
#endif
