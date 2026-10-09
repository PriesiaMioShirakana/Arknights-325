#ifndef STRONGHOLD_SIMULATION_FIELD_HPP
#define STRONGHOLD_SIMULATION_FIELD_HPP
#include <array>
#include <cstdint>
#include <span>
#include <optional>
#include <vector>

namespace Stronghold
{
	inline constexpr int FieldRows = 19;
	inline constexpr int FieldColumns = 21;
	inline constexpr int FieldTiles = FieldRows * FieldColumns;

	enum class FieldBuild { NONE, ALL, MELEE, RANGED };
	enum class ObstacleKind : unsigned { BLOCK = 1, CRATE = 2 };
	enum class FieldTerrain { NONE, MIRE, SMOG, DEEPSEA, INFECTION };

	// 适配器将地图黑板的百分数/方向解析一次，运行中只读取固定数值。
	struct Airflow
	{
		int MyX{};
		int MyY{1};
		double MyAllyEqualAttack{};
		double MyAllyOppositeAttack{};
		double MyAllyVerticalAttack{};
		double MyEnemyEqualSpeed{};
		double MyEnemyOppositeSpeed{};
	};

	struct TerrainRules
	{
		double MyMireInterval{1};
		double MyMireAttackSpeed{-5};
		double MyMireMovePerStack{-0.05};
		unsigned MyMireMaxStacks{10};
		double MyMireHeavyMass{3};
		double MyDeepseaDamage{40};
		double MyDeepseaAttackSpeed{-60};
		double MyDeepseaMoveMultiplier{0.6};
		double MyInfectionDamage{70};
		double MyInfectionAttackPercent{0.2};
		double MyInfectionAttackSpeed{20};
		double MyInfectionDuration{300};
	};

	struct FieldTile
	{
		bool MyWalkable{true};
		bool MyFlyable{true};
		bool MyLow{true};
		FieldBuild MyBuild{FieldBuild::ALL};
		bool MyGoal{};
		FieldTerrain MyTerrain{};
		bool MyElevated{}; // 地图初始射击台/沙丘；普通 relocate 读取这个地形标志。
		std::optional<bool> MyDeploymentElevation{}; // 地图卡仅在部署/移动再部署时覆盖高度。
	};

	struct FieldRect
	{
		int MyFirstRow{};
		int MyLastRow{FieldRows - 1};
		int MyFirstColumn{};
		int MyLastColumn{FieldColumns - 1};
	};

	struct FieldDefinition
	{
		std::array<FieldTile, FieldTiles> MyTiles{};
		FieldRect MyRect{};
		TerrainRules MyTerrainRules{};
		std::array<std::optional<Airflow>, FieldTiles> MyAirflow{};
		std::array<unsigned char, FieldTiles> MyInitialObstacles{};
	};

	struct FieldPoint
	{
		int MyRow{};
		int MyColumn{};
	};

	struct FlowField
	{
		int MyDestination{-1};
		std::array<int, FieldTiles> MyDistance{};
		std::array<int, FieldTiles> MyParent{};
		std::array<int, FieldTiles> MyPenalty{};
		std::array<int, FieldTiles> MyNext{};
		std::array<int, FieldTiles> MyOfficial{};
		std::array<int, FieldTiles> MyCost{};
		mutable std::array<double, FieldTiles> MyLength{};
		std::uint64_t MyVersion{};
		bool MyAllowDiagonal{true};
		bool MyIgnoreObstacles{};
	};

	// 地图按值持有不可变地形和可变障碍层。寻路保持 JS 的队列顺序／严格改进／平滑顺序。
	// 缓存最多 64 个目的地；返回的场引用只在下一次障碍修改或缓存淘汰前有效。
	class FieldGrid final
	{
	public:
		explicit FieldGrid(FieldDefinition _definition = {});
		[[nodiscard]] static constexpr int Key(int _row, int _column) noexcept { return _row * FieldColumns + _column; }
		[[nodiscard]] static constexpr bool InBounds(int _row, int _column) noexcept
		{ return _row >= 0 && _row < FieldRows && _column >= 0 && _column < FieldColumns; }
		[[nodiscard]] bool InRect(int _row, int _column) const noexcept;
		[[nodiscard]] FieldTile Tile(int _row, int _column) const noexcept;
		[[nodiscard]] bool GroundPassable(int _row, int _column, bool _ignoreObstacles = false) const noexcept;
		[[nodiscard]] bool Walkable(int _row, int _column, bool _ignoreObstacles = false) const noexcept;
		[[nodiscard]] bool FlyPassable(int _row, int _column) const noexcept;
		[[nodiscard]] bool CanStand(int _row, int _column, bool _ranged = false) const noexcept;
		[[nodiscard]] bool Blockable(int _row, int _column) const noexcept;
		[[nodiscard]] bool Obstacle(int _row, int _column, ObstacleKind _kind) const noexcept;
		[[nodiscard]] const FieldRect& Rect() const noexcept { return _MyDefinition.MyRect; }
		[[nodiscard]] std::uint64_t Version() const noexcept { return _MyVersion; }
		void SetObstacle(int _row, int _column, bool _enabled, ObstacleKind _kind = ObstacleKind::BLOCK);
		[[nodiscard]] const FlowField& Flow(int _row, int _column, bool _allowDiagonal = true, bool _ignoreObstacles = false);
		[[nodiscard]] static double Length(const FlowField& _field, int _key);
		// 输出缓冲由调用方复用；不可达返回 false 且清空结果，起点与终点均包含在成功路径中。
		bool Waypoints(FieldPoint _start, FieldPoint _end, std::vector<FieldPoint>& _output, bool _allowDiagonal = true, bool _ignoreObstacles = false);
		[[nodiscard]] bool StraightClear(double _x, double _y, FieldPoint _end) const;

	private:
		[[nodiscard]] FlowField Build(int _row, int _column, bool _allowDiagonal, bool _ignoreObstacles) const;
		void Spfa(FlowField& _field, bool _preferBlockable) const;
		FieldDefinition _MyDefinition{};
		std::array<unsigned char, FieldTiles> _MyObstacles{};
		std::array<int, FieldTiles> _MyUnblockable{};
		std::vector<FlowField> _MyFields{};
		std::size_t _MyReplacement{};
		std::uint64_t _MyVersion{};
	};
}
#endif
