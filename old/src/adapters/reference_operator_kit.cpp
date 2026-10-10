#include <stronghold/simulation/operator_kits.hpp>
#include <stronghold/adapters/reference_ally.hpp>

namespace Stronghold
{
	CombatDefinition AllyRecord::MakeKitDefinition(bool _genericTalents) const
	{
		if (!MyOperatorKit)
		{
			auto definition = MyTokenKit ? MakeDefinition(MyBaseAttack) : MakeGenericDefinition(_genericTalents);
			definition.MyTokenKit = MyTokenKit;
			if (MyTokenKit)
			{
				definition.MySkill = {};
				if (MyTokenKit->MyTrueDamage) definition.MyAttack.MyDamageType = DamageType::TRUE_DAMAGE;
				if (std::isgreater(MyTokenKit->MyBlockedScale, 1)) { definition.MyAttack.MyScaling = AttackScaling::BLOCKED; definition.MyAttack.MyConditionalScale = MyTokenKit->MyBlockedScale; }
				if (MyTokenKit->MyKind == TokenKitKind::OBELISK || MyTokenKit->MyKind == TokenKitKind::CAT_SHIELD || MyTokenKit->MyKind == TokenKitKind::ICE_TARGET || MyTokenKit->MyKind == TokenKitKind::ROSMON_GEAR || MyTokenKit->MyKind == TokenKitKind::SEABORN || MyTokenKit->MyKind == TokenKitKind::DELIVERY_TARGET || MyTokenKit->MyKind == TokenKitKind::CGBIRD_PHANTOM) definition.MyAttack.MyDisabled = true;
				definition.MyGenericSkill = nullptr;
				if (MyTokenKit->MyKind == TokenKitKind::MANIFOLD && MyTokenKit->MyManifold)
				{
					const auto& manifold = *MyTokenKit->MyManifold;
					definition.MyAttack.MyDisabled = true;
					definition.MySkill = {.MyKind = SkillKind::TOGGLE, .MyTrigger = SkillTrigger::NEVER,
						.MySpCost = manifold.MySpCost, .MyInitialSp = std::min(manifold.MySpCost, manifold.MyInitialSp)};
				}
				if (MyTokenKit->MyKind == TokenKitKind::CHAMPAGNE || MyTokenKit->MyKind == TokenKitKind::CURSE_DOLL)
				{
					definition.MySkill.MyKind = SkillKind::PASSIVE;
					definition.MyAttack.MyDisabled = true;
				}
			}
			if (MyStartingFlags) definition.MyInitialBuffs.push_back({.MyKey = "token:starting", .MyFlags = StatusFlags(MyStartingFlags),
				.MyPersistent = true, .MyAllowDead = true});
			return definition;
		}
		const auto& kit = *MyOperatorKit;
		auto definition = MakeDefinitionWithSkill(kit.MySkill ? kit.MySkill : MyGenericSkill, false);
		definition.MyOperatorKit = &kit.MyRules;
		if (!kit.MyBaseRange.empty()) definition.MyRange.assign(kit.MyBaseRange.begin(), kit.MyBaseRange.end());
		AppendPermanentTalents(definition, kit.MyTalents);
		const auto base = [&](AttackProfile& attack)
		{
			if (kit.MyBaseSluggish) { attack.MyOnHitStatus = CombatStatus::SLUGGISH; attack.MyOnHitApplication.MyDuration = *kit.MyBaseSluggish; }
			if (kit.MyBasePriority) attack.MyPriority = *kit.MyBasePriority;
		};
		base(definition.MyAttack);
		if (definition.MySkill.MyAttack) base(*definition.MySkill.MyAttack);
		if (kit.MySkillNoHeal) definition.MySkill.MyFlags.set(static_cast<std::size_t>(CombatStatus::NO_HEAL));
		if (kit.MyPriority || kit.MySluggish)
		{
			if (!definition.MySkill.MyAttack) definition.MySkill.MyAttack = definition.MyAttack;
			auto& attack = *definition.MySkill.MyAttack;
			if (kit.MyPriority) attack.MyPriority = *kit.MyPriority;
			if (kit.MySluggish) { attack.MyOnHitStatus = CombatStatus::SLUGGISH; attack.MyOnHitApplication.MyDuration = *kit.MySluggish; }
		}
		if (kit.MyChainNoFalloff && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyChainFalloff = 0;
		if (kit.MyHealing)
		{
			if (!definition.MySkill.MyAttack) definition.MySkill.MyAttack = definition.MyAttack;
			definition.MySkill.MyAttack->MyHealing = true;
			const auto* sunbr = std::get_if<SunbrKit>(&kit.MyRules);
			if (sunbr) definition.MySkill.MyAttack->MyRanged = false;
			if (!sunbr || !sunbr->MyCooking)
			{
				definition.MySkill.MyHealSkill = true;
				if (sunbr && MySkill && MySkill->MyHasRange)
				{
					definition.MySkill.MyTrigger = SkillTrigger::SKILL_RANGE;
					definition.MySkill.MyTriggerAllies = true;
					definition.MySkill.MyTriggerRange.assign(MySkill->MyRange.begin(), MySkill->MyRange.end());
				}
			}
		}
		if (const auto* silent = std::get_if<SilentKit>(&kit.MyRules); silent && silent->MyDrone)
			definition.MySkill.MyAttack.reset(); // 只补库存的瞬发技能不等待一次治疗攻击。
		if (const auto* standin = std::get_if<StandinKit>(&kit.MyRules))
		{
			if (standin->MyKind == StandinKind::SHARP_LORD && definition.MySkill.MyAttack)
			{
				definition.MySkill.MyAttack->MyScaling = AttackScaling::NONE;
				definition.MySkill.MyAttack->MyDamageMultiplier = 1;
			}
			if (standin->MyKind == StandinKind::STORMEYE && standin->MySkill == 3 && definition.MySkill.MyAttack)
				definition.MySkill.MyAttack->MyOperatorEachHit = true;
			if (standin->MyKind == StandinKind::TOUCH)
				definition.MyMedic = MedicKitDefinition{.MyHealSp = standin->MyHealSp, .MyDeathSp = standin->MyDeathSp,
					.MySkillHpRatio = standin->MyGospelHealthRatio, .MySkillHealMultiplier = standin->MyGospelHealScale, .MySkillExtraHeal = standin->MyGospelExtraHeal};
		}
		if (const auto* diy = std::get_if<DiyOperatorKit>(&kit.MyRules))
		{
			if (diy->MyKind == DiyOperatorKind::CHEN && diy->MySkill == 3)
			{
				definition.MySkill.MyFlags.set(static_cast<std::size_t>(CombatStatus::INVULNERABLE));
				definition.MySkill.MyFlags.set(static_cast<std::size_t>(CombatStatus::NO_BLOCK));
			}
			if (diy->MyKind == DiyOperatorKind::SIEGE && diy->MySkill == 2 && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyAllInRange = true;
			if (((diy->MyKind == DiyOperatorKind::SIEGE && diy->MySkill == 3) || (diy->MyKind == DiyOperatorKind::PALLAS && diy->MySkill == 2)) && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyOperatorEachHit = true;
			if (diy->MyKind == DiyOperatorKind::AMGOAT && diy->MySkill == 3 && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyAllInRange = true;
			if (((diy->MyKind == DiyOperatorKind::AMGOAT && diy->MySkill == 2) || (diy->MyKind == DiyOperatorKind::CHEN && diy->MySkill == 1)) && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyOperatorEachHit = true;
			if (diy->MyKind == DiyOperatorKind::POCA && diy->MySkill == 3 && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyProjectileSpeed = 0;
			if (diy->MyKind == DiyOperatorKind::CGBIRD && diy->MyBaseHealTargets)
			{
				definition.MyAttack.MyMaxTargets = diy->MyBaseHealTargets;
				if (definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyMaxTargets = diy->MyBaseHealTargets;
			}
		}
		const auto* wildmn = std::get_if<WildmnKit>(&kit.MyRules);
		const auto* liskam = std::get_if<LiskamKit>(&kit.MyRules);
		const auto* mint = std::get_if<MintKit>(&kit.MyRules);
		if ((wildmn && wildmn->MyCharge) || (liskam && liskam->MyArc) || (mint && mint->MyVortex))
		{
			if (!definition.MySkill.MyAttack) definition.MySkill.MyAttack = definition.MyAttack;
			definition.MySkill.MyAttack->MyOperatorSkillHit = true;
		}
		if (const auto* excu = std::get_if<ExcuKit>(&kit.MyRules); excu && excu->MyAllFront)
		{
			if (!definition.MySkill.MyAttack) definition.MySkill.MyAttack = definition.MyAttack;
			definition.MySkill.MyAttack->MyDamageMultiplier = definition.MyAttack.MyConditionalScale;
			definition.MySkill.MyAttack->MyScaling = AttackScaling::NONE;
		}
		if (std::holds_alternative<GrabdsKit>(kit.MyRules))
		{
			definition.MyAttack.MyOnHitStatus.reset();
			if (definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyOnHitStatus.reset();
		}
		if (const auto* gravel = std::get_if<GravelKit>(&kit.MyRules))
		{
			definition.MyStats.MyDeploymentCost = std::max(0.0, definition.MyStats.MyDeploymentCost + gravel->MyCost);
			definition.MyAttack.MyHits = 2;
			definition.MyAttack.MyHitMultiplier = 0.5;
			if (definition.MySkill.MyAttack)
			{
				definition.MySkill.MyAttack->MyHits = 2;
				definition.MySkill.MyAttack->MyHitMultiplier = 0.5;
			}
		}
		if (std::holds_alternative<TippiKit>(kit.MyRules))
		{
			definition.MyAttack.MyBlockFlying = false;
			if (definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyBlockFlying = false;
			definition.MySkill.MyFlags.set(static_cast<std::size_t>(CombatStatus::BLOCK_FLYING));
			definition.MySkill.MyFlags.set(static_cast<std::size_t>(CombatStatus::LIFTOFF));
		}
		if (const auto* flower = std::get_if<FlowerKit>(&kit.MyRules); flower && flower->MyTargets)
		{
			definition.MyAttack.MyMaxTargets = flower->MyTargets;
			if (definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyMaxTargets = flower->MyTargets;
		}
		if (const auto* whitew = std::get_if<WhitewKit>(&kit.MyRules))
		{
			const auto silence = [&](AttackProfile& _attack)
			{
				if (!std::isgreater(whitew->MySilence, 0)) return;
				_attack.MyOnHitStatus = CombatStatus::SILENCE;
				_attack.MyOnHitApplication.MyDuration = whitew->MySilence;
			};
			silence(definition.MyAttack);
			if (definition.MySkill.MyAttack) silence(*definition.MySkill.MyAttack);
			if (whitew->MyWolfSoul && definition.MySkill.MyAttack)
			{
				definition.MySkill.MyAttack->MyScaling = AttackScaling::NONE;
				definition.MySkill.MyAttack->MyDamageMultiplier = 1;
			}
		}
		if (const auto* branch = std::get_if<BranchKit>(&kit.MyRules); branch && branch->MyResolve && definition.MySkill.MyAttack)
			definition.MySkill.MyAttack->MyHitAllBlocked = true;
		if (const auto* ashlok = std::get_if<AshlokKit>(&kit.MyRules); ashlok && ashlok->MyRangedSkill)
		{
			definition.MySkill.MyFlags.set(static_cast<std::size_t>(CombatStatus::NO_BLOCK));
			if (definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyFortress = false;
		}
		if (const auto* skadi = std::get_if<SkadiKit>(&kit.MyRules))
			definition.MyStats.MyRedeploySeconds = std::max(0.0, definition.MyStats.MyRedeploySeconds + skadi->MyRedeploy);
		if (const auto* pinecn = std::get_if<PinecnKit>(&kit.MyRules))
		{
			if (pinecn->MySpike && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyTags = DamageTag::SKILL | DamageTag::PINECN_SPIKE;
			else if (!definition.MySkill.MyRange.empty()) definition.MySkill.MyTriggerRange = definition.MySkill.MyRange;
		}
		if (const auto* blemsh = std::get_if<BlemshKit>(&kit.MyRules))
		{
			definition.MyAttack.MyHitSleep = true;
			definition.MySkill.MyHealSkill = false;
			if (definition.MySkill.MyAttack)
			{
				definition.MySkill.MyAttack->MyHitSleep = true;
				definition.MySkill.MyAttack->MyOperatorSkillHit = blemsh->MySkill != BlemshSkillKind::SLEEP;
			}
		}
		if (const auto* malist = std::get_if<MalistKit>(&kit.MyRules))
		{
			const auto insight = [&](AttackProfile& _attack, bool _ramp)
			{
				_attack.MyScaling = AttackScaling::NONE;
				_attack.MyPerTargetFunnel = _ramp;
				_attack.MyCriticalProbability = malist->MyProbability;
				_attack.MyCriticalScale = malist->MyCriticalScale;
			};
			insight(definition.MyAttack, true);
			if (definition.MySkill.MyAttack) insight(*definition.MySkill.MyAttack, !malist->MyDouble);
		}
		if (const auto* slbell = std::get_if<WeakeningKit>(&kit.MyRules); slbell && slbell->MyTargets > 1)
		{
			definition.MyAttack.MyMaxTargets = slbell->MyTargets;
			if (definition.MySkill.MyAttack && !slbell->MySlow) definition.MySkill.MyAttack->MyMaxTargets = slbell->MyTargets;
		}
		if (const auto* shotst = std::get_if<ShotstKit>(&kit.MyRules))
		{
			definition.MyAttack.MyScaling = AttackScaling::FLYING;
			definition.MyAttack.MyConditionalScale *= shotst->MyFlyingScale;
			if (definition.MySkill.MyAttack)
			{
				definition.MySkill.MyAttack->MyScaling = AttackScaling::FLYING;
				definition.MySkill.MyAttack->MyConditionalScale *= shotst->MyFlyingScale;
				definition.MySkill.MyAttack->MyOperatorSkillHit = !shotst->MyBurst;
			}
		}
		if (const auto* vulpis = std::get_if<VulpisKit>(&kit.MyRules); vulpis && definition.MySkill.MyAttack)
		{
			if (vulpis->MySkill == VulpisSkillKind::CAMOUFLAGE)
			{
				definition.MySkill.MyAttack->MyHitAllBlocked = true;
				definition.MySkill.MyAttack->MyOnHitStatus = CombatStatus::STUN;
				definition.MySkill.MyAttack->MyOnHitApplication.MyDuration = vulpis->MyStun;
			}
			else if (vulpis->MySkill == VulpisSkillKind::PUNISH) definition.MySkill.MyAttack->MyOperatorSkillHit = true;
		}
		if (const auto* kjera = std::get_if<KjeraKit>(&kit.MyRules))
		{
			if (!kjera->MyLockDrones) definition.MyAttack.MyScaling = AttackScaling::NONE;
			definition.MyAttack.MyPerTargetFunnel = !kjera->MyLockDrones;
			if (definition.MySkill.MyAttack)
			{
				if (!kjera->MyLockDrones || kjera->MyExtraDrones) definition.MySkill.MyAttack->MyScaling = AttackScaling::NONE;
				definition.MySkill.MyAttack->MyPerTargetFunnel = !kjera->MyLockDrones;
				definition.MySkill.MyAttack->MyLockedFunnel = kjera->MyLockDrones && kjera->MyExtraDrones;
				definition.MySkill.MyAttack->MyOperatorSkillHit = kjera->MyExtraDrones;
			}
		}
		if (const auto* archet = std::get_if<ArchetKit>(&kit.MyRules); archet && archet->MySkill == ArchetSkillKind::SCATTER && definition.MySkill.MyAttack)
			definition.MySkill.MyAttack->MyOperatorSkillHit = true;
		if (const auto* mostma = std::get_if<MostmaKit>(&kit.MyRules); mostma && mostma->MySkill == MostmaSkillKind::RIPPLE && definition.MySkill.MyAttack)
		{
			definition.MySkill.MyAttack->MyAllInRange = true;
			definition.MySkill.MyAttack->MyRanged = false;
			definition.MySkill.MyAttack->MySplashRadius = 0;
			definition.MySkill.MyAttack->MyOperatorSkillHit = true;
		}
		if (std::holds_alternative<BldskKit>(kit.MyRules)) definition.MySkill.MyAttack.reset();
		if (const auto* gvial = std::get_if<GvialKit>(&kit.MyRules); gvial && gvial->MySkill == GvialSkillKind::PULL && definition.MySkill.MyAttack) {
			definition.MySkill.MyAttack->MyOperatorEachHit = !gvial->MyHiddenVariant;
			definition.MySkill.MyAttack->MyOperatorSkillHit = gvial->MyHiddenVariant;
		}
		if (const auto* gvial = std::get_if<GvialKit>(&kit.MyRules); gvial && gvial->MyHiddenVariant && std::isgreater(gvial->MyBlockedScale, 1))
		{
			definition.MyAttack.MyScaling = AttackScaling::BLOCKED; definition.MyAttack.MyConditionalScale = gvial->MyBlockedScale;
			if (definition.MySkill.MyAttack) { definition.MySkill.MyAttack->MyScaling = AttackScaling::BLOCKED; definition.MySkill.MyAttack->MyConditionalScale = gvial->MyBlockedScale; }
		}
		if (const auto* svrash = std::get_if<SvrashKit>(&kit.MyRules); svrash && svrash->MySlash && definition.MySkill.MyAttack)
		{
			definition.MySkill.MyAttack->MyRanged = false; definition.MySkill.MyAttack->MyScaling = AttackScaling::NONE;
			definition.MySkill.MyAttack->MyDamageMultiplier = 1;
		}
		if ((std::holds_alternative<LumenKit>(kit.MyRules) && std::get<LumenKit>(kit.MyRules).MySkill == LumenSkillKind::SHOWER) ||
			(std::holds_alternative<Agoat2Kit>(kit.MyRules) && std::get<Agoat2Kit>(kit.MyRules).MySkill == Agoat2SkillKind::VEIL)) definition.MySkill.MyAttack.reset();
		if (const auto* rosmon = std::get_if<RosmonKit>(&kit.MyRules); rosmon && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyOperatorEachHit = rosmon->MySkill == RosmonSkillKind::THOUGHT;
		if (const auto* whitw = std::get_if<Whitw2Kit>(&kit.MyRules))
		{
			if (whitw->MySkill == Whitw2SkillKind::LAZY)
			{
				definition.MyAttack.MyHits = 2;
				if (definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyHits = 2;
			}
			else if (whitw->MySkill == Whitw2SkillKind::HUNT && definition.MySkill.MyAttack)
			{
				definition.MySkill.MyAttack->MyPerTargetFunnel = true;
				definition.MySkill.MyAttack->MyFunnelPerHit = true;
				definition.MySkill.MyAttack->MyOperatorEachHit = true;
			}
		}
		if (const auto* cello = std::get_if<CelloKit>(&kit.MyRules))
		{
			if (cello->MyOnlySkill) { definition.MyAttack.MyOnlyDuringSkill = true; definition.MyAttack.MyPriority = TargetPriority::NOT_BURST; }
			if (definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyOperatorSkillHit = cello->MySkill == CelloSkillKind::ECSTASY;
		}
		if (const auto* halo = std::get_if<Halo2Kit>(&kit.MyRules); halo && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyOperatorSkillHit = halo->MySkill != Halo2SkillKind::LINKS;
		if (const auto* agoat = std::get_if<Agoat2Kit>(&kit.MyRules); agoat && agoat->MySkill == Agoat2SkillKind::ECHO && definition.MySkill.MyAttack)
			definition.MySkill.MyAttack->MyElementHealRatio = definition.MyAttack.MyElementHealRatio * agoat->MyHealScale;
		if (const auto* nearl = std::get_if<Nearl2Kit>(&kit.MyRules))
		{
			if (nearl->MySkill == Nearl2SkillKind::NIGHT) { definition.MySkill.MySpType = SpType::NONE; definition.MySkill.MySpCost = 0; }
			if (std::isgreater(nearl->MyBlockedScale, 1))
			{
				definition.MyAttack.MyScaling = AttackScaling::BLOCKED; definition.MyAttack.MyConditionalScale = nearl->MyBlockedScale;
				if (definition.MySkill.MyAttack) { definition.MySkill.MyAttack->MyScaling = AttackScaling::BLOCKED; definition.MySkill.MyAttack->MyConditionalScale = nearl->MyBlockedScale; }
			}
		}
		if (const auto* siege = std::get_if<Siege2Kit>(&kit.MyRules); siege && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyOperatorSkillHit = siege->MySkill == Siege2SkillKind::REFORGE;
		if (const auto* pasngr = std::get_if<PasngrKit>(&kit.MyRules); pasngr && definition.MySkill.MyAttack)
		{
			definition.MySkill.MyAttack->MyChainCount = pasngr->MyChainCount;
			if (pasngr->MyFalloff) definition.MySkill.MyAttack->MyChainFalloff = *pasngr->MyFalloff;
			if (pasngr->MySluggish) definition.MySkill.MyAttack->MyChainSluggish = *pasngr->MySluggish;
		}
		if (const auto* pepe = std::get_if<PepeKit>(&kit.MyRules); pepe && pepe->MySkill == PepeSkillKind::TREMOR && definition.MySkill.MyAttack)
		{ definition.MySkill.MyAttack->MySplashRadius = 1; definition.MySkill.MyAttack->MyOperatorSkillHit = true; }
		if (const auto* qiubai = std::get_if<QiubaiKit>(&kit.MyRules); qiubai && definition.MySkill.MyAttack)
		{
			definition.MySkill.MyAttack->MyOperatorSkillHit = qiubai->MySkill == QiubaiSkillKind::FEATHER;
			if (qiubai->MySkill == QiubaiSkillKind::SNOW) definition.MySkill.MyAttack->MyScaling = AttackScaling::NONE;
		}
		if (const auto* mlynar = std::get_if<MlynarKit>(&kit.MyRules); mlynar && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyCanHitFlying = mlynar->MySkill == MlynarSkillKind::GLORY;
		if (const auto* f12yin = std::get_if<F12yinKit>(&kit.MyRules); f12yin && definition.MySkill.MyAttack)
		{
			definition.MySkill.MyAttack->MyHitAllBlocked = f12yin->MySkill == F12yinSkillKind::SWEEP;
			definition.MySkill.MyAttack->MyOperatorEachHit = f12yin->MySkill == F12yinSkillKind::QUAKE;
		}
		if (const auto* nymph = std::get_if<NymphKit>(&kit.MyRules); nymph && nymph->MySkill == NymphSkillKind::FEAR && definition.MySkill.MyAttack)
		{ definition.MySkill.MyAttack->MySplashRadius = nymph->MySplashRadius; definition.MySkill.MyAttack->MyOperatorSkillHit = true; }
		if (const auto* svash = std::get_if<Svash2Kit>(&kit.MyRules); svash && svash->MySkill == Svash2SkillKind::CHANGE && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyOperatorEachHit = true;
		if (const auto* ghost = std::get_if<Ghost2Kit>(&kit.MyRules))
		{
			definition.MyProfession.MyDollNoAttack = true;
			if (ghost->MySkill == Ghost2SkillKind::WEIGHT && definition.MySkill.MyAttack)
			{ definition.MySkill.MyAttack->MyHitAllBlocked = true; definition.MySkill.MyAttack->MyOperatorEachHit = true; }
		}
		if (const auto* dusk = std::get_if<DuskKit>(&kit.MyRules); dusk && definition.MySkill.MyAttack)
		{
			definition.MySkill.MyAttack->MySplashRadius = dusk->MySkill == DuskSkillKind::INK ? 0 : 1.7;
			definition.MySkill.MyAttack->MyAllInRange = dusk->MySkill == DuskSkillKind::INK;
		}
		if (const auto* demkni = std::get_if<DemkniKit>(&kit.MyRules))
		{
			definition.MySkill.MyHealSkill = demkni->MySkill == DemkniSkillKind::MEDICINE;
			if (demkni->MySkill == DemkniSkillKind::TRIAGE && definition.MySkill.MyAttack)
			{
				definition.MySkill.MyTriggerAllies = true; definition.MySkill.MyTriggerHpAtMost = 0.5;
				definition.MySkill.MyTriggerRange.assign(demkni->MyRange.begin(), demkni->MyRange.end());
				definition.MySkill.MyAttack->MyHealing = true; definition.MySkill.MyAttack->MyRanged = false; definition.MySkill.MyAttack->MyHealHpAtMost = 0.5;
			}
		}
		if (const auto* horn = std::get_if<HornKit>(&kit.MyRules))
		{
			if (std::isgreater(horn->MyBlockedScale, 1))
			{
				definition.MyAttack.MyScaling = AttackScaling::BLOCKED; definition.MyAttack.MyConditionalScale = horn->MyBlockedScale;
				if (definition.MySkill.MyAttack) { definition.MySkill.MyAttack->MyScaling = AttackScaling::BLOCKED; definition.MySkill.MyAttack->MyConditionalScale = horn->MyBlockedScale; }
			}
			if (horn->MySkill == HornSkillKind::FLARE && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyOperatorSkillHit = true;
		}
		if (const auto* surtr = std::get_if<SurtrKit>(&kit.MyRules); surtr && surtr->MySkill == SurtrSkillKind::BLADE && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyOperatorSkillHit = true;
		if (const auto* blaze = std::get_if<Blaze2Kit>(&kit.MyRules); blaze && definition.MySkill.MyAttack)
		{
			definition.MySkill.MyAttack->MyOperatorEachHit = blaze->MySkill == Blaze2SkillKind::GROUND;
			if (blaze->MySkill == Blaze2SkillKind::FURNACE) { definition.MySkill.MyAttack->MySplashRadius = 1.7; definition.MySkill.MyAttack->MySplashHitsFlying = true; }
		}
		if (const auto* titi = std::get_if<TitiKit>(&kit.MyRules))
		{
			definition.MyAttack.MyHitSleep = true;
			if (definition.MySkill.MyAttack) { definition.MySkill.MyAttack->MyHitSleep = true; definition.MySkill.MyAttack->MyOperatorSkillHit = titi->MySkill != TitiSkillKind::WARD; }
		}
		if (const auto* excu = std::get_if<Excu2Kit>(&kit.MyRules))
		{
			if (excu->MyBaseHeal) definition.MyProfession.MySelfHeal = *excu->MyBaseHeal;
			if (excu->MySkill == Excu2SkillKind::VERDICT && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyOperatorEachHit = true;
		}
		if (std::holds_alternative<Texas2Kit>(kit.MyRules)) { definition.MySkill.MySpType = SpType::NONE; definition.MySkill.MySpCost = 0; }
		if (const auto* hsguma = std::get_if<HsgumaKit>(&kit.MyRules); hsguma && hsguma->MySaw && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyAllInRange = true;
		if (const auto* mudrok = std::get_if<MudrokKit>(&kit.MyRules); mudrok && definition.MySkill.MyAttack)
		{
			if (mudrok->MySkill == MudrokSkillKind::HAMMER)
			{
				definition.MySkill.MyAttack->MyAllInRange = true; definition.MySkill.MyAttack->MyCanHitFlying = false;
				definition.MySkill.MyAttack->MyGroundOnly = true; definition.MySkill.MyAttack->MyOperatorSkillHit = true;
			}
			else definition.MySkill.MyAttack->MyHitAllBlocked = true;
		}
		if (const auto* glady = std::get_if<GladyKit>(&kit.MyRules); glady && definition.MySkill.MyAttack)
		{
			definition.MySkill.MyAttack->MyOperatorEachHit = glady->MySkill == GladySkillKind::GRASP;
			definition.MySkill.MyAttack->MyOperatorSkillHit = glady->MySkill == GladySkillKind::RIP;
		}
		if (const auto* aroma = std::get_if<AromaKit>(&kit.MyRules); aroma && !aroma->MyLanding && definition.MySkill.MyAttack)
			definition.MySkill.MyAttack->MyOperatorEachHit = true;
		if (const auto* ines = std::get_if<InesKit>(&kit.MyRules))
		{
			if (ines->MySkill == InesSkillKind::STEALTH) definition.MySkill.MyFlags.set(static_cast<std::size_t>(CombatStatus::STEALTH));
			if (ines->MySkill == InesSkillKind::RECALL) { definition.MySkill.MySpType = SpType::NONE; definition.MySkill.MySpCost = 0; }
			if (definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyOperatorSkillHit = ines->MySkill == InesSkillKind::BLEED;
		}
		if (const auto* precision = std::get_if<PrecisionKit>(&kit.MyRules))
		{
			if (precision->MyCamouflage) definition.MySkill.MyFlags.set(static_cast<std::size_t>(CombatStatus::CAMOUFLAGE));
			if (definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyProgressiveHits = precision->MyProgressive;
		}
		if (const auto* rmixer = std::get_if<RmixerKit>(&kit.MyRules))
		{
			definition.MySkill.MyNoRangeExtend = rmixer->MySkill == RmixerSkillKind::COUNTER && !definition.MySkill.MyRange.empty();
			if (definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyOperatorSkillHit = rmixer->MySkill == RmixerSkillKind::RELOAD;
		}
		return definition;
	}
}
