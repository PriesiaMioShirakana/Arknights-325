#ifndef STRONGHOLD_SIMULATION_COMBAT_TYPES_HPP
#define STRONGHOLD_SIMULATION_COMBAT_TYPES_HPP
#include <stronghold/simulation/choice_effects.hpp>
#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <limits>
#include <functional>
#include <stronghold/simulation/content_tags.hpp>
#include <string>
#include <stronghold/domain/board.hpp>
#include <stronghold/domain/range.hpp>
#include <stronghold/simulation/attributes.hpp>
#include <stronghold/simulation/field.hpp>
#include <stronghold/domain/combat_math.hpp>
#include <vector>

namespace Stronghold
{
	class ContentRegistry;
	class FinalAssault;
	using UnitId = std::uint32_t;

	struct WorldPoint
	{
		double MyX{};
		double MyY{};
	};

	// 巨型单位受击矩形，尺寸和偏移均以格为单位；没有此字段的单位按中心点判定。
	struct HitArea
	{
		double MyWidth{};
		double MyHeight{};
		double MyOffsetX{};
		double MyOffsetY{};
	};

	// 推力允许点源、方向和两种特殊修正；坐标／方向非有限分量按原规则退回默认值。
	// MyFromFacing 仅用于来源与目标重合时的朝向回退，不参与方向推力的角度修正。
	struct PushOptions
	{
		std::optional<WorldPoint> MyFrom{};
		WorldPoint MyDirection{};
		std::optional<Facing> MyFromFacing{};
		bool MyFixed{};
		bool MyFixedAngle{};
		bool MyInward{};
		bool MyEffect{};
	};

	struct PullOptions
	{
		WorldPoint MyTo{};
		std::optional<WorldPoint> MyCenter{};
		double MyStopRadius{0.6708};
		// 非零时中心取该单位当前位置；它已阻挡目标时保持接触，不释放阻挡。
		UnitId MyCenterUnit{};
	};

	[[nodiscard]] inline double Distance(WorldPoint _a, WorldPoint _b) noexcept
	{ return std::hypot(_a.MyX - _b.MyX, _a.MyY - _b.MyY); }

	[[nodiscard]] constexpr RangeOffset RotateOffset(RangeOffset _offset, Facing _facing) noexcept
	{
		switch (_facing)
		{
		case Facing::UP: return {_offset.MyColumn, 0 - _offset.MyRow};
		case Facing::DOWN: return {0 - _offset.MyColumn, _offset.MyRow};
		case Facing::LEFT: return {0 - _offset.MyRow, 0 - _offset.MyColumn};
		default: return _offset;
		}
	}

	enum class UnitSide
	{
		ALLY,
		ENEMY
	};

	enum class UnitKind { OPERATOR, TOKEN, DEVICE, ENEMY };
	inline constexpr std::size_t NoPlayer = std::numeric_limits<std::size_t>::max();

	enum class TargetPriority
	{
		DEFAULT,
		FLYING,
		LOW_DEFENSE,
		LOWEST_HEALTH,
		HIGHEST_HEALTH,
		NEAREST,
		FARTHEST,
		HIGH_DEFENSE,
		RANGED,
		LOWEST_HEALTH_RATIO,
		HIGHEST_ATTACK,
		BOSS,
		ELITE,
		NOT_BURST,
		GROUND,
		HEAVIEST
	};

	enum class RouteStepKind
	{
		MOVE,
		WAIT,
		DISAPPEAR,
		APPEAR
	};

	enum class BattleEndReason
	{
		RUNNING,
		CLEARED,
		TIMEOUT,
		FORCED
	};

	enum class BattleEventKind
	{
		DEPLOYED,
		SPAWNED,
		BLOCKED,
		ATTACKED,
		DAMAGED,
		HEALED,
		DIED,
		LEAKED,
		FINISHED,
		STATUS_APPLIED,
		STATUS_REMOVED,
		COINS_GAINED,
		LAYERS_GAINED
	};

