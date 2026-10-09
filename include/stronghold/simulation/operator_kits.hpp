#ifndef STRONGHOLD_SIMULATION_OPERATOR_KITS_HPP
#define STRONGHOLD_SIMULATION_OPERATOR_KITS_HPP
#include <limits>
#include <variant>
#include <span>
#include <string_view>
#include <optional>
#include <stronghold/domain/range.hpp>
#include <stronghold/simulation/attributes.hpp>

namespace Stronghold
{
	struct InsiderKit
	{
		double MyDelay{};
		double MySelfAmmo{};
		double MyAllyAmmo{};
	};

	// 完全由通用技能、攻击配置和常驻属性表描述的内置角色无需独立运行时类型。
	struct BasicOperatorKit {};

	struct LeiziKit
	{
		double MyUnblockedScale{1};
	};

	struct UdflowKit
	{
		double MyDuration{};
		double MyInterval{1};
		double MyDamage{};
		double MySeaDamage{};
		bool MyReveal{};
	};

	struct VignaKit
	{
		double MyProbability{};
		double MySkillProbability{};
		double MyAttack{};
		double MyHealthThreshold{};
		double MyLowHealthScale{1};
	};

	struct VendlaKit
	{
		double MyHealingScale{1};
		double MyCounterScale{};
		double MyTaunt{1};
		bool MyProtection{};
	};

	struct ProveKit
	{
		double MyHealthDrop{};
		double MyScalePerDrop{};
		double MyProbability{};
		double MyFrontProbability{};
		double MyCriticalScale{1};
		bool MyHunt{};
	};

	struct TexasKit
	{
		double MyInitialDp{};
		double MyDp{};
		double MyScale{};
		double MyStun{};
		std::span<const RangeOffset> MyRange{};
		bool MySwordRain{};
	};

	struct CaperKit
	{
		double MyProbability{};
		double MyCriticalScale{1};
		double MyNearScale{1};
	};

	struct SunbrKit
	{
		double MyProbability{};
		double MyCriticalScale{1};
		double MyStun{};
		double MyHealthThreshold{};
		double MyHealingScale{1};
		double MyCookingSeconds{};
		double MyCookingDefense{};
		double MyServingAttack{};
		bool MyCooking{};
	};

	struct EstellKit
	{
		double MyHealRatio{};
		double MyHealthThreshold{};
		double MyPhysicalReduction{};
		std::span<const RangeOffset> MyRange{};
		bool MyHasRange{};
	};

	struct PodegoKit
	{
		double MyAuraAttack{};
		double MySpPerSecond{};
		double MyZoneDuration{5};
		double MyZoneScale{};
		bool MyHealing{};
	};

	struct PithstKit
	{
		double MyElementRatio{};
		double MyEliteElementRatio{};
	};

	struct TinmanKit
	{
		double MyDuration{10};
		double MyRadius{1.5};
		double MyDamageScale{};
		double MyRegenRatio{};
		double MyWeaken{};
		double MyWitherScale{1};
		double MySpPerSecond{};
		bool MyZone{};
		bool MyWeakZone{};
	};

	struct IndigoKit
	{
		double MyProbability{};
		double MyBindDuration{};
		double MySkillProbabilityScale{1};
		double MyDamageScale{};
		double MyInterval{0.5};
		bool MyMaze{};
	};

	struct UtageKit
	{
		double MyMaxAttackSpeed{};
		double MyMinHealthRatio{};
		double MyProtectThreshold{};
		double MyProtection{};
		double MyHealthLoss{};
		bool MyBreach{};
		bool MyRest{};
	};

	struct WildmnKit
	{
		double MyCostCut{1};
		double MyCostCap{1};
		double MyForce{1};
		bool MyEveryDeploy{};
		bool MyCharge{};
	};

	struct LiskamKit
	{
		double MySp{};
		double MyProbability{};
		double MyStun{};
		double MySelfStun{};
		std::span<const RangeOffset> MyRange{};
		bool MyHasRange{};
		bool MyDefense{};
		bool MyArc{};
		bool MyReveal{};
	};

	struct ExcuKit
	{
		bool MyAllFront{};
	};

	struct SilentKit
	{
		double MyAuraAttackSpeed{};
		double MyGroundHealScale{1};
		std::string_view MyToken{};
		unsigned MyStockCap{1};
		bool MyDrone{};
	};

	struct SlchanKit
	{
		double MyAttack{};
		double MyDefense{};
		double MyForce{1};
		double MyDragDamage{};
		double MyDragDistance{1};
		double MyScale{};
		double MyStun{};
		unsigned MyTargets{1};
		std::span<const RangeOffset> MyRange{};
		bool MyHasRange{};
		bool MyChain{};
	};

	struct GrabdsKit
	{
		double MySluggish{0.8};
		double MyBeastSluggish{};
		double MySpPerSecond{};
		double MySleep{};
		unsigned MyTargets{1};
		std::span<const RangeOffset> MyRange{};
		bool MyHasRange{};
		bool MyQuiet{};
	};

	struct HaroldKit
	{
		double MyResistance{};
		double MyRecoveryScale{1};
		bool MyTriage{};
	};

	struct PapyrsKit
	{
		double MyShieldScale{};
		double MyShieldDuration{8};
		double MySkillShieldScale{1};
		unsigned MyExtraChains{};
		bool MyLockSkill{};
	};

	struct GhostKit
	{
		double MyBlockedScale{1};
		double MyStun{};
		bool MyUndying{};
	};

	struct BubbleKit
	{
		double MyAttackDebuff{};
		double MyDebuffDuration{5};
		double MyBlockingDefense{};
		double MyCounterScale{};
		bool MyCounter{};
	};

	struct HumusPeak
	{
		double MyHealthRatio{1};
		double MyAttack{};
	};

	struct HumusKit
	{
		double MyOverhealCap{};
		double MyHeal{};
		std::span<const HumusPeak> MyPeaks{};
		bool MyPeakSkill{};
		bool MyCut{};
	};

