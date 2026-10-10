#ifndef STRONGHOLD_SIMULATION_BATTLE_HPP
#define STRONGHOLD_SIMULATION_BATTLE_HPP
#include <memory>
#include <stronghold/simulation/content_registry.hpp>
#include <stronghold/simulation/combat_types.hpp>

namespace Stronghold
{
	class BattleCore;
	class SkillBase;
	class OperatorBase;
	class EffectExecutor;
	class Unit;
	class Environment;

	class Battle final
	{
	public:
		explicit Battle(BattleInput _input);
		~Battle();
		Battle(const Battle&) = delete;
		Battle& operator=(const Battle&) = delete;
		Battle(Battle&&) noexcept;
		Battle& operator=(Battle&&) noexcept;

		[[nodiscard]] SkillBase& Skill(UnitId _unit);
		[[nodiscard]] OperatorBase& Operator(UnitId _unit);
		[[nodiscard]] Stronghold::Unit& UnitInterface(UnitId _unit);
		[[nodiscard]] Environment& World() const noexcept;
		std::uint64_t AttachMechanism(UnitId _unit, MechanismDefinition _definition, std::string_view _playerId = {});
		std::uint64_t AttachMechanism(UnitId _unit, ComponentReference _reference, std::string_view _playerId = {});
		bool RemoveMechanism(std::uint64_t _handle);

		void Start();

		void Step();

		void Advance(std::uint64_t _ticks);

		void ForceEnd(BattleEndReason _reason = BattleEndReason::FORCED);
		// 首领技能的显式团队 LP 效果；返回实际扣除值。普通战场、未开始和已结束时无效果。
		double LoseTeamLife(double _amount);
		void SetObstacle(int _row, int _column, bool _enabled, ObstacleKind _kind = ObstacleKind::BLOCK);
		// 位移同步完成，返回实际移动格数；沿地形逐段检查，移动后解除阻挡并重算路线。
		// ID 必须存在；非战斗状态、死亡、静态刚体或失衡免疫时返回零。
		double Displace(UnitId _enemy, WorldPoint _direction, double _distance);
		double Push(UnitId _enemy, double _force, const PushOptions& _options = {});
		double Pull(UnitId _enemy, double _force, const PullOptions& _options);
		double PullToFront(UnitId _enemy, UnitId _source, double _force);
		[[nodiscard]] double PushDistance(UnitId _enemy, double _force, bool _effect = false) const;

		[[nodiscard]] bool Started() const noexcept;

		[[nodiscard]] bool Finished() const noexcept;

		[[nodiscard]] double Time() const noexcept;

		[[nodiscard]] std::uint64_t Tick() const noexcept;
		[[nodiscard]] std::uint32_t RandomState() const noexcept;

		// 单位引用跨新增单位保持有效；容器迭代器不跨会创建单位的回调保留，使用 ID 或下标。
		[[nodiscard]] const std::deque<CombatUnit>& Units() const noexcept;

		[[nodiscard]] std::span<const BattlePlayerState> Players() const noexcept;
		[[nodiscard]] std::span<const BattlePlayerState> Owners() const noexcept;
		[[nodiscard]] const BattlePlayerState& UnitOwner(UnitId _unit) const;
		[[nodiscard]] bool SameOwner(UnitId _left, UnitId _right) const;
		[[nodiscard]] bool Teammates(std::size_t _left, std::size_t _right) const;


		[[nodiscard]] const CombatUnit& Unit(UnitId _id) const;

		// 内置与 CUSTOM 内容共用的位置／范围查询；绝对格范围仍保持调用者给定的坐标。
		[[nodiscard]] RulePositionMode PositionMode() const noexcept;
		[[nodiscard]] bool GarrisonEffectsAfterExit() const noexcept;

		[[nodiscard]] bool RetainGrantedGarrisonsAfterExit() const noexcept;

		[[nodiscard]] WorldPoint RulePosition(UnitId _unit) const;
		[[nodiscard]] const std::bitset<FieldTiles>& RuleRange(UnitId _unit) const;
		[[nodiscard]] std::span<const int> RuleRangeKeys(UnitId _unit) const;
		[[nodiscard]] bool InRuleRange(UnitId _source, UnitId _target) const;

