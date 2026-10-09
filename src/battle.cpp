#include <algorithm>
#include <numeric>
#include <set>
#include <stronghold/simulation/battle.hpp>
#include <stronghold/domain/final_assault.hpp>

namespace Stronghold
{
	namespace
	{
		bool Bounded(double _value, double _minimum = 0, double _maximum = 1e9)
		{
			return std::isfinite(_value) && _value >= _minimum && _value <= _maximum;
		}

		void ValidateSkill(const SkillDefinition& _skill)
		{
			if (static_cast<unsigned>(_skill.MyKind) > static_cast<unsigned>(SkillKind::TOGGLE) || static_cast<unsigned>
				(_skill.MySpType) > static_cast<unsigned>(SpType::NONE) || static_cast<unsigned>(_skill.MyTrigger) >
				static_cast<unsigned>(SkillTrigger::NEVER) || !Bounded(_skill.MySpCost) || !Bounded(_skill.MyInitialSp)
				|| !Bounded(_skill.MyDuration, 0, 3600) || !Bounded(_skill.MyAmmo) || _skill.MyMaxCharges == 0 || !
				Bounded(_skill.MyTriggerHpAtMost, 0, 1) || _skill.MyRangeExtend < -100 || _skill.MyRangeExtend > 100)
				throw std::invalid_argument("invalid skill definition");
			for (const auto* grid : {&_skill.MyRange, &_skill.MyTriggerRange})
				for (const auto offset : *grid)
					if (offset.MyRow < -100 || offset.MyRow > 100 || offset.MyColumn < -100 || offset.MyColumn > 100)
						throw std::invalid_argument("invalid skill range");
			AttributeModifiers modifiers;
			modifiers.Add(_skill.MyModifiers);
		}

		void ValidateAttack(const AttackProfile& _attack)
		{
			if (static_cast<unsigned>(_attack.MyScaling) > static_cast<unsigned>(AttackScaling::REINFORCEMENT) || !Bounded(_attack.MyConditionalScale) || !Bounded(_attack.MyCriticalScale) || !Bounded(_attack.MyOperatorBonusScale) ||
				(_attack.MyCriticalProbability && !Bounded(*_attack.MyCriticalProbability, 0, 1)) ||
				!Bounded(_attack.MyHealFarMultiplier) || !Bounded(_attack.MyHealNearDistance) || !_attack.MyHits || !_attack.MyMaxTargets || !Bounded(_attack.MyAttackScale) || !
				Bounded(_attack.MyDamageMultiplier) || (_attack.MyHitMultiplier && !Bounded(*_attack.MyHitMultiplier))
				|| !Bounded(_attack.MyHealScale) || !Bounded(_attack.MySplashRadius) || !Bounded(_attack.MySplashScale)
				|| !Bounded(_attack.MyChainRadius) || !Bounded(_attack.MyChainFalloff, 0, 1) || !
				Bounded(_attack.MyChainSluggish) || !Bounded(_attack.MyHealChainFalloff, 0, 1) || !
				Bounded(_attack.MyElementHealRatio) || !Bounded(_attack.MyHealHpAtMost, 0, 1) || (_attack.MyOnHitStatus
					&& *_attack.MyOnHitStatus >= CombatStatus::COUNT) || !Bounded(_attack.MyProjectileSpeed, 0, 1000) ||
				static_cast<unsigned>(_attack.MyPriority) > static_cast<unsigned>(TargetPriority::HEAVIEST) ||
				static_cast<unsigned>(_attack.MyDamageType) > static_cast<unsigned>(DamageType::ELEMENTAL))
				throw std::invalid_argument("invalid attack profile");
		}
	}

	void Battle::ValidatePoint(WorldPoint _point, bool _spawn)
	{
		const auto margin = _spawn ? 0.5 : 0.0;
		if (!Bounded(_point.MyX, -margin, 20 + margin) || !Bounded(_point.MyY, -margin, 18 + margin))
			throw std::invalid_argument("combat point outside 19 by 21 field");
	}

