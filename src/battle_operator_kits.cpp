#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		bool HasBeforeAttackHook(const OperatorKitDefinition& _kit)
		{
			if (std::holds_alternative<VignaKit>(_kit) || std::holds_alternative<IndigoKit>(_kit) || std::holds_alternative<PapyrsKit>(_kit) || std::holds_alternative<BlemshKit>(_kit) || std::holds_alternative<MalistKit>(_kit) || std::holds_alternative<KjeraKit>(_kit)) return true;
			if (const auto* bldsk = std::get_if<BldskKit>(&_kit); bldsk && bldsk->MyBandage) return true;
			if (std::holds_alternative<LumenKit>(_kit) || std::holds_alternative<Halo2Kit>(_kit) || std::holds_alternative<Agoat2Kit>(_kit)) return true;
			if (std::holds_alternative<PepeKit>(_kit) || std::holds_alternative<RosmonKit>(_kit)) return true;
			if (std::holds_alternative<F12yinKit>(_kit)) return true;
			if (const auto* svash = std::get_if<Svash2Kit>(&_kit); svash && svash->MySkill == Svash2SkillKind::CHANGE) return true;
			if (const auto* ghost = std::get_if<Ghost2Kit>(&_kit); ghost && ghost->MySkill == Ghost2SkillKind::WEIGHT) return true;
			if (const auto* dusk = std::get_if<DuskKit>(&_kit); dusk && dusk->MySkill == DuskSkillKind::FREEHAND) return true;
			if (const auto* surtr = std::get_if<SurtrKit>(&_kit); surtr && surtr->MySkill == SurtrSkillKind::GIANT) return true;
			if (std::holds_alternative<FlamtlKit>(_kit) || std::holds_alternative<EtlchiKit>(_kit)) return true;
			if (const auto* fartth = std::get_if<FartthKit>(&_kit); fartth && fartth->MySkill == FartthSkillKind::ALLIED) return true;
			if (const auto* gnosis = std::get_if<GnosisKit>(&_kit); gnosis && gnosis->MySkill == GnosisSkillKind::HYPOTHERMIA) return true;
			if (const auto* precision = std::get_if<PrecisionKit>(&_kit); precision && precision->MyVolleyExtra) return true;
			if (const auto* vigil = std::get_if<VigilKit>(&_kit); vigil && vigil->MySkill == VigilSkillKind::GIFT) return true;
			if (const auto* harold = std::get_if<HaroldKit>(&_kit); harold && harold->MyTriage) return true;
			if (const auto* rockr = std::get_if<RockrKit>(&_kit); rockr && rockr->MyOverload) return true;
			const auto* prove = std::get_if<ProveKit>(&_kit);
			return prove && prove->MyHunt;
		}
	}

	void Battle::InstallOperatorKit(CombatUnit& _unit)
	{
		const auto* kit = _unit.MyDefinition.MyOperatorKit;
		if (!kit) return;
		if (HasBeforeAttackHook(*kit)) ++_MyOperatorBeforeAttackHandlers;
		if (const auto* precision = std::get_if<PrecisionKit>(kit); precision && precision->MyVolleyExtra) _MyInitialSpCarriers.push_back(_unit.MyId);
		if (std::holds_alternative<VendlaKit>(*kit)) _MyVendlas.push_back({.MyUnit = _unit.MyId});
		if (std::holds_alternative<TexasKit>(*kit)) _MyTexasUnits.push_back(_unit.MyId);
		if (std::holds_alternative<EstellKit>(*kit)) _MyEstells.push_back(_unit.MyId);
		if (std::holds_alternative<UtageKit>(*kit)) _MyUtages.push_back(_unit.MyId);
		if (std::holds_alternative<WildmnKit>(*kit)) _MyWildmns.push_back(_unit.MyId);
		if (std::holds_alternative<SlchanKit>(*kit)) _MySlchans.push_back(_unit.MyId);
		if (std::holds_alternative<BubbleKit>(*kit)) _MyBubbles.push_back(_unit.MyId);
		if (std::holds_alternative<RockrKit>(*kit)) _MyRockrs.push_back(_unit.MyId);
		if (std::holds_alternative<KazemaKit>(*kit)) _MyKazemas.push_back(_unit.MyId);
		if (std::holds_alternative<AkkordKit>(*kit)) _MyAkkords.push_back(_unit.MyId);
		if (std::holds_alternative<Swire2Kit>(*kit)) _MySwire2s.push_back(_unit.MyId);
		if (const auto* gravel = std::get_if<GravelKit>(kit); gravel && gravel->MyAuraCostLimit && std::islessgreater(gravel->MyAuraDefense, 0))
			InstallOperatorAura(_unit.MyId, {.MyKey = "talent:gravel", .MyAttribute = Attribute::DEFENSE_PERCENT,
				.MyValue = gravel->MyAuraDefense, .MyCostLimit = gravel->MyAuraCostLimit});
		if (const auto* flower = std::get_if<FlowerKit>(kit); flower && std::isgreater(flower->MyRegenRatio, 0))
			InstallOperatorAura(_unit.MyId, {.MyKey = "flower:lavender", .MyAttribute = Attribute::HEALTH_REGEN,
				.MyValue = flower->MyRegenRatio, .MyValueFrom = OperatorAuraValue::SOURCE_ATTACK, .MyStacking = OperatorAuraStacking::PER_SOURCE,
				.MyInterval = 0.25, .MyInitialDelay = 0.25, .MyDuration = 0.5});
		if (std::holds_alternative<HaroldKit>(*kit))
		{
			_MyHarolds.push_back(_unit.MyId);
			_unit.MyHaroldHalf.reserve(_MyAllyIds.size());
		}
		if (const auto* silent = std::get_if<SilentKit>(kit))
		{
			if (silent->MyDrone) _MySilents.push_back(_unit.MyId);
			if (std::islessgreater(silent->MyAuraAttackSpeed, 0))
				InstallOperatorAura(_unit.MyId, {.MyKey = "talent:silent", .MyAttribute = Attribute::ATTACK_SPEED,
					.MyValue = silent->MyAuraAttackSpeed, .MyProfession = OperatorProfession::MEDIC});
		}
		if (const auto* tinman = std::get_if<TinmanKit>(kit); tinman && tinman->MyWitherScale > 1) _MyTinmanWither = true;
		if (const auto* podego = std::get_if<PodegoKit>(kit))
		{
			if (podego->MyHealing) _MyPodegos.push_back(_unit.MyId);
			if (std::islessgreater(podego->MyAuraAttack, 0))
				InstallOperatorAura(_unit.MyId, {.MyKey = "talent:podego", .MyAttribute = Attribute::ATTACK_PERCENT,
					.MyValue = podego->MyAuraAttack, .MyProfession = OperatorProfession::SUPPORT});
		}
		if (const auto* udflow = std::get_if<UdflowKit>(kit); udflow && udflow->MyReveal)
			Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::OPERATOR_REVEAL, .MySource = _unit.MyId, .MyInterval = 0.2});
		if (const auto* liskam = std::get_if<LiskamKit>(kit); liskam && liskam->MyReveal)
			Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::OPERATOR_REVEAL, .MySource = _unit.MyId, .MyInterval = 0.2});
		if (const auto* angel = std::get_if<AngelKit>(kit); angel && std::isgreater(angel->MyGroundAttackSpeed, 0))
			InstallGroundAttackSpeed(_unit.MyId, "trait:angel_ground", angel->MyGroundAttackSpeed, angel->MyGroundCount);
		if (const auto* ayer = std::get_if<AyerKit>(kit))
			InstallOperatorAura(_unit.MyId, {.MyKey = "talent:ayer_aspd", .MyAttribute = Attribute::ATTACK_SPEED,
				.MyValue = ayer->MyAuraAttackSpeed, .MyStacking = OperatorAuraStacking::REPLACE, .MyInterval = 0.2, .MyDuration = 0.45,
				.MyRange = ayer->MyTalentRange, .MyOperatorsOnly = true, .MyDropOutside = true});
		if (const auto* swire = std::get_if<SwireKit>(kit))
			_unit.MySkillAura = InstallOperatorAura(_unit.MyId, {.MyKey = "talent:swire_guide", .MyAttribute = Attribute::ATTACK_PERCENT,
				.MyValue = swire->MyAuraAttack, .MyStacking = OperatorAuraStacking::REPLACE, .MyInterval = 0.2, .MyDuration = 0.45,
				.MyRange = swire->MyTalentRange, .MySkillRange = swire->MySkillRange, .MySkillScale = swire->MySkillScale, .MyMeleeOnly = true, .MyDropOutside = true});
		if (const auto* haini = std::get_if<HainiKit>(kit))
		{
			_MyHainis.push_back(_unit.MyId);
			_unit.MyTalentAura = InstallOperatorAura(_unit.MyId, {.MyKey = "talent:haini_fragile", .MyAttribute = Attribute::DAMAGE_TAKEN_MULTIPLIER,
				.MyValue = haini->MyFragile - 1, .MyStacking = OperatorAuraStacking::REPLACE, .MyInterval = 0.2, .MyDuration = 0.45,
				.MyDropOutside = true, .MyEnemies = true, .MyNormalEnemies = true, .MyAttackRange = true});
			_MyOperatorAuras[_unit.MyTalentAura].MyOffset = 1;
			if (haini->MySlow) _unit.MySkillAura = InstallOperatorAura(_unit.MyId, {.MyKey = "skill:haini_slow", .MyAttribute = Attribute::MOVE_MULTIPLIER,
				.MyValue = haini->MyMoveMultiplier, .MyStacking = OperatorAuraStacking::REPLACE, .MyInterval = 0, .MyDuration = 0.15,
				.MyDropOutside = true, .MySkillActive = true, .MyEnemies = true, .MyNormalEnemies = true, .MyAttackRange = true});
		}
		if (std::holds_alternative<BlockingDefenseKit>(*kit)) _MyBlockingDefenders.push_back(_unit.MyId);
		if (std::holds_alternative<SnhuntKit>(*kit)) _MySnhunts.push_back(_unit.MyId);
		if (const auto* skadi = std::get_if<SkadiKit>(kit)) InstallSkadi(_unit.MyId, *skadi);
		if (const auto* blemsh = std::get_if<BlemshKit>(kit))
		{
			_MyBlemshs.push_back(_unit.MyId);
			if (blemsh->MySkill == BlemshSkillKind::SLEEP)
				_unit.MySkillAura = InstallOperatorAura(_unit.MyId, {.MyKey = "blemsh:regen", .MyAttribute = Attribute::HEALTH_REGEN,
					.MyValue = blemsh->MyRegenRatio, .MyValueFrom = OperatorAuraValue::SOURCE_ATTACK, .MyStacking = OperatorAuraStacking::PER_SOURCE,
					.MyInterval = 0, .MyDuration = 0.5, .MyRange = blemsh->MyHealRange, .MySkillActive = true, .MySkipHidden = true});
		}
		if (std::holds_alternative<MalistKit>(*kit) || std::holds_alternative<KjeraKit>(*kit)) _unit.MyFunnelRamps.reserve(std::max<std::size_t>(1, _MyEnemyIds.size()));
		if (const auto* kjera = std::get_if<KjeraKit>(kit); kjera && kjera->MyLockDrones)
		{
			_MyLockedFunnelUsers.push_back(_unit.MyId);
			_unit.MyLockedDrones.reserve(kjera->MyDrones);
			_unit.MyDroneQueues.reserve(std::max<std::size_t>(kjera->MyDrones, _MyEnemyIds.size()));
		}
		if (const auto* lisa = std::get_if<LisaKit>(kit))
		{
			_unit.MyFoxKey = "lisa:fox:" + std::to_string(_unit.MyId);
			if (std::isgreater(lisa->MySpRecovery, 0)) InstallOperatorAura(_unit.MyId, {.MyKey = "aura:spRecovery", .MyAttribute = Attribute::SP_RECOVERY_FLAT, .MyValue = lisa->MySpRecovery,
				.MyStacking = OperatorAuraStacking::HIGHEST_PRESENT, .MyProfession = OperatorProfession::SUPPORT, .MyInterval = 0.25, .MyInitialDelay = 0.25, .MyDuration = 0.5, .MyNoSource = true});
			Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::LISA_AURA, .MySource = _unit.MyId, .MyInterval = 0.25});
		}
		if (const auto* cello = std::get_if<CelloKit>(kit))
		{
			_MyCellos.push_back(_unit.MyId);
			Schedule({.MyAt = Time() + 1, .MyKind = ScheduledKind::CELLO_PULSE, .MySource = _unit.MyId, .MyInterval = 1});
			if (std::isgreater(cello->MyFragile, 0) || std::isgreater(cello->MyModuleFragile, 0)) Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::CELLO_PULSE, .MySource = _unit.MyId, .MyHandle = 1, .MyInterval = 0.25});
			if (std::isgreater(cello->MyDot, 0)) Schedule({.MyAt = Time() + cello->MyDotInterval, .MyKind = ScheduledKind::CELLO_PULSE, .MySource = _unit.MyId, .MyHandle = 2, .MyInterval = cello->MyDotInterval});
		}
		if (const auto* reed = std::get_if<Reed2Kit>(kit))
		{
			_MyReed2s.push_back(_unit.MyId); _unit.MyReedCarriers.reserve(reed->MyCarriers); _unit.MyReedFireKey = "reed2:fireball:" + std::to_string(_unit.MyId);
			if (std::isgreater(reed->MyInjuredScale, 1)) Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::REED2_MODULE, .MySource = _unit.MyId, .MyInterval = 0.25});
		}
		if (const auto* halo = std::get_if<Halo2Kit>(kit))
		{
			_MyHalo2s.push_back(_unit.MyId); _unit.MyHaloLocks.reserve(halo->MyTargets); _unit.MyHaloStay.reserve(256);
			Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::HALO2_PULSE, .MySource = _unit.MyId, .MyInterval = 0.25});
		}
		if (const auto* agoat = std::get_if<Agoat2Kit>(kit))
		{
			_unit.MyAgoatMistKey = "agoat2:mist:" + std::to_string(_unit.MyId);
			if (std::islessgreater(agoat->MyModuleSpeed, 0)) Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::AGOAT2_PULSE, .MySource = _unit.MyId, .MyInterval = 0.25});
			Schedule({.MyAt = Time() + 0.5, .MyKind = ScheduledKind::AGOAT2_PULSE, .MySource = _unit.MyId, .MyHandle = 1, .MyInterval = 0.5});
		}
		if (std::holds_alternative<Siege2Kit>(*kit))
		{
			_MySiege2s.push_back(_unit.MyId); _unit.MyGoldenLions.reserve(8); _unit.MySiegeSeen.reserve(32); _unit.MyNextExtraRangeKeys.reserve(FieldTiles);
			Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::SIEGE2_PULSE, .MySource = _unit.MyId, .MyInterval = 0.25});
		}
		if (const auto* snow = std::get_if<Sbell2Kit>(kit))
		{
			_MySbell2s.push_back(_unit.MyId); _unit.MySnow.resize(FieldTiles); _unit.MySnowLast.reserve(256);
			if (std::isgreater(snow->MyCrowdDamage, 0)) Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::SBELL2_MODULE, .MySource = _unit.MyId, .MyInterval = 0.25});
		}
		if (std::holds_alternative<BlkkgtKit>(*kit)) { _MyBlkkgts.push_back(_unit.MyId); _unit.MyBlkkgtSeen.reserve(32); }
		if (const auto* yu = std::get_if<YuKit>(kit))
		{
			_MyYus.push_back(_unit.MyId);
			const std::array intervals{0.1, yu->MyInterval, yu->MyHealInterval};
			for (unsigned i = 0; i < intervals.size(); ++i) Schedule({.MyAt = Time() + intervals[i], .MyKind = ScheduledKind::YU_PULSE, .MySource = _unit.MyId, .MyHandle = i, .MyInterval = intervals[i]});
		}
		if (const auto* lumen = std::get_if<LumenKit>(kit))
		{
			_MyLumens.push_back(_unit.MyId); _unit.MyLumenRainKey = "lumen:rain:" + std::to_string(_unit.MyId);
			if (std::isgreater(lumen->MyPermanentResist, 0))
			{
				StatusFlags flags; flags.set(static_cast<std::size_t>(CombatStatus::RESIST));
				(void)AddBuff(_unit.MyId, {.MyKey = "lumen:scopeResist", .MyFlags = flags, .MyPersistent = true, .MyAllowDead = true,
					.MyStrength = BuffStrength{.MyValue = lumen->MyPermanentResist}, .MyStatus = CombatStatus::RESIST});
			}
		}
		if (std::holds_alternative<PasngrKit>(*kit))
		{
			_unit.MyPasngrEnhanceKey = std::string("pasngr:enhance:") + std::to_string(_unit.MyId);
			Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.25});
		}
		if (const auto* pepe = std::get_if<PepeKit>(kit))
		{
			if (pepe->MySkill == PepeSkillKind::STAMP) _MyPepes.push_back(_unit.MyId);
			if (std::islessgreater(pepe->MyTeamAttack, 0)) (void)InstallOperatorAura(_unit.MyId, {.MyKey = "pepe:lotus", .MyAttribute = Attribute::ATTACK_PERCENT,
				.MyValue = pepe->MyTeamAttack, .MyStacking = OperatorAuraStacking::REPLACE, .MyProfession = OperatorProfession::WARRIOR, .MyInterval = 0.5, .MyInitialDelay = 0.5,
				.MyDuration = 0.75, .MySkipHidden = true, .MyIgnoreIsolation = true, .MyNoSource = true});
		}
		if (const auto* qiubai = std::get_if<QiubaiKit>(kit))
		{
			_unit.MyQiubaiBound.reserve(32);
			if (std::isgreater(qiubai->MyCrowdCount, 0) && std::islessgreater(qiubai->MyCrowdSpeed, 0))
				Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.25});
		}
		if (std::holds_alternative<LemuenKit>(*kit))
		{
			if (_MyLemuens.empty()) Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::LEMUEN_WANTED, .MyInterval = 0.25});
			_MyLemuens.push_back(_unit.MyId); _unit.MyLemuenLocks.reserve(16); _unit.MyNextExtraRangeKeys.reserve(FieldTiles);
		}
		if (std::holds_alternative<Thorn2Kit>(*kit))
		{
			Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::THORN2_VISION, .MySource = _unit.MyId, .MyInterval = 0.25});
			Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::THORN2_ZONES, .MySource = _unit.MyId, .MyInterval = 0.25});
		}
		if (std::holds_alternative<MlynarKit>(*kit))
		{
			_MyMlynars.push_back(_unit.MyId); _unit.MyMlynarHits.reserve(16);
			Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.2});
		}
		if (const auto* f12yin = std::get_if<F12yinKit>(kit); f12yin && std::islessgreater(f12yin->MyHealthySpeed, 0))
			Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.2});
		if (const auto* nymph = std::get_if<NymphKit>(kit))
		{
			_MyNymphs.push_back(_unit.MyId);
			if (std::isgreater(nymph->MyBurstSp, 0)) Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.25});
		}
		if (std::holds_alternative<SntllaKit>(*kit)) Schedule({.MyAt = Time() + 0.5, .MyKind = ScheduledKind::SNTLLA_TALENT, .MySource = _unit.MyId, .MyInterval = 0.5});
		if (const auto* aglina = std::get_if<AglinaKit>(kit))
		{
			if (std::islessgreater(aglina->MyAttackSpeed, 0)) (void)InstallOperatorAura(_unit.MyId, {.MyKey = "aglina:field", .MyAttribute = Attribute::ATTACK_SPEED,
				.MyValue = aglina->MyAttackSpeed, .MyStacking = OperatorAuraStacking::REPLACE, .MyInterval = 0.25, .MyInitialDelay = 0.25, .MyDuration = 0.5, .MyNoSource = true});
			if (std::isgreater(aglina->MyRegeneration, 0)) (void)InstallOperatorAura(_unit.MyId, {.MyKey = "aglina:parttime", .MyAttribute = Attribute::HEALTH_REGEN,
				.MyValue = aglina->MyRegeneration, .MyStacking = OperatorAuraStacking::REPLACE, .MyInterval = 0.25, .MyInitialDelay = 0.25, .MyDuration = 0.5, .MySkillInactive = true, .MyNoSource = true});
			if (std::isgreater(aglina->MyEnemySp, 0)) Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.25});
		}
		if (const auto* svash = std::get_if<Svash2Kit>(kit))
		{
			_MySvash2s.push_back(_unit.MyId);
			if (const auto* token = FindTokenTemplate(_unit.MyId, svash->MyEye)) _unit.MyEyeCost = token->MyStats.MyDeploymentCost;
			Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::SVASH2_SNOW, .MySource = _unit.MyId, .MyInterval = 0.25});
		}
		if (const auto* ghost = std::get_if<Ghost2Kit>(kit))
		{
			_MyGhost2s.push_back(_unit.MyId); _unit.MyGhostHeavy.reserve(8);
			Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::GHOST2_SLOW, .MySource = _unit.MyId, .MyInterval = 0.25});
			Schedule({.MyAt = Time() + 1, .MyKind = ScheduledKind::GHOST2_DAMAGE, .MySource = _unit.MyId, .MyInterval = 1});
			if (std::islessgreater(ghost->MyDollAttack, 0) || std::islessgreater(ghost->MyDollHealth, 0))
				Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.2});
		}
		if (const auto* demkni = std::get_if<DemkniKit>(kit))
		{
			if (demkni->MySkill == DemkniSkillKind::MEDICINE) _MyDemknis.push_back(_unit.MyId);
			Schedule({.MyAt = Time() + 1, .MyKind = ScheduledKind::DEMKNI_SUIT, .MySource = _unit.MyId, .MyInterval = 1});
		}
		if (const auto* horn = std::get_if<HornKit>(kit))
		{
			if (std::islessgreater(horn->MyTeamAttack, 0)) InstallOperatorAura(_unit.MyId, {.MyKey = "horn:fortress", .MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = horn->MyTeamAttack,
				.MyStacking = OperatorAuraStacking::REPLACE, .MyProfession = OperatorProfession::TANK, .MyInterval = 0.25, .MyInitialDelay = 0.25, .MyDuration = 0.5, .MyIgnoreIsolation = true, .MyNoSource = true});
			if (std::islessgreater(horn->MyUnblockedSpeed, 0)) Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.2});
			if (horn->MySkill == HornSkillKind::FLARE)
			{
				_unit.MyFlares.reserve(8);
				Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::HORN_FLARES, .MySource = _unit.MyId, .MyInterval = 0.25});
			}
		}
		if (const auto* surtr = std::get_if<SurtrKit>(kit); surtr && (std::islessgreater(surtr->MyUnblockedSpeed, 0) || std::isgreater(surtr->MyArtsFragile, 0)))
			Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.2});
		if (const auto* etlchi = std::get_if<EtlchiKit>(kit))
		{
			_MyEtlchis.push_back(_unit.MyId); _unit.MyCandles.reserve(etlchi->MyCandles); _unit.MySickles.reserve(2);
			if (std::islessgreater(etlchi->MyCrowdSpeed, 0) && std::isgreater(etlchi->MyCrowdCount, 0)) Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.2});
		}
		if (const auto* blaze = std::get_if<Blaze2Kit>(kit))
		{
			_MyBlazes.push_back(_unit.MyId);
			if (std::isgreater(blaze->MyBurstSp, 0)) Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.25});
			if (blaze->MySkill == Blaze2SkillKind::GROUND) Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::BLAZE_GROUND, .MySource = _unit.MyId, .MyInterval = 0.25});
		}
		if (std::holds_alternative<TitiKit>(*kit))
		{
			_MyTitis.push_back(_unit.MyId); _unit.MySleepWards.reserve(2); _unit.MySleepStarts.reserve(_MyEnemyIds.size());
			Schedule({.MyAt = Time() + 1, .MyKind = ScheduledKind::TITI_DREAM, .MySource = _unit.MyId, .MyInterval = 1});
			Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::TITI_VIGOR, .MySource = _unit.MyId, .MyInterval = 0.25});
		}
		if (const auto* excu = std::get_if<Excu2Kit>(kit))
		{
			_unit.MyVerdictTargets.reserve(_MyEnemyIds.size());
			if (std::islessgreater(excu->MyCrowdSpeed, 0) && std::isgreater(excu->MyCrowdCount, 0))
				Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.2});
		}
		if (const auto* cetsyr = std::get_if<CetsyrKit>(kit))
		{
			_MyCetsyrs.push_back(_unit.MyId); _unit.MyNoInspire = !cetsyr->MyOrbitMotes;
			_unit.MyMoteReady.reserve(cetsyr->MyMotes + 3); _unit.MyMoteReady.assign(cetsyr->MyMotes, -std::numeric_limits<double>::infinity());
			_unit.MyMoteHits.reserve(_MyEnemyIds.size()); _unit.MyMoteKey = cetsyr->MyOrbitMotes ? "cetsyr:mote" : "cetsyr:mote:" + std::to_string(_unit.MyId);
			Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::CETSYR_MOTES, .MySource = _unit.MyId, .MyInterval = 0.25});
			if (std::islessgreater(cetsyr->MyModuleAttack, 0))
			{
				const auto interval = cetsyr->MyOrbitMotes ? 0.25 : 0.2;
				Schedule({.MyAt = Time() + interval, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = interval});
			}
		}
		if (std::holds_alternative<BldskKit>(*kit)) _MyBldsks.push_back(_unit.MyId);
		if (std::holds_alternative<GvialKit>(*kit) || std::holds_alternative<BillroKit>(*kit))
		{
			if (std::holds_alternative<GvialKit>(*kit)) _unit.MyDeferredHits.reserve(_MyUnits.size());
			else _unit.MyBillroMarked.reserve(_MyEnemyIds.size());
			const auto interval = std::holds_alternative<GvialKit>(*kit) && std::get<GvialKit>(*kit).MyHiddenVariant ? 0.2 : 0.1;
			Schedule({.MyAt = Time() + interval, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = interval});
		}
		if (const auto* flamtl = std::get_if<FlamtlKit>(kit))
		{
			InstallOperatorAura(_unit.MyId, {.MyKey = "flamtl:dodge", .MyAttribute = Attribute::PHYSICAL_DODGE, .MyValue = flamtl->MyNationDodge,
				.MyStacking = OperatorAuraStacking::REPLACE, .MyInterval = 0.2, .MyInitialDelay = 0.2, .MyDuration = 0.25,
				.MyOwnerOnly = true, .MyIgnoreIsolation = true, .MyNation = "kazimierz"});
			if (!flamtl->MyBlockingModifiers.empty()) Schedule({.MyAt = Time() + 0.1, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.1});
		}
		if (std::holds_alternative<FartthKit>(*kit))
		{
			_unit.MyExtraRangeKeys.reserve(FieldTiles); _unit.MyNextExtraRangeKeys.reserve(FieldTiles);
			Schedule({.MyAt = Time() + 0.1, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.1});
		}
		if (const auto* aura = std::get_if<SpAuraKit>(kit); aura && (!aura->MyGlobal || std::isgreater(aura->MyRecovery, 0)))
			InstallOperatorAura(_unit.MyId, {.MyKey = "aura:spRecovery", .MyAttribute = Attribute::SP_RECOVERY_FLAT, .MyValue = aura->MyRecovery,
				.MyStacking = aura->MyGlobal ? OperatorAuraStacking::HIGHEST_PRESENT : OperatorAuraStacking::STRONGEST_LIVING,
				.MyInterval = aura->MyGlobal ? 0.25 : 0.2, .MyInitialDelay = aura->MyGlobal ? 0.25 : 0.2, .MyDuration = aura->MyGlobal ? 0.5 : 0.25,
				.MyOwnerOnly = !aura->MyGlobal, .MyNoSource = aura->MyGlobal});
		if (const auto* svrash = std::get_if<SvrashKit>(kit))
		{
			for (const auto id : _MyAllyIds)
				if (Unit(id).MyKind == UnitKind::OPERATOR && Unit(id).MyOwner == _unit.MyOwner)
					(void)AddBuff(id, {.MyKey = "svrash:leader", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::REDEPLOY_MULTIPLIER, .MyValue = svrash->MyRedeployMultiplier}}, .MyPersistent = true, .MyAllowDead = true});
			Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::OPERATOR_REVEAL, .MySource = _unit.MyId, .MyInterval = 0.2});
		}
		if (const auto* texas = std::get_if<Texas2Kit>(kit))
		{
			_unit.MyTexas2DotKey = "texas2:drizzleDot:" + std::to_string(_unit.MyId);
			if (std::islessgreater(texas->MyLonelyAttack, 0)) Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.2});
		}
		if (const auto* hsguma = std::get_if<HsgumaKit>(kit))
		{
			InstallOperatorAura(_unit.MyId, {.MyKey = "hsguma:def", .MyAttribute = Attribute::DEFENSE_PERCENT, .MyValue = hsguma->MyAuraDefense,
				.MyStacking = OperatorAuraStacking::REPLACE, .MyProfession = OperatorProfession::TANK, .MyInterval = 0.2, .MyInitialDelay = 0.2, .MyDuration = 0.25,
				.MyOwnerOnly = true, .MyIgnoreIsolation = true});
			if (std::islessgreater(hsguma->MyBlockingDefense, 0)) Schedule({.MyAt = Time() + 0.1, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.1});
		}
		if (const auto* mudrok = std::get_if<MudrokKit>(kit))
		{
			_unit.MyMudrokSlowKey = "mudrok:slow:" + std::to_string(_unit.MyId);
			if (!mudrok->MyLonelyModifiers.empty()) Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::OPERATOR_MODULE, .MySource = _unit.MyId, .MyInterval = 0.2});
		}
		if (std::holds_alternative<GnosisKit>(*kit))
		{
			_unit.MyHypothermia.reserve(_MyEnemyIds.size());
			Schedule({.MyAt = Time() + 0.1, .MyKind = ScheduledKind::GNOSIS_AURA, .MySource = _unit.MyId, .MyInterval = 0.1});
			Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::GNOSIS_RESIST, .MySource = _unit.MyId, .MyInterval = 0.2});
		}
		if (std::holds_alternative<LionhdKit>(*kit)) Schedule({.MyAt = Time() + 0.1, .MyKind = ScheduledKind::LIONHD_PRESENCE, .MySource = _unit.MyId, .MyInterval = 0.1});
		if (std::holds_alternative<ReckprKit>(*kit)) { _MyReckprs.push_back(_unit.MyId); _unit.MyGuardHealKey = std::get<ReckprKit>(*kit).MySharedGuard ? "reckpr:guard" : "reckpr:guard:" + std::to_string(_unit.MyId); }
		if (const auto* glady = std::get_if<GladyKit>(kit))
		{
			static constexpr std::array<std::string_view, 5> Abyssal{{"char_143_ghost", "char_263_skadi", "char_474_glady", "char_4145_ulpia", "char_1023_ghost2"}};
			_MyGladys.push_back(_unit.MyId); _unit.MyTornadoSlowKey = "glady:slow:" + std::to_string(_unit.MyId);
			InstallOperatorAura(_unit.MyId, {.MyKey = "glady:tide", .MyAttribute = Attribute::HEALTH_REGEN_RATIO, .MyValue = glady->MyRegenRatio,
				.MyStacking = OperatorAuraStacking::REPLACE, .MyInterval = 0.2, .MyInitialDelay = 0.2, .MyDuration = 0.25,
				.MyOwnerOnly = true, .MyIgnoreIsolation = true, .MyCharacters = Abyssal, .MyGroup = "abyssal"});
		}
		if (const auto* cathy = std::get_if<CathyKit>(kit); cathy && cathy->MyForge)
			_unit.MySkillAura = InstallOperatorAura(_unit.MyId, {.MyKey = "cathy:forge", .MyStacking = OperatorAuraStacking::PER_SOURCE, .MyInterval = 0,
				.MyDuration = 0.25, .MyOperatorsOnly = true, .MyModifiers = cathy->MyForgeModifiers, .MyOwnerOnly = true,
				.MyRequiredBuff = "cathy:shield", .MySkipSelf = true, .MyIgnoreIsolation = true});
		if (const auto* mizuki = std::get_if<MizukiKit>(kit))
		{
			Schedule({.MyAt = Time() + 0.1, .MyKind = ScheduledKind::MIZUKI_PRESENCE, .MySource = _unit.MyId, .MyInterval = 0.1});
			if (mizuki->MySlow) InstallOperatorAura(_unit.MyId, {.MyKey = "mizuki:slow", .MyAttribute = Attribute::MOVE_MULTIPLIER,
				.MyValue = *mizuki->MySlow, .MyStacking = OperatorAuraStacking::PER_SOURCE, .MyInterval = 0.2, .MyInitialDelay = 0.2, .MyDuration = 0.25,
				.MyEnemies = true, .MyAttackRange = true, .MySkipHidden = true});
		}
		if (std::holds_alternative<AromaKit>(*kit))
		{
			_MyAromas.push_back(_unit.MyId); _unit.MyAromaMarked.reserve(_MyEnemyIds.size()); _unit.MyAromaFloating.reserve(_MyEnemyIds.size());
			Schedule({.MyAt = Time() + BattleClock::StepSeconds, .MyKind = ScheduledKind::AROMA_LANDING, .MySource = _unit.MyId, .MyInterval = BattleClock::StepSeconds});
		}
		if (std::holds_alternative<InesKit>(*kit))
		{
			_MyIneses.push_back(_unit.MyId);
			_unit.MyInesWoven.reserve(_MyEnemyIds.size()); _unit.MyInesAttackVictims.reserve(_MyEnemyIds.size()); _unit.MyInesSpeedVictims.reserve(_MyEnemyIds.size());
			const auto suffix = std::to_string(_unit.MyId);
			_unit.MyInesAttackKey = "ines:atkSteal:" + suffix; _unit.MyInesSpeedKey = "ines:aspd:" + suffix; _unit.MyInesSentryKey = "ines:sentry:" + suffix;
			Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::INES_SENTRY, .MySource = _unit.MyId, .MyInterval = 0.2});
		}
		if (const auto* rosesa = std::get_if<RosesaKit>(kit))
		{
			_MyRosesas.push_back(_unit.MyId); _unit.MyDeferredHits.reserve(_MyUnits.size());
			InstallOperatorAura(_unit.MyId, {.MyKey = "rosesa:heal", .MyAttribute = Attribute::HEALING_TAKEN_MULTIPLIER,
				.MyValue = rosesa->MyHealingScale, .MyStacking = OperatorAuraStacking::PER_SOURCE, .MyInterval = 0.2, .MyInitialDelay = 0.2, .MyDuration = 0.25,
				.MyOperatorsOnly = true, .MyAttackRange = true, .MySkipHidden = true});
		}
		if (const auto* rmixer = std::get_if<RmixerKit>(kit))
		{
			Schedule({.MyAt = Time() + 0.1, .MyKind = ScheduledKind::RMIXER_SHIELD, .MySource = _unit.MyId, .MyInterval = 0.1});
			if (rmixer->MyReveal) Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::OPERATOR_REVEAL, .MySource = _unit.MyId, .MyInterval = 0.2});
		}
		if (const auto* mostma = std::get_if<MostmaKit>(kit))
		{
			_unit.MyTimeLocked.reserve(_MyEnemyIds.size());
			InstallOperatorAura(_unit.MyId, {.MyKey = "aura:spRecovery", .MyAttribute = Attribute::SP_RECOVERY_FLAT,
				.MyValue = mostma->MySpRecovery, .MyStacking = OperatorAuraStacking::STRONGEST_LIVING, .MyProfession = OperatorProfession::CASTER,
				.MyInterval = 0.2, .MyInitialDelay = 0.2, .MyDuration = 0.25, .MyOwnerOnly = true});
			_unit.MyTalentAura = InstallOperatorAura(_unit.MyId, {.MyKey = "mostma:slow", .MyAttribute = Attribute::MOVE_MULTIPLIER,
				.MyValue = mostma->MyMoveSpeed, .MyStacking = OperatorAuraStacking::PER_SOURCE, .MyInterval = 0.2, .MyInitialDelay = 0.2, .MyDuration = 0.25,
				.MySkillScale = mostma->MySkillSlowScale, .MyEnemies = true, .MyAttackRange = true, .MySkipHidden = true, .MyMinimum = 0, .MyStatus = CombatStatus::SLOW});
			_MyOperatorAuras[_unit.MyTalentAura].MyOffset = 1;
		}
		if (const auto* slbell = std::get_if<WeakeningKit>(kit))
		{
			InstallOperatorAura(_unit.MyId, {.MyKey = "talent:weak_fragile", .MyAttribute = Attribute::DAMAGE_TAKEN_MULTIPLIER,
				.MyValue = slbell->MyFragile, .MyStacking = OperatorAuraStacking::REPLACE, .MyInterval = 0.1, .MyDuration = 0.25,
				.MyDropOutside = true, .MyEnemies = true, .MyAttackRange = true, .MyHealthRatioBelow = slbell->MyHealthRatio});
			if (slbell->MySkillAura) _unit.MySkillAura = InstallOperatorAura(_unit.MyId, {.MyKey = slbell->MySlow ? "skill:slbell_aspd" : "skill:slbell_shred",
				.MyStacking = OperatorAuraStacking::REPLACE, .MyInterval = 0, .MyDuration = 0.15,
				.MyDropOutside = true, .MySkillActive = true, .MyEnemies = true, .MyAttackRange = true, .MyModifiers = slbell->MySkillModifiers});
		}
		if (std::holds_alternative<VulpisKit>(*kit))
		{
			_MyVulpises.push_back(_unit.MyId);
			_unit.MyVulpisMarks.reserve(std::max<std::size_t>(1, _MyEnemyIds.size()));
		}
		if (const auto* archet = std::get_if<ArchetKit>(kit))
		{
			_MyArchets.push_back(_unit.MyId);
			Schedule({.MyAt = Time() + archet->MyTacticsInterval, .MyKind = ScheduledKind::ARCHET_TACTICS, .MySource = _unit.MyId, .MyInterval = archet->MyTacticsInterval});
			if (std::isgreater(archet->MyGroundAttackSpeed, 0)) InstallGroundAttackSpeed(_unit.MyId, "trait:archet_ground", archet->MyGroundAttackSpeed, 1);
		}
		if (std::holds_alternative<VigilKit>(*kit))
		{
			_MyVigils.push_back(_unit.MyId);
			_unit.MyWolfMarked.reserve(_MyEnemyIds.size());
			Schedule({.MyAt = Time(), .MyKind = ScheduledKind::VIGIL_MARK, .MySource = _unit.MyId, .MyInterval = 0.1});
		}
		if (const auto* mint = std::get_if<MintKit>(kit))
			_unit.MySkillAura = InstallOperatorAura(_unit.MyId, {.MyKey = "talent:mint_geo", .MyAttribute = Attribute::DEFENSE_PERCENT,
				.MyValue = mint->MyAuraDefense, .MyStacking = OperatorAuraStacking::REPLACE, .MyInterval = 0.2, .MyDuration = 0.45,
				.MyRange = mint->MyTalentRange, .MyDropOutside = true, .MySkillInactive = true});
	}

	void Battle::ReleaseOperatorHooks()
	{
		// 原版在帧末释放永久移除者拥有的监听；普通撤退保留，命中回调与独立区域不属此列。
		for (const auto id : _MyPendingOperatorReleases)
		{
			auto& unit = _MyUnits[Index(id)];
			if (!unit.MyRemoved || unit.MyAlive || unit.MyOperatorHooksReleased) continue;
			unit.MyOperatorHooksReleased = true;
			if (HasBeforeAttackHook(*unit.MyDefinition.MyOperatorKit)) --_MyOperatorBeforeAttackHandlers;
		}
		_MyPendingOperatorReleases.clear();
	}

	bool Battle::OperatorBeforeAttack(CombatUnit& _unit, std::vector<UnitId>& _targets)
	{
		if (!_MyOperatorBeforeAttackHandlers) return true;
		if (const auto* kit = _unit.MyDefinition.MyOperatorKit; kit && !_unit.MyOperatorHooksReleased)
		{
			if (std::holds_alternative<RosmonKit>(*kit)) RosmonTargets(_unit, _targets);
			if (const auto* halo = std::get_if<Halo2Kit>(kit)) Halo2BeforeAttack(_unit, *halo, _targets);
			if (const auto* agoat = std::get_if<Agoat2Kit>(kit)) Agoat2BeforeAttack(_unit, *agoat, _targets);
			if (const auto* lumen = std::get_if<LumenKit>(kit)) LumenBeforeAttack(_unit, *lumen, _targets);
			if (const auto* pepe = std::get_if<PepeKit>(kit)) PepeBeforeAttack(_unit, *pepe, _targets);
			else if (const auto* f12yin = std::get_if<F12yinKit>(kit))
			{
				const auto chance = f12yin->MySkill == F12yinSkillKind::QUAKE && _unit.MySkill.MyActive ? f12yin->MySkillChance : f12yin->MyChance;
				_unit.MyF12yinCritical = std::isgreater(chance, 0) && std::isless(_MyRandom.Next(), std::clamp(chance, 0.0, 1.0));
			}
			else if (const auto* svash = std::get_if<Svash2Kit>(kit); svash && svash->MySkill == Svash2SkillKind::CHANGE && _unit.MySkill.MyActive) TargetsOnLine(_unit, _targets);
			else if (const auto* dusk = std::get_if<DuskKit>(kit); dusk && dusk->MySkill == DuskSkillKind::FREEHAND && _unit.MySkill.MyActive) PreferUnblocked(_unit, _targets);
			else if (std::holds_alternative<BlemshKit>(*kit))
			{
				for (const auto id : _MyEnemyIds)
				{
					const auto& enemy = Unit(id);
					if (!enemy.MyAlive || enemy.MyHidden || !enemy.MyStatuses.Has(CombatStatus::SLEEP) || enemy.MyStatuses.Has(CombatStatus::UNTARGETABLE) || enemy.Flying() || !InRuleRange(_unit.MyId, id)) continue;
					const auto count = std::max<std::size_t>(1, _targets.size());
					std::erase(_targets, id); _targets.insert(_targets.begin(), id);
					if (_targets.size() > count) _targets.resize(count);
					break;
				}
			}
			else if (std::holds_alternative<PapyrsKit>(*kit) && _unit.MySkill.MyActive && _unit.MyPapyrsLock)
			{
				const auto id = _unit.MyPapyrsLock;
				if (Unit(id).MyAlive && InRuleRange(_unit.MyId, id)) _targets.assign(1, id);
			}
			else if (const auto* rockr = std::get_if<RockrKit>(kit); rockr && rockr->MyOverload && _unit.MySkill.MyActive)
			{
				const auto id = _unit.MyRockrLock;
				if (id && TargetableEnemy(Unit(id), EffectiveAttack(_unit)) && InRuleRange(_unit.MyId, id)) _targets.assign(1, id);
				else _unit.MyRockrLock = _targets.empty() ? 0 : _targets.front();
			}
			else if (const auto* harold = std::get_if<HaroldKit>(kit); harold && harold->MyTriage && _unit.MySkill.MyActive)
				HaroldBeforeAttack(_unit, _targets);
			else if (const auto* bldsk = std::get_if<BldskKit>(kit); bldsk && bldsk->MyBandage) BldskBeforeAttack(_unit, *bldsk, _targets);
			else if (std::holds_alternative<FlamtlKit>(*kit)) FlamtlBeforeAttack(_unit, _targets);
			else if (const auto* fartth = std::get_if<FartthKit>(kit); fartth && fartth->MySkill == FartthSkillKind::ALLIED && _unit.MySkill.MyActive) FartthBeforeAttack(_unit, _targets);
			else if (const auto* gnosis = std::get_if<GnosisKit>(kit); gnosis && gnosis->MySkill == GnosisSkillKind::HYPOTHERMIA && _unit.MySkill.MyActive) GnosisBeforeAttack(_unit, _targets);
			else if (const auto* precision = std::get_if<PrecisionKit>(kit); precision && precision->MyVolleyExtra) PrecisionBeforeAttack(_unit, *precision, _targets);
			else if (const auto* vigna = std::get_if<VignaKit>(kit))
			{
				const auto chance = _unit.MySkill.MyActive ? vigna->MySkillProbability : vigna->MyProbability;
				if (chance > 0 && (chance >= 1 || _MyRandom.Next() < chance))
					(void)AddBuff(_unit.MyId, {.MyKey = "vigna:proc", .MyDuration = 0.1,
						.MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, vigna->MyAttack}}});
			}
			else if (const auto* prove = std::get_if<ProveKit>(kit); prove && prove->MyHunt && _unit.MySkill.MyActive)
			{
				const auto huntable = [&](UnitId id) { return Unit(id).MyHealth / Unit(id).MyStats.MyMaxHealth <= 0.8 + 1e-9; };
				if (!std::ranges::all_of(_targets, huntable))
				{
					const auto count = static_cast<unsigned>(_targets.size()); _targets.clear();
					for (const auto id : _MyEnemyIds)
						if (TargetableEnemy(Unit(id), EffectiveAttack(_unit)) && InRuleRange(_unit.MyId, id) && huntable(id)) _targets.push_back(id);
					SortOperatorTargets(_unit.MyId, _targets, count);
				}
			}
			else if (std::holds_alternative<IndigoKit>(*kit) &&
				std::ranges::any_of(_targets, [&](UnitId id) { return Unit(id).MyStatuses.Has(CombatStatus::BIND); }))
			{
				const auto count = static_cast<unsigned>(std::max<std::size_t>(1, _targets.size())); _targets.clear();
				const auto& profile = EffectiveAttack(_unit);
				for (const auto id : _MyEnemyIds)
				{
					const auto& enemy = Unit(id);
					if (!enemy.MyStatuses.Has(CombatStatus::BIND) && TargetableEnemy(enemy, profile) &&
						(enemy.MyBlockedBy == _unit.MyId || InRange(_unit, enemy))) _targets.push_back(id);
				}
				SortOperatorTargets(_unit.MyId, _targets, count, &profile);
			}
		}
		if (const auto* kit = _unit.MyDefinition.MyOperatorKit; kit && !_unit.MyOperatorHooksReleased)
		{
			if (const auto* kjera = std::get_if<KjeraKit>(kit); kjera && _unit.MySkill.MyActive) KjeraBeforeAttack(_unit, *kjera, _targets);
		}
		if (const auto* kit = _unit.MyDefinition.MyOperatorKit; kit && (std::holds_alternative<MalistKit>(*kit) || std::holds_alternative<KjeraKit>(*kit)) && !_unit.MyOperatorHooksReleased)
			std::erase_if(_unit.MyFunnelRamps, [&](const FunnelRampEntry& _entry) { return !std::ranges::contains(_targets, _entry.MyTarget); });
		RedirectCandles(_unit.MyId, _targets);
		WolfBeforeAttack(_unit);
		if (const auto* ghost = _unit.MyDefinition.MyOperatorKit ? std::get_if<Ghost2Kit>(_unit.MyDefinition.MyOperatorKit) : nullptr;
			ghost && ghost->MySkill == Ghost2SkillKind::WEIGHT && _unit.MySkill.MyActive && !_unit.MyOperatorHooksReleased)
		{
			_unit.MyGhostHeavy.clear();
			for (const auto id : _targets) if (std::isgreaterequal(Unit(id).MyHealth / Unit(id).MyStats.MyMaxHealth, _unit.MyHealth / _unit.MyStats.MyMaxHealth - 1e-9)) _unit.MyGhostHeavy.push_back(id);
		}
		if (const auto* surtr = _unit.MyDefinition.MyOperatorKit ? std::get_if<SurtrKit>(_unit.MyDefinition.MyOperatorKit) : nullptr; surtr && surtr->MySkill == SurtrSkillKind::GIANT && !_unit.MyOperatorHooksReleased)
			_unit.MySurtrSolo = _unit.MySkill.MyActive && std::ranges::count_if(_targets, [&](UnitId _id) { return Unit(_id).MyAlive; }) == 1;
		// 原 beforeAttack 总线在处理器执行后过滤目标；没有处理器时保留原强制攻击语义。
		std::erase_if(_targets, [&](UnitId id) { return !Unit(id).MyAlive; });
		return _unit.MyAlive && !_targets.empty();
	}

	void Battle::GrantInsiderAmmo(UnitId _unit, std::uint64_t _deployment)
	{
		const auto& source = Unit(_unit);
		if (!source.MyAlive || source.MyDeploySequence != _deployment) return;
		const auto& kit = std::get<InsiderKit>(*source.MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch();
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (id != _unit && ally.MyKind == UnitKind::OPERATOR && ally.MyOwner == source.MyOwner && ally.MyAlive &&
				ally.MyDefinition.MySkill.MyKind == SkillKind::AMMO && std::ranges::contains(ally.MyDefinition.MyIdentity.MyBonds, "lateranoShip")) scratch.MyTargets.push_back(id);
		}
		const auto target = scratch.MyTargets.empty() ? UnitId{} : scratch.MyTargets[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MyTargets.size()))];
		--_MyAttackDepth;
		if (!target) return;
		const auto found = std::ranges::find_if(_MyInsiderGrants, [&](const auto& g) { return g.MySource == _unit && g.MyTarget == target; });
		const InsiderAmmoGrant grant{_unit, target, _deployment, kit.MyAllyAmmo};
		if (found == _MyInsiderGrants.end()) _MyInsiderGrants.push_back(grant); else *found = grant;
	}

	void Battle::OperatorReveal(UnitId _unit)
	{
		const auto& source = Unit(_unit);
		if (!source.MyAlive || source.MyOperatorHooksReleased) return;
		const bool pulse = source.MyDefinition.MyOperatorKit && (std::holds_alternative<RmixerKit>(*source.MyDefinition.MyOperatorKit) || std::holds_alternative<SvrashKit>(*source.MyDefinition.MyOperatorKit));
		for (std::size_t i = 0; i < _MyEnemyIds.size(); ++i)
		{
			const auto id = _MyEnemyIds[i];
			const auto& enemy = Unit(id);
			if (enemy.MyAlive && !enemy.MyHidden && enemy.MyStatuses.Has(CombatStatus::STEALTH) && InRuleRange(_unit, id))
				{
				if (pulse)
				{
					StatusFlags flags; flags.set(static_cast<std::size_t>(CombatStatus::REVEAL));
					(void)AddBuff(id, {.MyKey = "aura:reveal", .MyDuration = 0.25, .MyFlags = flags});
				}
				else (void)ApplyStatus(id, CombatStatus::REVEAL, 0.3, _unit);
			}
		}
	}

	void Battle::NotifyOperatorKits(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::DEPLOY && _event.MyUnit) WildmnDeploy(_event.MyUnit);
		NotifyVendlas(_event);
		NotifyOperatorObservers(_event);
		if (_event.MyKind == ContentEventKind::BATTLE_START)
		{
			for (std::size_t i = 0, count = _MySlchans.size(); i < count; ++i) SlchanTick(_MySlchans[i]);
			for (std::size_t i = 0, count = _MyBubbles.size(); i < count; ++i) BubbleTick(_MyBubbles[i]);
			for (std::size_t i = 0, count = _MyAkkords.size(); i < count; ++i) AkkordTick(_MyAkkords[i]);
			for (const auto id : _MyGhost2s) Ghost2Team(id);
			for (const auto id : _MyTexasUnits)
			{
				const auto& unit = Unit(id); const auto& texas = std::get<TexasKit>(*unit.MyDefinition.MyOperatorKit);
				if (texas.MyInitialDp > 0) (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, texas.MyInitialDp);
			}
			return;
		}
		if (_event.MyKind == ContentEventKind::SKILL_START && _event.MyUnit && Unit(_event.MyUnit).MyDefinition.MySkill.MyKind == SkillKind::AMMO)
		{
			double extra = 0;
			std::erase_if(_MyInsiderGrants, [&](const auto& grant)
			{
				if (grant.MyTarget != _event.MyUnit) return false;
				const auto& source = Unit(grant.MySource);
				if (!source.MyAlive || source.MyDeploySequence != grant.MyDeployment) return true;
				extra += grant.MyAmount; return false;
			});
			if (extra > 0) (void)AddSkillAmmo(_event.MyUnit, extra);
		}
		if (_event.MyKind != ContentEventKind::DEPLOY && _event.MyKind != ContentEventKind::SKILL_START && _event.MyKind != ContentEventKind::SKILL_ENDING &&
			_event.MyKind != ContentEventKind::SKILL_TICK && _event.MyKind != ContentEventKind::BEFORE_HEAL && _event.MyKind != ContentEventKind::SP_GAIN &&
			_event.MyKind != ContentEventKind::BEFORE_DAMAGE && _event.MyKind != ContentEventKind::DAMAGED && _event.MyKind != ContentEventKind::ATTACK &&
			_event.MyKind != ContentEventKind::AMMO_USED && _event.MyKind != ContentEventKind::BARD_REGEN && _event.MyKind != ContentEventKind::BUFF_TICK && _event.MyKind != ContentEventKind::SKILL_END && _event.MyKind != ContentEventKind::BEFORE_KILL) return;
		const auto id = _event.MySource ? _event.MySource : _event.MyUnit;
		if (!id) return;
		auto& unit = _MyUnits[Index(id)]; const auto* rules = unit.MyDefinition.MyOperatorKit;
		if (!rules || unit.MyOperatorHooksReleased) return;
		if (const auto* insider = std::get_if<InsiderKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::DEPLOY && insider->MyAllyAmmo > 0)
				Schedule({.MyAt = Time() + insider->MyDelay, .MyKind = ScheduledKind::INSIDER_AMMO, .MySource = id, .MyVersion = unit.MyDeploySequence});
			if (_event.MyKind == ContentEventKind::SKILL_START && unit.MyAlive && unit.MyDefinition.MySkill.MyKind == SkillKind::AMMO &&
				Time() - unit.MyDeployedAt + 1e-9 >= insider->MyDelay && insider->MySelfAmmo > 0) (void)AddSkillAmmo(id, insider->MySelfAmmo);
		}
		else if (const auto* leizi = std::get_if<LeiziKit>(rules))
			ScaleAttackByBlock(_event, leizi->MyUnblockedScale, false);
		else if (const auto* udflow = std::get_if<UdflowKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyDamage.MyIsAttack && _event.MyTarget && udflow->MyDuration > 0 && udflow->MyDamage > 0)
			{
				const auto& target = Unit(_event.MyTarget);
				if (target.MySide != UnitSide::ENEMY || !target.MyAlive) return;
				const auto amount = std::ranges::contains(target.MyDefinition.MyEnemyTags, "seamonster") ? udflow->MySeaDamage : udflow->MyDamage;
				(void)AddBuff(target.MyId, {.MyKey = "udflow:dot:" + std::to_string(id), .MySource = id, .MyDuration = udflow->MyDuration,
					.MyRefresh = BuffRefresh::EXTEND, .MyInterval = udflow->MyInterval,
					.MyTickEffects = {{.MyAmount = amount, .MyDamageType = DamageType::ARTS, .MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::DOT)}}});
			}
		}
		else if (const auto* vigna = std::get_if<VignaKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::ATTACK) (void)RemoveBuff(id, "vigna:proc");
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyDamage.MyIsAttack && _event.MyTarget)
			{
				const auto& target = Unit(_event.MyTarget);
				if (target.MySide == UnitSide::ENEMY && target.MyHealth / target.MyStats.MyMaxHealth < vigna->MyHealthThreshold) _event.MyDamage.MyAmount *= vigna->MyLowHealthScale;
			}
		}
		else if (const auto* prove = std::get_if<ProveKit>(rules))
		{
			if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MyDamage.MyIsAttack || !_event.MyTarget) return;
			const auto& target = Unit(_event.MyTarget); if (target.MySide != UnitSide::ENEMY) return;
			if (prove->MyHealthDrop > 0 && prove->MyScalePerDrop > 0)
				_event.MyDamage.MyAmount *= 1 + std::clamp(1 - target.MyHealth / target.MyStats.MyMaxHealth, 0.0, 1.0) / prove->MyHealthDrop * prove->MyScalePerDrop;
			if (_event.MyDamage.MyIsSplash || HasTag(_event.MyDamage.MyTags, DamageTag::CHAIN)) return;
			const auto origin = RulePosition(unit); const auto forward = RotateOffset({0, 1}, unit.MyFacing);
			const bool front = BodyOnTile(target, static_cast<int>(std::floor(origin.MyY + 0.5)) + forward.MyRow,
				static_cast<int>(std::floor(origin.MyX + 0.5)) + forward.MyColumn);
			const auto chance = front ? prove->MyFrontProbability : prove->MyProbability;
			if (chance > 0 && (chance >= 1 || _MyRandom.Next() < chance)) _event.MyDamage.MyAmount *= prove->MyCriticalScale;
		}
		else if (const auto* texas = std::get_if<TexasKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::SKILL_START) TexasSkill(id, *texas);
		}
		else if (const auto* caper = std::get_if<CaperKit>(rules))
		{
			if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MyDamage.MyIsAttack || !_event.MyTarget || Unit(_event.MyTarget).MySide != UnitSide::ENEMY) return;
			const auto origin = RulePosition(unit);
			if (BodyTileReach(Unit(_event.MyTarget), static_cast<int>(std::floor(origin.MyY + 0.5)), static_cast<int>(std::floor(origin.MyX + 0.5))) <= 1) _event.MyDamage.MyAmount *= caper->MyNearScale;
			if (!_event.MyDamage.MyIsSplash && caper->MyProbability > 0 && (caper->MyProbability >= 1 || _MyRandom.Next() < caper->MyProbability)) _event.MyDamage.MyAmount *= caper->MyCriticalScale;
		}
		else if (const auto* sunbr = std::get_if<SunbrKit>(rules))
		{
			SunbrSkill(id, *sunbr, _event.MyKind);
			if (_event.MyKind == ContentEventKind::BEFORE_HEAL && unit.MySkill.MyActive && !_event.MyHealOptions.MyRegen && !_event.MyHealOptions.MySelf && _event.MyTarget &&
				Unit(_event.MyTarget).MyHealth / Unit(_event.MyTarget).MyStats.MyMaxHealth < sunbr->MyHealthThreshold) _event.MyAmount *= sunbr->MyHealingScale;
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyDamage.MyIsAttack && !_event.MyDamage.MyIsSplash &&
				!HasTag(_event.MyDamage.MyTags, DamageTag::CHAIN) && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY &&
				sunbr->MyProbability > 0 && (sunbr->MyProbability >= 1 || _MyRandom.Next() < sunbr->MyProbability))
			{
				_event.MyDamage.MyAmount *= sunbr->MyCriticalScale; _event.MyDamage.MySunbrProc = true;
			}
			if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyDamage.MySunbrProc && _event.MyTarget && Unit(_event.MyTarget).MyAlive)
				(void)ApplyStatus(_event.MyTarget, CombatStatus::STUN, sunbr->MyStun, id);
		}
		else if (const auto* podego = std::get_if<PodegoKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::SKILL_START && !podego->MyHealing) PodegoStartZone(id, *podego);
			OperatorEnemyTimeSp(_event, podego->MySpPerSecond);
		}
		else if (const auto* utage = std::get_if<UtageKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && utage->MyBreach && unit.MySkill.MyActive &&
				_event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY && _event.MyDamage.MyIsAttack &&
				_event.MyDamage.MyType == DamageType::PHYSICAL) _event.MyDamage.MyType = DamageType::ARTS;
			if (_event.MyKind == ContentEventKind::SKILL_START)
			{
				if (utage->MyRest) ReleaseBlocked(_MyUnits[Index(id)]);
				else if (utage->MyBreach)
				{
					const auto loss = unit.MyHealth * utage->MyHealthLoss;
					if (loss > 0 && unit.MyHealth - loss >= 1) (void)LoseHealth(id, id, loss);
				}
			}
		}
		else if (const auto* indigo = std::get_if<IndigoKit>(rules); indigo && indigo->MyMaze)
		{
			if (_event.MyKind == ContentEventKind::SKILL_START) _MyUnits[Index(id)].MyIndigoAccumulator = 0;
			if (_event.MyKind == ContentEventKind::SKILL_TICK) IndigoTick(id, *indigo, _event.MyDelta);
		}
		else if (const auto* tinman = std::get_if<TinmanKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::SKILL_START && tinman->MyZone) TinmanStartZone(id, *tinman);
			if (_event.MyKind == ContentEventKind::SP_GAIN && _event.MySpReason == SpReason::TIME && unit.MyTinmanZones > 0 && tinman->MySpPerSecond > 0)
				_event.MyAmount += tinman->MySpPerSecond * BattleClock::StepSeconds;
		}
		else if (const auto* liskam = std::get_if<LiskamKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::SKILL_START && liskam->MyDefense)
				(void)AddBuff(id, {.MyKey = "liskam:block", .MyDuration = unit.MyDefinition.MySkill.MyDuration + 0.05});
			if (_event.MyKind == ContentEventKind::SKILL_ENDING)
			{
				if (liskam->MyDefense) (void)RemoveBuff(id, "liskam:block");
				else if (liskam->MyArc && unit.MyAlive && _event.MySkillReason != SkillReason::DEATH && std::isgreater(liskam->MySelfStun, 0))
					(void)ApplyStatus(id, CombatStatus::STUN, liskam->MySelfStun, id);
			}
		}
		else if (const auto* silent = std::get_if<SilentKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::BEFORE_HEAL && _event.MyTarget && Unit(_event.MyTarget).MyGround)
				_event.MyAmount *= silent->MyGroundHealScale;
			if (_event.MyKind == ContentEventKind::SKILL_START && silent->MyDrone) (void)ReleaseSkillSummon(id, silent->MyToken, silent->MyStockCap);
		}
		else if (const auto* slchan = std::get_if<SlchanKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::DEPLOY && Started()) SlchanTick(id);
			if (_event.MyKind == ContentEventKind::SKILL_START && slchan->MyChain) SlchanSkill(id, *slchan);
		}
		else if (const auto* grabds = std::get_if<GrabdsKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::SKILL_START && grabds->MyQuiet) GrabdsSkill(id, *grabds);
			OperatorEnemyTimeSp(_event, grabds->MySpPerSecond);
		}
		else if (const auto* papyrs = std::get_if<PapyrsKit>(rules); papyrs && papyrs->MyLockSkill)
			PapyrsSkill(id, *papyrs, _event.MyKind);
		else if (const auto* ghost = std::get_if<GhostKit>(rules))
		{
			ScaleAttackByBlock(_event, ghost->MyBlockedScale, true);
			if (_event.MyKind == ContentEventKind::SKILL_ENDING && ghost->MyUndying && unit.MyAlive && _event.MySkillReason != SkillReason::DEATH && std::isgreater(ghost->MyStun, 0))
				(void)ApplyStatus(id, CombatStatus::STUN, ghost->MyStun, id);
		}
		else if (std::holds_alternative<BubbleKit>(*rules))
		{
			if (_event.MyKind == ContentEventKind::DEPLOY && Started()) BubbleTick(id);
		}
		else if (const auto* humus = std::get_if<HumusKit>(rules))
		{
			if (humus->MyPeakSkill)
			{
				if (_event.MyKind == ContentEventKind::SKILL_START || _event.MyKind == ContentEventKind::SKILL_TICK) HumusPeakBuff(id, *humus);
				if (_event.MyKind == ContentEventKind::SKILL_ENDING) (void)RemoveBuff(id, "humus:peak");
			}
			else if (humus->MyCut && _event.MyKind == ContentEventKind::ATTACK && unit.MySkill.MyActive && unit.MySkill.MyPending && unit.MyAlive && std::isgreater(humus->MyHeal, 0))
				(void)Heal(id, id, humus->MyHeal, {.MySelf = true});
		}
		else if (const auto* rockr = std::get_if<RockrKit>(rules); rockr && rockr->MyOverload)
			RockrSkill(id, *rockr, _event);
		else if (const auto* kazema = std::get_if<KazemaKit>(rules)) KazemaSkill(id, *kazema, _event.MyKind);
		else if (const auto* gravel = std::get_if<GravelKit>(rules)) GravelSkill(id, *gravel, _event);
		else if (const auto* tippi = std::get_if<TippiKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::DEPLOY) _MyUnits[Index(id)].MyTippiLastHit = Time() - tippi->MyStackTime;
			if (_event.MyKind == ContentEventKind::SKILL_START || _event.MyKind == ContentEventKind::SKILL_ENDING) ReleaseBlocked(_MyUnits[Index(id)]);
		}
		else if (const auto* akkord = std::get_if<AkkordKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::DEPLOY && Started()) AkkordTick(id);
			if (_event.MyKind == ContentEventKind::ATTACK && akkord->MySonic && unit.MySkill.MyActive) AkkordSonic(id, *akkord);
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && _event.MyDamage.MyIsAttack && std::islessgreater(akkord->MyDistanceScale, 0))
			{
				const auto distance = Distance(RulePosition(unit), RulePosition(Unit(_event.MyTarget)));
				const auto fraction = std::isgreater(akkord->MyMaxDistance, akkord->MyMinDistance) ?
					std::clamp((distance - akkord->MyMinDistance) / (akkord->MyMaxDistance - akkord->MyMinDistance), 0.0, 1.0) : 1;
				_event.MyDamage.MyAmount *= 1 + akkord->MyDistanceScale * fraction;
			}
		}
		else if (const auto* branch = std::get_if<BranchKit>(rules)) BranchSkill(id, *branch, _event);
		else if (const auto* ashlok = std::get_if<AshlokKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::DEPLOY) AshlokDeploy(id, *ashlok);
			ScaleAttackByBlock(_event, ashlok->MyBlockedScale, true);
		}
		else if (std::holds_alternative<AngelKit>(*rules))
		{
			if (_event.MyKind == ContentEventKind::DEPLOY) Schedule({.MyAt = Time(), .MyKind = ScheduledKind::ANGEL_BLESS, .MySource = id});
		}
		else if (const auto* ayer = std::get_if<AyerKit>(rules))
		{
			if (ayer->MyBlade && unit.MySkill.MyActive && _event.MyKind == ContentEventKind::ATTACK) AyerAttack(id, *ayer);
		}
		else if (std::holds_alternative<SwireKit>(*rules))
		{
			if (unit.MySkillAura != NoPlayer && (_event.MyKind == ContentEventKind::SKILL_START || _event.MyKind == ContentEventKind::SKILL_ENDING)) RefreshOperatorAura(unit.MySkillAura);
		}
		else if (const auto* skadi = std::get_if<SkadiKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::DEPLOY) _MyUnits[Index(id)].MySkadiRevived = false;
			ScaleAttackByBlock(_event, skadi->MyBlockedScale, true);
		}
		else if (const auto* swire = std::get_if<Swire2Kit>(rules)) Swire2Skill(id, *swire, _event);
		else if (const auto* philae = std::get_if<PhilaeKit>(rules)) PhilaeSkill(id, *philae, _event);
		else if (const auto* forcer = std::get_if<ForcerKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::SKILL_START && forcer->MyPush) ForcerSkill(id, *forcer);
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY &&
				std::isgreaterequal(Unit(_event.MyTarget).MyStats.MyMass, forcer->MyHeavyMass)) _event.MyDamage.MyDefenseIgnoreFlat += forcer->MyDefenseIgnore;
			if (_event.MyKind == ContentEventKind::DEPLOY && !_event.MyInitial && !unit.MyGround && unit.MyOwner != NoPlayer && std::isgreater(forcer->MyRefundRatio, 0))
				(void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, unit.MyDefinition.MyStats.MyDeploymentCost * forcer->MyRefundRatio);
		}
		else if (const auto* mint = std::get_if<MintKit>(rules)) MintSkill(id, *mint, _event);
		else if (const auto* haini = std::get_if<HainiKit>(rules)) HainiSkill(id, *haini, _event);
		else if (const auto* pinecn = std::get_if<PinecnKit>(rules)) PinecnSkill(id, *pinecn, _event);
		else if (const auto* snhunt = std::get_if<SnhuntKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::SKILL_START) SnhuntSkill(id, *snhunt);
		}
		else if (const auto* blemsh = std::get_if<BlemshKit>(rules)) BlemshSkill(id, *blemsh, _event);
		else if (const auto* weakening = std::get_if<WeakeningKit>(rules))
		{
			if (weakening->MySummon && _event.MyKind == ContentEventKind::SKILL_START) (void)ReleaseSkillSummon(id, weakening->MyToken, 1);
			if (weakening->MySkillAura && (_event.MyKind == ContentEventKind::SKILL_START || _event.MyKind == ContentEventKind::SKILL_TICK || _event.MyKind == ContentEventKind::SKILL_ENDING)) RefreshOperatorAura(unit.MySkillAura);
		}
		else if (const auto* shotst = std::get_if<ShotstKit>(rules); shotst && shotst->MyBurst && _event.MyKind == ContentEventKind::SKILL_START) ShotstBurst(id, *shotst);
		else if (const auto* vulpis = std::get_if<VulpisKit>(rules)) VulpisSkill(id, *vulpis, _event);
		else if (const auto* kjera = std::get_if<KjeraKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::DEPLOY) KjeraDeploy(id, *kjera);
			if (kjera->MyLockDrones && kjera->MyExtraDrones && (_event.MyKind == ContentEventKind::SKILL_START || _event.MyKind == ContentEventKind::SKILL_ENDING))
			{ unit.MyLockedDrones.clear(); unit.MyDroneQueues.clear(); }
		}
		else if (const auto* archet = std::get_if<ArchetKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::DEPLOY) (void)AddBuff(id, {.MyKey = "talent:archet_shield", .MyShield = {.MyHits = 1}, .MyShieldBreakSp = archet->MyShieldSp});
			if (_event.MyKind == ContentEventKind::SKILL_START && archet->MySkill == ArchetSkillKind::PURSUIT) ArchetPursuit(id, *archet);
		}
		else if (const auto* vigil = std::get_if<VigilKit>(rules)) VigilSkill(id, *vigil, _event);
		else if (const auto* mostma = std::get_if<MostmaKit>(rules)) MostmaSkill(id, *mostma, _event);
		else if (const auto* rmixer = std::get_if<RmixerKit>(rules)) RmixerSkill(id, *rmixer, _event);
		else if (const auto* precision = std::get_if<PrecisionKit>(rules)) PrecisionSkill(id, *precision, _event);
		else if (const auto* beewax = std::get_if<BeewaxKit>(rules)) BeewaxSkill(id, *beewax, _event);
		else if (const auto* ines = std::get_if<InesKit>(rules)) InesSkill(id, *ines, _event);
		else if (const auto* mizuki = std::get_if<MizukiKit>(rules); mizuki && _event.MyKind == ContentEventKind::ATTACK) MizukiAttack(id, *mizuki, _event);
		else if (const auto* aroma = std::get_if<AromaKit>(rules); aroma && _event.MyKind == ContentEventKind::BEFORE_DAMAGE) AromaHit(_event, *aroma);
		else if (const auto* rosmon = std::get_if<RosmonKit>(rules)) RosmonSkill(id, *rosmon, _event);
		else if (const auto* cello = std::get_if<CelloKit>(rules)) CelloSkill(id, *cello, _event);
		else if (const auto* reed = std::get_if<Reed2Kit>(rules)) Reed2Skill(id, *reed, _event);
		else if (const auto* halo = std::get_if<Halo2Kit>(rules))
		{
			if (_event.MyKind == ContentEventKind::DEPLOY) unit.MyHaloStacks = 0;
			if (halo->MySkill == Halo2SkillKind::LINKS && (_event.MyKind == ContentEventKind::SKILL_START || _event.MyKind == ContentEventKind::SKILL_ENDING)) unit.MyHaloLocks.clear();
		}
		else if (const auto* agoat = std::get_if<Agoat2Kit>(rules)) Agoat2Skill(id, *agoat, _event);
		else if (const auto* nearl = std::get_if<Nearl2Kit>(rules)) Nearl2Skill(id, *nearl, _event);
		else if (const auto* siege = std::get_if<Siege2Kit>(rules)) Siege2Skill(id, *siege, _event);
		else if (const auto* snow = std::get_if<Sbell2Kit>(rules)) Sbell2Skill(id, *snow, _event);
		else if (const auto* yu = std::get_if<YuKit>(rules)) YuSkill(id, *yu, _event);
		else if (const auto* blkkgt = std::get_if<BlkkgtKit>(rules)) BlkkgtSkill(id, *blkkgt, _event);
		else if (const auto* lumen = std::get_if<LumenKit>(rules)) LumenSkill(id, *lumen, _event);
		else if (const auto* pasngr = std::get_if<PasngrKit>(rules)) PasngrSkill(id, *pasngr, _event);
		else if (const auto* pepe = std::get_if<PepeKit>(rules)) PepeSkill(id, *pepe, _event);
		else if (const auto* qiubai = std::get_if<QiubaiKit>(rules)) QiubaiSkill(id, *qiubai, _event);
		else if (const auto* lemuen = std::get_if<LemuenKit>(rules)) LemuenSkill(id, *lemuen, _event);
		else if (const auto* thorn = std::get_if<Thorn2Kit>(rules); thorn && _event.MyKind == ContentEventKind::SKILL_START) Thorn2Start(id, *thorn);
		else if (const auto* mlynar = std::get_if<MlynarKit>(rules)) MlynarSkill(id, *mlynar, _event);
		else if (const auto* aglina = std::get_if<AglinaKit>(rules)) AglinaSkill(id, *aglina, _event);
		else if (const auto* sntlla = std::get_if<SntllaKit>(rules)) SntllaSkill(id, *sntlla, _event);
		else if (const auto* nymph = std::get_if<NymphKit>(rules)) NymphSkill(id, *nymph, _event);
		else if (const auto* svash = std::get_if<Svash2Kit>(rules)) Svash2Skill(id, *svash, _event);
		else if (const auto* ghost = std::get_if<Ghost2Kit>(rules)) Ghost2Skill(id, *ghost, _event);
		else if (const auto* dusk = std::get_if<DuskKit>(rules)) DuskSkill(id, *dusk, _event);
		else if (const auto* lisa = std::get_if<LisaKit>(rules)) LisaSkill(id, *lisa, _event);
		else if (const auto* demkni = std::get_if<DemkniKit>(rules)) DemkniSkill(id, *demkni, _event);
		else if (const auto* horn = std::get_if<HornKit>(rules)) HornSkill(id, *horn, _event);
		else if (const auto* surtr = std::get_if<SurtrKit>(rules)) SurtrSkill(id, *surtr, _event);
		else if (const auto* etlchi = std::get_if<EtlchiKit>(rules)) EtlchiSkill(id, *etlchi, _event);
		else if (const auto* ulpia = std::get_if<UlpiaKit>(rules)) UlpiaSkill(id, *ulpia, _event);
		else if (const auto* blaze = std::get_if<Blaze2Kit>(rules)) Blaze2Skill(id, *blaze, _event);
		else if (const auto* titi = std::get_if<TitiKit>(rules)) TitiSkill(id, *titi, _event);
		else if (const auto* excu = std::get_if<Excu2Kit>(rules)) Excu2Skill(id, *excu, _event);
		else if (const auto* cetsyr = std::get_if<CetsyrKit>(rules)) CetsyrSkill(id, *cetsyr, _event);
		else if (const auto* gvial = std::get_if<GvialKit>(rules)) GvialSkill(id, *gvial, _event);
		else if (const auto* billro = std::get_if<BillroKit>(rules)) BillroSkill(id, *billro, _event);
		else if (const auto* bldsk = std::get_if<BldskKit>(rules)) BldskSkill(id, *bldsk, _event);
		else if (const auto* flamtl = std::get_if<FlamtlKit>(rules)) FlamtlSkill(id, *flamtl, _event);
		else if (const auto* fartth = std::get_if<FartthKit>(rules)) FartthSkill(id, *fartth, _event);
		else if (const auto* svrash = std::get_if<SvrashKit>(rules); svrash && _event.MyKind == ContentEventKind::DAMAGED && _event.MyDamage.MyIsAttack && _event.MyDamage.MyType == DamageType::PHYSICAL)
			OperatorAddition(id, _event.MyTarget, svrash->MyAdditionScale, static_cast<DamageTags>(DamageTag::MODULE));
		else if (const auto* texas = std::get_if<Texas2Kit>(rules)) Texas2Skill(id, *texas, _event);
		else if (const auto* mudrok = std::get_if<MudrokKit>(rules)) MudrokSkill(id, *mudrok, _event);
		else if (const auto* gnosis = std::get_if<GnosisKit>(rules)) GnosisSkill(id, *gnosis, _event);
		else if (const auto* lionhd = std::get_if<LionhdKit>(rules); lionhd && lionhd->MyBurst && _event.MyKind == ContentEventKind::SKILL_START) LionhdSkill(id, *lionhd);
		else if (const auto* reckpr = std::get_if<ReckprKit>(rules); reckpr && _event.MyKind == ContentEventKind::BEFORE_HEAL)
			LowHealthHealBonus(_event, reckpr->MyHealthRatio, reckpr->MySharedGuard && !std::isgreater(reckpr->MyHealScale, 1) ? 0 : reckpr->MyHealScale, false, reckpr->MySharedGuard, reckpr->MySharedGuard);
		else if (const auto* glady = std::get_if<GladyKit>(rules)) GladySkill(id, *glady, _event);
		else if (const auto* pithst = std::get_if<PithstKit>(rules))
		{
			if (!(pithst->MyElementRatio > 0) || _event.MyKind != ContentEventKind::DAMAGED || !_event.MyTarget || !(_event.MyAmount > 0) || _event.MyElement ||
				_event.MyDamage.MyType == DamageType::ELEMENTAL || HasTag(_event.MyDamage.MyTags, DamageTag::BURST)) return;
			const auto& target = Unit(_event.MyTarget);
			if (target.MySide != UnitSide::ENEMY || !(target.MyHealth > 0)) return;
			const bool elite = target.MySpawnTag == EnemySpawnTag::BOSS || target.MyDefinition.MyElite || target.MyDefinition.MyLeader;
			const auto amount = unit.MyStats.MyAttack * (elite ? pithst->MyEliteElementRatio : pithst->MyElementRatio);
			if (!(amount > 0)) return;
			for (const auto element : {Element::NEURAL, Element::BURN, Element::APOPTOSIS})
				if (Unit(target.MyId).MyHealth > 0) (void)DealElement(id, target.MyId, {.MyElement = element, .MyAmount = amount, .MyTags = static_cast<DamageTags>(DamageTag::TALENT)});
		}
	}
}