		[[nodiscard]] BattleResult Result() const;

		[[nodiscard]] std::span<const ContentError> ContentErrors() const noexcept;

		[[nodiscard]] std::vector<BattleEvent> DrainEvents();

		// Explicit effects: source 0 means unattributed; otherwise IDs must exist, amounts finite and nonnegative.
		// Valid effects before Start or after finish are no-ops. Returned amounts are actual HP changes.
		double DealDamage(UnitId _source, UnitId _target, double _amount, DamageType _type);

		double DealDamage(UnitId _source, UnitId _target, const DamageInfo& _damage);

		double Heal(UnitId _source, UnitId _target, double _amount, HealOptions _options = {});

		void AddShield(UnitId _target, Shield _shield);

		// 元素损伤进入独立量表，不经过护盾或物理／法术防御；元素伤害仍走 DealDamage。
		double DealElement(UnitId _source, UnitId _target, ElementHit _hit);
		double ReduceElement(UnitId _target, double _amount, std::optional<Element> _element = std::nullopt);
		void BurstElement(UnitId _source, UnitId _target, Element _element);
		[[nodiscard]] std::optional<ElementDisplay> ElementView(UnitId _unit) const;

		// Duration is already resolved by the caller (resistance and content modifiers are separate).
		// Reapplication extends to the longer remaining duration; zero duration does nothing.
		bool ApplyStatus(
			UnitId _target,
			CombatStatus _status,
			double _seconds,
			UnitId _source = 0,
			bool _force = false
		);

		bool ApplyStatus(UnitId _target, CombatStatus _status, const StatusApplication& _application);

		bool RemoveStatus(UnitId _target, CombatStatus _status);

		std::uint64_t AddBuff(UnitId _target, BuffDefinition _definition);

		// 普通同名增益取绝对值最强者；较弱而更晚到期的值在强者结束后恢复。
		bool ApplyStrongest(UnitId _target, std::string _key, double _duration, BuffStrength _strength, UnitId _source = 0);

		std::size_t RemoveBuff(UnitId _target, std::string_view _key);

		bool RemoveBuff(UnitId _target, std::uint64_t _buff);

		double LoseHealth(UnitId _source, UnitId _target, double _amount, bool _silent = false);
		double AddDp(std::string_view _playerId, double _amount);
		// 结算钩子也可记账；仅存在的玩家接受有限正数，返回实际新增资金。
		double AddCoins(std::string_view _playerId, double _amount);
		// 层数钩子先修改增量，再按当前层数限制；满层时不触发钩子。结算钩子也可调用。
		// 非零来源必须是有效单位 ID；未知玩家、非有限/非正增量和关闭增长的战场返回零。
		double AddBondLayers(std::string_view _playerId, std::string_view _bondId, double _amount, LayerGainOptions _options = {});


		// 技力接口分别表达“回复”和“修改”；INITIAL 绕过阻回，用于部署／联防继承。
		// 借出已装备道具的战斗效果；目录来自 BattleInput，重复借出同 ID 只刷新到期时间。
		std::size_t LendEquipment(UnitId _from, UnitId _to, unsigned _maximumTier = 5, double _duration = 60);

		struct LentEquipmentView
		{
			UnitId MyUnit{};
			std::string_view MyItem{};
			double MyUntil{};
		};

		[[nodiscard]] std::vector<LentEquipmentView> LentEquipment(UnitId _unit) const;
		double GainSp(UnitId _unit, double _amount, SpReason _reason = SpReason::GRANTED, bool _silent = false);
		void SetSpTotal(UnitId _unit, double _total);
		void SetSpCostMultiplier(UnitId _unit, double _multiplier);
		[[nodiscard]] double SpCost(UnitId _unit) const;
		[[nodiscard]] double SpTotal(UnitId _unit) const;
		bool ActivateSkill(UnitId _unit, bool _free = false, SkillReason _reason = SkillReason::MANUAL);
		void EndSkill(UnitId _unit, SkillReason _reason = SkillReason::STOPPED);
		bool ForceAttack(UnitId _unit, std::span<const UnitId> _targets = {}, bool _noAmmo = false);
		void AddSkillAmmo(UnitId _unit, double _amount);
		void ExtendSkill(UnitId _unit, double _seconds);
		void AddSkillCharge(UnitId _unit, int _amount = 1);