	void Battle::ValidateDefinition(const CombatDefinition& _definition, bool _enemy)
	{
		if (static_cast<unsigned>(_definition.MyOperatorProfession) > static_cast<unsigned>(OperatorProfession::SPECIAL))
			throw std::invalid_argument("invalid operator profession");
		if (_definition.MyTokenKit && (_enemy || _definition.MyTokenKit->MyKind > TokenKitKind::ROSMON_GEAR || !Bounded(_definition.MyTokenKit->MyLifetime, 0, 3600) ||
			!Bounded(_definition.MyTokenKit->MyBurstScale) || !Bounded(_definition.MyTokenKit->MyBurstStun) || !Bounded(_definition.MyTokenKit->MySluggish) ||
			!Bounded(_definition.MyTokenKit->MyBlockedDefense, -1e9) ||
			std::isnan(_definition.MyTokenKit->MyMatureTime) || std::isless(_definition.MyTokenKit->MyMatureTime, 0)))
			throw std::invalid_argument("invalid token kit");
		if (_definition.MyDeviceShieldRate && !Bounded(*_definition.MyDeviceShieldRate)) throw std::invalid_argument("invalid device shield rate");
		if (_definition.MyTokenKit && _definition.MyTokenKit->MyKind == TokenKitKind::CAT_SHIELD)
		{
			if (!_definition.MyTokenKit->MyCatShield) throw std::invalid_argument("shield device requires its rules");
			const auto& rule = *_definition.MyTokenKit->MyCatShield;
			if (!Bounded(rule.MyIdle) || !Bounded(rule.MyInterval, 0.000001) || !Bounded(rule.MyMaxRatio) || !Bounded(rule.MyRefill)) throw std::invalid_argument("invalid shield device rules");
		}
		if (_definition.MyOriginalDeploymentCost && !Bounded(*_definition.MyOriginalDeploymentCost)) throw std::invalid_argument("invalid original deployment cost");
		if (_definition.MyTokenKit)
		{
			AttributeModifiers modifiers; modifiers.Add(_definition.MyTokenKit->MyAuraModifiers);
			for (const auto cell : _definition.MyTokenKit->MyBurstRange)
				if (cell.MyRow < -100 || cell.MyRow > 100 || cell.MyColumn < -100 || cell.MyColumn > 100) throw std::invalid_argument("invalid token burst range");
		}
		if (_definition.MyTokenKit && _definition.MyTokenKit->MyKind == TokenKitKind::WOLF_PACK)
		{
			if (!_definition.MyTokenKit->MyWolf) throw std::invalid_argument("wolf pack requires its rules");
			const auto& wolf = *_definition.MyTokenKit->MyWolf;
			if (!Bounded(wolf.MyInterval) || !Bounded(wolf.MyBlockPerShadow) || !wolf.MyMaxShadows || wolf.MyMaxShadows > 1000000 ||
				!Bounded(wolf.MyDefenseIgnore) || !Bounded(wolf.MyBlockedReduction) || !Bounded(wolf.MyAdditionScale) || !Bounded(wolf.MyTaunt, -1e9))
				throw std::invalid_argument("invalid wolf pack rules");
		}
		if (_definition.MyOperatorKit)
		{
			if (_enemy) throw std::invalid_argument("operator kit on enemy");
			const auto valid = std::visit([](const auto& kit)
			{
				using T = std::decay_t<decltype(kit)>;
				if constexpr (std::is_same_v<T, InsiderKit>) return Bounded(kit.MyDelay) && Bounded(kit.MySelfAmmo) && Bounded(kit.MyAllyAmmo);
				else if constexpr (std::is_same_v<T, BasicOperatorKit>) return true;
				else if constexpr (std::is_same_v<T, LeiziKit>) return Bounded(kit.MyUnblockedScale);
				else if constexpr (std::is_same_v<T, UdflowKit>) return Bounded(kit.MyDuration) && Bounded(kit.MyInterval) && Bounded(kit.MyDamage) && Bounded(kit.MySeaDamage);
				else if constexpr (std::is_same_v<T, VignaKit>) return Bounded(kit.MyProbability, 0, 1) && Bounded(kit.MySkillProbability, 0, 1) && Bounded(kit.MyAttack, -1e9) &&
					Bounded(kit.MyHealthThreshold, 0, 1) && Bounded(kit.MyLowHealthScale);
				else if constexpr (std::is_same_v<T, VendlaKit>) return Bounded(kit.MyHealingScale) && Bounded(kit.MyCounterScale) && Bounded(kit.MyTaunt, -1e9);
				else if constexpr (std::is_same_v<T, ProveKit>) return Bounded(kit.MyHealthDrop) && Bounded(kit.MyScalePerDrop) &&
					Bounded(kit.MyProbability, 0, 1) && Bounded(kit.MyFrontProbability, 0, 1) && Bounded(kit.MyCriticalScale);
				else if constexpr (std::is_same_v<T, TexasKit>)
				{
					for (const auto cell : kit.MyRange) if (cell.MyRow < -100 || cell.MyRow > 100 || cell.MyColumn < -100 || cell.MyColumn > 100) return false;
					return Bounded(kit.MyInitialDp) && Bounded(kit.MyDp) && Bounded(kit.MyScale) && Bounded(kit.MyStun);
				}
				else if constexpr (std::is_same_v<T, CaperKit>) return Bounded(kit.MyProbability, 0, 1) && Bounded(kit.MyCriticalScale) && Bounded(kit.MyNearScale);
				else if constexpr (std::is_same_v<T, SunbrKit>) return Bounded(kit.MyProbability, 0, 1) && Bounded(kit.MyCriticalScale) && Bounded(kit.MyStun) &&
					Bounded(kit.MyHealthThreshold, 0, 1) && Bounded(kit.MyHealingScale) && Bounded(kit.MyCookingSeconds) && Bounded(kit.MyCookingDefense, -1e9) && Bounded(kit.MyServingAttack, -1e9);
				else if constexpr (std::is_same_v<T, EstellKit>)
				{
					for (const auto cell : kit.MyRange) if (cell.MyRow < -100 || cell.MyRow > 100 || cell.MyColumn < -100 || cell.MyColumn > 100) return false;
					return Bounded(kit.MyHealRatio) && Bounded(kit.MyHealthThreshold, 0, 1) && Bounded(kit.MyPhysicalReduction, 0, 1);
				}
				else if constexpr (std::is_same_v<T, PodegoKit>) return Bounded(kit.MyAuraAttack, -1e9) && Bounded(kit.MySpPerSecond) &&
					Bounded(kit.MyZoneDuration, 0, 3600) && Bounded(kit.MyZoneScale);
				else if constexpr (std::is_same_v<T, PithstKit>) return Bounded(kit.MyElementRatio) && Bounded(kit.MyEliteElementRatio);
				else if constexpr (std::is_same_v<T, TinmanKit>) return Bounded(kit.MyDuration, 0, 3600) && Bounded(kit.MyRadius, 0, 100) &&
					Bounded(kit.MyDamageScale) && Bounded(kit.MyRegenRatio) && Bounded(kit.MyWeaken, 0, 1) && Bounded(kit.MyWitherScale) && Bounded(kit.MySpPerSecond);
				else if constexpr (std::is_same_v<T, IndigoKit>) return Bounded(kit.MyProbability, 0, 1) && Bounded(kit.MyBindDuration, 0, 3600) &&
					Bounded(kit.MySkillProbabilityScale) && Bounded(kit.MyDamageScale) && Bounded(kit.MyInterval, 0, 3600);
				else if constexpr (std::is_same_v<T, UtageKit>) return Bounded(kit.MyMaxAttackSpeed) && Bounded(kit.MyMinHealthRatio, 0, 1) &&
					Bounded(kit.MyProtectThreshold, 0, 1) && Bounded(kit.MyProtection, 0, 1) && Bounded(kit.MyHealthLoss, 0, 1);
				else if constexpr (std::is_same_v<T, WildmnKit>) return Bounded(kit.MyCostCut) && Bounded(kit.MyCostCap) && Bounded(kit.MyForce, -1e9);
				else if constexpr (std::is_same_v<T, LiskamKit>)
				{
					for (const auto cell : kit.MyRange) if (cell.MyRow < -100 || cell.MyRow > 100 || cell.MyColumn < -100 || cell.MyColumn > 100) return false;
					return Bounded(kit.MySp) && Bounded(kit.MyProbability, 0, 1) && Bounded(kit.MyStun) && Bounded(kit.MySelfStun);
				}
				else if constexpr (std::is_same_v<T, ExcuKit>) return true;
				else if constexpr (std::is_same_v<T, SilentKit>) return Bounded(kit.MyAuraAttackSpeed, -1e9) && Bounded(kit.MyGroundHealScale) &&
					kit.MyStockCap > 0 && (!kit.MyDrone || !kit.MyToken.empty());
				else if constexpr (std::is_same_v<T, SlchanKit> || std::is_same_v<T, GrabdsKit>)
				{
					for (const auto cell : kit.MyRange) if (cell.MyRow < -100 || cell.MyRow > 100 || cell.MyColumn < -100 || cell.MyColumn > 100) return false;
					if constexpr (std::is_same_v<T, SlchanKit>) return kit.MyTargets > 0 && Bounded(kit.MyAttack, -1e9) && Bounded(kit.MyDefense, -1e9) &&
						Bounded(kit.MyForce, -1e9) && Bounded(kit.MyDragDamage) && Bounded(kit.MyDragDistance, 0.01) && Bounded(kit.MyScale) && Bounded(kit.MyStun);
					else return kit.MyTargets > 0 && Bounded(kit.MySluggish) && Bounded(kit.MyBeastSluggish) &&
						Bounded(kit.MySpPerSecond) && Bounded(kit.MySleep);
				}
				else if constexpr (std::is_same_v<T, HaroldKit>) return Bounded(kit.MyResistance, 0, 1) && Bounded(kit.MyRecoveryScale);
				else if constexpr (std::is_same_v<T, PapyrsKit>) return Bounded(kit.MyShieldScale) && Bounded(kit.MyShieldDuration) && Bounded(kit.MySkillShieldScale);
				else if constexpr (std::is_same_v<T, GhostKit>) return Bounded(kit.MyBlockedScale) && Bounded(kit.MyStun);
				else if constexpr (std::is_same_v<T, BubbleKit>) return Bounded(kit.MyAttackDebuff, -1e9) && Bounded(kit.MyDebuffDuration) &&
					Bounded(kit.MyBlockingDefense, -1e9) && Bounded(kit.MyCounterScale);
				else if constexpr (std::is_same_v<T, HumusKit>)
				{
					double previous = 1;
					for (const auto peak : kit.MyPeaks)
					{
						if (!Bounded(peak.MyHealthRatio, 0, previous) || !Bounded(peak.MyAttack, -1e9)) return false;
						previous = peak.MyHealthRatio;
					}
					return Bounded(kit.MyOverhealCap) && Bounded(kit.MyHeal);
				}
				else if constexpr (std::is_same_v<T, RockrKit>) return Bounded(kit.MyStackAttack, -1e9) && Bounded(kit.MyStackInterval) &&
					kit.MyMaxStacks <= 1000000 && Bounded(kit.MyOverloadAttack, -1e9) && Bounded(kit.MyOverloadScale);
				else if constexpr (std::is_same_v<T, KazemaKit>) return !kit.MyToken.empty() && Bounded(kit.MyBurstScale) && Bounded(kit.MyHealthLoss, 0, 1) && Bounded(kit.MyDollAttack, -1e9);
				else if constexpr (std::is_same_v<T, GravelKit>) return Bounded(kit.MyCost, -1e9) && Bounded(kit.MyAuraDefense, -1e9) &&
					(!kit.MyAuraCostLimit || Bounded(*kit.MyAuraCostLimit)) && Bounded(kit.MyShieldRatio) && Bounded(kit.MyDefense) && Bounded(kit.MyDuration, 0, 3600);
				else if constexpr (std::is_same_v<T, TippiKit>) return Bounded(kit.MyStackTime) && Bounded(kit.MyProbability, 0, 1);
				else if constexpr (std::is_same_v<T, FlowerKit>) return Bounded(kit.MyRegenRatio);
				else if constexpr (std::is_same_v<T, AkkordKit>) return Bounded(kit.MyAllyAttack, -1e9) && Bounded(kit.MySonicScale) &&
					Bounded(kit.MyRadius) && Bounded(kit.MyMinDistance, -1e9) && Bounded(kit.MyMaxDistance, -1e9) && Bounded(kit.MyDistanceScale);
				else if constexpr (std::is_same_v<T, WhitewKit>) return Bounded(kit.MySilence) && Bounded(kit.MyAdditionScale) && Bounded(kit.MyBlockProbability, 0, 1);
				else if constexpr (std::is_same_v<T, BranchKit>)
				{
					for (const auto cell : kit.MyRange) if (cell.MyRow < -100 || cell.MyRow > 100 || cell.MyColumn < -100 || cell.MyColumn > 100) return false;
					return Bounded(kit.MyHealRatio) && Bounded(kit.MyBlockedReduction) && Bounded(kit.MyTremble) && Bounded(kit.MyResistance, 0, 0.95);
				}
				else if constexpr (std::is_same_v<T, AshlokKit>) return Bounded(kit.MyAttack, -1e9) && Bounded(kit.MyGroundAttack, -1e9) && Bounded(kit.MyGroundCount) && Bounded(kit.MyBlockedScale);
				else if constexpr (std::is_same_v<T, AngelKit>) return Bounded(kit.MyBlessAttack, -1e9) && Bounded(kit.MyBlessHealth, -1e9) && Bounded(kit.MyGroundAttackSpeed, -1e9) && kit.MyGroundCount > 0;
				else if constexpr (std::is_same_v<T, AyerKit> || std::is_same_v<T, SwireKit>)
				{
					for (const auto cell : kit.MyTalentRange) if (cell.MyRow < -100 || cell.MyRow > 100 || cell.MyColumn < -100 || cell.MyColumn > 100) return false;
					if constexpr (std::is_same_v<T, AyerKit>) return Bounded(kit.MyAuraAttackSpeed, -1e9) && Bounded(kit.MyBladeScale) && Bounded(kit.MyAdditionScale);
					else
					{
						for (const auto cell : kit.MySkillRange) if (cell.MyRow < -100 || cell.MyRow > 100 || cell.MyColumn < -100 || cell.MyColumn > 100) return false;
						return Bounded(kit.MyAuraAttack, -1e9) && Bounded(kit.MySkillScale);
					}
				}
				else if constexpr (std::is_same_v<T, SkadiKit>) return Bounded(kit.MyTeamAttack, -1e9) && Bounded(kit.MyRedeploy, -1e9) && Bounded(kit.MyBlockedScale) &&
					Bounded(kit.MyReviveHealthMultiplier, 0.05) && Bounded(kit.MyReviveAttackSpeed, -1e9) && Bounded(kit.MyReviveHealthRatio);
				else if constexpr (std::is_same_v<T, Swire2Kit>) return kit.MySkill <= Swire2SkillKind::CASH && !kit.MyToken.empty() &&
					Bounded(kit.MyCoinCap, 0, 1000000) && Bounded(kit.MyCoinCost, 0.000001) && Bounded(kit.MyStartCoins) && Bounded(kit.MyPaymentCoins) &&
					Bounded(kit.MyPaymentAttack, -1e9) && kit.MyPaymentStacks > 0 && Bounded(kit.MyModuleAttack, -1e9) && kit.MyModuleStacks > 0 &&
					Bounded(kit.MyReviveCost) && Bounded(kit.MyReviveCostScale) && Bounded(kit.MyReviveHealth) && Bounded(kit.MyHealScale) &&
					Bounded(kit.MyHealRatio, 0, 1) && Bounded(kit.MyDamageScale) && Bounded(kit.MySluggish) && Bounded(kit.MyForce, -1e9);
				else if constexpr (std::is_same_v<T, PhilaeKit>) return Bounded(kit.MyElementResistance, 0, 1) && Bounded(kit.MyApoptosisSp) && Bounded(kit.MyElementScale) &&
					Bounded(kit.MyBarrier) && Bounded(kit.MyRageAttack, -1e9) && Bounded(kit.MyCounterScale) && Bounded(kit.MyCounterElement) && Bounded(kit.MyCounterCooldown);
				else if constexpr (std::is_same_v<T, ForcerKit>)
				{
					for (const auto cell : kit.MyRange) if (cell.MyRow < -100 || cell.MyRow > 100 || cell.MyColumn < -100 || cell.MyColumn > 100) return false;
					return Bounded(kit.MyHeavyMass, -1e9) && Bounded(kit.MyDefenseIgnore) && Bounded(kit.MyRefundRatio) && Bounded(kit.MyForce, -1e9) &&
						Bounded(kit.MyDirectStun) && Bounded(kit.MyWallStun) && Bounded(kit.MyBrushStun);
				}
				else if constexpr (std::is_same_v<T, MintKit>)
				{
					for (const auto cell : kit.MyTalentRange) if (cell.MyRow < -100 || cell.MyRow > 100 || cell.MyColumn < -100 || cell.MyColumn > 100) return false;
					return Bounded(kit.MyAuraDefense, -1e9) && Bounded(kit.MyTaunt, -1e9) && Bounded(kit.MyKeepDefense, -1e9) && Bounded(kit.MyKeepResistance, -1e9) &&
						Bounded(kit.MyForce, -1e9) && Bounded(kit.MyEndScale);
				}
				else if constexpr (std::is_same_v<T, HainiKit>) return Bounded(kit.MyFragile) && Bounded(kit.MyMoveMultiplier) && Bounded(kit.MyKillStep) && Bounded(kit.MyMaxMultiplier);
				else if constexpr (std::is_same_v<T, PinecnKit>)
				{
					for (const auto value : kit.MyAttackSteps) if (!Bounded(value, -1e9)) return false;
					return Bounded(kit.MySpDuration) && Bounded(kit.MySpRecovery, -1e9) && Bounded(kit.MyDefenseIgnore);
				}
				else if constexpr (std::is_same_v<T, SnhuntKit>) return Bounded(kit.MyMovingScale) && Bounded(kit.MyStillScale) && kit.MyShots > 0 && kit.MyShots <= 1000000 &&
					Bounded(kit.MyBeastScale) && Bounded(kit.MyCold) && Bounded(kit.MyReloadExtra);
				else if constexpr (std::is_same_v<T, BlemshKit>)
				{
					for (const auto cell : kit.MyHealRange) if (cell.MyRow < -100 || cell.MyRow > 100 || cell.MyColumn < -100 || cell.MyColumn > 100) return false;
					return kit.MySkill <= BlemshSkillKind::INCARNATE && Bounded(kit.MyAttackSp) && Bounded(kit.MySleepScale) && Bounded(kit.MyAdditionScale) &&
						Bounded(kit.MyHealScale) && Bounded(kit.MyRegenRatio) && Bounded(kit.MyLowHealthThreshold, 0, 1) && Bounded(kit.MyLowHealthHealScale);
				}
				else if constexpr (std::is_same_v<T, MalistKit>) return Bounded(kit.MyProbability, 0, 1) && Bounded(kit.MyCriticalScale);
				else if constexpr (std::is_same_v<T, WeakeningKit>)
				{
					AttributeModifiers modifiers; modifiers.Add(kit.MySkillModifiers);
					return Bounded(kit.MyHealthRatio, 0, 1) && Bounded(kit.MyFragile) && kit.MyTargets > 0 && (!kit.MySummon || !kit.MyToken.empty());
				}
				else if constexpr (std::is_same_v<T, BlockingDefenseKit>) return Bounded(kit.MyDefense, -1e9);
				else if constexpr (std::is_same_v<T, ShotstKit>) return Bounded(kit.MyFlyingScale) && Bounded(kit.MySkillScale) && Bounded(kit.MyShred, -1e9) && Bounded(kit.MyShredDuration) && kit.MyTargets > 0;
				else if constexpr (std::is_same_v<T, VulpisKit>)
				{
					for (const auto cell : kit.MyRange) if (cell.MyRow < -100 || cell.MyRow > 100 || cell.MyColumn < -100 || cell.MyColumn > 100) return false;
					return kit.MySkill <= VulpisSkillKind::CAMOUFLAGE && Bounded(kit.MyDp) && Bounded(kit.MyDamageScale) && Bounded(kit.MySluggish) && Bounded(kit.MyStun) &&
						kit.MyTargets > 0 && Bounded(kit.MyAttackSpeed, -1e9) && Bounded(kit.MyMarkDuration) && Bounded(kit.MyMarkScale) && Bounded(kit.MyDpBonus, -1e9) &&
						Bounded(kit.MyQuietTime) && Bounded(kit.MyRegenRatio) && Bounded(kit.MyBlockingAttack, -1e9) && Bounded(kit.MyBlockingDefense, -1e9);
				}
				else if constexpr (std::is_same_v<T, KjeraKit>) return Bounded(kit.MyAttack, -1e9) && Bounded(kit.MyGroundAttack, -1e9) && Bounded(kit.MyGroundTiles) &&
					kit.MyDrones > 0 && kit.MyDrones <= 1000000 && Bounded(kit.MyColdProbability, 0, 1) && Bounded(kit.MyCold);
				else if constexpr (std::is_same_v<T, ArchetKit>) return kit.MySkill <= ArchetSkillKind::STORM && Bounded(kit.MyTacticsInterval, 0.000001) && Bounded(kit.MyTacticsSp) &&
					Bounded(kit.MyShieldSp) && Bounded(kit.MyGroundAttackSpeed, -1e9) && kit.MyHits > 0 && kit.MyHits <= 1000000 && kit.MyScatterTargets <= 1000000 && Bounded(kit.MyScale);
				else if constexpr (std::is_same_v<T, VigilKit>) return kit.MySkill <= VigilSkillKind::DIGNITY && !kit.MyToken.empty() && kit.MyInitialWolves > 0 &&
					kit.MyMaxWolves > 0 && kit.MyMaxWolves <= 1000000 && Bounded(kit.MyDefenseIgnore) && Bounded(kit.MyAdditionScale) && Bounded(kit.MyDp) &&
					Bounded(kit.MyDpInterval) && Bounded(kit.MyDpCap) && Bounded(kit.MyGiftHeal) && Bounded(kit.MyGiftScale) && Bounded(kit.MyGiftDp);
				else if constexpr (std::is_same_v<T, MostmaKit>) return kit.MySkill <= MostmaSkillKind::RIPPLE && Bounded(kit.MySpRecovery) && Bounded(kit.MyMoveSpeed, -1e9) &&
					Bounded(kit.MySkillSlowScale) && Bounded(kit.MyDamageScale) && Bounded(kit.MyForce, -1e9);
				else if constexpr (std::is_same_v<T, RmixerKit>) return kit.MySkill <= RmixerSkillKind::COUNTER && Bounded(kit.MyReload) && Bounded(kit.MyGuardCost, 1) &&
					kit.MyCounterTargets > 0 && Bounded(kit.MyCounterRatio) && Bounded(kit.MyDefense, -1e9) && Bounded(kit.MyAttackSpeed, -1e9) && Bounded(kit.MyStackDuration) &&
					kit.MyStacks > 0 && Bounded(kit.MyShieldInterval) && Bounded(kit.MyShieldRatio);
				else if constexpr (std::is_same_v<T, PrecisionKit>) return Bounded(kit.MyProbability, 0, 1) && Bounded(kit.MyScale) && Bounded(kit.MyStun) && Bounded(kit.MyUpgradeAfter) &&
					kit.MyInitialHits > 0 && kit.MyUpgradedHits > 0 && Bounded(kit.MyLowHealthRatio, 0, 1) && Bounded(kit.MyLowHealthScale) && Bounded(kit.MyInitialSp);
				else if constexpr (std::is_same_v<T, BeewaxKit>) return !kit.MyToken.empty() && Bounded(kit.MyKeepDefense, -1e9) && Bounded(kit.MyKeepResistance, -1e9) &&
					Bounded(kit.MyRegenRatio) && Bounded(kit.MyBurstScale) && Bounded(kit.MyStun) && Bounded(kit.MyLifetime);
				else if constexpr (std::is_same_v<T, InesKit>) return kit.MySkill <= InesSkillKind::RECALL && Bounded(kit.MyDp) && Bounded(kit.MyBleedScale) && Bounded(kit.MyBleedDuration) &&
					Bounded(kit.MyStealSpeed) && Bounded(kit.MyMaxSpeed) && Bounded(kit.MyBind) && Bounded(kit.MyStealAttack) && !std::isnan(kit.MyMaxAttack) && !std::isless(kit.MyMaxAttack, 0) &&
					Bounded(kit.MyMoveMultiplier) && Bounded(kit.MyFirstRedeploy, -1e9) && Bounded(kit.MyAttack, -1e9) && Bounded(kit.MyRecallRadius) && Bounded(kit.MyRecallScale) && kit.MyRecallTargets > 0;
				else if constexpr (std::is_same_v<T, RosesaKit>) return Bounded(kit.MyHealingScale) && Bounded(kit.MyDamageScale, 0, 1) && Bounded(kit.MyDelayDuration) && Bounded(kit.MyDelayInterval, 0.1);
				else if constexpr (std::is_same_v<T, MizukiKit>) return kit.MySkill <= MizukiSkillKind::MIRROR && kit.MyTargets > 0 && Bounded(kit.MyTalentScale) && Bounded(kit.MyAwakenScale) &&
					Bounded(kit.MyStatusDuration) && Bounded(kit.MySelfLoss) && Bounded(kit.MyHealthThreshold, 0, 1) && Bounded(kit.MyAttack, -1e9) && (!kit.MySlow || Bounded(*kit.MySlow));
				else if constexpr (std::is_same_v<T, AromaKit>) return Bounded(kit.MyFirstScale) && Bounded(kit.MyLevitate) && Bounded(kit.MyDistanceScale) && Bounded(kit.MyMinDistance) &&
					Bounded(kit.MyMaxDistance) && Bounded(kit.MyFlyingScale) && Bounded(kit.MyLandingScale);
				else if constexpr (std::is_same_v<T, LisaKit>) return Bounded(kit.MySpRecovery) && Bounded(kit.MyFragile) && Bounded(kit.MyBoost) && Bounded(kit.MyHealRatio) && Bounded(kit.MyModuleSp);
				else if constexpr (std::is_same_v<T, DemkniKit>) return kit.MySkill <= DemkniSkillKind::CALCIFY && Bounded(kit.MyGrowthInterval, 1) && kit.MyMaxStacks > 0 &&
					Bounded(kit.MyHealSp) && Bounded(kit.MyHealScale) && Bounded(kit.MyLowHealthThreshold, 0, 1) && Bounded(kit.MyLowHealScale);
				else if constexpr (std::is_same_v<T, HornKit>) return kit.MySkill <= HornSkillKind::DEFENSE && Bounded(kit.MyTeamAttack, -1e9) && Bounded(kit.MyReviveHeal) &&
					Bounded(kit.MyBlockedScale) && Bounded(kit.MyUnblockedSpeed, -1e9) && Bounded(kit.MyFlareRadius) && Bounded(kit.MyFlareDuration) &&
					Bounded(kit.MyMagicScale) && Bounded(kit.MyOverloadAttack, -1e9) && Bounded(kit.MyMainDuration) && Bounded(kit.MyOverloadDuration, 0.1) && Bounded(kit.MyLossInterval, 0.05) && Bounded(kit.MyPeakLoss);
				else if constexpr (std::is_same_v<T, SurtrKit>) return kit.MySkill <= SurtrSkillKind::TWILIGHT && Bounded(kit.MyEmberDuration) && Bounded(kit.MyInterval, 0.05) &&
					Bounded(kit.MyPeakLoss) && Bounded(kit.MyRamp, 0.1) && Bounded(kit.MySoloScale) && Bounded(kit.MyUnblockedSpeed, -1e9) && Bounded(kit.MyArtsFragile);
				else if constexpr (std::is_same_v<T, EtlchiKit>) return kit.MySkill <= EtlchiSkillKind::CANDLE && !kit.MyCandleId.empty() && Bounded(kit.MyCandleHealth) && Bounded(kit.MyCandleDefense) &&
					Bounded(kit.MyCandleResistance) && kit.MyCandles > 0 && Bounded(kit.MySteal) && Bounded(kit.MyStealCap) && Bounded(kit.MyDot) && Bounded(kit.MyDotDuration) &&
					Bounded(kit.MyDotInterval, 0.000001) && Bounded(kit.MyHealthThreshold, 0, 1) && Bounded(kit.MyHealRatio) && Bounded(kit.MyReduction, 0, 1) &&
					Bounded(kit.MyCrowdSpeed, -1e9) && Bounded(kit.MyCrowdCount) && Bounded(kit.MySickleScale) && Bounded(kit.MySickleInterval, 0.1);
				else if constexpr (std::is_same_v<T, UlpiaKit>) return kit.MySkill <= UlpiaSkillKind::PATH && !kit.MyToken.empty() && kit.MyReach > 0 && kit.MyReach <= 1000000 &&
					Bounded(kit.MyRadius) && Bounded(kit.MyScale) && Bounded(kit.MyStun) && Bounded(kit.MyForce, -1e9) && kit.MyTargets > 0 && Bounded(kit.MyHealthThreshold, 0, 1) &&
					Bounded(kit.MyHeal) && Bounded(kit.MyLowHeal) && Bounded(kit.MyTalentScale) && kit.MyMaxStacks > 0 && kit.MySharedMaxStacks > 0;
				else if constexpr (std::is_same_v<T, Blaze2Kit>) return kit.MySkill <= Blaze2SkillKind::FURNACE && Bounded(kit.MyMeltdownScale) && Bounded(kit.MyMeltdownHeal) &&
					Bounded(kit.MyDownShield) && Bounded(kit.MyDownRegen) && Bounded(kit.MyReviveStun) && Bounded(kit.MyBurstMultiplier) && Bounded(kit.MyBurstSp) &&
					Bounded(kit.MyLoss) && Bounded(kit.MyAttackLoss) && Bounded(kit.MyBurnBonus) && Bounded(kit.MyAmmoRefill) && Bounded(kit.MyRadius) &&
					Bounded(kit.MyAidDuration) && Bounded(kit.MyAidInterval, 0.1) && Bounded(kit.MyArtsScale) && Bounded(kit.MyElementScale) && Bounded(kit.MyMoveMultiplier);
				else if constexpr (std::is_same_v<T, TitiKit>) return kit.MySkill <= TitiSkillKind::BLOOM && Bounded(kit.MyStillScale) && Bounded(kit.MyDreamScale) && Bounded(kit.MyTalentScale) &&
					Bounded(kit.MyHealthThreshold, 0, 1) && Bounded(kit.MyAuraSpeed, -1e9) && Bounded(kit.MySleep) && Bounded(kit.MySleepChance, 0, 1) &&
					Bounded(kit.MyMinScale) && Bounded(kit.MyMaxScale) && Bounded(kit.MyChainSleep) && Bounded(kit.MyRadius) && kit.MyChainTargets > 0;
				else if constexpr (std::is_same_v<T, Excu2Kit>) return kit.MySkill <= Excu2SkillKind::VERDICT && Bounded(kit.MyExtraChance, 0, 1) && Bounded(kit.MyChancePerAmmo) &&
					Bounded(kit.MyFactionAmmo) && Bounded(kit.MyFactionCap) && Bounded(kit.MyDodgeChance, 0, 1) && Bounded(kit.MyRefill) && Bounded(kit.MyAttackPerAmmo, -1e9) &&
					kit.MyMaxStacks > 0 && Bounded(kit.MyFinalScale) && Bounded(kit.MyHealScale) && (!kit.MyBaseHeal || Bounded(*kit.MyBaseHeal)) && Bounded(kit.MyCrowdSpeed, -1e9) && Bounded(kit.MyCrowdCount);
				else if constexpr (std::is_same_v<T, CetsyrKit>) return kit.MySkill <= CetsyrSkillKind::REWEAVE && (!kit.MyTraitRatio || Bounded(*kit.MyTraitRatio)) && kit.MyMotes <= 1000000 &&
					Bounded(kit.MyCooldown) && Bounded(kit.MySkillCooldown) && Bounded(kit.MyAllyRadius) && Bounded(kit.MyMoteDuration) && Bounded(kit.MyTraitScale) &&
					Bounded(kit.MyEnemyRadius) && Bounded(kit.MyMoteScale) && Bounded(kit.MyBind) && Bounded(kit.MyInspire) && Bounded(kit.MyRedistributeInterval, kit.MyOrbitMotes ? 0.1 : 0.5) &&
					Bounded(kit.MySarkazReduction, 0, 1) && Bounded(kit.MyModuleAttack, -1e9) && Bounded(kit.MyModuleCount) && Bounded(kit.MyAngularSpeed, -1e9) && Bounded(kit.MyBaseRatio);
				else if constexpr (std::is_same_v<T, GvialKit>) return kit.MySkill <= GvialSkillKind::DEFER && Bounded(kit.MyAttack, -1e9) && Bounded(kit.MyDefense, -1e9) &&
					Bounded(kit.MyAttackPerBlock, -1e9) && Bounded(kit.MyDefensePerBlock, -1e9) && Bounded(kit.MyHealthThreshold, 0, 1) && Bounded(kit.MyHealingScale) &&
					Bounded(kit.MyLowHealthHealingScale) && Bounded(kit.MyBlockedScale) && Bounded(kit.MyReductionThreshold, 0, 1) && Bounded(kit.MyPhysicalReduction, 0, 1) &&
					Bounded(kit.MyDeferral, 0, 0.99) && Bounded(kit.MyDelayDuration, 0.000001) && Bounded(kit.MyDelayInterval, BattleClock::StepSeconds) && Bounded(kit.MyLifeSteal) && Bounded(kit.MyForce, -1e9);
				else if constexpr (std::is_same_v<T, BillroKit>) return kit.MySkill <= BillroSkillKind::DEVOUR && Bounded(kit.MyAttack, -1e9) && Bounded(kit.MyKeepDefense, -1e9) &&
					Bounded(kit.MyKeepResistance, -1e9) && Bounded(kit.MyHeal) && Bounded(kit.MyChargedHeal) && Bounded(kit.MySpRecovery) && Bounded(kit.MyEnemyScale) &&
					Bounded(kit.MyEnemyCap) && Bounded(kit.MyMarkScale) && Bounded(kit.MyBind) && Bounded(kit.MySluggish);
				else if constexpr (std::is_same_v<T, BldskKit>) return Bounded(kit.MyBandageHeal) && Bounded(kit.MyAttack, -1e9) && Bounded(kit.MyDuration) && Bounded(kit.MyInterval, 0.1) &&
					Bounded(kit.MyHealthLoss) && Bounded(kit.MySelfSp) && Bounded(kit.MyAllySp) && Bounded(kit.MyHealthRatio, 0, 1) && Bounded(kit.MyHealScale);
				else if constexpr (std::is_same_v<T, FlamtlKit>)
				{
					AttributeModifiers check; check.Add(kit.MyBlockingModifiers);
					return kit.MySkill <= FlamtlSkillKind::FLAME && Bounded(kit.MyDp) && kit.MyPulses <= 1000000 && Bounded(kit.MyProbability, 0, 1) && Bounded(kit.MyScale) &&
						Bounded(kit.MyStun) && kit.MyTargets > 0 && Bounded(kit.MyDodge, 0, 1) && Bounded(kit.MyDodgeDuration) && Bounded(kit.MyNationDodge, 0, 1);
				}
				else if constexpr (std::is_same_v<T, FartthKit>) return kit.MySkill <= FartthSkillKind::LINE && Bounded(kit.MyQuietTime) && Bounded(kit.MyAttack, -1e9) &&
					Bounded(kit.MySurvivorSp) && Bounded(kit.MyFarScale) && Bounded(kit.MyDistanceScale) && Bounded(kit.MyMinDistance) && Bounded(kit.MyMaxDistance);
				else if constexpr (std::is_same_v<T, SpAuraKit>) return Bounded(kit.MyRecovery);
				else if constexpr (std::is_same_v<T, SvrashKit>) return Bounded(kit.MyRedeployMultiplier) && Bounded(kit.MyAdditionScale);
				else if constexpr (std::is_same_v<T, Texas2Kit>) return kit.MySkill <= Texas2SkillKind::RAIN && Bounded(kit.MyBurstScale) && Bounded(kit.MyBurstStun) &&
					Bounded(kit.MyScale) && Bounded(kit.MyStun) && Bounded(kit.MyInterval, 0.1) && kit.MyTargets > 0 && Bounded(kit.MyResistance, -1e9) &&
					Bounded(kit.MyDebuffDuration) && Bounded(kit.MySilence) && Bounded(kit.MyDotDuration) && Bounded(kit.MyDotDamage) && Bounded(kit.MyDotInterval, 0.1) &&
					Bounded(kit.MyHealRatio) && Bounded(kit.MyAttackSpeed, -1e9) && Bounded(kit.MyDamageReduction, 0, 1) && Bounded(kit.MyLonelyAttack, -1e9);
				else if constexpr (std::is_same_v<T, HsgumaKit>) return Bounded(kit.MyBlockProbability, 0, 1) && Bounded(kit.MyAuraDefense, -1e9) && Bounded(kit.MyBlockingDefense, -1e9) && Bounded(kit.MyCounterScale);
				else if constexpr (std::is_same_v<T, MudrokKit>)
				{
					AttributeModifiers check; check.Add(kit.MyAwakeModifiers); check.Add(kit.MyLonelyModifiers);
					return kit.MySkill <= MudrokSkillKind::DORMANT && Bounded(kit.MySleep) && Bounded(kit.MyMoveMultiplier) && Bounded(kit.MyStun) && Bounded(kit.MyProbability, 0, 1) &&
						Bounded(kit.MySkillHeal) && kit.MyMaxLayers >= 0 && kit.MyLayerGain >= 0 && Bounded(kit.MyLayerInterval, 0.000001) && Bounded(kit.MyLayerHeal) &&
						Bounded(kit.MySarkazReduction, 0, 1) && Bounded(kit.MyBlockedScale);
				}
				else if constexpr (std::is_same_v<T, GnosisKit>) return kit.MySkill <= GnosisSkillKind::HYPOTHERMIA && Bounded(kit.MyScale) && Bounded(kit.MyCold) && Bounded(kit.MyAttackCold) &&
					Bounded(kit.MyColdFragile) && Bounded(kit.MyFreezeFragile) && Bounded(kit.MySpRecovery) && Bounded(kit.MyResistDelay) && Bounded(kit.MyResist, 0, 1);
				else if constexpr (std::is_same_v<T, LionhdKit>) return Bounded(kit.MyScale) && Bounded(kit.MyResistance, -1e9) && Bounded(kit.MyDuration) && Bounded(kit.MyAttack, -1e9) && Bounded(kit.MyMaxStacks);
				else if constexpr (std::is_same_v<T, ReckprKit>) return Bounded(kit.MyGuardDuration) && Bounded(kit.MyGuardHeal) && Bounded(kit.MyProbability, 0, 1) && Bounded(kit.MySp) &&
					Bounded(kit.MyDuration) && Bounded(kit.MyAttackSpeed, -1e9) && kit.MyMaxStacks > 0 && Bounded(kit.MyHealthRatio, 0, 1) && Bounded(kit.MyHealScale);
				else if constexpr (std::is_same_v<T, CathyKit>) { AttributeModifiers check; check.Add(kit.MyForgeModifiers); return true; }
				else if constexpr (std::is_same_v<T, GladyKit>) return kit.MySkill <= GladySkillKind::TORNADO && Bounded(kit.MyForce, -1e9) && Bounded(kit.MyFarRadius) && Bounded(kit.MyFarForce) &&
					Bounded(kit.MyDragDamage) && Bounded(kit.MyDragDistance, 0.000001) && Bounded(kit.MyInterval, 0.1) && Bounded(kit.MyScale) && Bounded(kit.MyMoveMultiplier) &&
					Bounded(kit.MyRegenRatio) && Bounded(kit.MySeaReduction, 0, 1) && Bounded(kit.MyMassLimit) && Bounded(kit.MyMassScale);
				else return false;
			}, *_definition.MyOperatorKit);
			if (!valid) throw std::invalid_argument("invalid operator kit");
		}
		if (_definition.MyGenericSkill)
		{
			const auto& effect = *_definition.MyGenericSkill;
			if (_enemy || (effect.MyElement && *effect.MyElement >= Element::COUNT) ||
				(effect.MyBurstType && *effect.MyBurstType > DamageType::ELEMENTAL) || effect.MyCounterType > DamageType::ELEMENTAL ||
				(effect.MyShieldDecay && !(effect.MyShieldDuration > 0)) ||
				(!effect.MyDebuff.empty() && (effect.MyDebuffKey.empty() || effect.MyDebuffKey.size() > 256)) ||
				(effect.MyShield > 0 && (effect.MyShieldKey.empty() || effect.MyShieldKey.size() > 256))) throw std::invalid_argument("invalid generic skill effect");
			for (const auto value : {effect.MyProbability, effect.MySelfStun, effect.MyElementRatio, effect.MyForce.value_or(0), effect.MyHealAlly,
				effect.MyDebuffDuration, effect.MyDp, effect.MyLoseHp, effect.MyHealHp, effect.MyShield, effect.MyShieldDuration,
				effect.MyStartBurst, effect.MyEndBurst, effect.MyCounterScale, effect.MyCounterCooldown})
				if (!Bounded(value, -1e9)) throw std::invalid_argument("invalid generic skill parameter");
			for (const auto statuses : {effect.MyHitStatuses, effect.MyStartStatuses})
				for (const auto status : statuses) if (status.MyStatus >= CombatStatus::COUNT || !Bounded(status.MyDuration)) throw std::invalid_argument("invalid generic skill status");
			AttributeModifiers modifiers; modifiers.Add(effect.MyDebuff);
		}
		for (const auto& effect : _definition.MyEquipmentEffects)
		{
			const auto& p = effect.MyParameters;
			if (effect.MyKey.empty() || effect.MyKey.size() > 200 || effect.MyPartner.size() > 200 ||
				p.MyKind < EquipmentEffectKind::DISTANCE_DAMAGE || p.MyKind > EquipmentEffectKind::STEAM_HEART ||
				!Bounded(p.MyValue, -1e9) || !Bounded(p.MyExtra, -1e9) || !Bounded(p.MyProbability, -1e9) ||
				!Bounded(p.MyDuration) || !Bounded(p.MyInterval) || !Bounded(p.MyThreshold, -1e9) || p.MyMaximum > 1000000)
				throw std::invalid_argument("invalid equipment behaviour");
		}
		if (_definition.MyYanyou)
		{
			const auto& kit = *_definition.MyYanyou;
			if (_enemy || !Bounded(kit.MyRangeRadius, 0, 100) || !Bounded(kit.MyMoveSpeed) || !Bounded(kit.MyBurnRatio) ||
				!Bounded(kit.MyFragileMultiplier) || !Bounded(kit.MyFlameScale) || !Bounded(kit.MyFlameRadius, 0, 100) || kit.MyDeployLimit > 1000000)
				throw std::invalid_argument("invalid Yanyou kit");
		}
		if (_definition.MyMedic)
		{
			const auto& medic = *_definition.MyMedic;
			if (!Bounded(medic.MyHealSp) || !Bounded(medic.MyDeathSp) || !Bounded(medic.MySkillHpRatio, 0, 1) ||
				!Bounded(medic.MySkillHealMultiplier) || !Bounded(medic.MySkillExtraHeal)) throw std::invalid_argument("invalid medic kit");
		}
		if (_definition.MyHitArea)
		{
			const auto& area = *_definition.MyHitArea;
			if (!std::isfinite(area.MyWidth) || !std::isfinite(area.MyHeight) || !std::isgreater(area.MyWidth, 0) || !
				std::isgreater(area.MyHeight, 0) || !std::isfinite(area.MyOffsetX) || !std::isfinite(area.MyOffsetY))
				throw std::invalid_argument("invalid hit area");
		}
		if (_definition.MyTraitFrontRange)
			for (const auto offset : *_definition.MyTraitFrontRange)
				if (offset.MyRow < -100 || offset.MyRow > 100 || offset.MyColumn < -100 || offset.MyColumn > 100)
					throw std::invalid_argument("invalid trait range offset");
		const auto& profession = _definition.MyProfession;
		if (static_cast<unsigned>(profession.MyKind) > static_cast<unsigned>(ProfessionTrait::DOLLKEEPER) ||
			(_enemy && profession.MyKind != ProfessionTrait::NONE) || !Bounded(profession.MyAmmoMax) ||
			!Bounded(profession.MyFunnelInitial) || !Bounded(profession.MyFunnelDelta) || !Bounded(profession.MyFunnelMax) ||
			profession.MyStoreMax == std::numeric_limits<unsigned>::max() || !Bounded(profession.MyGuardDefense) ||
			!Bounded(profession.MyGuardResistance) || !Bounded(profession.MyDodge, 0, 1))
			throw std::invalid_argument("invalid profession definition");
		for (const auto value : {profession.MySelfHeal, profession.MyHealRatio, profession.MyDpOnKill, profession.MyHpDrain,
			profession.MyMerchantInterval, profession.MyMerchantCost, profession.MyRampMax, profession.MyRampInitial, profession.MyAuraRatio, profession.MyShockScale})
			if (!Bounded(value)) throw std::invalid_argument("invalid profession parameter");
		if (!Bounded(profession.MyDollDuration, 0, 3600) || !Bounded(profession.MyDollHealthMultiplier, 0.05))
			throw std::invalid_argument("invalid doll parameters");
		if (!Bounded(profession.MyRampTime, 1e-9)) throw std::invalid_argument("invalid profession ramp time");
		ValidateSkill(_definition.MySkill);
		ValidateAttack(_definition.MyAttack);
		if (_definition.MySkill.MyAttack)
			ValidateAttack(*_definition.MySkill.MyAttack);
		const auto& stats = _definition.MyStats;
		// 所有基值必须有限，避免 NaN 穿过排序或浮点转整数进入未定义行为。
		for (const auto value : {
				 stats.MyMaxHealth,
				 stats.MyAttack,
				 stats.MyDefense,
				 stats.MyResistance,
				 stats.MyAttackSpeed,
				 stats.MyBaseAttackTime,
				 stats.MyMoveSpeed,
				 stats.MyTaunt,
				 stats.MyRedeploySeconds,
				 stats.MyDeploymentCost,
				 stats.MySpRecovery,
				 stats.MyHealthRegen,
				 stats.MyMass,
				 stats.MyElementalResistance,
				 stats.MyElementResistance,
				 stats.MyDefenseIgnorePercent,
				 stats.MyDefenseIgnoreFlat,
				 stats.MyResistanceIgnorePercent,
				 stats.MyResistanceIgnoreFlat,
				 stats.MyPhysicalDodge,
				 stats.MyArtsDodge,
				 stats.MyDamageDealtMultiplier,
				 stats.MyPhysicalDealtMultiplier,
				 stats.MyArtsDealtMultiplier,
				 stats.MyDamageTakenMultiplier,
				 stats.MyPhysicalTakenMultiplier,
				 stats.MyArtsTakenMultiplier,
				 stats.MyTrueTakenMultiplier,
				 stats.MyElementTakenMultiplier,
				 stats.MyElementalTakenMultiplier,
				 stats.MyHealingDealtMultiplier,
				 stats.MyHealingTakenMultiplier,
				 stats.MyAttackScaleMultiplier,
				 stats.MyRedeployMultiplier,
				 stats.MySpCostFlat,
				 stats.MyExtraTargets,
				 stats.MyBlockRadiusScale
			 })
			if (!std::isfinite(value))
				throw std::invalid_argument("non-finite combat attribute");
		const auto& attack = _definition.MyAttack;
		if (_definition.MyId.empty() || _definition.MyId.size() > 128 || !Bounded(stats.MyMaxHealth, 1) || !
			Bounded(stats.MyAttack) || !Bounded(stats.MyDefense) || !Bounded(stats.MyResistance, 0, 100) || !
			Bounded(stats.MyAttackSpeed, -1e6, 1e6) || !Bounded(stats.MyBaseAttackTime, 0.1, 3600) || !
			Bounded(stats.MyMoveSpeed, 0, 100) || stats.MyBlockCount < 0 || stats.MyBlockCount > 100 || !
			Bounded(stats.MyTaunt, -1e6, 1e6) || !Bounded(stats.MyRedeploySeconds, 0, 3600) || !
			Bounded(stats.MyDeploymentCost) || attack.MyMaxTargets == 0 || attack.MyMaxTargets > 600 || static_cast<
				unsigned>(attack.MyPriority) > static_cast<unsigned>(TargetPriority::HEAVIEST) || static_cast<unsigned>(
				attack.MyDamageType) > static_cast<unsigned>(DamageType::ELEMENTAL) || !
			Bounded(attack.MyProjectileSpeed, 0, 1000) || !Bounded(attack.MyEnemyRange, 0, 100) || !
			Bounded(attack.MyAnimationDuration, 0, 3600) || (attack.MyAnimationHit && !Bounded(
				*attack.MyAnimationHit,
				0,
				attack.MyAnimationDuration
			)) || _definition.MyBlockWeight < 1 || _definition.MyBlockWeight > 100 || _definition.MyRange.size() > 399)
			throw std::invalid_argument("invalid combat definition: " + _definition.MyId);

		for (const auto offset : _definition.MyRange)
			if (offset.MyRow < -100 || offset.MyRow > 100 || offset.MyColumn < -100 || offset.MyColumn > 100)
				throw std::invalid_argument("invalid combat range offset");
	}

