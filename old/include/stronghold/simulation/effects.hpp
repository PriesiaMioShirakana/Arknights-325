#ifndef STRONGHOLD_SIMULATION_EFFECTS_HPP
#define STRONGHOLD_SIMULATION_EFFECTS_HPP
#include <variant>
#include <stronghold/simulation/content_registry.hpp>

namespace Stronghold
{
	enum class EffectTargetKind { UNIT, PLAYER, TERRAIN };
	enum class SelectorKind { SELF, PROVIDED, EVENT_UNIT, EVENT_SOURCE, EVENT_TARGET, ENEMIES, ALLIES, OWN_UNITS, TEAMMATE_UNITS, PLAYERS, OWNER, TEAMMATES, TERRAIN, CUSTOM };
	enum class SelectorRange { GLOBAL, SOURCE, MASK, RADIUS, OFFSETS };
	enum class SelectorOrder { STABLE, RANGE_KEYS, ATTACK_PRIORITY, LOWEST_HEALTH_RATIO, NEAREST, HEAVIEST, RANDOM };

	struct EffectTarget
	{
		EffectTargetKind MyKind{EffectTargetKind::UNIT};
		UnitId MyUnit{};
		std::size_t MyPlayer{NoPlayer};
		FieldPoint MyTile{};
	};

	// MyProvided 与 MyEvent 只在同步执行期间借用；延迟机制复制 ID 并清除事件指针。
	struct EffectContext
	{
		UnitId MySource{};
		std::size_t MyOwner{NoPlayer};
		UnitId MyEventUnit{};
		UnitId MyEventSource{};
		UnitId MyEventTarget{};
		double MyEventAmount{};
		std::span<const UnitId> MyProvided{};
		double MyDelta{1};
		bool MyAllowAfterFinish{};
		ContentEvent* MyEvent{};
	};

	struct SelectorDefinition
	{
		SelectorKind MyKind{SelectorKind::ENEMIES};
		SelectorRange MyRange{SelectorRange::GLOBAL};
		SelectorOrder MyOrder{SelectorOrder::STABLE};
		std::bitset<FieldTiles> MyMask{};
		std::optional<WorldPoint> MyCenter{};
		double MyRadius{};
		unsigned MyLimit{};
		bool MyAliveOnly{true};
		bool MyIncludeHidden{};
		bool MyExcludeSource{};
		bool MyTargetable{true};
		bool MyCanHitFlying{true};
		bool MyHitSleep{};
		bool MyGroundOnly{};
		bool MyWoundedOnly{};
		std::optional<double> MyMaximumHealthRatio{};
		std::optional<UnitKind> MyUnitKind{};
		std::optional<OperatorProfession> MyProfession{};
		std::string_view MyNation{};
		std::optional<FieldTerrain> MyTerrain{};
		std::optional<FieldBuild> MyBuild{};
		ComponentReference MyCustom{};
		bool MyIncludeVirtualPlayers{};
		std::span<const RangeOffset> MyOffsets{}; // 技能独立范围，以来源的规则位置和朝向展开，不受攻击距离影响。
		std::optional<SpType> MySpType{};
		bool MyAttackHurtSpOnly{};
		bool MyEliteOnly{};
		bool MyRespectIsolation{};
	};

	class SelectorBase
	{
	public:
		virtual ~SelectorBase() = default;
		virtual void Select(const Battle& _battle, const EffectContext& _context, std::vector<EffectTarget>& _output) const = 0;
	};

	struct EffectAmount
	{
		double MyFlat{};
		double MySourceAttack{};
		double MySourceDefense{};
		double MySourceMaxHealth{};
		double MyTargetMaxHealth{};
		double MyEventAmount{};
		bool MyPerSecond{};
	};

	struct DamageOperation
	{
		EffectAmount MyAmount{};
		DamageInfo MyDamage{};
	};

	struct HealOperation
	{
		EffectAmount MyAmount{};
		HealOptions MyOptions{};
	};