	struct RockrKit
	{
		double MyStackAttack{};
		double MyStackInterval{};
		unsigned MyMaxStacks{};
		double MyOverloadAttack{};
		double MyOverloadScale{1};
		bool MyOverload{};
	};

	struct KazemaKit
	{
		std::string_view MyToken{};
		double MyBurstScale{};
		double MyHealthLoss{};
		double MyDollAttack{};
		bool MySummon{};
		bool MyCut{};
	};

	struct GravelKit
	{
		double MyCost{};
		double MyAuraDefense{};
		std::optional<double> MyAuraCostLimit{};
		double MyShieldRatio{};
		double MyDefense{};
		double MyDuration{};
		bool MyShadow{};
	};

	struct TippiKit
	{
		double MyStackTime{};
		double MyProbability{1};
		bool MyOnDamage{};
	};

	struct FlowerKit
	{
		double MyRegenRatio{};
		unsigned MyTargets{};
	};

	struct AkkordKit
	{
		double MyAllyAttack{};
		double MySonicScale{};
		double MyRadius{0.9};
		double MyMinDistance{};
		double MyMaxDistance{4};
		double MyDistanceScale{};
		bool MySonic{};
	};

	// 内置角色以静态值类型扩展，不创建虚函数对象；每种规则只保存自身参数。
	struct WhitewKit
	{
		double MySilence{};
		double MyAdditionScale{};
		double MyBlockProbability{};
		bool MySundial{};
		bool MyWolfSoul{};
	};

	struct BranchKit
	{
		double MyHealRatio{};
		double MyBlockedReduction{1};
		double MyTremble{};
		double MyResistance{};
		std::span<const RangeOffset> MyRange{};
		bool MyHasRange{};
		bool MyResolve{};
	};

	struct AshlokKit
	{
		double MyAttack{};
		double MyGroundAttack{};
		double MyGroundCount{4};
		double MyBlockedScale{1};
		bool MyRangedSkill{};
	};

	struct AngelKit
	{
		double MyBlessAttack{};
		double MyBlessHealth{};
		double MyGroundAttackSpeed{};
		unsigned MyGroundCount{1};
	};

	struct AyerKit
	{
		double MyAuraAttackSpeed{};
		double MyBladeScale{1};
		double MyAdditionScale{};
		std::span<const RangeOffset> MyTalentRange{};
		bool MyBlade{};
	};

	struct SwireKit
	{
		double MyAuraAttack{};
		double MySkillScale{1};
		std::span<const RangeOffset> MyTalentRange{};
		std::span<const RangeOffset> MySkillRange{};
	};

	struct SkadiKit
	{
		double MyTeamAttack{};
		double MyRedeploy{};
		double MyBlockedScale{1};
		double MyReviveHealthMultiplier{1};
		double MyReviveAttackSpeed{};
		double MyReviveHealthRatio{1};
		bool MyRevive{};
	};

	enum class Swire2SkillKind { HEAL, BOMB, CASH };

	struct Swire2Kit
	{
		Swire2SkillKind MySkill{Swire2SkillKind::BOMB};
		std::string_view MyToken{};
		double MyCoinCap{3};
		double MyCoinCost{1};
		double MyStartCoins{1};
		double MyPaymentCoins{1};
		double MyPaymentAttack{};
		unsigned MyPaymentStacks{8};
		double MyModuleAttack{};
		unsigned MyModuleStacks{5};
		double MyReviveCost{5};
		double MyReviveCostScale{2};
		double MyReviveHealth{0.7};
		double MyHealScale{};
		double MyHealRatio{0.7};
		double MyDamageScale{1};
		double MySluggish{2};
		double MyForce{};
	};

	struct PhilaeKit
	{
		double MyElementResistance{};
		double MyApoptosisSp{};
		double MyElementScale{1};
		double MyBarrier{};
		double MyRageAttack{};
		double MyCounterScale{1};
		double MyCounterElement{};
		double MyCounterCooldown{2};
		bool MyCounter{};
	};

	struct ForcerKit
	{
		double MyHeavyMass{3};
		double MyDefenseIgnore{};
		double MyRefundRatio{};
		double MyForce{1};
		double MyDirectStun{1};
		double MyWallStun{1};
		double MyBrushStun{1};
		std::span<const RangeOffset> MyRange{};
		bool MyHasRange{};
		bool MyPush{};
	};

	struct MintKit
	{
		double MyAuraDefense{};
		double MyTaunt{-1};
		double MyKeepDefense{};
		double MyKeepResistance{};
		double MyForce{};
		double MyEndScale{};
		std::span<const RangeOffset> MyTalentRange{};
		bool MyVortex{};
	};

	struct HainiKit
	{
		double MyFragile{1};
		double MyMoveMultiplier{1};
		double MyKillStep{};
		double MyMaxMultiplier{1};
		bool MySlow{};
	};

	struct PinecnKit
	{
		double MySpDuration{60};
		double MySpRecovery{};
		double MyDefenseIgnore{};
		std::span<const double> MyAttackSteps{};
		bool MySpike{};
	};

	struct SnhuntKit
	{
		double MyMovingScale{1};
		double MyStillScale{1};
		unsigned MyShots{2};
		double MyBeastScale{1};
		double MyCold{3};
		double MyReloadExtra{};
		bool MyVolley{};
	};

	enum class BlemshSkillKind { HEAL, SLEEP, INCARNATE };

	struct BlemshKit
	{
		BlemshSkillKind MySkill{BlemshSkillKind::INCARNATE};
		double MyAttackSp{1};
		double MySleepScale{1};
		double MyAdditionScale{};
		double MyHealScale{};
		double MyRegenRatio{};
		double MyLowHealthThreshold{0.5};
		double MyLowHealthHealScale{1};
		std::span<const RangeOffset> MyHealRange{};
	};

	struct MalistKit
	{
		double MyProbability{};
		double MyCriticalScale{1};
		bool MyDouble{};
	};

	struct WeakeningKit
	{
		double MyHealthRatio{0.4};
		double MyFragile{1};
		unsigned MyTargets{1};
		std::span<const AttributeChange> MySkillModifiers{};
		bool MySlow{};
		bool MySkillAura{true};
		std::string_view MyToken{};
		bool MySummon{};
	};