	enum class CombatStatus
	{
		STUN,
		DISARM,
		NO_MOVE,
		UNBLOCKABLE,
		FREEZE,
		COLD,
		SLEEP,
		SLOW,
		SLUGGISH,
		BIND,
		FRAGILE,
		ARTS_FRAGILE,
		PHYSICAL_FRAGILE,
		ELEMENTAL_FRAGILE,
		SILENCE,
		FEAR,
		TREMBLE,
		STEALTH,
		CAMOUFLAGE,
		REVEAL,
		INVULNERABLE,
		LEVITATE,
		GROUNDBIND,
		PALSY,
		TAUNT,
		WEAKEN,
		ATTACK_SPEED_DOWN,
		DEFENSE_DOWN,
		RESISTANCE_DOWN,
		ATTRACT,
		RESIST,
		NO_HEAL,
		HEAL_FREE,
		UNTARGETABLE,
		NO_SP,
		NO_BLOCK,
		NO_DISPLACE,
		FLOAT,
		LIFTOFF,
		ISOLATED,
		SELF_BOUND,
		BLOCK_FLYING,
		BURST_LOCK,
		HIT_COUNT,
		HIT_COUNT_ARTS,
		COUNT
	};

	struct StatusTail
	{
		double MyEnd{};
		double MyValue{};
		double MyStrength{};
	};

	struct StatusApplication
	{
		double MyDuration{std::numeric_limits<double>::infinity()};
		UnitId MySource{};
		std::optional<double> MyValue{};
		std::optional<double> MyStackAs{};
		bool MyForce{};
		bool MyResistApplied{};
		std::optional<WorldPoint> MyPoint{};
	};

	struct FearStamp
	{
		std::uint64_t MySequence{};
		WorldPoint MyHit{};
		WorldPoint MySource{};
		bool MySelf{};
	};

	struct CombatStatuses
	{
		std::array<double, static_cast<std::size_t>(CombatStatus::COUNT)> MyRemaining{};
		std::array<double, static_cast<std::size_t>(CombatStatus::COUNT)> MyValues{};
		std::array<double, static_cast<std::size_t>(CombatStatus::COUNT)> MyStrengths{};
		std::array<std::optional<StatusTail>, static_cast<std::size_t>(CombatStatus::COUNT)> MyTails;
		std::optional<WorldPoint> MyAttractPoint{};
		double MyResistAccumulator{};
		std::optional<FearStamp> MyFear{};
		double MyStealthOffUntil{};
		std::bitset<static_cast<std::size_t>(CombatStatus::COUNT)> MyBuffFlags;

		[[nodiscard]] bool Has(CombatStatus _status) const noexcept
		{
			const auto index = static_cast<std::size_t>(_status);
			return index < MyRemaining.size() && (std::isgreater(MyRemaining[index], 0) || MyBuffFlags[index]);
		}
	};


	using StatusFlags = std::bitset<static_cast<std::size_t>(CombatStatus::COUNT)>;

	enum class BuffRefresh
	{
		REPLACE,
		EXTEND,
		STACK,
		INDEPENDENT,
		KEEP
	};

	// 静态位标记让内置与自定义效果区分环境/持续伤害，不在伤害热路径比较字符串。
	enum class DamageTag : std::uint32_t { TERRAIN = 1, DOT = 2, PERIODIC = 4, DEEPSEA = 8, TALENT = 16, AFTERSHOCK = 32, HP_LOSS = 64 };
	using DamageTags = std::uint32_t;
	[[nodiscard]] constexpr DamageTags operator|(DamageTag _left, DamageTag _right) noexcept
	{ return static_cast<DamageTags>(_left) | static_cast<DamageTags>(_right); }
	[[nodiscard]] constexpr DamageTags operator|(DamageTags _left, DamageTag _right) noexcept
	{ return _left | static_cast<DamageTags>(_right); }
	[[nodiscard]] constexpr bool HasTag(DamageTags _tags, DamageTag _tag) noexcept
	{ return (_tags & static_cast<DamageTags>(_tag)) != 0; }

	struct DamageInfo
	{
		double MyAmount{};
		DamageType MyType{DamageType::PHYSICAL};
		double MyMultiplier{1};
		double MyDefenseIgnorePercent{};
		double MyDefenseIgnoreFlat{};
		double MyResistanceIgnorePercent{};
		double MyResistanceIgnoreFlat{};
		bool MyCanDodge{true};
		bool MySourceless{};
		bool MyIgnoreSelect{};
		bool MyHitSleep{};
		bool MyNoSp{}; // 多段攻击后续段、生命流失等不能重复触发受击回复。
		DamageTags MyTags{};
		bool MyIsAttack{};
		UnitId MyTraitAlly{}; // 咒愈师的指定治疗对象；零表示选范围内生命比例最低者。
		bool MySilent{};
	};

	// 再生绕过治疗倍率和禁疗（healFree），但仍通知回调；溢出盾上限为目标当前最大生命。
	struct HealOptions
	{
		bool MySelf{};
		bool MyRegen{};
		bool MyIgnoreHealFree{};
		bool MyThrough{};
		bool MyOverheal{};
		double MyOverhealDuration{std::numeric_limits<double>::infinity()};
		bool MySilent{};
	};