	struct HealthLossOperation
	{
		EffectAmount MyAmount{};
		bool MySilent{};
	};

	struct SpOperation
	{
		EffectAmount MyAmount{};
		SpReason MyReason{SpReason::GRANTED};
		bool MySetTotal{};
	};

	struct StatusOperation
	{
		CombatStatus MyStatus{};
		StatusApplication MyApplication{};
		bool MyRemove{};
	};

	struct BuffOperation
	{
		BuffDefinition MyDefinition{};
		ComponentReference MyPreset{};
		bool MyRemove{};
	};

	struct ShieldOperation
	{
		EffectAmount MyAmount{};
		Shield MyShield{};
	};

	struct StrongestBuffOperation
	{
		std::string_view MyKey{};
		double MyDuration{};
		BuffStrength MyStrength{};
	};

	struct ElementOperation
	{
		EffectAmount MyAmount{};
		ElementHit MyHit{};
	};

	enum class PlayerOperationKind { DP, COINS, BOND_LAYERS };

	struct PlayerOperation
	{
		PlayerOperationKind MyKind{PlayerOperationKind::DP};
		EffectAmount MyAmount{};
		std::string MyBond{};
	};

	struct ObstacleOperation
	{
		bool MyEnabled{true};
		ObstacleKind MyKind{ObstacleKind::BLOCK};
	};

	struct PushOperation
	{
		double MyForce{};
		bool MyPull{};
		bool MyEffect{};
	};

	struct SkillOperation
	{
		double MyAmmo{};
		double MyDuration{};
		int MyCharges{};
	};

	struct CustomOperation
	{
		ComponentReference MyReference{};
	};

	struct EventOperation
	{
		double MyMultiplier{1};
		EffectAmount MyAddition{};
		bool MyCancel{};
	};

	using EffectOperation = std::variant<DamageOperation, HealOperation, HealthLossOperation, SpOperation, StatusOperation,
		BuffOperation, ShieldOperation, ElementOperation, PlayerOperation, ObstacleOperation, PushOperation, SkillOperation, EventOperation, CustomOperation, StrongestBuffOperation>;

	class OperationBase
	{
	public:
		virtual ~OperationBase() = default;
		virtual void Apply(Battle& _battle, const EffectContext& _context, const EffectTarget& _target) const = 0;
	};

	struct EffectStep
	{
		SelectorDefinition MySelector{};
		std::vector<EffectOperation> MyOperations{};
	};

	// 每步冻结选择结果，按目标／操作顺序执行；嵌套回调使用各自的复用工作区。
	struct EffectProgram
	{
		std::vector<EffectStep> MySteps{};
	};

	enum class MechanismScope { SOURCE, OWNER, GLOBAL };

	struct MechanismDefinition
	{
		ContentEventKind MyEvent{ContentEventKind::TICK};
		MechanismScope MyScope{MechanismScope::SOURCE};
		SelectorDefinition MyCondition{};
		bool MyNeedsTarget{};
		double MyDelay{};
		double MyInterval{};
		bool MyRequiresSource{true};
		bool MyGarrison{};
		bool MyDuringSkill{};
		EffectProgram MyEffects{};
	};

	class EffectExecutor final
	{
	public:
		static void Select(const Battle& _battle, const EffectContext& _context, const SelectorDefinition& _selector, std::vector<EffectTarget>& _output);
		static void Apply(Battle& _battle, const EffectContext& _context, const EffectTarget& _target, const EffectOperation& _operation);
		static void Execute(Battle& _battle, const EffectContext& _context, const EffectProgram& _program);
		static void Execute(Battle& _battle, const EffectContext& _context, const SelectorDefinition& _selector, std::span<const EffectOperation> _operations);
		[[nodiscard]] static double Amount(const Battle& _battle, const EffectContext& _context, const EffectTarget& _target, const EffectAmount& _amount);
	};
}
#endif
