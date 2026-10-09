#ifndef STRONGHOLD_ADAPTERS_REFERENCE_STAGE_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_STAGE_HPP
#include <stronghold/adapters/reference_combat.hpp>

namespace Stronghold
{
	// 编译期地图装置元数据；角色名保留原协议拼写，仅在建局适配阶段识别。
	struct StageDeviceRecord
	{
		std::string_view MyId{};
		std::string_view MyAlias{};
		std::string_view MyRole{};
		FieldPoint MyPosition{};
		bool MyActive{};
		CombatStats MyStats{};
		std::span<const FieldPoint> MyRange{};
		Blackboard MySkill{};
		std::optional<Airflow> MyAirflow{};
		double MyFragilityDuration{};
	};

	struct DeviceOverride
	{
		std::string_view MyAlias{};
		bool MyActive{};
		std::string_view MyPlayerId{}; // 空 ID 表示全场；所属玩家的设置优先于全场设置。
	};

	struct StageSetup
	{
		FieldDefinition MyField{};
		std::vector<DeviceSpawn> MyDevices{};
		std::vector<TurretSpawn> MyTurrets{};
	};

	struct StageRecord
	{
		std::string_view MyId{};
		std::string_view MyName{};
		std::span<const FieldTile> MyTiles{};
		TerrainRules MyRules{};
		std::span<const StageDeviceRecord> MyDevices{};

		// 一次建局解析：返回值拥有可变地图/装置，之后的热路径不查询黑板或 JSON。
		[[nodiscard]] StageSetup Prepare(FieldRect _rect, std::span<const BattlePlayerInput> _players,
			std::span<const DeviceOverride> _overrides = {}) const;
		// 替换该输入的地图与地图装置，保留玩家、出生队列和时间配置。
		void Apply(BattleInput& _input, FieldRect _rect, std::span<const DeviceOverride> _overrides = {}) const;
	};

	[[nodiscard]] std::span<const StageRecord> ReferenceBattleStages() noexcept;
	[[nodiscard]] const StageRecord& ReferenceBattleStage(std::string_view _id);
}
#endif