	enum class BuffEffectKind
	{
		DAMAGE,
		HEAL,
		HEALTH_LOSS
	};

	struct BuffEffect
	{
		BuffEffectKind MyKind{};
		double MyAmount{};
		DamageType MyDamageType{DamageType::TRUE_DAMAGE};
		bool MyPerSecond{};
		bool MyCanDodge{true};
		bool MySourceless{};
		bool MyNoSp{};
		DamageTags MyTags{};
	};

	// 枚举顺序也是显示优先级：同满度时神经、侵蚀、灼燃、凋亡、旧版 necrosis。
	enum class Element { NEURAL, EROSION, BURN, APOPTOSIS, NECROSIS, COUNT };

	struct ElementHit
	{
		Element MyElement{};
		double MyAmount{};
		double MyMultiplier{1};
		bool MyHitSleep{};
		bool MyIgnoreSelect{};
		bool MySourceless{};
	};

	struct ElementState
	{
		std::array<double, static_cast<std::size_t>(Element::COUNT)> MyGauges{};
		bool MyBurstPending{}; // 爆发回调期间立即锁定，防止同一单位重入爆发。
	};

	struct ElementDisplay
	{
		Element MyElement{};
		double MyFill{};
		double MyCooldownEnd{};
		double MyCooldown{};
	};

	// 内置到期行为静态分派；自定义 Buff 仍只通过 CUSTOM_BUFF 扩展。
	enum class BuiltinBuff { NONE, DOLL_SWITCH, DOLL_FORM };

	struct BuffDefinition
	{
		std::string MyKey{};
		UnitId MySource{};
		double MyDuration{std::numeric_limits<double>::infinity()};
		unsigned MyStacks{1};
		unsigned MyMaxStacks{1};
		BuffRefresh MyRefresh{BuffRefresh::REPLACE};
		std::optional<std::vector<AttributeChange>> MyModifiers{};
		std::optional<StatusFlags> MyFlags{};
		bool MyPersistent{};
		double MyInterval{};
		std::vector<BuffEffect> MyTickEffects{};
		std::vector<BuffEffect> MyExpireEffects{};
		std::vector<BuffEffect> MyRemoveEffects{};
		Shield MyShield{};
		ContentReference MyContent{};
		std::optional<Element> MyElementBurst{};
		std::optional<double> MyStealthRestore{};
		BuiltinBuff MyBuiltin{};
		bool MyAllowDead{}; // 战斗开始的再部署修正等效果可显式作用于退场单位。
	};

	struct CombatBuff
	{
		BuffDefinition MyDefinition{};
		std::uint64_t MyId{};
		double MyRemaining{};
		double MyAccumulator{};
	};

	// 职业的目标相关攻击倍率只修正主目标；溅射／连锁使用各自定义，不继承这次主目标的倍率。
	enum class AttackScaling { NONE, FLYING, UNBLOCKED, DISTANT, FRONT, HUNTER, FUNNEL, REINFORCEMENT };

	// 内置职业状态以值组合到单位；不创建回调对象，不经过 CUSTOM 注册表。
	enum class ProfessionTrait { NONE, HUNTER, FUNNEL, MYSTIC, PHALANX, BEARER, STALKER, MUSHA, REAPER, INCANTATION, CHARGER, GEEK, MERCHANT, LIBRATOR, BARD, LOOPSHOOTER, BOMBARDER, TACTICIAN, SKYWALKER, DOLLKEEPER };

	struct ProfessionDefinition
	{
		ProfessionTrait MyKind{};
		double MyAmmoMax{8};
		double MyFunnelInitial{0.2};
		double MyFunnelDelta{0.15};
		double MyFunnelMax{1.1};
		unsigned MyStoreMax{3};
		double MyGuardDefense{2};
		double MyGuardResistance{20};
		double MyDodge{0.5};
		double MySelfHeal{50};
		double MyHealRatio{0.5};
		double MyDpOnKill{1};
		double MyHpDrain{0.03};
		double MyMerchantInterval{3};
		double MyMerchantCost{3};
		double MyRampMax{2};
		double MyRampTime{40};
		double MyRampInitial{};
		double MyAuraRatio{0.1};
		double MyShockScale{0.5};
		unsigned MyShockCount{1};
		double MyDollDuration{20};
		double MyDollHealthMultiplier{1}; // 适配器按当前技能／模组选定该干员自己的替身数据。
		bool MyDollNoAttack{};
	};

