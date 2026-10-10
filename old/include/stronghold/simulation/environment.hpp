#ifndef STRONGHOLD_SIMULATION_ENVIRONMENT_HPP
#define STRONGHOLD_SIMULATION_ENVIRONMENT_HPP
#include <stronghold/simulation/combat_types.hpp>

namespace Stronghold
{
	class Battle;
	class BattleCore;
	class Unit;
	class SkillBase;
	class EffectExecutor;
	class Selector;
	struct EffectContext;
	struct SelectorDefinition;
	struct EffectTarget;
	struct EffectProgram;
	class PlayerTarget;
	class TerrainTarget;

	// 环境负责世界查询、玩家／地形目标、调度和全局结算；单位负责接受效果。
	// Battle 对外仍是开局／推进门面，环境及单位引用均由稳定的战场核心持有。
	class Environment final
	{
	public:
		Environment(const Environment&) = delete;
		Environment& operator=(const Environment&) = delete;
		[[nodiscard]] Unit& GetUnit(UnitId _unit) const;
		[[nodiscard]] const CombatUnit& State(UnitId _unit) const;
		[[nodiscard]] SkillBase& Skill(UnitId _unit) const;
		[[nodiscard]] std::span<const BattlePlayerState> Owners() const noexcept;
		[[nodiscard]] const BattlePlayerState& Owner(UnitId _unit) const;
		[[nodiscard]] PlayerTarget Player(std::size_t _index) const;
		[[nodiscard]] TerrainTarget Tile(FieldPoint _point) const;
		[[nodiscard]] double Time() const noexcept;
		[[nodiscard]] bool Started() const noexcept;
		[[nodiscard]] bool Finished() const noexcept;
		[[nodiscard]] WorldPoint RulePosition(UnitId _unit) const;
		[[nodiscard]] const std::bitset<FieldTiles>& RuleRange(UnitId _unit) const;
		void Select(const EffectContext& _context, const SelectorDefinition& _definition, std::vector<EffectTarget>& _output) const;
		void Execute(const EffectContext& _context, const EffectProgram& _program);
		// 为旧 ContentRegistry 扩展提供桥接；新组件无需依赖 Battle 门面。
		[[nodiscard]] Battle& BattleView() const noexcept;

	private:
		friend class BattleCore;
		friend class Unit;
		friend class SkillBase;
		friend class EffectExecutor;
		friend class PlayerTarget;
		friend class TerrainTarget;
		explicit Environment(BattleCore& _core) noexcept : _MyCore(_core) {}

		BattleCore& _MyCore;
	};

	class PlayerTarget final
	{
	public:
		[[nodiscard]] const BattlePlayerState& State() const;
		double AddDp(double _amount);
		double AddCoins(double _amount);
		double AddBondLayers(std::string_view _bond, double _amount, LayerGainOptions _options = {});

	private:
		friend class Environment;
		PlayerTarget(Environment& _environment, std::size_t _index) noexcept : _MyEnvironment(_environment), _MyIndex(_index) {}

		Environment& _MyEnvironment;
		std::size_t _MyIndex{};
	};

	class TerrainTarget final
	{
	public:
		[[nodiscard]] FieldPoint Position() const noexcept { return _MyPoint; }

		[[nodiscard]] FieldTile State() const;
		void SetObstacle(bool _enabled, ObstacleKind _kind = ObstacleKind::BLOCK);

	private:
		friend class Environment;
		TerrainTarget(Environment& _environment, FieldPoint _point) noexcept : _MyEnvironment(_environment), _MyPoint(_point) {}

		Environment& _MyEnvironment;
		FieldPoint _MyPoint{};
	};
}
#endif