	struct BlockingDefenseKit
	{
		double MyDefense{};
	};

	struct ShotstKit
	{
		double MyFlyingScale{1};
		double MySkillScale{1};
		double MyShred{};
		double MyShredDuration{5};
		unsigned MyTargets{5};
		bool MyBurst{};
	};

	enum class VulpisSkillKind { PUNISH, TORTURE, CAMOUFLAGE };

	struct VulpisKit
	{
		VulpisSkillKind MySkill{VulpisSkillKind::CAMOUFLAGE};
		double MyDp{};
		double MyDamageScale{};
		double MySluggish{};
		double MyStun{};
		unsigned MyTargets{6};
		std::span<const RangeOffset> MyRange{};
		bool MyHasRange{};
		double MyAttackSpeed{};
		double MyMarkDuration{10};
		double MyMarkScale{};
		double MyDpBonus{};
		double MyQuietTime{4};
		double MyRegenRatio{};
		double MyBlockingAttack{};
		double MyBlockingDefense{};
	};

	struct KjeraKit
	{
		double MyAttack{};
		double MyGroundAttack{};
		double MyGroundTiles{2};
		unsigned MyDrones{1};
		double MyColdProbability{};
		double MyCold{};
		bool MyExtraDrones{};
		bool MyLockDrones{};
	};

	enum class ArchetSkillKind { SCATTER, PURSUIT, STORM };

	struct ArchetKit
	{
		ArchetSkillKind MySkill{ArchetSkillKind::STORM};
		double MyTacticsInterval{2.5};
		double MyTacticsSp{1};
		double MyShieldSp{};
		double MyGroundAttackSpeed{};
		unsigned MyHits{5};
		unsigned MyScatterTargets{3};
		double MyScale{1};
	};

	enum class VigilSkillKind { CALL, GIFT, DIGNITY };

	struct VigilKit
	{
		VigilSkillKind MySkill{VigilSkillKind::DIGNITY};
		std::string_view MyToken{};
		unsigned MyInitialWolves{2};
		unsigned MyMaxWolves{3};
		double MyDefenseIgnore{};
		double MyAdditionScale{};
		double MyDp{};
		double MyDpInterval{1.5};
		double MyDpCap{};
		double MyGiftHeal{};
		double MyGiftScale{1};
		double MyGiftDp{};
	};

	enum class MostmaSkillKind { ATTACK, TIME_LOCK, RIPPLE };

	struct MostmaKit
	{
		MostmaSkillKind MySkill{MostmaSkillKind::RIPPLE};
		double MySpRecovery{0.4};
		double MyMoveSpeed{-0.15};
		double MySkillSlowScale{1};
		double MyDamageScale{1};
		double MyForce{};
	};

	enum class RmixerSkillKind { RELOAD, GUARD, COUNTER };

	struct RmixerKit
	{
		RmixerSkillKind MySkill{RmixerSkillKind::COUNTER};
		double MyReload{};
		double MyGuardCost{30};
		unsigned MyCounterTargets{3};
		double MyCounterRatio{0.6};
		double MyDefense{};
		double MyAttackSpeed{};
		double MyStackDuration{10};
		unsigned MyStacks{3};
		double MyShieldInterval{8};
		double MyShieldRatio{0.15};
		bool MyReveal{};
	};

	struct PrecisionKit
	{
		double MyProbability{};
		double MyScale{1};
		double MyStun{};
		bool MyVolleyExtra{};
		bool MyProgressive{};
		double MyUpgradeAfter{40};
		unsigned MyInitialHits{2};
		unsigned MyUpgradedHits{4};
		double MyLowHealthRatio{};
		double MyLowHealthScale{1};
		double MyInitialSp{};
		bool MyCamouflage{};
	};

	struct BeewaxKit
	{
		std::string_view MyToken{};
		double MyKeepDefense{};
		double MyKeepResistance{};
		double MyRegenRatio{0.04};
		double MyBurstScale{2};
		double MyStun{1};
		double MyLifetime{20};
		bool MySummon{};
	};

	enum class InesSkillKind { BLEED, STEALTH, RECALL };

	struct InesKit
	{
		InesSkillKind MySkill{InesSkillKind::STEALTH};
		double MyDp{1};
		double MyBleedScale{0.4};
		double MyBleedDuration{3};
		double MyStealSpeed{5};
		double MyMaxSpeed{50};
		double MyBind{5};
		double MyStealAttack{90};
		double MyMaxAttack{std::numeric_limits<double>::infinity()};
		double MyMoveMultiplier{0.7};
		double MyFirstRedeploy{};
		double MyAttack{};
		double MyRecallRadius{1.4};
		double MyRecallScale{1.1};
		unsigned MyRecallTargets{4};
	};

	struct RosesaKit
	{
		double MyHealingScale{1.15};
		double MyDamageScale{0.8};
		double MyDelayDuration{5};
		double MyDelayInterval{1};
	};

	enum class MizukiSkillKind { AWAKEN, BIND, MIRROR };

	struct MizukiKit
	{
		MizukiSkillKind MySkill{MizukiSkillKind::BIND};
		unsigned MyTargets{1};
		unsigned MyExtraTargets{1};
		double MyTalentScale{0.5};
		double MyAwakenScale{2};
		double MyStatusDuration{0.8};
		double MySelfLoss{0.15};
		double MyHealthThreshold{0.5};
		double MyAttack{0.1};
		std::optional<double> MySlow{};
	};

	struct AromaKit
	{
		double MyFirstScale{1.1};
		double MyLevitate{2.5};
		double MyDistanceScale{};
		double MyMinDistance{};
		double MyMaxDistance{4};
		double MyFlyingScale{0.55};
		double MyLandingScale{0.65};
		bool MyLanding{};
	};

	struct CathyKit
	{
		std::span<const AttributeChange> MyForgeModifiers{};
		bool MyForge{};
	};

	enum class GladySkillKind { RIP, GRASP, TORNADO };