	struct ProfessionState
	{
		double MyAmmo{};
		double MyReloadAccumulator{};
		UnitId MyFunnelTarget{};
		UnitId MyReinforcement{}; // 稳定 ID；已有援军存活时，拥有者再次部署不重复召唤。
		double MyFunnelScale{};
		unsigned MyStored{};
		std::uint64_t MyBoomerangsOut{};
		bool MyFortressMelee{};
		bool MyDoll{};
		bool MyDollSwitching{};
		double MyRamp{};
		std::uint64_t MySelfHealTick{std::numeric_limits<std::uint64_t>::max()};
		unsigned MySelfHealCount{};
		std::string MyAuraKey{}; // 吟游者初始化时生成一次；周期刷新不重新拼接 ID。
	};

	// Resolved base attack data. Profession traits and skills supply their own profiles later.
	struct AttackProfile
	{
		DamageType MyDamageType{DamageType::PHYSICAL};
		bool MyDisabled{};
		bool MyHealing{};
		bool MyCanHitFlying{};
		bool MyBlockFlying{};
		bool MyRanged{};
		bool MyAttackWhileMoving{};
		std::size_t MyMaxTargets{1};
		TargetPriority MyPriority{TargetPriority::DEFAULT};
		double MyProjectileSpeed{}; // Allies: zero means instant. Ranged enemies use upstream speed 10.
		double MyEnemyRange{};
		double MyAnimationDuration{};
		std::optional<double> MyAnimationHit{};
		bool MyHitSleep{};
		bool MyGroundOnly{};
		bool MyNoHeal{};
		double MyAttackScale{1};
		double MyDamageMultiplier{1};
		std::optional<double> MyHitMultiplier{}; // 在防御后乘算；拆分伤害只有首段回复受击 SP。
		double MyHealScale{1};
		unsigned MyHits{1};
		bool MyAllInRange{};
		double MySplashRadius{};
		double MySplashScale{1};
		bool MySplashHitsFlying{};
		unsigned MyChainCount{1};
		double MyChainRadius{1.7};
		double MyChainFalloff{0.15};
		double MyChainSluggish{};
		unsigned MyHealChainCount{1};
		double MyHealChainFalloff{0.25};
		double MyElementHealRatio{};
		double MyHealHpAtMost{1};
		std::optional<CombatStatus> MyOnHitStatus{};
		StatusApplication MyOnHitApplication{};
		bool MyOnlyDuringSkill{}; // 解放者／阵法术师等：技力照常恢复，技能外不普攻。
		bool MyHitAllBlocked{}; // 强攻手等的目标上限取当前阻挡数，至少为一。
		AttackScaling MyScaling{};
		double MyConditionalScale{1};
		double MyHealFarMultiplier{1};
		double MyHealNearDistance{2};
		bool MyBoomerang{};
		bool MyFortress{};
	};

	// 技能状态机是普通值类型；内置逻辑不经过注册表或虚函数。
	enum class SkillKind { NONE, DURATION, AMMO, INSTANT, CHARGES, PASSIVE, TOGGLE };
	enum class SpType { TIME, ATTACK, HURT, NONE };
	enum class SpReason { TIME, ATTACK, HURT, GRANTED, INITIAL };
	enum class SkillTrigger { DEFAULT, SP_FULL, SEARCH, CUSTOM_RANGE, SKILL_RANGE, ACTIVE_RANGE, GLOBAL, TAKE_DAMAGE, NEVER };
	enum class SkillReason { MANUAL, TRIGGER, DEPLOY, PASSIVE, DURATION, AMMO, INSTANT, WITHDRAWN, STOPPED, DEATH, SUBSTITUTE };

	struct SkillDefinition
	{
		SkillKind MyKind{SkillKind::NONE};
		SpType MySpType{SpType::TIME};
		SkillTrigger MyTrigger{SkillTrigger::DEFAULT};
		double MySpCost{};
		double MyInitialSp{};
		unsigned MyMaxCharges{1};
		double MyDuration{};
		double MyAmmo{};
		bool MyManual{true};
		bool MyActivateOnDeploy{};
		bool MyHealSkill{};
		bool MyTriggerAllies{};
		double MyTriggerHpAtMost{1};
		std::vector<RangeOffset> MyTriggerRange{};
		std::vector<RangeOffset> MyRange{};
		int MyRangeExtend{};
		bool MyNoRangeExtend{};
		// 适配器预先合并基础和技能配置，热路径直接引用完整配置，不逐字段构建临时对象。
		std::optional<AttackProfile> MyAttack{};
		std::vector<AttributeChange> MyModifiers{};
		StatusFlags MyFlags{};
	};