	Battle::Battle(BattleInput _input)
		: _MyInput(std::move(_input)), _MyRandom(_MyInput.MySeed)
	{
		if (_MyInput.MyFinalAssault)
		{
			auto& assault = _MyInput.MyFinalAssault->get();
			if (!_MyInput.MyBossBattle || (_MyInput.MySharedBoss && &_MyInput.MySharedBoss->get() != &assault.Pool()))
				throw std::invalid_argument("final assault requires its own boss pool");
			for (const auto& player : _MyInput.MyPlayers)
				if (std::ranges::find(assault.Players(), player.MyPlayerId, &BossPlayerProgress::MyPlayerId) == assault.Players().end())
					throw std::invalid_argument("battle player does not participate in final assault");
			_MyInput.MySharedBoss = std::ref(assault.Pool());
		}
		const bool unlimitedBoss = _MyInput.MyBossBattle && std::isinf(_MyInput.MyTimeLimit) && std::isgreater(_MyInput.MyTimeLimit, 0);
		if (static_cast<unsigned>(_MyInput.MyRulePositionMode) > static_cast<unsigned>(RulePositionMode::INITIAL))
			throw std::invalid_argument("invalid rule position mode");
		if (_MyInput.MyPlayers.empty() || _MyInput.MyPlayers.size() > 20 || _MyInput.MySpawns.size() > 10000 ||
			(!unlimitedBoss && !Bounded(_MyInput.MyTimeLimit, BattleClock::StepSeconds, 3600)) || !Bounded(_MyInput.MyInitialDp) || !Bounded(
				_MyInput.MyDpPerSecond
			) || !Bounded(_MyInput.MyMaxDp))
			throw std::invalid_argument("invalid battle input");
		if (_MyInput.MyField)
		{
			_MyGrid.emplace(*_MyInput.MyField);
			_MyHasTerrain = std::ranges::any_of(
				_MyInput.MyField->MyTiles,
				[](const FieldTile& _tile) { return _tile.MyTerrain != FieldTerrain::NONE; }
			) || std::ranges::any_of(_MyInput.MyField->MyAirflow, [](const auto& _flow) { return _flow.has_value(); });
		}
		// 道路仅用于查询，不执行 WAIT 等动作；位置仍在构造期验证，避免取整 NaN 或溢出。
		for (const auto& route : _MyInput.MyGroundRoutes)
		{
			ValidatePoint(route.MyStart, true);
			ValidatePoint(route.MyEnd, true);
			for (const auto& step : route.MySteps)
			{
				if (static_cast<unsigned>(step.MyKind) > static_cast<unsigned>(RouteStepKind::APPEAR))
					throw std::invalid_argument("invalid ground route step");
				if (step.MyKind == RouteStepKind::MOVE || step.MyKind == RouteStepKind::APPEAR)
					ValidatePoint(step.MyPosition, true);
			}
		}
		if (!std::ranges::is_sorted(_MyInput.MyEquipmentTemplates, {}, &EquipmentTemplate::MyId)) throw std::invalid_argument("equipment catalog must be sorted");
		for (const auto& item : _MyInput.MyEquipmentTemplates)
		{
			if (item.MyId.empty() || item.MyId.size() > 180 || item.MyStats.size() > 64 || item.MyEffects.size() > 64)
				throw std::invalid_argument("invalid equipment catalog entry");
			CombatDefinition check{.MyId = "equipment-template"};
			for (const auto& effect : item.MyEffects) check.MyEquipmentEffects.push_back({std::string(item.MyId), effect.MyParameters, std::string(effect.MyPartner)});
			ValidateDefinition(check, false);
			for (const auto& stat : item.MyStats) ValidateBuff(BuffDefinition{.MyKey = "item:" + std::string(item.MyId) + "@lend:stat:" + std::string(stat.MyBuff),
				.MyModifiers = std::vector<AttributeChange>(stat.MyModifiers.begin(), stat.MyModifiers.end())});
		}
		std::set<std::string, std::less<>> playerIds;
		std::set<std::pair<int, int>> occupied;
		for (const auto& player : _MyInput.MyPlayers)
		{
			if (player.MyPlayerId.empty() || !playerIds.insert(player.MyPlayerId).second || player.MyUnits.size() > 36)
				throw std::invalid_argument("invalid battle player");
			const auto owner = _MyPlayers.size();
			_MyPlayers.emplace_back(BattlePlayerState{.MyPlayerId = player.MyPlayerId, .MyDp = _MyInput.MyInitialDp});
			const auto firstUnit = _MyUnits.size();
			std::set<std::uint64_t> pieceIds;
			for (const auto& deployment : player.MyUnits)
			{
				ValidateDefinition(deployment.MyDefinition, false);
				ValidateContent(deployment.MyDefinition.MyContent, ContentTag::CUSTOM_OPERATOR);
				ValidatePoint(deployment.MyPosition);
				if (deployment.MyKind != UnitKind::OPERATOR && deployment.MyKind != UnitKind::TOKEN)
					throw std::invalid_argument("initial ally must be an operator or token");

				const auto position = deployment.MyPosition;
				if (std::floor(position.MyX) != position.MyX || std::floor(position.MyY) != position.MyY || !occupied.
					emplace(static_cast<int>(position.MyY), static_cast<int>(position.MyX)).second || deployment.
					MyPieceUid == 0 || !pieceIds.insert(deployment.MyPieceUid).second || static_cast<unsigned>(
						deployment.MyFacing) > static_cast<unsigned>(Facing::LEFT))
					throw std::invalid_argument("invalid ally deployment");
				auto& unit = _MyUnits.emplace_back();
				unit.MyId = static_cast<UnitId>(_MyUnits.size());
				_MyAllyIds.emplace_back(unit.MyId);
				unit.MyOwner = owner;
				unit.MyKind = deployment.MyKind;
				unit.MyCarry = deployment.MyCarry;
				unit.MyDeferred = deployment.MyDeferred;
				unit.MyDefinition = deployment.MyDefinition;
				unit.MyStats = ResolveStats(unit.MyDefinition.MyStats, {});
				unit.MyPieceUid = deployment.MyPieceUid;
				unit.MyPosition = unit.MyHome = deployment.MyPosition;
				unit.MyFacing = deployment.MyFacing;
				unit.MyGround = deployment.MyGround;
				unit.MyGroundPassable = deployment.MyGroundPassable;
				if (_MyGrid)
				{
					const auto row = static_cast<int>(unit.MyPosition.MyY), column = static_cast<int>(unit.MyPosition.
								   MyX);
					if (!_MyGrid->InRect(row, column))
						throw std::invalid_argument("deployment outside field rect");
					unit.MyGround = _MyGrid->Tile(row, column).MyLow;
					unit.MyGroundPassable = _MyGrid->GroundPassable(row, column, true);
				}
				unit.MyHealth = unit.MyStats.MyMaxHealth;
			}
			// 棋盘输入可先列出召唤物；全部创建后用同一玩家的棋子 UID 解析拥有者。
			for (std::size_t i = 0; i < player.MyUnits.size(); ++i)
			{
				const auto ownerUid = player.MyUnits[i].MyOwnerPieceUid;
				if (!ownerUid) continue;
				auto& unit = _MyUnits[firstUnit + i];
				for (std::size_t j = firstUnit; j < _MyUnits.size(); ++j)
					if (_MyUnits[j].MyPieceUid == ownerUid && _MyUnits[j].MyKind == UnitKind::OPERATOR)
						unit.MyOwnerUnit = _MyUnits[j].MyId;
				if (unit.MyKind != UnitKind::TOKEN || !unit.MyOwnerUnit)
					throw std::invalid_argument("token owner must be an operator in the same player input");
			}
			std::set<std::pair<std::uint64_t, std::string_view>> tokenTemplates;
			for (const auto& entry : player.MyTokenTemplates)
			{
				const auto ownerUnit = std::ranges::find_if(player.MyUnits, [&](const auto& _unit)
					{ return _unit.MyPieceUid == entry.MyOwnerPieceUid && _unit.MyKind == UnitKind::OPERATOR; });
				if (ownerUnit == player.MyUnits.end() || !tokenTemplates.emplace(entry.MyOwnerPieceUid, entry.MyDefinition.MyId).second)
					throw std::invalid_argument("token template requires a unique owner and token id");
				ValidateDefinition(entry.MyDefinition, false);
				ValidateContent(entry.MyDefinition.MyContent, ContentTag::CUSTOM_OPERATOR);
				for (const auto& buff : entry.MyDefinition.MyInitialBuffs) ValidateBuff(buff);
			}
		}
		if (_MyInput.MySharedBoss)
			for (const auto& player : _MyPlayers) _MyInput.MySharedBoss->get().PreparePlayer(player.MyPlayerId);
		_MyAllyCount = _MyUnits.size();
		DockSkillSummons();
		// 阵营 ID 表预留初始规模；单位本体分段存放，后续召唤不使已有引用失效。
		_MyAllyIds.reserve(_MyAllyCount + 16);
		_MyEnemyIds.reserve(_MyInput.MySpawns.size());
		for (const auto& spawn : _MyInput.MySpawns)
		{
			ValidateSpawnMetadata(spawn);
			ValidateDefinition(spawn.MyDefinition, true);
			ValidateContent(spawn.MyDefinition.MyContent, ContentTag::CUSTOM_ENEMY);
			ValidatePoint(spawn.MyRoute.MyStart, true);
			ValidatePoint(spawn.MyRoute.MyEnd);
			if (!Bounded(spawn.MyTime, 0, 3600) || spawn.MyRoute.MySteps.size() > 4096 || spawn.MyLifeCost < 0 || spawn.
				MyLifeCost > 1000000)
				throw std::invalid_argument("invalid enemy spawn");
			const auto owner = Owner(spawn.MyOwnerId);
			if (spawn.MyCounted)
				++_MyPlayers[owner].MyTotal;
			for (const auto& step : spawn.MyRoute.MySteps)
			{
				if (static_cast<unsigned>(step.MyKind) > static_cast<unsigned>(RouteStepKind::APPEAR) || !Bounded(
					step.MyWaitSeconds,
					0,
					3600
				))
					throw std::invalid_argument("invalid route step");
				ValidatePoint(step.MyPosition);
			}
		}
		for (const auto& player : _MyInput.MyPlayers)
		{
			auto& state = _MyPlayers[Owner(player.MyPlayerId)];
			state.MyBonds.reserve(player.MyBonds.size()); state.MyLayerGains.reserve(player.MyBonds.size());
			for (const auto& bond : player.MyBonds)
			{
				if (bond.MyCount < 0) throw std::invalid_argument("negative bond count");
				SetBondLayers(player.MyPlayerId, bond.MyId, bond.MyLayers);
				auto& live = *std::ranges::find(state.MyBonds, bond.MyId, &BondLayer::MyId);
				live.MyCount = bond.MyCount; live.MyActive = bond.MyActive; live.MyTier = bond.MyTier;
			}
		}
		std::ranges::stable_sort(_MyInput.MySpawns, {}, &EnemySpawn::MyTime);
		_MyTotal = static_cast<std::size_t>(std::ranges::count(_MyInput.MySpawns, true, &EnemySpawn::MyCounted));
		const auto capacity = _MyAllyCount + _MyInput.MySpawns.size();
		_MyTargets.reserve(capacity);
		_MyTargetCandidates.reserve(capacity);
		_MyProjectiles.reserve(capacity);
		_MyScheduled.reserve(capacity + _MyInput.MyColdWinds.size());
		_MyDueActions.reserve(capacity + _MyInput.MyColdWinds.size());
		for (const auto& wind : _MyInput.MyColdWinds) (void)StartColdWind(wind);
		_MyArrivedProjectiles.reserve(capacity);
		_MyRouteTails.reserve(_MyTotal);
		for (const auto& spawn : _MyInput.MySpawns)
		{
			const auto& route = spawn.MyRoute;
			auto& tail = _MyRouteTails.emplace_back(route.MySteps.size() + 2, 0.0);
			auto position = route.MyStart;
			for (std::size_t i = 0; i <= route.MySteps.size(); ++i)
			{
				const auto step = i == route.MySteps.size()
									  ? RouteStep{.MyKind = RouteStepKind::MOVE, .MyPosition = route.MyEnd}
									  : route.MySteps[i];
				if (step.MyKind == RouteStepKind::MOVE)
					tail[i] = Distance(position, step.MyPosition);
				if (step.MyKind == RouteStepKind::MOVE || step.MyKind == RouteStepKind::APPEAR)
					position = step.MyPosition;
			}
			for (std::size_t i = tail.size() - 1; i > 0; --i)
				tail[i - 1] += tail[i];
		}
		_MyRouteTailVersions.assign(_MyInput.MySpawns.size(), std::numeric_limits<std::uint64_t>::max());
		std::size_t customCount = _MyInput.MyContentBindings.size();
		for (const auto& unit : _MyUnits)
			customCount += IsCustom(unit.MyDefinition.MyContent.MyTag) ? 1U : 0U;
		for (const auto& spawn : _MyInput.MySpawns)
			customCount += IsCustom(spawn.MyDefinition.MyContent.MyTag) ? 1U : 0U;
		_MyContentInstances.reserve(customCount);
		for (const auto& unit : _MyUnits)
			AttachContent(unit.MyDefinition.MyContent, unit.MyId, 0, unit.MyOwner);
		_MyInsiderGrants.reserve(_MyAllyIds.size());
		_MyTexasUnits.reserve(_MyAllyIds.size());
		_MyEstells.reserve(_MyAllyIds.size()); _MyPodegos.reserve(_MyAllyIds.size());
		_MyUtages.reserve(_MyAllyIds.size());
		_MyWildmns.reserve(_MyAllyIds.size());
		_MySilents.reserve(_MyAllyIds.size()); _MySlchans.reserve(_MyAllyIds.size()); _MyHarolds.reserve(_MyAllyIds.size());
		_MyBubbles.reserve(_MyAllyIds.size()); _MyRockrs.reserve(_MyAllyIds.size());
		_MyKazemas.reserve(_MyAllyIds.size()); _MyAkkords.reserve(_MyAllyIds.size());
		_MySwire2s.reserve(_MyAllyIds.size()); _MyChampagnes.reserve(_MyAllyIds.size());
		_MyHainis.reserve(_MyAllyIds.size()); _MySnhunts.reserve(_MyAllyIds.size()); _MyBlemshs.reserve(_MyAllyIds.size());
		_MyBlockingDefenders.reserve(_MyAllyIds.size()); _MyCurseDolls.reserve(_MyAllyIds.size());
		_MyVulpises.reserve(_MyAllyIds.size()); _MyArchets.reserve(_MyAllyIds.size());
		_MyGladys.reserve(_MyAllyIds.size());
		_MyAromas.reserve(_MyAllyIds.size());
		_MyIneses.reserve(_MyAllyIds.size()); _MyRosesas.reserve(_MyAllyIds.size());
		_MyInitialSpCarriers.reserve(_MyAllyIds.size());
		_MyLockedFunnelUsers.reserve(_MyAllyIds.size());
		_MyVigils.reserve(_MyAllyIds.size()); _MyWolves.reserve(_MyAllyIds.size());
		_MyPendingOperatorReleases.reserve(_MyAllyIds.size());
		for (const auto id : _MyAllyIds)
		{
			InstallProfession(_MyUnits[Index(id)]);
			InstallOperatorKit(_MyUnits[Index(id)]);
		}
		_MyMedics.reserve(_MyAllyIds.size() + _MyInput.MyPlayers.size());
		for (const auto id : _MyAllyIds) InstallUnitEffects(_MyUnits[Index(id)]);
		if (_MyInput.MyYanyouDefinition)
		{
			ValidateDefinition(*_MyInput.MyYanyouDefinition, false);
			if (!_MyInput.MyYanyouDefinition->MyYanyou) throw std::invalid_argument("missing Yanyou kit");
		}
		for (const auto& variant : _MyInput.MyMapCharacters)
		{
			if (variant.MyMinimumElites > variant.MyMaximumElites) throw std::invalid_argument("invalid map character condition");
			for (const auto& character : variant.MyCharacters)
			{
				ValidateDefinition(character.MyDefinition, false);
				ValidateContent(character.MyDefinition.MyContent, ContentTag::CUSTOM_OPERATOR);
				for (const auto& slot : character.MyPositions)
				{
					ValidatePoint({static_cast<double>(slot.MyPosition.MyColumn), static_cast<double>(slot.MyPosition.MyRow)});
					if (slot.MyFacing > Facing::LEFT) throw std::invalid_argument("invalid map character direction");
				}
			}
		}
		InstallChoiceEffects();
		InstallBandEffects();
		InstallCoreBonds();
		InstallAddonBonds();
		InstallGarrisons();
		for (const auto& binding : _MyInput.MyContentBindings)
		{
			const auto tag = binding.MyContent.MyTag;
			if (tag != ContentTag::CUSTOM_BOND && tag != ContentTag::CUSTOM_BOND_EFFECT)
				throw std::invalid_argument("battle content binding must be a custom bond or bond effect");
			ValidateContent(binding.MyContent, tag);
			AttachContent(binding.MyContent, 0, 0, Owner(binding.MyPlayerId));
		}
	}