	struct GladyKit
	{
		GladySkillKind MySkill{GladySkillKind::TORNADO};
		double MyForce{};
		double MyFarRadius{};
		double MyFarForce{};
		double MyDragDamage{};
		double MyDragDistance{1};
		double MyInterval{1.5};
		double MyScale{0.85};
		double MyMoveMultiplier{0.5};
		double MyRegenRatio{0.025};
		double MySeaReduction{0.25};
		double MyMassLimit{3};
		double MyMassScale{1.3};
	};

	enum class GnosisSkillKind { THOUGHT, BURST, HYPOTHERMIA };

	struct GnosisKit
	{
		GnosisSkillKind MySkill{GnosisSkillKind::BURST};
		double MyScale{1.3};
		double MyCold{2.5};
		double MyAttackCold{1};
		double MyColdFragile{1.25};
		double MyFreezeFragile{1.5};
		double MySpRecovery{};
		double MyResistDelay{10};
		double MyResist{0.5};
	};

	struct LionhdKit
	{
		double MyScale{1.7};
		double MyResistance{};
		double MyDuration{6};
		double MyAttack{0.04};
		double MyMaxStacks{5};
		bool MyBurst{true};
	};

	struct ReckprKit
	{
		double MyGuardDuration{10};
		double MyGuardHeal{80};
		double MyProbability{1};
		double MySp{1};
		double MyDuration{8};
		double MyAttackSpeed{16};
		unsigned MyMaxStacks{1};
		double MyHealthRatio{};
		double MyHealScale{};
		bool MySharedGuard{};
		bool MyGuard{true};
	};

	enum class Texas2SkillKind { DRIZZLE, STORM, RAIN };

	struct Texas2Kit
	{
		Texas2SkillKind MySkill{Texas2SkillKind::RAIN};
		std::span<const RangeOffset> MyRange{};
		double MyBurstScale{1.15};
		double MyBurstStun{1.5};
		double MyScale{0.85};
		double MyStun{0.2};
		double MyInterval{1};
		unsigned MyTargets{2};
		double MyResistance{};
		double MyDebuffDuration{8};
		double MySilence{5};
		double MyDotDuration{5};
		double MyDotDamage{260};
		double MyDotInterval{1};
		double MyHealRatio{1};
		double MyAttackSpeed{8};
		double MyDamageReduction{0.25};
		double MyLonelyAttack{};
	};

	struct HsgumaKit
	{
		double MyBlockProbability{0.25};
		double MyAuraDefense{0.06};
		double MyBlockingDefense{};
		double MyCounterScale{0.65};
		bool MyCounter{true};
		bool MySaw{};
	};

	enum class MudrokSkillKind { DEFENSE, HAMMER, DORMANT };

	struct MudrokKit
	{
		MudrokSkillKind MySkill{MudrokSkillKind::HAMMER};
		std::span<const RangeOffset> MyRange{};
		std::span<const AttributeChange> MyAwakeModifiers{};
		std::span<const AttributeChange> MyLonelyModifiers{};
		double MySleep{10};
		double MyMoveMultiplier{0.4};
		double MyStun{0.5};
		double MyProbability{0.3};
		double MySkillHeal{0.04};
		int MyMaxLayers{3};
		int MyLayerGain{1};
		double MyLayerInterval{9};
		double MyLayerHeal{0.2};
		double MySarkazReduction{0.3};
		double MyBlockedScale{};
	};

	enum class FlamtlSkillKind { EVADE, RED_PINE, FLAME };

	struct FlamtlKit
	{
		FlamtlSkillKind MySkill{FlamtlSkillKind::FLAME};
		std::span<const RangeOffset> MyRange{};
		std::span<const AttributeChange> MyBlockingModifiers{};
		double MyDp{1};
		unsigned MyPulses{8};
		double MyProbability{0.6};
		double MyScale{1.8};
		double MyStun{0.5};
		unsigned MyTargets{6};
		double MyDodge{0.4};
		double MyDodgeDuration{10};
		double MyNationDodge{0.22};
	};

	enum class FartthSkillKind { QUICK, ALLIED, LINE };

	struct FartthKit
	{
		FartthSkillKind MySkill{FartthSkillKind::LINE};
		double MyQuietTime{10};
		double MyAttack{0.15};
		double MySurvivorSp{};
		double MyFarScale{1.25};
		double MyDistanceScale{};
		double MyMinDistance{1};
		double MyMaxDistance{4.5};
	};

	struct SpAuraKit
	{
		double MyRecovery{0.3};
		bool MyGlobal{};
	};

	struct SvrashKit
	{
		double MyRedeployMultiplier{0.9};
		double MyAdditionScale{};
		bool MySlash{};
	};

	enum class GvialSkillKind { HEAL, PULL, DEFER };

	struct GvialKit
	{
		GvialSkillKind MySkill{GvialSkillKind::DEFER};
		double MyAttack{0.1};
		double MyDefense{0.1};
		double MyAttackPerBlock{0.04};
		double MyDefensePerBlock{0.04};
		double MyHealthThreshold{0.5};
		double MyHealingScale{1.2};
		double MyLowHealthHealingScale{1.4};
		double MyBlockedScale{};
		double MyReductionThreshold{};
		double MyPhysicalReduction{};
		double MyDeferral{0.5};
		double MyDelayDuration{20};
		double MyDelayInterval{0.1};
		double MyLifeSteal{0.3};
		double MyForce{1};
		bool MyHiddenVariant{};
	};

	enum class BillroSkillKind { GUARD, CHAINS, DEVOUR };

	struct BillroKit
	{
		BillroSkillKind MySkill{BillroSkillKind::CHAINS};
		double MyAttack{0.1};
		double MyKeepDefense{};
		double MyKeepResistance{};
		double MyHeal{0.4};
		double MyChargedHeal{0.8};
		double MySpRecovery{0.6};
		double MyEnemyScale{};
		double MyEnemyCap{5};
		double MyMarkScale{0.2};
		double MyBind{0.3};
		double MySluggish{0.3};
	};