	// 来源单位为零时使用绝对格掩码；否则实时读取该友军当前射程，离场／隐藏时不生效。
	// 只保存稳定 ID 和固定大小掩码，不为召唤物范围注册捕获 this 的函数对象。
	struct SkillTriggerArea
	{
		UnitId MySourceUnit{};
		std::bitset<FieldTiles> MyMask{};
		bool MyCanHitFlying{true};
		bool MyHitSleep{};
		bool MyGroundOnly{};
	};

	struct SkillTriggerRange
	{
		std::uint64_t MyId{};
		SkillTriggerArea MyArea{};
	};

	struct SkillState
	{
		double MySp{};
		unsigned MyCharges{};
		bool MyActive{};
		bool MyPending{};
		double MyTimeLeft{};
		double MyAmmoLeft{};
		double MyAmmoMax{};
		double MySpCostMultiplier{1};
		double MyOperationReadyAt{-std::numeric_limits<double>::infinity()};
		double MyLastStart{-std::numeric_limits<double>::infinity()};
		std::uint64_t MyActivations{};
		std::uint64_t MyBuff{};
		std::bitset<399> MyTriggerMask{};
		std::vector<SkillTriggerRange> MyExtraRanges{};
	};

	[[nodiscard]] constexpr bool IsTimedSkill(SkillKind _kind) noexcept
	{
		return _kind == SkillKind::DURATION || _kind == SkillKind::AMMO || _kind == SkillKind::TOGGLE;
	}

	struct CombatDefinition
	{
		std::string MyId{};
		CombatStats MyStats{};
		AttackProfile MyAttack{};
		std::vector<RangeOffset> MyRange{{0, 0}, {0, 1}}; // Authored facing RIGHT.
		bool MyFlying{};
		int MyBlockWeight{1};
		bool MyStunImmune{};
		StatusFlags MyImmunities{};
		bool MyLeader{};
		ContentReference MyContent{};
		SkillDefinition MySkill{};
		bool MyWalksWhileFlying{};
		bool MyStaticBody{};
		// 共享首领池实体与敌人 BOSS 阶级不同；只有前者免疫位移。
		bool MySharedBoss{};
		bool MyElite{};
		std::optional<HitArea> MyHitArea{};
		std::optional<std::vector<RangeOffset>> MyTraitFrontRange{}; // 缺省用朝向的正前方整行；空表表示无强化格。
		ProfessionDefinition MyProfession{};
	};

	// The route adapter supplies resolved waypoints. This engine does not invent paths through terrain.
	struct RouteStep
	{
		RouteStepKind MyKind{RouteStepKind::MOVE};
		WorldPoint MyPosition{};
		double MyWaitSeconds{};
	};

	struct CombatRoute
	{
		WorldPoint MyStart{};
		std::vector<RouteStep> MySteps{};
		WorldPoint MyEnd{};
	};

	// 联防仅继承生命比例、技力和退场标记；技能活动状态、弹药、Buff 不跨场传递。
	// 缺省／非有限数值使用本场初始值，有限数值按原规则截断。
	struct CarryState
	{
		std::optional<double> MyHealthRatio{};
		std::optional<double> MySp{};
		bool MyDown{};
	};

	struct AllyDeployment
	{
		std::uint64_t MyPieceUid{};
		CombatDefinition MyDefinition{};
		WorldPoint MyPosition{};
		Facing MyFacing{Facing::RIGHT};
		bool MyGround{true};
		bool MyGroundPassable{true}; // Fenced ground tiles cannot block walking enemies.
		UnitKind MyKind{UnitKind::OPERATOR}; // 初始棋盘仅接收 OPERATOR／TOKEN。
		std::optional<CarryState> MyCarry{};
		bool MyDeferred{}; // 保留格子，等待内容显式调用 Redeploy；不启动自动再部署。
		std::uint64_t MyOwnerPieceUid{}; // 可选；同一玩家的拥有者；允许召唤物在输入中位于拥有者之前。
	};

	// 盟约层数是每局可变值；具体盟约效果读取它，不共享跨战斗静态状态。
	struct BondLayer
	{
		std::string MyId{};
		double MyLayers{};
		std::int64_t MyCount{};
		bool MyActive{};
		unsigned MyTier{};
	};