	std::size_t Battle::Owner(std::string_view _id) const
	{
		for (std::size_t i = 0; i < _MyPlayers.size(); ++i)
			if (_MyPlayers[i].MyPlayerId == _id)
				return i;
		throw std::invalid_argument("unknown battle player");
	}

	void Battle::Start()
	{
		if (_MyStarted || Finished())
			return;
		_MyStarted = true;
		for (const auto& device : _MyInput.MyDevices)
			if (!device.MyAfterDeployment) (void)SpawnDevice(device);
		const auto firstSequence = _MyDeploySequence;
		// 两阶段部署，每阶段按各玩家的第 i 个单位交替执行；初始 ID／行动顺序不改变。
		std::vector<std::vector<std::size_t>> orders(_MyPlayers.size());
		for (std::size_t i = 0; i < orders.size(); ++i) orders[i].reserve(_MyInput.MyPlayers[i].MyUnits.size());
		_MyStartDeploying = true;
		for (const auto kind : {UnitKind::OPERATOR, UnitKind::TOKEN})
		{
			for (auto& order : orders) order.clear();
			for (std::size_t i = 0; i < _MyAllyCount; ++i)
				if (_MyUnits[i].MyKind == kind && !_MyUnits[i].MyDeferred)
					orders[_MyUnits[i].MyOwner].emplace_back(i);
			std::size_t longest = 0;
			for (std::size_t i = 0; i < orders.size(); ++i)
			{
				std::ranges::sort(orders[i], [&](std::size_t _a, std::size_t _b)
				{
					const auto a = _MyUnits[_a].MyHome;
					const auto b = _MyUnits[_b].MyHome;
					if (std::islessgreater(a.MyX, b.MyX))
						return _MyInput.MyPlayers[i].MyMirrorDeployment ? std::isgreater(a.MyX, b.MyX) : std::isless(a.MyX, b.MyX);
					return std::isgreater(a.MyY, b.MyY);
				});
				longest = std::max(longest, orders[i].size());
			}
			for (std::size_t i = 0; i < longest; ++i)
				for (const auto& order : orders)
					if (i < order.size() && !Finished()) Deploy(_MyUnits[order[i]], true);
		}
		_MyStartDeploying = false;
		// 拥有者部署钩子带入的召唤物也参与仇恨重排，但其弹道生命标识保持原值。
		std::vector<UnitId> summons;
		summons.reserve(_MyAllyIds.size());
		for (const auto id : _MyAllyIds)
		{
			const auto& unit = Unit(id);
			if (unit.MyKind == UnitKind::TOKEN && unit.MyAlive && unit.MyDeploySequence > firstSequence)
				summons.emplace_back(id);
		}
		std::ranges::sort(summons, {}, [&](UnitId _id) { return Unit(_id).MyDeploySequence; });
		for (const auto id : summons) _MyUnits[Index(id)].MyAggroSequence = ++_MyDeploySequence;
		if (_MyEquipmentEnemyQueries || !_MyInput.MyEquipmentTemplates.empty()) BuildEnemyIndex();
		for (std::size_t i = 0; i < _MyAllyIds.size() && !Finished(); ++i)
		{
			auto& unit = _MyUnits[Index(_MyAllyIds[i])];
			if (unit.MyKind == UnitKind::OPERATOR && unit.MyAlive && unit.MyCarry && unit.MyCarry->MyDown)
			{
				unit.MyHealth = 0;
				RemoveUnit(unit, RemovalReason::FORCED_EXIT, 0, false);
			}
		}
		for (const auto& device : _MyInput.MyDevices)
			if (device.MyAfterDeployment) (void)SpawnDevice(device);
		for (const auto& turret : _MyInput.MyTurrets) (void)SpawnTurret(turret);
		ContentEvent event{.MyKind = ContentEventKind::BATTLE_START};
		if (!Finished())
			NotifyContent(event);
		// battleStart 的再部署倍率也覆盖刚刚强制退场的干员；按真实退场时间起算。
		for (const auto id : _MyAllyIds)
		{
			auto& unit = _MyUnits[Index(id)];
			if (unit.MyKind == UnitKind::OPERATOR && !unit.MyAlive && !unit.MyRemoved && unit.MyRemovalReason == RemovalReason::FORCED_EXIT)
				unit.MyRespawnAt = unit.MyRemovedAt + std::max(0.0, unit.MyStats.MyRedeploySeconds * unit.MyStats.MyRedeployMultiplier);
		}
	}