	struct BldskKit
	{
		bool MyBandage{true};
		double MyBandageHeal{0.15};
		double MyAttack{};
		double MyDuration{15};
		double MyInterval{1};
		double MyHealthLoss{0.03};
		double MySelfSp{2};
		double MyAllySp{2};
		double MyHealthRatio{};
		double MyHealScale{};
		bool MyMergedHeal{};
	};

	enum class CetsyrSkillKind { PAST, TOMORROW, REWEAVE };

	struct CetsyrKit
	{
		CetsyrSkillKind MySkill{CetsyrSkillKind::REWEAVE};
		std::optional<double> MyTraitRatio{};
		unsigned MyMotes{3};
		double MyCooldown{6};
		double MySkillCooldown{3};
		double MyAllyRadius{1.15};
		double MyMoteDuration{6};
		double MyTraitScale{1.5};
		double MyEnemyRadius{2};
		double MyMoteScale{2.2};
		double MyBind{3};
		double MyInspire{0.65};
		double MyRedistributeInterval{2};
		double MySarkazReduction{0.1};
		double MyModuleAttack{};
		double MyModuleCount{2};
		bool MyOrbitMotes{};
		double MyAngularSpeed{0.5235987755982988};
		double MyBaseRatio{0.1};
	};

	enum class Excu2SkillKind { EXECUTE, GUNFIGHT, VERDICT };

	struct Excu2Kit
	{
		Excu2SkillKind MySkill{Excu2SkillKind::GUNFIGHT};
		double MyExtraChance{};
		double MyChancePerAmmo{};
		double MyFactionAmmo{1};
		double MyFactionCap{4};
		double MyDodgeChance{};
		double MyRefill{1};
		double MyAttackPerAmmo{};
		unsigned MyMaxStacks{30};
		double MyFinalScale{};
		double MyHealScale{1};
		std::optional<double> MyBaseHeal{};
		double MyCrowdSpeed{};
		double MyCrowdCount{};
	};

	enum class TitiSkillKind { ERODE, WARD, BLOOM };

	struct TitiKit
	{
		TitiSkillKind MySkill{TitiSkillKind::BLOOM};
		double MyStillScale{};
		double MyDreamScale{};
		double MyTalentScale{1};
		double MyHealthThreshold{0.5};
		double MyAuraSpeed{};
		double MySleep{5};
		double MySleepChance{};
		double MyMinScale{1};
		double MyMaxScale{1};
		double MyChainSleep{5};
		double MyRadius{1.5};
		unsigned MyChainTargets{1};
	};

	enum class Blaze2SkillKind { AID, GROUND, FURNACE };

	struct Blaze2Kit
	{
		Blaze2SkillKind MySkill{Blaze2SkillKind::FURNACE};
		double MyMeltdownScale{};
		double MyMeltdownHeal{};
		double MyDownShield{};
		double MyDownRegen{};
		double MyReviveStun{};
		double MyBurstMultiplier{1};
		double MyBurstSp{};
		double MyLoss{};
		double MyAttackLoss{};
		double MyBurnBonus{};
		double MyAmmoRefill{};
		double MyRadius{1.5};
		double MyAidDuration{20};
		double MyAidInterval{1};
		double MyArtsScale{};
		double MyElementScale{};
		double MyMoveMultiplier{0.5};
	};

	enum class UlpiaSkillKind { CONTACT, BOUNDARY, PATH };

	struct UlpiaKit
	{
		UlpiaSkillKind MySkill{UlpiaSkillKind::PATH};
		std::string_view MyToken{"token_10039_ulpia_block"};
		std::span<const RangeOffset> MyRange{};
		unsigned MyReach{6};
		double MyRadius{1.5};
		double MyScale{};
		double MyStun{};
		double MyForce{1};
		unsigned MyTargets{2};
		double MyHealthThreshold{0.5};
		double MyHeal{};
		double MyLowHeal{};
		double MyTalentScale{1};
		std::span<const AttributeChange> MyGrowth{};
		std::span<const AttributeChange> MySharedGrowth{};
		unsigned MyMaxStacks{9};
		unsigned MySharedMaxStacks{9};
	};

	enum class EtlchiSkillKind { ROSE, SICKLE, CANDLE };

	struct EtlchiKit
	{
		EtlchiSkillKind MySkill{EtlchiSkillKind::CANDLE};
		std::string_view MyCandleId{"enemy_5601_entlec"};
		double MyCandleHealth{0.6};
		double MyCandleDefense{1};
		double MyCandleResistance{1};
		unsigned MyCandles{3};
		double MySteal{};
		double MyStealCap{};
		double MyDot{};
		double MyDotDuration{5};
		double MyDotInterval{1};
		double MyHealthThreshold{};
		double MyHealRatio{};
		double MyReduction{};
		double MyCrowdSpeed{};
		double MyCrowdCount{};
		double MySickleScale{};
		double MySickleInterval{0.5};
	};

	enum class SurtrSkillKind { BLADE, GIANT, TWILIGHT };

	struct SurtrKit
	{
		SurtrSkillKind MySkill{SurtrSkillKind::TWILIGHT};
		double MyEmberDuration{8};
		double MyInterval{0.2};
		double MyPeakLoss{0.2};
		double MyRamp{60};
		double MySoloScale{1};
		double MyUnblockedSpeed{};
		double MyArtsFragile{};
	};

	enum class HornSkillKind { FLARE, STORM, DEFENSE };

	struct HornKit
	{
		HornSkillKind MySkill{HornSkillKind::DEFENSE};
		double MyTeamAttack{};
		std::span<const AttributeChange> MyReviveModifiers{};
		double MyReviveHeal{1};
		double MyBlockedScale{1};
		double MyUnblockedSpeed{};
		double MyFlareRadius{1.7};
		double MyFlareDuration{6};
		double MyMagicScale{};
		double MyOverloadAttack{};
		double MyMainDuration{12};
		double MyOverloadDuration{12};
		double MyLossInterval{0.2};
		double MyPeakLoss{};
	};