	struct LayerGainOptions
	{
		UnitId MySource{};
		std::optional<WorldPoint> MyTile{};
		std::string_view MyReason{}; // 仅本次调用/同步内容回调借用。
	};

	struct BattlePlayerInput
	{
		std::string MyPlayerId{};
		std::vector<AllyDeployment> MyUnits{}; // Stable input order controls ally updates, independently of deployment.
		bool MyMirrorDeployment{};
		std::vector<BondLayer> MyBonds{};
		std::optional<bool> MyRightHalf{}; // 联防右半场不镜像部署；未指定时沿用镜像标志。
		std::vector<BattleChoiceEffect> MyChoiceEffects{};
		std::optional<std::size_t> MyHandUnits{}; // 无显式准备条件时，旧输入可提供手牌数量回退。
	};

	enum class EnemySpawnTag { NONE, BOSS, PART, BOUNTY };

	// 已施加到基础定义的出生修正原样保存，供漏怪重建和频次敌人读取；不会在核心内重复乘算。
	struct EnemySpawnModifiers
	{
		std::optional<double> MyHealth{};
		std::optional<double> MyAttack{};
		std::optional<double> MyDefense{};
		std::optional<double> MyResistance{};
		std::optional<double> MySpeed{};
		std::optional<double> MySupplyHealth{};
		std::string MySlot{};
		std::string MyBountyId{};
		std::optional<std::int64_t> MyBountyCoins{}; // 内容设置、无卡片 ID 的联防赏金。
	};

	struct BountyReward
	{
		double MyCoins{};
		std::string MyOwnerId{}; // 可为不在当前联防战斗中的原玩家，支付时按原规则回退。
	};

	struct LeakedEnemy
	{
		std::string MyEnemyId{};
		std::optional<EnemySpawnModifiers> MyModifiers{};
		int MyLifeCost{1};
		std::string MySourcePlayer{};
		EnemySpawnTag MyTag{};
		bool MyCounted{};
		bool MyBoss{};
	};

	struct PendingEnemy
	{
		std::string MyEnemyId{};
		double MyTime{};
		EnemySpawnTag MyTag{};
		std::string MySourcePlayer{};
	};

	struct EnemySpawn
	{
		double MyTime{};
		std::string MyOwnerId{};
		CombatDefinition MyDefinition{};
		CombatRoute MyRoute{};
		int MyLifeCost{1};
		bool MyCounted{true};
		EnemySpawnTag MyTag{};
		std::optional<EnemySpawnModifiers> MyModifiers{};
		std::string MySourcePlayer{};
		std::optional<BountyReward> MyBounty{};
	};

	// 适配器提供已解析的召唤物定义；核心持有它的值，不借用临时加载结果。
	struct TokenSpawn
	{
		CombatDefinition MyDefinition{};
		WorldPoint MyPosition{};
		UnitId MyOwnerUnit{};
		std::string MyPlayerId{};
		std::optional<Facing> MyFacing{};
		std::optional<double> MyHealth{};
		double MyDuration{};
		bool MyUntargetable{};
	};

	struct DeviceSpawn
	{
		std::string MyId{};
		WorldPoint MyPosition{};
		double MyHealth{100};
		double MyDefense{};
		double MyResistance{};
		int MyBlockCount{99};
		bool MyObstacle{};
		ObstacleKind MyObstacleKind{ObstacleKind::CRATE};
		double MyAttack{};
		double MyAttackTime{1};
		double MyAttackSpeed{100};
		bool MyRemoveOnDeploy{};
		bool MyAfterDeployment{};
	};

	struct TurretSpawn
	{
		std::string MyAlias{};
		DeviceSpawn MyDevice{};
		std::string MyPlayerId{};
		std::bitset<FieldTiles> MyRange{}; // 已解析的绝对格范围，移动装置不旋转这个范围。
		double MyAttackSpeedPerLayer{};
		double MyMaxAttackSpeedBonus{std::numeric_limits<double>::infinity()};
		double MyFragilityPerLayer{};
		double MyMaxDamageScale{std::numeric_limits<double>::infinity()};
		double MyFragilityDuration{};
	};

	// 内置寒风采用层数线性公式；每阵风重新读取盟约，不捕获战斗对象或外部回调。
	// 空玩家表示全场；指定盟约时必须指定玩家。首次延迟缺省为间隔，负值按零处理。
	struct ColdWindDefinition
	{
		std::string MyPlayerId{};
		std::string MyBondId{};
		double MyInterval{};
		double MyBaseDuration{};
		double MyDurationPerLayer{};
		std::optional<double> MyFirstDelay{};
		bool MyOwnerOnly{};
	};