	bool Battle::Deploy(
		CombatUnit& _unit,
		bool _initial,
		std::optional<WorldPoint> _tile,
		std::optional<double> _keepSp
	)
	{
		const auto position = _tile.value_or(_unit.MyHome);
		if (_unit.MyAlive || _unit.MyRemoving || !CanDeploy(position, _unit.MyId))
			return false;
		const auto carry = (_initial || (!_unit.MyDeploySequence && _MyStartDeploying)) ? _unit.MyCarry : std::nullopt;
		const auto carrySp = carry && carry->MySp && std::isfinite(*carry->MySp) ? carry->MySp : std::nullopt;
		_unit.MyRemoved = false;
		_unit.MyBody.reset();
		_unit.MyDownAtHome = false;
		_unit.MyAlive = true;
		_unit.MyCountdown.reset();
		_unit.MyHidden = false;
		_unit.MyPosition = position;
		if (_MyGrid)
		{
			const auto row = static_cast<int>(position.MyY), column = static_cast<int>(position.MyX);
			const auto tile = _MyGrid->Tile(row, column);
			_unit.MyGround = tile.MyLow && !tile.MyDeploymentElevation.value_or(tile.MyElevated);
			_unit.MyGroundPassable = _MyGrid->GroundPassable(row, column, true);
		}
		_unit.MyHealth = _unit.MyStats.MyMaxHealth;
		_unit.MyAttackCooldown = 0;
		_unit.MyLastAttackAt = -std::numeric_limits<double>::infinity();
		_unit.MyStatuses = {};
		_unit.MyElements = {};
		_unit.MyDeploySequence = ++_MyDeploySequence;
		_unit.MyDeployedAt = Time();
		_unit.MyAggroSequence = _unit.MyDeploySequence;
		Recalculate(_unit);
		_unit.MyHealth = _unit.MyStats.MyMaxHealth;
		if (carry && _unit.MyKind == UnitKind::OPERATOR && carry->MyHealthRatio && std::isfinite(*carry->MyHealthRatio))
			_unit.MyHealth = std::max(1.0, _unit.MyStats.MyMaxHealth * std::clamp(*carry->MyHealthRatio, 0.01, 1.0));
		RefreshRange(_unit);
		ResetSkill(_unit, _initial, carrySp);
		if (_keepSp && !_unit.MySkill.MyActive && _unit.MyDefinition.MySkill.MyKind != SkillKind::PASSIVE)
			SetSpTotal(_unit.MyId, *_keepSp);
		Emit(BattleEventKind::DEPLOYED, _unit.MyId);
		RefreshTerrain(_unit, true);
		ContentEvent event{.MyKind = ContentEventKind::DEPLOY, .MyUnit = _unit.MyId, .MyInitial = _initial};
		NotifyContent(event);
		// 修改而非获得技力：部署时赠送的 SP 不与继承值叠加；已启动的持续技能不会被回填。
		if (carrySp && _unit.MyAlive) SetSpTotal(_unit.MyId, *carrySp);
		return true;
	}