		// 动态规则保留至内容再次设置（包括再部署）；更新时立即重算朝向／永久攻击距离。
		void SetSkillTrigger(UnitId _unit, SkillTrigger _rule, std::vector<RangeOffset> _grid = {});
		std::uint64_t AddSkillTriggerRange(UnitId _unit, SkillTriggerArea _area);
		bool RemoveSkillTriggerRange(UnitId _unit, std::uint64_t _range);
		bool UpdateSkillTriggerRange(UnitId _unit, std::uint64_t _range, SkillTriggerArea _area);

		// 创建返回稳定 ID；非法定义抛异常，合法但被占位／倒地体拒绝的召唤返回 0。
		UnitId SpawnToken(TokenSpawn _spawn);
		// 技能库存只会激活已布置且来源为 SKILL 的棋盘召唤物；没有棋子时不生成库存。
		UnitId ReleaseSkillSummon(UnitId _owner, std::string_view _token, unsigned _cap = 1);
		[[nodiscard]] unsigned SkillSummonStock(UnitId _owner, std::string_view _token) const;
		UnitId SpawnDevice(DeviceSpawn _spawn);
		// 炮台别名局内唯一，重复激活返回原 ID；找不到有效射程/空位时返回 0。
		UnitId SpawnTurret(TurretSpawn _spawn);
		void SetBondLayers(std::string_view _playerId, std::string_view _bondId, double _layers);
		[[nodiscard]] double BondLayers(std::string_view _playerId, std::string_view _bondId) const;
		// 可在 Start 前注册，返回局内稳定句柄；取消后保留阵数以供战报读取。
		// 合法定义在战斗结束后返回 0；未知非零句柄抛出 out_of_range。
		std::uint64_t StartColdWind(ColdWindDefinition _definition);
		bool CancelColdWind(std::uint64_t _handle);
		[[nodiscard]] std::uint64_t ColdWindGusts(std::uint64_t _handle) const;
		UnitId SpawnEnemy(EnemySpawn _spawn);
		void Retreat(UnitId _unit, bool _permanent = false, RemovalReason _reason = RemovalReason::RETREAT, bool _dying = false);
		bool Redeploy(UnitId _unit, bool _free = true, std::optional<WorldPoint> _tile = std::nullopt, bool _keepSp = false);
		[[nodiscard]] bool CanDeploy(WorldPoint _position, UnitId _excludedUnit = 0, bool _includeDown = true) const;
		[[nodiscard]] bool IsDown(UnitId _unit) const;
		[[nodiscard]] WorldPoint RestPosition(UnitId _unit) const;
		[[nodiscard]] bool ReservedTile(WorldPoint _position) const;
		// 基础射程内依次按道路、距离、朝向局部坐标选择；地形版本变化才重建道路缓存。
		[[nodiscard]] std::optional<WorldPoint> FindTacticalPoint(UnitId _unit);
		[[nodiscard]] const std::bitset<FieldTiles>& GroundPathTiles();
		bool Relocate(UnitId _unit, WorldPoint _position);
		bool MoveRedeploy(UnitId _unit, WorldPoint _position, bool _clearSp = false);
		void SetDownAtHome(UnitId _unit, bool _enabled);
		// 仅存活、尚处于本体的傀儡师能主动切换；致命伤使用相同的转换入口。
		bool EnterDoll(UnitId _unit);

		[[nodiscard]] double RemainingDistance(UnitId _enemy) const;

	private:
		friend class BattleCore;
		friend class SkillBase;
		friend class OperatorBase;
		friend class EffectExecutor;
		explicit Battle(BattleCore& _core) noexcept;
		std::unique_ptr<BattleCore> _MyImplementation;
		BattleCore* _MyView{};
	};
}
#endif