	enum class RemovalReason { KILLED, RETREAT, EXPIRED, LEAK, REMOVED, FORCED_EXIT, MERCHANT };

	struct ContentBinding
	{
		ContentReference MyContent{};
		std::string MyPlayerId{};
	};

	struct BattleInput
	{
		std::vector<BattlePlayerInput> MyPlayers{};
		std::vector<EnemySpawn> MySpawns{};
		double MyTimeLimit{60};
		double MyInitialDp{10};
		double MyDpPerSecond{1};
		double MyMaxDp{99};
		bool MyAutoFinish{true};
		std::uint32_t MySeed{1};
		bool MyBossBattle{};
		// 注册表由宿主持有并已 Seal；必须比本局活得更久，期间不得移动或重新赋值。
		std::optional<std::reference_wrapper<const ContentRegistry>> MyContentRegistry{};
		std::vector<ContentBinding> MyContentBindings{};
		std::optional<FieldDefinition> MyField{};
		std::vector<DeviceSpawn> MyDevices{};
		std::vector<TurretSpawn> MyTurrets{};
		std::vector<ColdWindDefinition> MyColdWinds{};
		// 宿主拥有血池并串行推进共享它的各场 Battle；血池必须比所有使用者活得更久且不得移动。
		// 无 shared_ptr／锁／系统调度依赖，后续比赛层可在这个显式边界组织多场战斗。
		std::optional<std::reference_wrapper<SharedBossPool>> MySharedBoss{};
		// 已由波次适配器筛选的非飞行道路；用于战术点，不从显式出生路线猜测道路归属。
		std::vector<CombatRoute> MyGroundRoutes{};
		// 可选的比赛层首领控制器；普通战场无此成本。与 MySharedBoss 同时给出时必须指向同一血池。
		// 每次命中和漏怪立即通知，不能等 DrainEvents 后再决定胜负，否则同帧先后顺序会丢失。
		std::optional<std::reference_wrapper<FinalAssault>> MyFinalAssault{};
		std::optional<bool> MyLayerGainsEnabled{}; // 缺省普通战斗开启、首领战关闭；联防输入应显式关闭。
	};

	struct UnitCombatTotals
	{
		double MyDamage{};
		double MyHealing{};
		double MyTaken{};
		std::uint64_t MyAttacks{};
		std::uint64_t MyKills{};
		double MyElementDamage{};
	};

	struct TerrainState
	{
		FieldTerrain MyTerrain{};
		double MySince{};
		unsigned MyMireStacks{};
		double MyMireTriggers{};
		double MyAirAttack{};
		double MyAirMultiplier{1};
		std::optional<WorldPoint> MyAirPosition{};
	};

	struct CombatUnit
	{
		UnitId MyId{};
		UnitSide MySide{};
		std::uint64_t MyPieceUid{};
		std::size_t MyOwner{};
		UnitKind MyKind{UnitKind::OPERATOR};
		EnemySpawnTag MySpawnTag{};
		UnitId MyOwnerUnit{};
		bool MyRemoved{};
		bool MyRemoving{};
		std::optional<WorldPoint> MyBody{};
		bool MyDownAtHome{};
		RemovalReason MyRemovalReason{RemovalReason::KILLED};
		double MyExpiresAt{std::numeric_limits<double>::infinity()};
		bool MyObstacle{};
		ObstacleKind MyObstacleKind{ObstacleKind::CRATE};
		CombatDefinition MyDefinition{};
		WorldPoint MyPosition{};
		WorldPoint MyHome{};
		Facing MyFacing{Facing::RIGHT};
		bool MyGround{true};
		bool MyGroundPassable{true};
		bool MyAlive{};
		bool MyHidden{};
		bool MyMoving{};
		bool MySwing{};
		double MyHealth{};
		double MyAttackCooldown{};
		double MyAttackStandUntil{};
		double MyRespawnAt{std::numeric_limits<double>::infinity()};
		double MyRemovedAt{-std::numeric_limits<double>::infinity()};
		UnitId MyBlockedBy{};
		std::vector<UnitId> MyBlocking{};
		std::uint64_t MyDeploySequence{}; // 一次部署的身份，用于弹道和内容生命周期判断。
		std::uint64_t MyAggroSequence{}; // 初始召唤物在所有干员之后；不改变部署身份。
		std::uint64_t MySpawnSequence{};
		std::size_t MySpawnIndex{};
		std::size_t MyRouteIndex{};
		std::optional<double> MyWaitLeft{};
		std::vector<Shield> MyShields{};
		std::bitset<399> MyRangeMask{};
		CombatStats MyStats{};
		std::vector<CombatBuff> MyBuffs{};
		double MyRegenAccumulator{};
		CombatStatuses MyStatuses{};
		UnitCombatTotals MyTotals{};
		SkillState MySkill{};
		std::bitset<399> MyBaseRangeMask{};
		std::bitset<FieldTiles> MyTraitFrontMask{};
		ProfessionState MyProfession{};
		double MyLastAttackAt{-std::numeric_limits<double>::infinity()};
		ElementState MyElements{};
		std::vector<WorldPoint> MyPath{};
		std::vector<double> MyPathSuffix{};
		std::size_t MyPathPoint{};
		std::size_t MyPlannedRoute{std::numeric_limits<std::size_t>::max()};
		std::uint64_t MyPathVersion{};
		CombatStatus MyInducedMode{CombatStatus::COUNT};
		std::vector<WorldPoint> MyInducedPath{};
		std::size_t MyInducedPoint{};
		std::uint64_t MyInducedVersion{};
		WorldPoint MyInducedLast{};
		WorldPoint MyInducedGoal{};
		int MyInducedTile{};
		std::vector<int> MyFearTiles{};
		std::uint64_t MyFearSequence{};
		TerrainState MyTerrainState{};
		std::optional<CarryState> MyCarry{};
		bool MyDeferred{};