	void Battle::SpawnDue()
	{
		while (_MyNextSpawn < _MyInput.MySpawns.size() && std::islessequal(
			_MyInput.MySpawns[_MyNextSpawn].MyTime,
			Time() + 1e-9
		))
		{
			const auto index = _MyNextSpawn++;
			(void)CreateEnemy(_MyInput.MySpawns[index], index);
			if (Finished())
				return;
		}
	}

	void Battle::Step()
	{
		// 内容回调可以请求结束，但不能重入时间推进；否则同一帧会被嵌套推进两次。
		if (_MyStepping)
			throw std::logic_error("battle step is not reentrant");
		struct StepGuard
		{
			bool& MyFlag;
			explicit StepGuard(bool& _flag) : MyFlag(_flag) { MyFlag = true; }
			~StepGuard() { MyFlag = false; }
		} guard(_MyStepping);
		if (Finished())
			return;
		Start();
		if (Finished())
			return;
		TickScheduled();
		if (Finished())
			return;
		SpawnDue();
		if (Finished())
			return;
		for (auto& player : _MyPlayers)
			player.MyDp = std::min(_MyInput.MyMaxDp, player.MyDp + _MyInput.MyDpPerSecond * BattleClock::StepSeconds);
		TickStatuses();
		TickBuffs();
		if (Finished())
			return;
		// 敌军阶段固定本帧长度；阶段中产生的敌军下一帧行动，友军召唤则沿原规则当帧行动。
		for (std::size_t i = 0, count = _MyEnemyIds.size(); i < count && !Finished(); ++i)
			if (auto& unit = _MyUnits[Index(_MyEnemyIds[i])]; unit.MyAlive)
				UpdateEnemy(unit);
		if (_MyEquipmentEnemyQueries || !_MyInput.MyEquipmentTemplates.empty()) BuildEnemyIndex();
		for (std::size_t i = 0; i < _MyAllyIds.size() && !Finished(); ++i)
			if (auto& unit = _MyUnits[Index(_MyAllyIds[i])]; unit.MyAlive && unit.MyKind != UnitKind::DEVICE)
				UpdateAlly(unit);
		if (Finished())
			return;
		UpdateProjectiles();
		if (Finished())
			return;
		CheckRedeploys();
		SyncBossUnits();
		TickTerrain();
		TickTurrets();
		ContentEvent event{.MyKind = ContentEventKind::TICK, .MyDelta = BattleClock::StepSeconds};
		NotifyContent(event);
		if (_MyContentFault && !Finished())
			Finish(BattleEndReason::FORCED);
		ReleaseOperatorHooks();
		if (Finished())
			return;
		_MyClock.Step();
		const bool livingEnemy = std::ranges::any_of(
			_MyUnits,
			[](const CombatUnit& _unit) { return _unit.MySide == UnitSide::ENEMY && _unit.MyAlive; }
		);
		const bool poolEmpty = _MyInput.MySharedBoss && !std::isgreater(_MyInput.MySharedBoss->get().Health(), 0);
		const bool poolHolds = _MyInput.MySharedBoss && _MyInput.MyBossBattle;
		if (poolEmpty || (_MyInput.MyAutoFinish && !poolHolds && _MyNextSpawn == _MyInput.MySpawns.size() && !livingEnemy))
			Finish(BattleEndReason::CLEARED);
		else if (std::isgreaterequal(Time(), _MyInput.MyTimeLimit - 1e-9))
			Finish(BattleEndReason::TIMEOUT);
	}

