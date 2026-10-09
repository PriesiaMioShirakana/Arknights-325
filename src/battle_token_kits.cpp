#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	const CombatDefinition* Battle::FindTokenTemplate(UnitId _owner, std::string_view _token) const
	{
		const auto& owner = Unit(_owner);
		if (owner.MyOwner == NoPlayer) return nullptr;
		const auto& templates = _MyInput.MyPlayers[owner.MyOwner].MyTokenTemplates;
		const auto found = std::ranges::find_if(templates, [&](const auto& _entry)
			{ return _entry.MyOwnerPieceUid == owner.MyPieceUid && _entry.MyDefinition.MyId == _token; });
		return found == templates.end() ? nullptr : &found->MyDefinition;
	}

	void Battle::DockSkillSummons()
	{
		_MySkillSummons.reserve(_MyAllyCount);
		_MyRosmonGears.reserve(_MyAllyCount);
		std::size_t index = 0;
		for (const auto& player : _MyInput.MyPlayers)
			for (const auto& deployment : player.MyUnits)
			{
				auto& unit = _MyUnits[index++];
				if (deployment.MyTokenSource > TokenSource::UNAVAILABLE ||
					(deployment.MyTokenSource != TokenSource::NONE && (unit.MyKind != UnitKind::TOKEN || !unit.MyOwnerUnit)))
					throw std::invalid_argument("skill summon requires a placed token and its owner");
				if (deployment.MyTokenSource == TokenSource::UNAVAILABLE) { unit.MyDeferred = true; continue; }
				if (deployment.MyTokenSource != TokenSource::SKILL) continue;
				auto found = std::ranges::find_if(_MySkillSummons, [&](const auto& _group)
					{ return _group.MyOwner == unit.MyOwnerUnit && _group.MyToken == unit.MyDefinition.MyId; });
				if (found == _MySkillSummons.end())
				{
					auto& group = _MySkillSummons.emplace_back(SkillSummonRuntime{.MyOwner = unit.MyOwnerUnit, .MyToken = unit.MyDefinition.MyId});
					group.MyPieces.reserve(static_cast<std::size_t>(std::ranges::count_if(_MyUnits, [&](const auto& _piece)
						{ return _piece.MyKind == UnitKind::TOKEN && _piece.MyOwnerUnit == unit.MyOwnerUnit && _piece.MyDefinition.MyId == unit.MyDefinition.MyId; })));
					found = std::prev(_MySkillSummons.end());
				}
				unit.MySummonGroup = static_cast<std::size_t>(found - _MySkillSummons.begin());
				found->MyPieces.push_back(unit.MyId);
				// 对照提交的 SKILL_SUMMON_START_DEPLOY 为 true，初始棋子不消耗库存。
			}
	}

	unsigned Battle::SkillSummonStock(UnitId _owner, std::string_view _token) const
	{
		(void)Unit(_owner);
		const auto found = std::ranges::find_if(_MySkillSummons, [&](const auto& _group) { return _group.MyOwner == _owner && _group.MyToken == _token; });
		return found == _MySkillSummons.end() ? 0 : found->MyStock;
	}

	UnitId Battle::ReleaseSkillSummon(UnitId _owner, std::string_view _token, unsigned _cap)
	{
		(void)Unit(_owner);
		if (!_MyStarted || Finished()) return 0;
		const auto found = std::ranges::find_if(_MySkillSummons, [&](const auto& _group) { return _group.MyOwner == _owner && _group.MyToken == _token; });
		if (found == _MySkillSummons.end()) return 0;
		const auto cap = std::max(1U, _cap);
		found->MyStock = found->MyStock < cap ? found->MyStock + 1 : cap;
		for (const auto id : found->MyPieces) if (DeployDockedSummon(id)) return id;
		return 0;
	}

	bool Battle::DeployDockedSummon(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (unit.MySummonGroup == NoPlayer || unit.MyAlive || unit.MyRemoved || Finished()) return false;
		auto& group = _MySkillSummons[unit.MySummonGroup];
		if (!group.MyStock || !Unit(group.MyOwner).MyAlive) return false;
		const auto wait = unit.MySummonReadyAt - Time();
		if (std::isgreater(wait, 1e-9) || !Redeploy(_unit))
		{
			if (!unit.MySummonRetry)
			{
				unit.MySummonRetry = true;
				Schedule({.MyAt = Time() + std::max(wait, 0.5), .MyKind = ScheduledKind::DOCKED_SUMMON_RETRY, .MyTarget = _unit});
			}
			return false;
		}
		if (group.MyStock) --group.MyStock;
		return true;
	}

	void Battle::TokenDeploy(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (unit.MyKind != UnitKind::TOKEN || !unit.MyDefinition.MyTokenKit) return;
		const auto& kit = *unit.MyDefinition.MyTokenKit;
		if (kit.MyKind == TokenKitKind::ROSMON_GEAR)
		{
			if (!std::ranges::contains(_MyRosmonGears, _unit)) _MyRosmonGears.push_back(_unit);
			TokenBurst(_unit, _unit, 0, kit.MyBurstStun, kit.MyBurstRange);
		}
		if (kit.MyKind == TokenKitKind::RADIANT_SWORD || kit.MyKind == TokenKitKind::GOLDEN_OATH)
		{
			unit.MyBoundSkillActivation.reset();
			if (unit.MyOwnerUnit)
			{
				const auto& owner = Unit(unit.MyOwnerUnit);
				if (owner.MySkill.MyActive && IsTimedSkill(owner.MyDefinition.MySkill.MyKind)) unit.MyBoundSkillActivation = owner.MySkill.MyActivations;
			}
			if (!std::ranges::contains(_MySkillBoundTokens, _unit)) _MySkillBoundTokens.push_back(_unit);
			if (kit.MyKind == TokenKitKind::RADIANT_SWORD)
			{
				const auto previous = LastDeployedOperator(unit.MyOwner);
				const bool combo = previous && std::ranges::contains(Unit(previous).MyDefinition.MyIdentity.MyBonds, "kazimierzShip");
				TokenBurst(_unit, _unit, kit.MyBurstScale, kit.MyBurstStun, kit.MyBurstRange, DamageType::TRUE_DAMAGE, combo ? 2 : 1);
			}
		}
		if (kit.MyKind == TokenKitKind::CAT_SHIELD) CatShieldConnect(_unit);
		if (kit.MyKind == TokenKitKind::OBELISK && !kit.MyBurstSuppressed) TokenBurst(_unit, _unit, kit.MyBurstScale, kit.MyBurstStun, kit.MyBurstRange);
		if (kit.MyKind == TokenKitKind::CURSE_DOLL)
		{
			unit.MyTokenAuraAccumulator = 0.2;
			if (unit.MySkillAura == NoPlayer)
				unit.MySkillAura = InstallOperatorAura(_unit, {.MyKey = "token:curseDoll", .MyStacking = OperatorAuraStacking::REPLACE,
					.MyInterval = 0, .MyDuration = 0.35, .MyEnemies = true, .MyAttackRange = true, .MySkipHidden = true,
					.MyModifiers = kit.MyAuraModifiers, .MyRefresh = BuffRefresh::EXTEND});
			if (!std::ranges::contains(_MyCurseDolls, _unit)) _MyCurseDolls.push_back(_unit);
		}
		if (kit.MyKind == TokenKitKind::CHAMPAGNE && kit.MyManagedBomb)
		{
			unit.MyBombFirstTick = Tick() + 1;
			if (!std::ranges::contains(_MyChampagnes, _unit)) _MyChampagnes.push_back(_unit);
		}
		const bool managed = unit.MyOwnerUnit && (Unit(unit.MyOwnerUnit).MyDefinition.MyOperatorKit || Unit(unit.MyOwnerUnit).MyDefinition.MyContent.MyTag == ContentTag::CUSTOM_OPERATOR);
		if (kit.MyKind == TokenKitKind::PAPER_DOLL && !managed) OrigamiBurst(_unit, kit.MyBurstScale, true);
		if (std::isgreater(kit.MyLifetime, 0))
		{
			unit.MyExpiresAt = Time() + kit.MyLifetime;
			if (kit.MyCountdown)
			{
				unit.MyCountdown = TokenCountdown{.MyFrom = Time(), .MyUntil = unit.MyExpiresAt};
				if (!std::ranges::any_of(unit.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "token:countdown"; }))
				{
					StatusFlags flags;
					for (const auto status : {CombatStatus::INVULNERABLE, CombatStatus::NO_HEAL, CombatStatus::HEAL_FREE}) flags.set(static_cast<std::size_t>(status));
					(void)AddBuff(_unit, {.MyKey = "token:countdown", .MyFlags = flags, .MyPersistent = true, .MyAllowDead = true});
				}
			}
			Schedule({.MyAt = unit.MyExpiresAt, .MyKind = ScheduledKind::TOKEN_KIT_EXPIRE, .MyTarget = _unit, .MyVersion = unit.MyDeploySequence});
		}
	}

	void Battle::CurseDollTick(UnitId _unit, double _delta)
	{
		auto& unit = _MyUnits[Index(_unit)];
		unit.MyTokenAuraAccumulator += _delta;
		if (std::isless(unit.MyTokenAuraAccumulator, 0.2 - 1e-9)) return;
		unit.MyTokenAuraAccumulator = 0;
		if (unit.MySkillAura != NoPlayer) RefreshOperatorAura(unit.MySkillAura);
	}

	void Battle::NotifyTokenKits(ContentEvent& _event, bool _late)
	{
		if (!_late && _event.MyKind == ContentEventKind::SKILL_END && _event.MyUnit)
			for (std::size_t i = 0, count = _MySkillBoundTokens.size(); i < count; ++i)
			{
				const auto& token = Unit(_MySkillBoundTokens[i]);
				if (token.MyAlive && token.MyOwnerUnit == _event.MyUnit && token.MyBoundSkillActivation == Unit(_event.MyUnit).MySkill.MyActivations)
					(void)Retreat(token.MyId, true, RemovalReason::EXPIRED);
			}
		WolfFatal(_event, _late);
		WolfDeath(_event, _late);
		if (!_late && _event.MyKind == ContentEventKind::DEATH && _event.MyUnit)
		{
			const auto& unit = Unit(_event.MyUnit);
			if (unit.MyShieldRecipient && Unit(unit.MyShieldRecipient).MyShieldDevice == unit.MyId) _MyUnits[Index(unit.MyShieldRecipient)].MyShieldDevice = 0;
		}
		if (!_late && _event.MyKind == ContentEventKind::DEATH)
			for (std::size_t i = 0, count = _MyCurseDolls.size(); i < count; ++i)
			{
				const auto& doll = Unit(_MyCurseDolls[i]);
				if (doll.MyAlive && doll.MyOwnerUnit == _event.MyUnit) Retreat(doll.MyId, true, RemovalReason::EXPIRED);
			}
		if (!_late && _event.MyKind == ContentEventKind::TICK)
		{
			for (std::size_t i = 0, count = _MyRosmonGears.size(); i < count; ++i) RosmonGearTick(_MyRosmonGears[i]);
			std::erase_if(_MyRosmonGears, [&](UnitId _id) { return Unit(_id).MyRemoved; });
			for (std::size_t i = 0, count = _MyChampagnes.size(); i < count; ++i)
				if (Unit(_MyChampagnes[i]).MyBombFirstTick <= Tick()) ChampagneTick(_MyChampagnes[i]);
			std::erase_if(_MyChampagnes, [&](UnitId _id) { return Unit(_id).MyRemoved; });
		}
		if (!_event.MyUnit) return;
		auto& unit = _MyUnits[Index(_event.MyUnit)];
		if (!_late && _event.MyKind == ContentEventKind::SKILL_TICK && unit.MyDefinition.MyTokenKit && unit.MyDefinition.MyTokenKit->MyKind == TokenKitKind::CURSE_DOLL)
			CurseDollTick(unit.MyId, _event.MyDelta);
		if (!_late && _event.MyKind == ContentEventKind::SKILL_TICK && unit.MyDefinition.MyTokenKit &&
			unit.MyDefinition.MyTokenKit->MyKind == TokenKitKind::CHAMPAGNE && !unit.MyDefinition.MyTokenKit->MyManagedBomb) ChampagneTick(unit.MyId);
		if (_late && _event.MyKind == ContentEventKind::DEATH && unit.MySummonGroup != NoPlayer && !Finished())
		{
			unit.MyRemoved = false;
			unit.MySummonReadyAt = Time() + std::max(0.0, unit.MyDefinition.MyStats.MyRedeploySeconds);
			(void)DeployDockedSummon(unit.MyId);
		}
		if (_event.MyKind != ContentEventKind::DEPLOY) return;
		if (!_late)
		{
			TokenDeploy(unit.MyId);
			for (const auto& group : _MySkillSummons)
				if (group.MyOwner == unit.MyId)
					for (const auto id : group.MyPieces) (void)DeployDockedSummon(id);
			return;
		}
		if (unit.MyKind != UnitKind::TOKEN || !unit.MyOwnerUnit || !unit.MyDefinition.MyTokenKit || !unit.MyDefinition.MyTokenKit->MyDeployLimit) return;
		const auto limit = unit.MyDefinition.MyTokenKit->MyDeployLimit;
		auto& scratch = AcquireAttackScratch();
		struct ScratchGuard
		{
			std::size_t& MyDepth;

			~ScratchGuard() { --MyDepth; }
		} guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyAllyIds)
		{
			const auto& token = Unit(id);
			if (token.MyAlive && token.MyKind == UnitKind::TOKEN && token.MyOwnerUnit == unit.MyOwnerUnit &&
				token.MyDefinition.MyId == unit.MyDefinition.MyId && token.MyDefinition.MyTokenKit) scratch.MyTargets.push_back(id);
		}
		if (scratch.MyTargets.size() <= limit) return;
		std::ranges::sort(scratch.MyTargets, {}, [&](UnitId _id) { return Unit(_id).MyDeploySequence; });
		const auto remove = scratch.MyTargets.size() - limit;
		for (std::size_t i = 0; i < remove; ++i)
			if (scratch.MyTargets[i] != unit.MyId) Retreat(scratch.MyTargets[i], true, RemovalReason::EXPIRED);
	}
}
