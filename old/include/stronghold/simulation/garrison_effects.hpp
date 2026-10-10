#ifndef STRONGHOLD_SIMULATION_GARRISON_EFFECTS_HPP
#define STRONGHOLD_SIMULATION_GARRISON_EFFECTS_HPP
#include <span>
#include <string_view>

namespace Stronghold
{
	enum class BattleGarrisonKind
	{
		GRANT, SKILL_GAIN, KILL_GAIN, DEATH_GAIN, AMMO_GAIN, FREEZE_GAIN, DEPLOY_GAIN,
		SLEEP_STUN_GAIN, EXTRA_GAIN, BASE_ATTRIBUTES, COMMON_ATTRIBUTES, ATTRIBUTES_BY_BOND,
		REDEPLOY_BY_BOND, DEPLOY_ATTRIBUTES, STATUS_DAMAGE, TAG_DAMAGE, WEAKNESS, NONE, COUNT
	};

	enum class GarrisonBondTarget { LIST, SELF, HIGHEST };

	enum class GarrisonGainAmount { FIXED, ROW_COUNT, TIER };

	enum class GarrisonCondition { NONE, ROW, COLUMN };

	enum class GarrisonGrantTarget { FRONT, SELF_FRONT, ROW_RIGHT, ALL };

	enum class GarrisonAmmoScope { SELF, FRONT, ADJACENT };

	struct BattleGarrisonRule
	{
		std::string_view MyId{};
		BattleGarrisonKind MyKind{BattleGarrisonKind::NONE};
		std::span<const std::string_view> MyBonds{};
		std::string_view MyGrantedGarrison{};
		std::string_view MyRequiredBond{};
		std::string_view MyEnemyTag{};
		GarrisonBondTarget MyBondTarget{};
		GarrisonGainAmount MyGainAmount{};
		GarrisonCondition MyCondition{};
		GarrisonGrantTarget MyGrantTarget{};
		GarrisonAmmoScope MyAmmoScope{};
		double MyAmount{};
		double MyMaximum{};
		double MyCheckCount{};
		double MyEvery{1};
		double MyProbability{1};
		double MyExtra{};
		double MyDivisor{1};
		double MyAttack{};
		double MyHealth{};
		double MyDefense{};
		double MyAttackSpeed{};
		double MyHpRegen{};
		double MySpRegen{};
		double MyRedeploy{};
		double MyDuration{};
		double MyDamageScale{};
		bool MyAlliedKills{};
		bool MyDollSwap{};
		bool MyBound{};
		bool MySluggish{};
	};

	struct BattleGarrisonRules
	{
		std::span<const BattleGarrisonRule> MyEffects{};
		std::span<const std::string_view> MyBondOrder{};
	};
}
#endif