		[[nodiscard]] bool Flying() const noexcept
		{
			return MySide == UnitSide::ENEMY
				? MyStatuses.Has(CombatStatus::LEVITATE) || (!MyStatuses.Has(CombatStatus::GROUNDBIND) &&
					(MyDefinition.MyFlying || MyStatuses.Has(CombatStatus::FLOAT)))
				: MyDefinition.MyFlying;
		}
	};

	// Result() 的值快照。SP 包含储存的充能，四舍五入到百分之一；活动技能只报告，不继承。
	struct UnitEndState
	{
		std::uint64_t MyPieceUid{};
		UnitId MyId{};
		std::string MyDefinitionId{};
		UnitKind MyKind{};
		double MyHealthRatio{};
		double MySp{};
		bool MySkillActive{};
		bool MyAlive{};

		[[nodiscard]] CarryState Carry() const noexcept
		{
			return CarryState{.MyHealthRatio = MyHealthRatio, .MySp = MySp, .MyDown = !MyAlive};
		}
	};

	struct BattlePlayerState
	{
		std::string MyPlayerId{};
		std::vector<BondLayer> MyBonds{};
		double MyTopBondLayers{};
		double MyDp{};
		std::size_t MyTotal{};
		std::size_t MyKilled{};
		std::size_t MyLeaked{};
		std::size_t MyDeaths{};
		std::int64_t MyLifeLost{}; // Raw sum; match-level LP caps belong to round settlement.
		double MyDamage{};
		double MyHealing{};
		double MyBossDamage{};
		bool MyPerfect{true};
		double MyCoins{};
		std::vector<LeakedEnemy> MyLeaks{};
		std::vector<UnitEndState> MyUnitsEnd{}; // 仅 Result() 填充；不在每帧复制。
		std::vector<BondLayer> MyLayerGains{}; // 本场累计收益，与可修改的实时层数分开；索引只追加不重排。
	};

	struct BattleEvent
	{
		BattleEventKind MyKind{};
		std::uint64_t MyTick{};
		UnitId MySource{};
		UnitId MyTarget{};
		double MyAmount{};
		std::optional<CombatStatus> MyStatus{};
		std::size_t MyPlayer{NoPlayer}; // COINS_GAINED 使用玩家索引，不伪装为单位 ID。
		std::size_t MyBondIndex{NoPlayer}; // LAYERS_GAINED 指向该玩家 MyLayerGains 中的稳定条目。
	};

	struct BattleResult
	{
		BattleEndReason MyReason{BattleEndReason::RUNNING};
		double MyTime{};
		std::size_t MyKilled{};
		std::size_t MyLeaked{};
		std::size_t MyTotal{};
		std::size_t MyUnspawned{};
		std::vector<BattlePlayerState> MyPlayers{};
		std::optional<double> MyBossHealthLeft{};
		std::vector<PendingEnemy> MyPendingEnemies{}; // 超时时仍未出生的全部动作，含非计数实体。
	};
}
#endif