	void Battle::Advance(std::uint64_t _ticks)
	{
		while (_ticks-- > 0 && !Finished())
			Step();
	}

	void Battle::ForceEnd(BattleEndReason _reason)
	{
		if (_reason != BattleEndReason::FORCED && _reason != BattleEndReason::TIMEOUT) throw std::invalid_argument("invalid forced battle reason");
		if (Finished())
			return;
		Start();
		if (!Finished()) Finish(_reason);
	}

	void Battle::CheckRedeploys()
	{
		if (_MyEquipmentEnemyQueries || !_MyInput.MyEquipmentTemplates.empty()) BuildEnemyIndex();
		for (std::size_t i = 0; i < _MyAllyIds.size() && !Finished(); ++i)
		{
			const auto& unit = Unit(_MyAllyIds[i]);
			if (unit.MyAlive || unit.MyRemoved || unit.MyKind != UnitKind::OPERATOR || std::isless(
				Time() + 1e-9,
				unit.MyRespawnAt
			))
				continue;
			(void)Redeploy(unit.MyId, false);
		}
	}

	void Battle::Finish(BattleEndReason _reason)
	{
		if (_reason == BattleEndReason::TIMEOUT)
		{
			for (const auto id : _MyEnemyIds)
				if (auto& unit = _MyUnits[Index(id)]; unit.MyAlive && unit.MySpawnTag != EnemySpawnTag::BOSS)
					Leak(unit, true);
			_MyUnspawned = 0;
			for (std::size_t i = _MyNextSpawn; i < _MyInput.MySpawns.size(); ++i)
				if (_MyInput.MySpawns[i].MyCounted)
					++_MyUnspawned;
			_MyTotal -= _MyUnspawned;
			for (std::size_t i = _MyNextSpawn; i < _MyInput.MySpawns.size(); ++i)
				if (_MyInput.MySpawns[i].MyCounted)
					--_MyPlayers[Owner(_MyInput.MySpawns[i].MyOwnerId)].MyTotal;
		}
		_MyReason = _reason;
		if (_MyInput.MySharedBoss) _MyBossHealthAtEnd = _MyInput.MySharedBoss->get().Health();
		_MyProjectiles.clear();
		Emit(BattleEventKind::FINISHED);
		ContentEvent event{.MyKind = ContentEventKind::BATTLE_END};
		NotifyContent(event);
	}

