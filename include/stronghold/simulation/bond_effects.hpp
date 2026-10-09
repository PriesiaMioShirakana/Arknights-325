#ifndef STRONGHOLD_SIMULATION_BOND_EFFECTS_HPP
#define STRONGHOLD_SIMULATION_BOND_EFFECTS_HPP
#include <array>
#include <limits>
#include <string_view>

namespace Stronghold
{
	enum class AddonBondKind : unsigned { PRECISE, SWIFT, SKILLFUL, ARCANE, STEADFAST, DEPUTY, RAID, INDOMITABLE, ASSIST, SOLO, ELITE, COUNT };

	inline constexpr std::array<std::string_view, static_cast<unsigned>(AddonBondKind::COUNT)> AddonBondIds{
		"preciShip", "swiftShip", "skillfulShip", "arcaneShip", "steadShip", "deputShip", "raidShip", "indomShip", "emptyShip", "soloShip", "suntShip"};

	// 生成器将黑板字段解析为固定数值；战斗热路径不查询字符串字典。
	struct AddonBondParameters
	{
		double MyAttack{};
		double MyAttackPerLayer{};
		double MyHealth{};
		double MyHealthPerLayer{};
		double MyDefense{};
		double MyDefensePerLayer{};
		double MyAttackSpeed{};
		double MyAttackSpeedPerLayer{};
		double MyDefenseIgnore{};
		double MyResistanceIgnore{};
		double MyProbability{};
		double MyProbabilityPerLayer{};
		double MyMilestone{std::numeric_limits<double>::infinity()};
		double MySp{};
		double MyExtraSp{};
		double MyDamageMultiplier{1};
		double MyDamagePerLayer{};
		double MyLowHealthMultiplier{1};
		double MyHealthThreshold{};
		double MyDuration{};
		double MyResistance{};
		double MyCooldown{};
		double MyThornDamage{};
		double MyThornPerLayer{};
		double MyFragile{1};
		double MyRedeployDelta{};
		double MyMilestoneAttackSpeed{};
		double MyIdleTime{10};
		double MyEliteDamageMultiplier{1};
		double MySpCostMultiplier{1};
	};

	struct AddonBondEffect
	{
		AddonBondKind MyKind{};
		AddonBondParameters MyParameters{};
	};

	enum class CoreBondKind : unsigned { YAN, SARGON, VICTORIA, KJERAG, LATERANO, EGIR, SIRACUSA, KAZIMIERZ, COUNT };

	inline constexpr std::array<std::string_view, static_cast<unsigned>(CoreBondKind::COUNT)> CoreBondIds{
		"yanShip", "sargonShip", "victoriaShip", "kjeragShip", "lateranoShip", "egirShip", "siracusaShip", "kazimierzShip"};

	struct CoreBondParameters
	{
		unsigned MyPowerCount{6};
		double MyAttack{};
		double MyGoldenAttack{};
		double MyAttackMaximum{};
		double MyAttackMaximumPerLayer{};
		double MyDuration{};
		double MyDurationPerLayer{};
		double MyAttackSpeed{};
		unsigned MyMaxStacks{25};
		double MyDamageMultiplier{1};
		double MyDamagePerLayer{};
		double MyColdMultiplier{1};
		double MyColdPerLayer{};
		double MyInterval{};
		double MyAmmoPercent{};
		double MyAmmoPerLayer{};
		double MyRadius{};
		double MyDamageAttackScale{};
		double MyUnblockedAttackScale{};
		double MyStun{};
		double MyLendDuration{};
		double MyLendDurationPerLayer{};
		unsigned MyLendMaximumTier{5};
		double MyHealth{};
		double MyHealthPerLayer{};
		double MyDevourDamage{};
		unsigned MyReviveMaximum{};
		double MyAttackSpeedPerLayer{};
		double MyBaseDamage{};
		double MyStealthTail{};
		double MyPrdStep{};
		double MyFear{};
		double MyAttackPerLayer{};
		unsigned MyExCount{9};
		double MySummonAttackMultiplier{1};
		double MySummonDamageResistance{};
		double MySummonShare{0.3};
	};

	struct CoreBondEffect
	{
		CoreBondKind MyKind{};
		CoreBondParameters MyParameters{};
		bool MyShareEquipment{};
	};
}
#endif