	struct LisaKit
	{
		bool MyFox{};
		double MySpRecovery{};
		double MyFragile{};
		double MyBoost{1};
		double MyHealRatio{};
		double MyModuleSp{};
	};

	enum class DemkniSkillKind { TRIAGE, MEDICINE, CALCIFY };

	struct DemkniKit
	{
		DemkniSkillKind MySkill{DemkniSkillKind::MEDICINE};
		std::span<const RangeOffset> MyRange{};
		double MyGrowthInterval{20};
		unsigned MyMaxStacks{5};
		std::span<const AttributeChange> MyGrowth{};
		double MyHealSp{};
		double MyHealScale{1};
		double MyLowHealthThreshold{};
		double MyLowHealScale{};
		std::span<const AttributeChange> MyCalcify{};
	};

	enum class DuskSkillKind { BRUSH, INK, FREEHAND };

	struct DuskKit
	{
		DuskSkillKind MySkill{DuskSkillKind::BRUSH};
		std::string_view MyToken{};
		double MyTokenDuration{25};
		double MyKillAttack{};
		unsigned MyMaxStacks{15};
		double MyHealthThreshold{0.5};
		double MyLowHealthScale{1};
	};

	enum class Ghost2SkillKind { SHARE, DESIRE, WEIGHT };

	struct Ghost2Kit
	{
		Ghost2SkillKind MySkill{Ghost2SkillKind::DESIRE};
		std::span<const RangeOffset> MyAround{};
		std::span<const RangeOffset> MyShareRange{};
		double MySlow{};
		double MyDamageScale{};
		double MyTeamHealth{};
		double MyDollAttack{};
		double MyDollHealth{};
		double MyExtraScale{};
		double MyHealthLoss{};
	};

	enum class Svash2SkillKind { PLAN, EDGE, CHANGE };

	struct Svash2Kit
	{
		Svash2SkillKind MySkill{Svash2SkillKind::EDGE};
		std::string_view MyEye{};
		std::span<const RangeOffset> MyRange{};
		unsigned MyTargets{6};
		unsigned MyMaxCasts{2};
		double MyScale{1};
		double MyCold{};
		double MyCostCut{};
		double MyShieldRatio{};
		double MyDp{};
		double MyPeriodicDp{1};
		double MyDpInterval{2};
		double MyDeploySp{};
		double MyRedeployMultiplier{1};
		double MyDefense{};
		double MyRegen{};
		double MyTalentDelay{15};
		double MyFragile{};
		double MyFragileDuration{2};
	};

	enum class F12yinSkillKind { HOOK, SWEEP, QUAKE };

	struct F12yinKit
	{
		F12yinSkillKind MySkill{F12yinSkillKind::SWEEP};
		double MyChance{};
		double MySkillChance{};
		double MyCriticalScale{1};
		double MyWeaken{};
		double MyWeakenDuration{3};
		double MyForce{1};
		double MyHealthySpeed{};
	};

	enum class AglinaSkillKind { CHARGE, PARTICLE, GRAVITY };

	struct AglinaKit
	{
		AglinaSkillKind MySkill{AglinaSkillKind::GRAVITY};
		double MyAttackSpeed{};
		double MyRegeneration{};
		double MyEnemySp{};
	};

	struct SntllaKit
	{
		bool MyIcicle{};
		double MyDelay{20};
		double MyAttack{};
		double MyResist{};
		double MyScale{1};
		double MyCold{};
	};

	enum class NymphSkillKind { LASH, FEAR, BREAK };

	struct NymphKit
	{
		NymphSkillKind MySkill{NymphSkillKind::FEAR};
		double MyElementRatio{};
		double MySoulScale{};
		double MySkillSoulScale{};
		double MyGrowth{};
		unsigned MyMaxStacks{10};
		double MyBurstMultiplier{1};
		double MyBurstSp{};
		double MyExtraScale{};
		double MySplashRadius{1.5};
		double MyFear{};
		bool MyHiddenVariant{};
		bool MyFieldWideGrowth{};
		double MyMaxAttackSpeed{};
	};

	enum class MlynarSkillKind { ANGER, SORROW, GLORY };

	struct MlynarKit
	{
		MlynarSkillKind MySkill{MlynarSkillKind::GLORY};
		double MyNearCount{3};
		double MyAttackScale{1};
		double MyNearScale{1};
		double MyDamageReduction{};
		double MyReflectScale{};
		double MyTraitScale{1};
		double MyPerKill{};
		double MyMarkScale{};
	};

	struct LinearRamp
	{
		double MyBase{};
		double MyIncrement{};
		std::optional<double> MyLimit{};

		[[nodiscard]] double Value(double _steps) const noexcept
		{
			const auto value = MyBase + MyIncrement * _steps;
			return std::isless(MyLimit.value_or(0), MyBase) ? std::max(MyLimit.value_or(0), value) : std::min(MyLimit.value_or(value), value);
		}
	};

	enum class Thorn2SkillKind { GUARD, TIDE, SEA };

	struct Thorn2Kit
	{
		Thorn2SkillKind MySkill{Thorn2SkillKind::TIDE};
		double MyDuration{12};
		double MyExtraDuration{};
		double MyRadius{1.1};
		double MyGrowth{};
		double MySpeed{};
		double MyHealingMultiplier{1};
		double MyDamageScale{};
		double MyHealingScale{};
		double MyDefense{};
		unsigned MyAnchors{3};
		double MyInterval{1};
		double MyMaxSteps{15};
		LinearRamp MyAttackRamp{};
		LinearRamp MyDefenseRamp{};
		LinearRamp MyResistanceRamp{};
		LinearRamp MyDamageRamp{};
		double MyRoadLength{6};
		double MyAllySpeed{};
		double MyAllyRoadSpeed{};
		double MyEnemySpeed{};
		double MyEnemyRoadSpeed{};
		double MyZoneSp{};
	};

	enum class LemuenSkillKind { GREETING, INVITATION, SALUTE };

