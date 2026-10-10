#ifndef STRONGHOLD_ADAPTERS_REFERENCE_WAVE_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_WAVE_HPP
#include <stronghold/adapters/reference_combat.hpp>

namespace Stronghold
{
	enum class WaveKind { NORMAL, BOSS, HIDDEN, TRAINING, ESCAPED };
	enum class WaveAction { SPAWN, ACTIVATE_PREDEFINED };

	// 表内坐标保留原始值；解析战斗输入时才按区域裁剪，绝不将半格路线取整。
	struct WaveRoute
	{
		WorldPoint MyStart{};
		WorldPoint MyEnd{};
		std::span<const RouteStep> MySteps{};
		bool MyFlying{};
		std::span<const std::size_t> MyPatrolSteps{}; // 原脚本 patrol 索引；普通编译按 MOVE，首领脚本可据此建立循环。
		WorldPoint MySpawnRandom{}; // 模板元数据，随机出生偏移由比赛波次选择层处理。
		[[nodiscard]] CombatRoute Compile(FieldRect _rect) const;
	};

	struct WaveSpawnGroup
	{
		double MyTime{};
		std::string_view MyEnemyId{};
		unsigned MyCount{1};
		double MyInterval{};
		std::size_t MyRoute{};
		std::string_view MySlot{};
		EnemySpawnTag MyTag{};
		std::string_view MyGroup{};
		std::string_view MyPack{};
		double MyWeight{1};
		bool MyUnharmful{};
		WaveAction MyAction{};
	};

	struct WaveBranchPhase { std::span<const WaveSpawnGroup> MyActions{}; };

	struct WaveBranch
	{
		std::string_view MyId{};
		std::span<const WaveBranchPhase> MyPhases{};
	};

	struct WaveDevice
	{
		std::string_view MyId{};
		std::string_view MyAlias{};
		WorldPoint MyPosition{};
		Facing MyFacing{Facing::UP};
		bool MyHidden{};
	};

	struct WaveUse
	{
		std::string_view MyModeId{};
		unsigned MyRound{};
		std::string_view MyBossId{};
	};

	struct EnemyMultipliers
	{
		double MyHealth{1};
		double MyAttack{1};
		double MyDefense{1};
		double MyResistance{1};
		double MySpeed{1};
	};

	struct WaveRecord
	{
		std::string_view MyId{};
		WaveKind MyKind{};
		bool MySolo{};
		std::string_view MyBossId{};
		double MyMaxPlayTime{}; // 数据中的真实秒；比赛层决定模拟倍速，适配器不擅自乘二。
		double MyInitialDp{10};
		double MyDpPerSecond{1};
		double MyMaxDp{99};
		unsigned MyCharacterLimit{};
		double MyMoveMultiplier{};
		std::string_view MyMusic{};
		std::span<const WaveRoute> MyRoutes{};
		std::span<const WaveRoute> MyExtraRoutes{};
		std::span<const WaveSpawnGroup> MySpawns{};
		std::span<const WaveBranch> MyBranches{};
		std::span<const EnemyRecord> MyEnemyOverrides{};
		std::span<const WaveDevice> MyDevices{};
		unsigned MyTotalCount{};
		Blackboard MySlotCounts{};
		std::span<const WaveUse> MyUsedBy{};

		// 返回合并后的关卡敌人，黑板键逐项覆盖，技能列表整体替换；视图具有静态生命周期。
		[[nodiscard]] const EnemyRecord& Enemy(std::string_view _id) const;
		[[nodiscard]] const WaveBranch& Branch(std::string_view _id) const;
		// 初始出生使用到的地面道路优先；没有有效地面索引时采用全部地面道路。忽略飞行／越界索引。
		[[nodiscard]] std::vector<CombatRoute> MakeGroundRoutes(std::span<const std::size_t> _usedRoutes, FieldRect _rect) const;
		// 展开模板的全部主出生组，保持同时间原组顺序。比赛中的加权组/槽位选择应先在上层完成。
		// owner 由原始起点的左右半场解析；基础属性先应用关卡覆盖，再应用显式倍率。
		[[nodiscard]] std::vector<EnemySpawn> MakeSpawns(
			std::span<const BattlePlayerInput> _players,
			FieldRect _rect,
			EnemyMultipliers _multipliers = {}
		) const;
	};

	[[nodiscard]] std::span<const WaveRecord> ReferenceWaves() noexcept;
	[[nodiscard]] const WaveRecord& ReferenceWave(std::string_view _id);
}
#endif
