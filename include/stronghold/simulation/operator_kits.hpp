#ifndef STRONGHOLD_SIMULATION_OPERATOR_KITS_HPP
#define STRONGHOLD_SIMULATION_OPERATOR_KITS_HPP
#include <variant>
#include <span>
#include <stronghold/domain/range.hpp>

namespace Stronghold
{
	struct InsiderKit
	{
		double MyDelay{};
		double MySelfAmmo{};
		double MyAllyAmmo{};
	};

	struct YakKit
	{
		double MyResistance{};
	};

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

	struct SkgoatKit {};

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

	struct GreyyKit {};

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

	// 内置角色以静态值类型扩展，不创建虚函数对象；每种规则只保存自身参数。
	using OperatorKitDefinition = std::variant<InsiderKit, YakKit, LeiziKit, UdflowKit, VignaKit, VendlaKit, ProveKit, TexasKit, CaperKit, SunbrKit,
		SkgoatKit, EstellKit, PodegoKit, GreyyKit, PithstKit, TinmanKit, IndigoKit, UtageKit>;
}
#endif