	struct LemuenKit
	{
		LemuenSkillKind MySkill{LemuenSkillKind::SALUTE};
		double MyWantedInterval{8};
		double MyWantedScale{1};
		double MyExtraditionDelay{20};
		double MyExtraditionAttack{};
		double MyOwnAmmo{};
		double MyAllyAmmo{};
		double MySurvivorSp{};
		double MyLockInterval{0.5};
		double MyInnerRadius{0.8};
		double MyOuterRadius{1.5};
		double MyInnerScale{1};
		double MyOuterScale{1};
		double MySpread{0.2};
		double MyAimBase{1};
		double MyAimIncrement{};
		double MyAimFinal{1};
		double MyAimInterval{0.25};
		double MyAimSteps{10};
		double MyAimDuration{2.5};
	};

	enum class PasngrSkillKind { TOUCH, FOCUS, STORM };

	struct PasngrKit
	{
		PasngrSkillKind MySkill{PasngrSkillKind::STORM};
		std::span<const RangeOffset> MyRange{};
		unsigned MyChainCount{4};
		std::optional<double> MyFalloff{};
		std::optional<double> MySluggish{};
		double MyInterval{0.5};
		unsigned MyStrikes{8};
		double MyStormScale{1};
		double MyHealthThreshold{0.8};
		double MyEnhanceScale{1};
		double MyEnhanceDuration{3};
		double MyLonelyAttack{};
		double MyLonelySp{};
	};

	enum class PepeSkillKind { STAMP, CHAOS, TREMOR };

	struct PepeKit
	{
		PepeSkillKind MySkill{PepeSkillKind::TREMOR};
		double MyKillSp{};
		double MyMaxSp{std::numeric_limits<double>::infinity()};
		double MyTeamAttack{};
		double MyRageSpeed{};
		unsigned MyMaxRage{2};
		double MyStackAttack{};
		unsigned MyMaxStacks{};
		double MyRadiusGrowth{};
		double MyStun{};
		double MyMainStun{};
		double MyCrowdScale{1};
		double MyCrowdCount{std::numeric_limits<double>::infinity()};
	};

	enum class QiubaiSkillKind { FEATHER, SHADOW, SNOW };

	struct QiubaiKit
	{
		QiubaiSkillKind MySkill{QiubaiSkillKind::SNOW};
		double MyGapScale{};
		double MyBothScale{1};
		double MyModuleArts{};
		double MyFirstBind{};
		double MyBindChance{};
		double MyBindDuration{};
		double MyCrowdCount{};
		double MyCrowdSpeed{};
		double MySkillBind{2};
		double MyBurstScale{1};
		double MyStartScale{1};
		double MyEndScale{1};
		unsigned MyMaxStacks{};
		double MyStackSpeed{};
	};

	enum class LumenSkillKind { RAIN, SHOWER, LIGHT };

	struct LumenKit
	{
		LumenSkillKind MySkill{LumenSkillKind::LIGHT};
		double MyHealScale{1};
		unsigned MyTargets{2};
		double MyRainScale{};
		double MyRainDuration{4};
		double MyRainInterval{1};
		double MyResist{0.5};
		double MyResistDuration{};
		double MyHealthyDuration{};
		double MyHealthyThreshold{0.75};
		double MyEmergencyScale{};
		double MyEmergencyCooldown{12};
		double MyPermanentResist{};
		bool MyDefault{};
	};

	enum class BlkkgtSkillKind { MIGHT, LAUGH, SILENCE };

	struct BlkkgtKit
	{
		BlkkgtSkillKind MySkill{BlkkgtSkillKind::SILENCE};
		std::span<const RangeOffset> MyRange{};
		unsigned MyTargets{1};
		unsigned MyFreeHits{2};
		unsigned MyBlockedHits{3};
		unsigned MySlashes{10};
		double MyInterval{0.3};
		double MyPullInterval{1};
		double MyScale{1};
		double MyFinalScale{1};
		double MyPullForce{};
		double MyFinalForce{};
		double MyChance{};
		double MySkillChance{};
		double MyCriticalScale{1};
		double MyTremble{};
		double MyFirstTremble{};
		double MyPenetration{};
		double MySkillScale{1};
	};

	enum class YuSkillKind { HOST, GUEST, WALL };

	struct YuKit
	{
		YuSkillKind MySkill{YuSkillKind::WALL};
		std::span<const RangeOffset> MyRange{};
		double MyProtection{};
		double MyDamageScale{};
		double MyElementRatio{};
		double MyInterval{1};
		double MyOperatorCount{4};
		double MyHealthRatio{};
		double MyElementHealRatio{};
		double MyHealInterval{1};
		double MySkillScale{1};
		double MyCounterRatio{};
		double MyWallChance{};
		double MyWallBurn{};
		double MyElementMultiplier{1};
		double MyModuleCount{std::numeric_limits<double>::infinity()};
		double MyArtsMultiplier{1};
		bool MyDefault{};
	};

	enum class Sbell2SkillKind { BREEZE, WAVES, BOW };

	struct Sbell2Kit
	{
		Sbell2SkillKind MySkill{Sbell2SkillKind::BOW};
		std::string_view MyIceToken{};
		unsigned MyMaxSnow{5};
		double MyInterval{5.5};
		double MySkillInterval{5.5};
		bool MyFirstSnow{};
		double MySlow{};
		double MyEntryScale{};
		unsigned MySpreads{20};
		double MySnowDamage{};
		double MySnowCold{};
		double MyCounterCold{};
		double MySelfFreeze{};
		double MyEnemyFreeze{};
		double MyReviveHealth{1};
		double MyCrowdDamage{};
		unsigned MyCrowdMax{5};
		double MyBurstScale{1};
		double MyCold{};
		double MyForce{};
		unsigned MyForwardSnow{5};
		double MyAttract{};
	};

	enum class Nearl2SkillKind { BLADE, NIGHT, SUN };

	struct Nearl2Kit
	{
		Nearl2SkillKind MySkill{Nearl2SkillKind::SUN};
		std::span<const RangeOffset> MyDawnRange{};
		std::string_view MySword{};
		double MyDawnScale{};
		double MyDawnStun{};
		double MySunScale{};
		double MySunStun{};
		unsigned MyShieldHits{};
		double MyRespawnMultiplier{1};
		double MyComboRespawn{1};
		double MyStandHealthMultiplier{1};
		double MyStandSpeed{};
		double MyStandHealth{1};
		double MyBlockedScale{1};
		bool MyCanStand{};
		bool MyDefault{};
	};

