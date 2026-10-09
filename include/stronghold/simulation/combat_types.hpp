#ifndef STRONGHOLD_SIMULATION_COMBAT_TYPES_HPP
#define STRONGHOLD_SIMULATION_COMBAT_TYPES_HPP
#include <deque>
#include <stronghold/simulation/bond_effects.hpp>
#include <stronghold/simulation/garrison_effects.hpp>
#include <stronghold/simulation/band_effects.hpp>
#include <stronghold/simulation/equipment_effects.hpp>
#include <stronghold/simulation/choice_effects.hpp>
#include <stronghold/simulation/operator_kits.hpp>
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
		UNDYING,
		WEIGHTLESS,
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
		bool MyReenter{};
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
	enum class DamageTag : std::uint64_t { TERRAIN = 1, DOT = 2, PERIODIC = 4, DEEPSEA = 8, TALENT = 16, AFTERSHOCK = 32, HP_LOSS = 64, ITEM = 128, STEAD_SHARE = 256, STEAD_THORN = 512, BOND = 1024, ADDITION = 2048, SKILL = 4096, BURST = 8192, COUNTER = 16384, REFLECT = 32768, CHAIN = 65536, ZONE = 131072, NECROSIS = 262144, APOPTOSIS = 524288, DRAG = 1048576, SONIC = 2097152, MODULE = 4194304, SUMMON = 8388608, TRAP = 16777216, PINECN_SPIKE = 33554432, SNHUNT = 67108864, CLOUDBEAST = 134217728, ELEMENTAL = 268435456, SLASH = 536870912, FIREWALL = 1073741824, SNOW = 2147483648, LINK = 4294967296ULL, ELEMENT_DAMAGE = 8589934592ULL, FIREBALL = 17179869184ULL, SCORCH = 34359738368ULL };
	using DamageTags = std::uint64_t;
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
		double MySteadCut{};
		bool MyIsSplash{};
		bool MyIsSkill{};
		std::uint64_t MySequence{};
		bool MyPrecisionCritical{};
		bool MyMlynarOwn{};
		bool MySunbrProc{}; // 同一次伤害的攻击前抽签结果；闪避／取消不会进入命中后的眩晕。
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
		bool MyHot{};
		bool MyAura{};
		bool MySkillHeal{};
		bool MyReflect{};
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
		bool MySourceAttackScale{};
		bool MyIsSkill{};
		bool MyTargetMaxHealthScale{};
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
		DamageTags MyTags{};
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
	enum class BuiltinBuff { NONE, DOLL_SWITCH, DOLL_FORM, SARGON_STACK, SIRACUSA_STEALTH, RMIXER_SHIELD };

	struct BuffStrengthTail
	{
		double MyValue{};
		double MyUntil{};
	};

	struct BuffStrength
	{
		double MyValue{};
		Attribute MyAttribute{};
		double MyScale{1};
		double MyOffset{};
		std::optional<BuffStrengthTail> MyTail{};
		std::optional<Attribute> MySecondAttribute{}; // 庇护等同一强度同时作用于两个属性，共用到期和较弱尾段。
	};

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
		std::optional<BuffStrength> MyStrength{};
		bool MyNotifyTick{}; // 内置内容显式订阅此 Buff 的 interval；普通 Buff 不广播周期事件。
		double MyShieldBreakSp{};
		std::optional<CombatStatus> MyStatus{}; // 状态来源 Buff 可由替身切换清除；普通属性增益保留。
	};

	struct CombatBuff
	{
		BuffDefinition MyDefinition{};
		std::uint64_t MyId{};
		double MyRemaining{};
		double MyAccumulator{};
	};

	// 职业的目标相关攻击倍率只修正主目标；溅射／连锁使用各自定义，不继承这次主目标的倍率。
	enum class AttackScaling { NONE, FLYING, UNBLOCKED, BLOCKED, DISTANT, FRONT, HUNTER, FUNNEL, REINFORCEMENT };

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
		bool MyGenericHit{}; // 随攻击配置保存到弹道；瞬发技能结束后仍执行该次命中回调。
		bool MyOperatorEachHit{};
		bool MyOperatorSkillHit{}; // 专属技能命中回调随攻击保存，落点处不重新检查技能是否仍开启。
		bool MyOnlyDuringSkill{}; // 解放者／阵法术师等：技力照常恢复，技能外不普攻。
		bool MyHitAllBlocked{}; // 强攻手等的目标上限取当前阻挡数，至少为一。
		AttackScaling MyScaling{};
		double MyConditionalScale{1};
		double MyHealFarMultiplier{1};
		double MyHealNearDistance{2};
		bool MyBoomerang{};
		bool MyFortress{};
		bool MySkillDamage{};
		DamageTags MyTags{};
		bool MyPerTargetFunnel{};
		bool MyLockedFunnel{};
		bool MyProgressiveHits{};
		std::optional<double> MyCriticalProbability{};
		double MyCriticalScale{1};
		double MyOperatorBonusScale{};
	};

	// 技能状态机是普通值类型；内置逻辑不经过注册表或虚函数。
	enum class SkillKind { NONE, DURATION, AMMO, INSTANT, CHARGES, PASSIVE, TOGGLE };
	enum class SpType { TIME, ATTACK, HURT, NONE };
	enum class SpReason { TIME, ATTACK, HURT, GRANTED, INITIAL, SKILL, TALENT, TRAIT };
	enum class SkillTrigger { DEFAULT, SP_FULL, SEARCH, CUSTOM_RANGE, SKILL_RANGE, ACTIVE_RANGE, GLOBAL, TAKE_DAMAGE, NEVER };
	enum class SkillReason { MANUAL, TRIGGER, DEPLOY, PASSIVE, DURATION, AMMO, INSTANT, WITHDRAWN, STOPPED, DEATH, SUBSTITUTE, NO_TARGET, TARGET, KILL, RECAST, DOWNED, ABNORMAL };

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

	struct GenericStatusEffect
	{
		CombatStatus MyStatus{};
		double MyDuration{};
	};

	// BUILT_IN 通用技能的构建期规则；借用的字符串和 spans 必须覆盖战场生命周期。
	struct GenericSkillEffects
	{
		std::string_view MyDebuffKey{};
		std::string_view MyShieldKey{};
		std::span<const GenericStatusEffect> MyHitStatuses{};
		std::span<const GenericStatusEffect> MyStartStatuses{};
		std::span<const AttributeChange> MyDebuff{};
		double MyProbability{1};
		double MySelfStun{};
		std::optional<Element> MyElement{};
		double MyElementRatio{};
		bool MyElementOfDamage{};
		bool MyHitElement{};
		std::optional<double> MyForce{};
		bool MyPull{};
		bool MyDirectional{};
		bool MyEffectPush{};
		double MyHealAlly{};
		bool MyHealOthersOnly{};
		bool MyHealAll{};
		double MyDebuffDuration{};
		bool MyAura{};
		double MyDp{};
		double MyLoseHp{};
		double MyHealHp{};
		double MyShield{};
		double MyShieldDuration{};
		bool MyShieldDecay{};
		double MyStartBurst{};
		double MyEndBurst{};
		std::optional<DamageType> MyBurstType{};
		unsigned MyStartTargets{};
		bool MyCounter{};
		double MyCounterScale{};
		bool MyCounterDefense{};
		DamageType MyCounterType{};
		bool MyCounterAround{};
		bool MyCounterGroundOnly{};
		double MyCounterCooldown{};
	};

	// 来源单位为零时使用绝对格掩码；否则读取该友军按开局位置模式计算的射程，离场／隐藏时不生效。
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

	struct UnitContentIdentity
	{
		std::string MyBaseChess{};
		unsigned MyTier{1};
		bool MyGolden{};
		bool MyMeleePosition{true};
		std::vector<std::string> MyBonds{};
		std::vector<std::string> MyItems{};
		std::vector<std::string> MyGarrisons{};
		std::string MyCharacterId{};
		std::string MyNationId{};
		std::string MyGroupId{};
	};

	// 医疗内容的数值在适配阶段解析；治疗与倒地事件只读取固定参数。
	struct MedicKitDefinition
	{
		double MyHealSp{};
		double MyDeathSp{};
		double MySkillHpRatio{};
		double MySkillHealMultiplier{1};
		double MySkillExtraHeal{};
	};

	struct YanyouKitDefinition
	{
		double MyRangeRadius{2};
		double MyMoveSpeed{0.5};
		double MyBurnRatio{};
		double MyFragileMultiplier{1};
		unsigned MyDeployLimit{2};
		double MyFlameScale{};
		double MyFlameRadius{};
	};

	enum class OperatorProfession { NONE, PIONEER, WARRIOR, TANK, SNIPER, CASTER, MEDIC, SUPPORT, SPECIAL };
	enum class TokenSource { NONE, SKILL, UNAVAILABLE };
	enum class TokenKitKind { HEAL_DRONE, PAPER_DOLL, CHAMPAGNE, CURSE_DOLL, WOLF_PACK, OBELISK, CAT_SHIELD, DUSK_DRAGON, ICE_TARGET, RADIANT_SWORD, GOLDEN_OATH, ROSMON_GEAR };

	struct WolfPackDefinition
	{
		double MyInterval{};
		double MyBlockPerShadow{1};
		unsigned MyMaxShadows{1};
		double MyDefenseIgnore{};
		double MyBlockedReduction{1};
		double MyAdditionScale{};
		double MyTaunt{};
		bool MyManaged{};
	};

	struct CatShieldDefinition
	{
		double MyIdle{};
		double MyInterval{1};
		double MyMaxRatio{};
		double MyRefill{};
	};

	struct TokenKitDefinition
	{
		TokenKitKind MyKind{};
		double MyLifetime{};
		unsigned MyDeployLimit{}; // 零表示没有数量上限。
		bool MyCountdown{};
		double MyBurstScale{};
		double MySluggish{};
		double MyMatureTime{std::numeric_limits<double>::infinity()};
		bool MyManagedBomb{};
		std::span<const AttributeChange> MyAuraModifiers{};
		std::optional<WolfPackDefinition> MyWolf{};
		std::span<const RangeOffset> MyBurstRange{};
		bool MyBurstSuppressed{};
		double MyBurstStun{};
		bool MyLifetimeSpecified{};
		std::optional<CatShieldDefinition> MyCatShield{};
		double MyBlockedScale{1};
		bool MyTrueDamage{};
		double MyBlockedDefense{};
	};

	struct TokenCountdown
	{
		double MyFrom{};
		double MyUntil{};
	};

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
		UnitContentIdentity MyIdentity{};
		std::vector<BuffDefinition> MyInitialBuffs{};
		std::optional<MedicKitDefinition> MyMedic{};
		std::optional<YanyouKitDefinition> MyYanyou{};
		std::vector<EquipmentEffect> MyEquipmentEffects{};
		std::vector<std::string> MyEnemyTags{};
		const GenericSkillEffects* MyGenericSkill{};
		const OperatorKitDefinition* MyOperatorKit{};
		OperatorProfession MyOperatorProfession{};
		std::optional<TokenKitDefinition> MyTokenKit{};
		std::optional<double> MyOriginalDeploymentCost{};
		std::optional<double> MyDeviceShieldRate{};
	};

	struct MapCharacterTile
	{
		FieldPoint MyPosition{};
		Facing MyFacing{Facing::RIGHT};
		bool MyMultiOnly{};
	};

	struct MapCharacterDefinition
	{
		CombatDefinition MyDefinition{};
		std::vector<MapCharacterTile> MyPositions{};
	};

	struct MapCharacterVariant
	{
		unsigned MyMinimumElites{};
		unsigned MyMaximumElites{std::numeric_limits<unsigned>::max()};
		std::vector<MapCharacterDefinition> MyCharacters{};
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
		TokenSource MyTokenSource{}; // 由拥有者当前技能／模组的 sources 解析，只管理棋盘召唤物。
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

	struct TokenTemplate
	{
		std::uint64_t MyOwnerPieceUid{};
		CombatDefinition MyDefinition{};
	};

	struct BattlePlayerInput
	{
		std::string MyPlayerId{};
		std::vector<AllyDeployment> MyUnits{}; // Stable input order controls ally updates, independently of deployment.
		bool MyMirrorDeployment{};
		std::vector<BondLayer> MyBonds{};
		std::optional<bool> MyRightHalf{}; // 联防右半场不镜像部署；未指定时沿用镜像标志。
		std::vector<BattleChoiceEffect> MyChoiceEffects{};
		std::vector<BattleBandEffect> MyBandEffects{};
		std::vector<AddonBondEffect> MyAddonBonds{};
		std::vector<CoreBondEffect> MyCoreBonds{};
		std::uint64_t MyGainedChess{}; // 当前准备回合获得干员数；天师古鼎等内容使用。
		std::optional<std::size_t> MyHandUnits{}; // 无显式准备条件时，旧输入可提供手牌数量回退。
		std::vector<TokenTemplate> MyTokenTemplates{}; // 已按本玩家干员的技能／模组组合，运行时不解析黑板。
	};

	enum class EnemySpawnTag { NONE, BOSS, PART, BOUNTY, DUCK };

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

	enum class RemovalReason { KILLED, RETREAT, EXPIRED, LEAK, REMOVED, FORCED_EXIT, MERCHANT, RAID };

	struct ContentBinding
	{
		ContentReference MyContent{};
		std::string MyPlayerId{};
	};

	// 仅改变友军的盟约／战斗特质和技能位置判定；敌人的路线位置始终实时。
	enum class RulePositionMode { CURRENT, INITIAL };

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
		// 构建期生成且按 ID 排序的装备目录；借用者保证目录及其 spans 比本局活得更久。
		std::span<const EquipmentTemplate> MyEquipmentTemplates{};
		// 按 ID 排序的静态特质表及盟约原始顺序；宿主保证其 spans 比本局活得更久。
		BattleGarrisonRules MyGarrisonRules{};
		std::optional<CombatDefinition> MyYanyouDefinition{};
		std::vector<FieldPoint> MyMapCharacterTiles{}; // 原地图全部医疗预留位置，即使当前没有外勤医疗战略。
		std::vector<MapCharacterVariant> MyMapCharacters{}; // 对所有参战玩家生效，按开战时仍在场的精锐数量选首个变体。
		RulePositionMode MyRulePositionMode{RulePositionMode::CURRENT};
		bool MyGarrisonEffectsAfterExit{}; // 退场后继续提供依赖来源在场的战斗特质；未部署单位不因此激活。
		bool MyRetainGrantedGarrisonsAfterExit{true}; // 接受者退场后保留获授特质；关闭则本场移除，不因重部署恢复。
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

	struct WolfGift
	{
		double MyScale{1};
		double MyDp{};
		bool MyPaid{};
	};

	struct TimedPosition
	{
		WorldPoint MyPoint{};
		double MyUntil{};
	};

	struct TimedTargetMark
	{
		UnitId MyTarget{};
		double MyTime{};
	};

	struct FunnelRampEntry
	{
		UnitId MyTarget{};
		std::uint64_t MyTick{};
		double MyScale{};
	};

	struct LockedDrone
	{
		UnitId MyTarget{};
		std::optional<double> MyScale{};
	};

	struct DroneDamageQueue
	{
		UnitId MyTarget{};
		std::vector<double> MyScales{};
		std::size_t MyHead{};
	};

	struct BombardmentLock
	{
		UnitId MyTarget{};
		WorldPoint MyPoint{};
		bool MyGone{};
	};

	struct AlchemyZone
	{
		WorldPoint MyPoint{};
		WorldPoint MyVelocity{};
		UnitId MyAnchor{};
		double MyElapsed{};
		double MyAccumulator{};
		double MyDuration{};
		double MyBurnScale{};
		std::vector<UnitId> MyBurnTargets{};
	};

	struct FireballCarrier
	{
		UnitId MyUnit{};
		double MyAccumulator{};
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
		std::optional<TokenCountdown> MyCountdown{};
		std::size_t MySummonGroup{NoPlayer};
		double MySummonReadyAt{-std::numeric_limits<double>::infinity()};
		bool MySummonRetry{};
		bool MyObstacle{};
		ObstacleKind MyObstacleKind{ObstacleKind::CRATE};
		CombatDefinition MyDefinition{};
		WorldPoint MyPosition{};
		WorldPoint MyHome{};
		double MyGenericCounterReadyAt{-std::numeric_limits<double>::infinity()};
		std::uint64_t MyGenericShieldBuff{};
		double MyGenericShieldLoss{};
		unsigned MyTinmanZones{};
		std::uint64_t MyTinmanZoneSequence{};
		double MyIndigoAccumulator{};
		double MyWildmnCostReduction{};
		bool MyWildmnDeployed{};
		bool MyOperatorHooksReleased{};
		double MyGrabdsQuietUntil{-1};
		std::vector<UnitId> MyHaroldHalf{};
		UnitId MyPapyrsLock{};
		bool MyPapyrsAbort{};
		UnitId MyRockrLock{};
		double MyRockrOverAt{};
		bool MyRockrOver{};
		UnitId MyKazemaDoll{};
		bool MyKazemaSub{};
		std::uint64_t MyGravelBuff{};
		unsigned MyGravelTicks{};
		double MyTippiLastHit{-std::numeric_limits<double>::infinity()};
		std::size_t MySkillAura{NoPlayer};
		std::size_t MyTalentAura{NoPlayer};
		std::optional<double> MyPreviousAmmo{};
		double MyBlemshRegenAccumulator{};
		double MyTokenAuraAccumulator{};
		std::vector<FunnelRampEntry> MyFunnelRamps{};
		std::vector<LockedDrone> MyLockedDrones{};
		std::vector<DroneDamageQueue> MyDroneQueues{};
		std::vector<UnitId> MyTimeLocked{};
		double MyTimeLockAccumulator{};
		std::uint64_t MyPrecisionHits{};
		bool MyPrecisionBoost{};
		double MyRmixerActiveAttackAt{-std::numeric_limits<double>::infinity()};
		double MyRmixerShieldLostAt{-std::numeric_limits<double>::infinity()};
		double MyRmixerCounterAt{-std::numeric_limits<double>::infinity()};
		bool MyRmixerInCounter{};
		std::uint64_t MyRmixerPreSequence{};
		double MyRmixerPreHealth{};
		std::vector<UnitId> MyInesWoven{};
		std::vector<UnitId> MyInesAttackVictims{};
		std::vector<UnitId> MyInesSpeedVictims{};
		double MyInesSpeed{};
		std::string MyInesAttackKey{};
		std::string MyInesSpeedKey{};
		std::string MyInesSentryKey{};
		std::bitset<FieldTiles> MyInesSentry{};
		std::optional<WorldPoint> MyInesSentryAt{};
		bool MyInesPlaced{};
		bool MyInesRetreated{};
		std::vector<std::uint64_t> MyDeferredHits{};
		std::uint64_t MyDeferredHitTick{};
		std::vector<UnitId> MyAromaMarked{};
		std::vector<UnitId> MyAromaFloating{};
		UnitId MyShieldDevice{};
		UnitId MyShieldRecipient{};
		std::optional<WorldPoint> MyTornado{};
		double MyTornadoAccumulator{};
		std::string MyTornadoSlowKey{};
		std::vector<UnitId> MyHypothermia{};
		std::string MyGuardHealKey{};
		bool MyTexas2Killed{};
		bool MyTexas2Casting{};
		bool MyTexas2Recast{};
		std::uint64_t MyTexas2RainVersion{};
		std::string MyTexas2DotKey{};
		int MyMudrokLayers{};
		bool MyMudrokAwake{};
		double MyMudrokDormantTime{};
		std::string MyMudrokSlowKey{};
		bool MyRiposte{};
		bool MyRiposteNow{};
		unsigned MyFlameGiven{};
		double MyFlameAccumulator{};
		double MyFlameStep{};
		double MyFartthHurtAt{-std::numeric_limits<double>::infinity()};
		std::vector<int> MyExtraRangeKeys{};
		std::vector<int> MyNextExtraRangeKeys{};
		double MyGvialDebt{};
		bool MyBillroCharged{};
		double MyBillroRamp{};
		std::vector<UnitId> MyBillroMarked{};
		UnitId MyBandageTarget{};
		UnitId MyBandageBonus{};
		bool MyBandageSkipSp{};
		bool MyNoInspire{};
		std::optional<double> MyBardRatio{};
		std::optional<double> MyReaperHeal{};
		double MyExcuSpent{};
		bool MyExtraAttack{};
		std::vector<UnitId> MyVerdictTargets{};
		std::vector<UnitId> MySleepWards{};
		std::vector<TimedTargetMark> MySleepStarts{};
		double MyWardAccumulator{};
		UnitId MyCandleOwner{};
		bool MyBlazeDowned{};
		std::uint64_t MyBlazeDownDeployment{};
		double MyBlazeAccumulator{};
		std::bitset<FieldTiles> MyBurnTiles{};
		std::optional<WorldPoint> MyAnchorHome{};
		UnitId MyAnchorMarker{};
		UnitId MyCandleOriginal{};
		bool MyNoLeak{};
		bool MyEtlchiReborn{};
		double MyStolenHealth{};
		std::vector<UnitId> MyCandles{};
		std::vector<UnitId> MySickles{};
		double MySickleAccumulator{};
		bool MySurtrEmber{};
		bool MySurtrSolo{};
		double MySurtrTime{};
		double MySurtrAccumulator{};
		std::string MyPasngrEnhanceKey{};
		unsigned MyPepeKills{};
		unsigned MyPepeRage{};
		unsigned MyPepeStacks{};
		double MyPepeRadius{1};
		bool MyPepeBoost{};
		double MyRosmonRadius{};
		unsigned MyRosmonShocks{};
		bool MyRosmonSaved{};
		UnitId MyCelloPartner{};
		double MyCelloPick{};
		double MyCelloBoost{1};
		double MyCelloAccumulator{};
		std::vector<FireballCarrier> MyReedCarriers{};
		double MyReedAccumulator{};
		std::string MyReedFireKey{};
		std::vector<UnitId> MyHaloLocks{};
		std::vector<double> MyHaloStay{};
		unsigned MyHaloStacks{};
		bool MyHaloLinking{};
		double MyAgoatAccumulator{};
		std::string MyAgoatMistKey{};
		UnitId MySunSword{};
		unsigned MyDawnTimes{1};
		bool MyNearlCombo{};
		bool MyNearlStood{};
		std::vector<UnitId> MyGoldenLions{};
		std::vector<UnitId> MySiegeSeen{};
		std::optional<std::uint64_t> MyBoundSkillActivation{};
		std::vector<unsigned> MySnow{};
		std::vector<int> MySnowLast{};
		double MySnowAccumulator{};
		double MySnowDamageAccumulator{};
		unsigned MySnowSpreads{};
		bool MySnowWaves{};
		bool MySnowBlessingUsed{};
		bool MyBlkkgtSlashing{};
		bool MyBlkkgtFinale{};
		unsigned MyBlkkgtSlashes{};
		double MyBlkkgtAccumulator{};
		double MyBlkkgtPullAccumulator{};
		std::vector<UnitId> MyBlkkgtSeen{};
		std::optional<double> MyYuWall{};
		bool MyYuVertical{};
		bool MyLumenAbnormal{};
		double MyLumenReady{};
		std::string MyLumenRainKey{};
		unsigned MyQiubaiStacks{};
		std::vector<UnitId> MyQiubaiBound{};
		unsigned MyNymphKeys{};
		double MyWantedTime{};
		UnitId MyLemuenAim{};
		double MyLemuenAimTime{};
		double MyLemuenLockAccumulator{};
		std::vector<BombardmentLock> MyLemuenLocks{};
		std::deque<AlchemyZone> MyAlchemyZones{};
		std::vector<UnitId> MyMlynarHits{};
		unsigned MyMlynarNear{};
		unsigned MyMlynarKills{};
		unsigned MyMlynarPending{};
		bool MyMlynarUp{};
		bool MyMlynarKeep{};
		std::optional<double> MyMlynarRamp{};
		bool MyF12yinCritical{};
		double MyGravityAccumulator{};
		double MyIcicleCooldown{};
		unsigned MyIcicleRow{};
		double MyEyeCost{14};
		std::optional<double> MySvashCostBase{};
		double MySvashShield{};
		unsigned MySvashCasts{};
		bool MySvashSwapped{};
		double MySvashDpAccumulator{};
		double MySvashRevealAccumulator{};
		std::vector<UnitId> MyGhostHeavy{};
		bool MyDuskSummoned{};
		double MyDuskUntil{};
		bool MyHornRevived{};
		bool MyHornFlare{};
		bool MyHornOverload{};
		double MyHornTime{};
		double MyHornAccumulator{};
		std::vector<TimedPosition> MyFlares{};
		double MyAreaHealAccumulator{};
		double MyAreaAuraAccumulator{};
		std::string MyFoxKey{};
		std::vector<double> MyMoteReady{};
		std::vector<TimedTargetMark> MyMoteHits{};
		std::string MyMoteKey{};
		double MyRedistributeAccumulator{};
		double MyInspireAccumulator{};
		std::vector<TimedTargetMark> MyVulpisMarks{};
		bool MyVulpisBusy{};
		bool MyVulpisKill{};
		double MyVulpisDuration{1};
		double MyLastHitAt{-std::numeric_limits<double>::infinity()};
		double MyPhilaeBarrier{};
		double MyPhilaeCounterAt{-std::numeric_limits<double>::infinity()};
		bool MySkadiRevived{};
		double MySwireCoins{};
		unsigned MySwireSaves{};
		double MySwireHealAt{-std::numeric_limits<double>::infinity()};
		double MySwireBombHold{-1};
		std::uint64_t MyBombFirstTick{};
		bool MyHadAttackTarget{};
		unsigned MyWolfShadows{};
		bool MyWolfTactical{};
		bool MyWolfReturning{};
		std::uint64_t MyWolfFormSequence{};
		double MyWolfReturnAt{};
		std::optional<WolfGift> MyWolfGift{};
		std::optional<WolfGift> MyWolfGiftActive{};
		std::vector<UnitId> MyWolfMarked{};
		double MyVigilAccumulator{};
		double MyVigilDp{};
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
		double MyDeployedAt{};
		bool MyIndomitableFree{};
		bool MyEliteSpCostApplied{};
		double MyIndomitableAt{-std::numeric_limits<double>::infinity()};
		double MySteadThornAt{-std::numeric_limits<double>::infinity()};
		double MySiracusaStealthEnd{std::numeric_limits<double>::infinity()};
		std::uint64_t MyAggroSequence{}; // 初始召唤物在所有干员之后；不改变部署身份。
		std::uint64_t MySpawnSequence{};
		std::size_t MySpawnIndex{};
		std::size_t MyRouteIndex{};
		std::optional<double> MyWaitLeft{};
		std::vector<Shield> MyShields{};
		std::size_t MyYanyouIndex{std::numeric_limits<std::size_t>::max()};
		std::uint64_t MyRangeRevision{};
		std::bitset<399> MyRangeMask{};
		std::vector<int> MyRangeKeys{}; // 保留数据格子顺序，随机选敌不得按单位出生顺序替代。
		std::bitset<FieldTiles> MyInitialRuleRangeMask{};
		std::vector<int> MyInitialRuleRangeKeys{}; // 仅初始位置模式构建，不能平移已被地图边界裁切的实时范围。
		std::bitset<FieldTiles> MyBaseTriggerMask{};
		CombatStats MyStats{};
		std::vector<CombatBuff> MyBuffs{};
		double MyRegenAccumulator{};
		CombatStatuses MyStatuses{};
		UnitCombatTotals MyTotals{};
		SkillState MySkill{};
		std::bitset<399> MyBaseRangeMask{};
		std::bitset<FieldTiles> MyTraitFrontMask{};
		std::bitset<FieldTiles> MyInitialTraitFrontMask{};
		ProfessionState MyProfession{};
		double MyLastRaidAt{-std::numeric_limits<double>::infinity()};
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
		std::uint64_t MyMedicExtraAttack{std::numeric_limits<std::uint64_t>::max()};
		double MySolventAt{-std::numeric_limits<double>::infinity()};

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