	BattleResult Battle::Result() const
	{
		BattleResult result{
			.MyReason = _MyReason,
			.MyTime = Time(),
			.MyKilled = _MyKilled,
			.MyLeaked = _MyLeaked,
			.MyTotal = _MyTotal,
			.MyUnspawned = _MyUnspawned,
			.MyPlayers = _MyPlayers,
			.MyBossHealthLeft = Finished() ? _MyBossHealthAtEnd : _MyInput.MySharedBoss ? std::optional<double>(_MyInput.MySharedBoss->get().Health()) : std::nullopt
		};
		for (std::size_t i = 0; i < result.MyPlayers.size(); ++i)
			result.MyPlayers[i].MyUnitsEnd.reserve(_MyInput.MyPlayers[i].MyUnits.size());
		for (const auto id : _MyAllyIds)
		{
			const auto& unit = Unit(id);
			if (!unit.MyPieceUid || (unit.MyKind != UnitKind::OPERATOR && unit.MyKind != UnitKind::TOKEN)) continue;
			result.MyPlayers[unit.MyOwner].MyUnitsEnd.emplace_back(UnitEndState{
				.MyPieceUid = unit.MyPieceUid,
				.MyId = unit.MyId,
				.MyDefinitionId = unit.MyDefinition.MyId,
				.MyKind = unit.MyKind,
				.MyHealthRatio = unit.MyAlive ? std::clamp(unit.MyHealth / unit.MyStats.MyMaxHealth, 0.0, 1.0) : 0.0,
				.MySp = std::round(SpTotal(id) * 100) / 100,
				.MySkillActive = unit.MySkill.MyActive && unit.MyDefinition.MySkill.MyKind != SkillKind::PASSIVE,
				.MyAlive = unit.MyAlive});
		}
		if (_MyReason == BattleEndReason::TIMEOUT)
		{
			result.MyPendingEnemies.reserve(_MyInput.MySpawns.size() - _MyNextSpawn);
			for (std::size_t i = _MyNextSpawn; i < _MyInput.MySpawns.size(); ++i)
			{
				const auto& spawn = _MyInput.MySpawns[i];
				result.MyPendingEnemies.emplace_back(PendingEnemy{.MyEnemyId = spawn.MyDefinition.MyId, .MyTime = spawn.MyTime,
					.MyTag = spawn.MyTag, .MySourcePlayer = spawn.MySourcePlayer});
			}
		}
		return result;
	}

	void Battle::Emit(
		BattleEventKind _kind,
		UnitId _source,
		UnitId _target,
		double _amount,
		std::optional<CombatStatus> _status,
		std::size_t _player
	)
	{
		constexpr std::size_t eventLimit = 50000;
		if (_MyEvents.size() == eventLimit)
			_MyEvents.pop_front();
		_MyEvents.emplace_back(BattleEvent{.MyKind = _kind, .MyTick = Tick(), .MySource = _source, .MyTarget = _target,
			.MyAmount = _amount, .MyStatus = _status, .MyPlayer = _player});
	}

	std::vector<BattleEvent> Battle::DrainEvents()
	{
		std::vector<BattleEvent> events(_MyEvents.begin(), _MyEvents.end());
		_MyEvents.clear();
		return events;
	}
}