	enum class Siege2SkillKind { REFORGE, HOMELAND, NAME };

	struct Siege2Kit
	{
		Siege2SkillKind MySkill{Siege2SkillKind::NAME};
		std::span<const RangeOffset> MyTalentRange{};
		std::span<const RangeOffset> MySkillRange{};
		std::string_view MyLion{};
		double MyBurstScale{};
		double MyReduction{};
		double MyAttackPerAlly{};
		unsigned MySpCount{2};
		double MySpRecovery{};
		double MyFragile{};
		double MyTrembleScale{1};
		double MyTremble{};
		double MyEliteTremble{};
		double MyFreeSpeed{};
		bool MyDefault{};
	};

	enum class Halo2SkillKind { STARS, GRAVITY, LINKS };

	struct Halo2Kit
	{
		Halo2SkillKind MySkill{Halo2SkillKind::LINKS};
		unsigned MyTargets{1};
		double MyShare{};
		unsigned MyBounces{3};
		double MyBounceRadius{1.7};
		unsigned MyPullTargets{2};
		double MyPullRadius{2};
		double MyPullForce{};
		double MyLinkScale{1};
		double MyStackSpeed{};
		unsigned MyMaxStacks{18};
		double MyFullAttack{};
		double MyFragile{1};
		double MyMatureFragile{1};
		double MyMatureAfter{7};
		bool MyDefault{};
	};

	enum class Agoat2SkillKind { DRIZZLE, VEIL, ECHO };

	struct Agoat2Kit
	{
		Agoat2SkillKind MySkill{Agoat2SkillKind::ECHO};
		unsigned MyShots{1};
		double MyHealScale{1};
		double MyRecovery{};
		double MyVeilScale{5};
		double MyVeilDuration{12};
		double MyMistScale{};
		double MyMistDuration{6};
		unsigned MyMaxStacks{3};
		double MyHealth{};
		double MyElementCut{};
		double MyTalentScale{1};
		double MyModuleSpeed{};
	};

	enum class CelloSkillKind { ECSTASY, REQUIEM, TANGO };

	struct CelloKit
	{
		CelloSkillKind MySkill{CelloSkillKind::ECSTASY};
		double MySkillElement{};
		double MyElementRatio{};
		double MySluggish{};
		double MyAmplification{1};
		double MyFragile{};
		double MyEliteScale{1};
		bool MyFieldWide{};
		double MyDot{};
		double MyDotInterval{1};
		double MyModuleFragile{};
		double MyTalentBoost{1};
		double MyGrantHealth{};
		double MyGrantAttack{};
		double MyGrantDefense{};
		bool MyOnlySkill{};
	};

	enum class Reed2SkillKind { QUICK, FIREBALLS, SEEDS };

	struct Reed2Kit
	{
		Reed2SkillKind MySkill{Reed2SkillKind::SEEDS};
		double MyChance{};
		double MySkillChance{};
		double MyScorchAttack{};
		double MyScorchFragile{};
		double MyScorchDuration{6};
		double MyDot{};
		double MyBurstScale{};
		double MyBurstRadius{1.7};
		unsigned MyCarriers{1};
		double MyFireballCooldown{1.5};
		double MyFireballScale{1};
		double MyHealShare{};
		double MyHealBoost{1};
		double MyInjuredScale{1};
		bool MyDefault{};
	};

	enum class RosmonSkillKind { THOUGHT, NERVES, WISH };

	struct RosmonKit
	{
		RosmonSkillKind MySkill{RosmonSkillKind::NERVES};
		std::string_view MyGear{};
		double MyExtraScale{1};
		int MyExtraShocks{};
		double MyStunChance{};
		double MyStun{};
		double MyStableAttack{};
	};

	using OperatorKitDefinition = std::variant<BasicOperatorKit, InsiderKit, LeiziKit, UdflowKit, VignaKit, VendlaKit, ProveKit, TexasKit, CaperKit, SunbrKit,
		EstellKit, PodegoKit, PithstKit, TinmanKit, IndigoKit, UtageKit, WildmnKit, LiskamKit,
		ExcuKit, SilentKit, SlchanKit, GrabdsKit, HaroldKit, PapyrsKit, GhostKit, BubbleKit, HumusKit, RockrKit,
		KazemaKit, GravelKit, TippiKit, FlowerKit, AkkordKit, WhitewKit, BranchKit, AshlokKit, AngelKit, AyerKit, SwireKit, SkadiKit, Swire2Kit, PhilaeKit, ForcerKit, MintKit, HainiKit, PinecnKit, SnhuntKit, BlemshKit, MalistKit, WeakeningKit, BlockingDefenseKit, ShotstKit, VulpisKit, KjeraKit, ArchetKit, VigilKit, MostmaKit, RmixerKit, PrecisionKit, BeewaxKit, InesKit, RosesaKit, MizukiKit, AromaKit, CathyKit, GladyKit, GnosisKit, LionhdKit, ReckprKit, Texas2Kit, HsgumaKit, MudrokKit, FlamtlKit, FartthKit, SpAuraKit, SvrashKit, GvialKit, BillroKit, BldskKit, CetsyrKit, Excu2Kit, TitiKit, Blaze2Kit, UlpiaKit, EtlchiKit, SurtrKit, HornKit, LisaKit, DemkniKit, DuskKit, Ghost2Kit, Svash2Kit, F12yinKit, AglinaKit, SntllaKit, NymphKit, MlynarKit, Thorn2Kit, LemuenKit, PasngrKit, PepeKit, QiubaiKit, LumenKit, BlkkgtKit, YuKit, Sbell2Kit, Nearl2Kit, Siege2Kit, Halo2Kit, Agoat2Kit, CelloKit, Reed2Kit, RosmonKit>;
}
#endif
