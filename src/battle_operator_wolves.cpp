#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		const WolfPackDefinition* WolfDefinition(const CombatUnit& _unit)
		{
			const auto& kit = _unit.MyDefinition.MyTokenKit;
			return kit && kit->MyKind == TokenKitKind::WOLF_PACK && kit->MyWolf ? &*kit->MyWolf : nullptr;
		}

		const VigilKit* VigilDefinition(const CombatUnit& _unit)
		{
			return _unit.MyDefinition.MyOperatorKit ? std::get_if<VigilKit>(_unit.MyDefinition.MyOperatorKit) : nullptr;
		}
	}

	bool Battle::WolfOnField(UnitId _unit) const
	{
		return _unit && (Unit(_unit).MyAlive || (Unit(_unit).MyWolfTactical && !Unit(_unit).MyRemoved));
	}

	void Battle::SetWolfShadows(UnitId _unit, unsigned _count)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto* kit = WolfDefinition(unit);
		if (!kit) return;
		unit.MyWolfShadows = _count;
		(void)AddBuff(_unit, {.MyKey = kit->MyManaged ? "token:wolves" : "wolf:shadows",
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::BLOCK_COUNT, .MyValue = kit->MyBlockPerShadow * _count}},
			.MyPersistent = !kit->MyManaged, .MyAllowDead = true});
	}

	bool Battle::AddWolfShadow(UnitId _unit, unsigned _maximum)
	{
		if (!_unit || !Unit(_unit).MyAlive || Unit(_unit).MyWolfShadows >= _maximum) return false;
		SetWolfShadows(_unit, Unit(_unit).MyWolfShadows + 1);
		return true;
	}

	void Battle::WolfDeploy(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto* kit = WolfDefinition(unit);
		if (!kit) return;
		if (!std::ranges::contains(_MyWolves, _unit)) _MyWolves.push_back(_unit);
		unsigned initial = kit->MyMaxShadows > 1 ? kit->MyMaxShadows - 1 : 1;
		if (unit.MyOwnerUnit)
		{
			auto& owner = _MyUnits[Index(unit.MyOwnerUnit)];
			if (const auto* vigil = VigilDefinition(owner)) initial = vigil->MyInitialWolves;
			if (owner.MyDefinition.MyProfession.MyKind == ProfessionTrait::TACTICIAN) owner.MyProfession.MyReinforcement = _unit;
		}
		SetWolfShadows(_unit, unit.MyWolfReturning ? 1 : std::clamp(initial, 1U, kit->MyMaxShadows));
		unit.MyHealth = unit.MyStats.MyMaxHealth;
		if (std::isgreater(kit->MyInterval, 0)) Schedule({.MyAt = Time() + kit->MyInterval, .MyKind = ScheduledKind::WOLF_GROW,
			.MySource = _unit, .MyInterval = kit->MyInterval, .MyVersion = unit.MyDeploySequence});
	}

	bool Battle::ReturnWolf(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (unit.MyAlive || !unit.MyWolfTactical || unit.MyRemoved || Finished() || (unit.MyOwnerUnit && !Unit(unit.MyOwnerUnit).MyAlive)) return false;
		unit.MyWolfReturning = true;
		struct ReturnGuard
		{
			bool& MyReturning;

			~ReturnGuard() { MyReturning = false; }
		};
		const ReturnGuard guard{.MyReturning = unit.MyWolfReturning};
		if (!Redeploy(_unit, true)) return false;
		unit.MyWolfTactical = false;
		++unit.MyWolfFormSequence;
		return true;
	}

	void Battle::WolfFatal(ContentEvent& _event, bool _late)
	{
		if (_event.MyKind != ContentEventKind::FATAL || !_event.MyUnit || _event.MyPrevented) return;
		auto& unit = _MyUnits[Index(_event.MyUnit)]; const auto* kit = WolfDefinition(unit);
		if (!kit || kit->MyManaged == _late || unit.MyWolfShadows <= 1) return;
		_event.MyPrevented = true;
		SetWolfShadows(unit.MyId, unit.MyWolfShadows - 1);
		unit.MyHealth = unit.MyStats.MyMaxHealth;
	}

	void Battle::WolfDeath(const ContentEvent& _event, bool _late)
	{
		if (_event.MyKind != ContentEventKind::DEATH || !_event.MyUnit) return;
		if (!_late)
		{
			for (std::size_t i = 0, count = _MyWolves.size(); i < count; ++i)
			{
				auto& wolf = _MyUnits[Index(_MyWolves[i])];
				if (wolf.MyOwnerUnit != _event.MyUnit) continue;
				if (wolf.MyWolfTactical)
				{
					wolf.MyWolfTactical = false; ++wolf.MyWolfFormSequence;
				}
				if (wolf.MyAlive && Unit(_event.MyUnit).MyDefinition.MyProfession.MyKind == ProfessionTrait::TACTICIAN)
					Retreat(wolf.MyId, true, RemovalReason::EXPIRED);
			}
			return;
		}
		auto& unit = _MyUnits[Index(_event.MyUnit)]; const auto* kit = WolfDefinition(unit);
		if (!kit || Finished() || (unit.MyOwnerUnit && !Unit(unit.MyOwnerUnit).MyAlive) ||
			(_event.MyRemovalReason != RemovalReason::KILLED && _event.MyRemovalReason != RemovalReason::RETREAT && _event.MyRemovalReason != RemovalReason::FORCED_EXIT)) return;
		unit.MyRemoved = false;
		unit.MyWolfTactical = true;
		const auto delay = std::isgreater(kit->MyInterval, 0) ? kit->MyInterval : std::max(0.0, unit.MyDefinition.MyStats.MyRedeploySeconds);
		unit.MyWolfReturnAt = Time() + delay;
		++unit.MyWolfFormSequence;
		SetWolfShadows(unit.MyId, 0);
		Schedule({.MyAt = unit.MyWolfReturnAt, .MyKind = ScheduledKind::WOLF_RETURN, .MySource = unit.MyId, .MyVersion = unit.MyWolfFormSequence});
	}

	void Battle::VigilDeploy(UnitId _unit, const VigilKit& _kit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (!unit.MyAlive || WolfOnField(unit.MyProfession.MyReinforcement)) return;
		UnitId waiting = 0, board = 0;
		for (const auto id : _MyAllyIds)
		{
			const auto& piece = Unit(id);
			if (piece.MyKind != UnitKind::TOKEN || piece.MyOwnerUnit != _unit || piece.MyDefinition.MyId != _kit.MyToken) continue;
			if (piece.MyAlive) { unit.MyProfession.MyReinforcement = id; return; }
			if (!waiting && !piece.MyRemoved) waiting = id;
			if (!board && piece.MyPieceUid) board = id;
		}
		if (waiting && Redeploy(waiting, true)) { unit.MyProfession.MyReinforcement = waiting; return; }
		const auto* definition = FindTokenTemplate(_unit, _kit.MyToken);
		if (!definition || !definition->MyTokenKit || !definition->MyTokenKit->MyWolf) return;
		std::optional<WorldPoint> tile;
		if (board)
		{
			const auto point = Unit(board).MyHome;
			const int row = static_cast<int>(point.MyY), column = static_cast<int>(point.MyX);
			if (row >= 0 && row < FieldRows && column >= 0 && column < FieldColumns && unit.MyBaseTriggerMask.test(static_cast<std::size_t>(FieldGrid::Key(row, column))) &&
				!ReservedTile(point) && (!_MyGrid || (_MyGrid->GroundPassable(row, column) && _MyGrid->CanStand(row, column)))) tile = point;
		}
		if (!tile) tile = FindTacticalPoint(_unit);
		if (!tile) return;
		auto body = *definition;
		body.MyTokenKit->MyWolf->MyManaged = true;
		body.MyTokenKit->MyWolf->MyMaxShadows = _kit.MyMaxWolves;
		unit.MyProfession.MyReinforcement = SpawnToken({.MyDefinition = std::move(body), .MyPosition = *tile, .MyOwnerUnit = _unit});
	}

	void Battle::VigilSkill(UnitId _unit, const VigilKit& _kit, const ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_kit.MySkill == VigilSkillKind::DIGNITY)
		{
			if (_event.MyKind == ContentEventKind::SKILL_START) { unit.MyVigilAccumulator = 0; unit.MyVigilDp = 0; }
			else if (_event.MyKind == ContentEventKind::SKILL_TICK && std::isgreater(_kit.MyDpInterval, 0) && std::isgreater(_kit.MyDp, 0))
			{
				unit.MyVigilAccumulator += _event.MyDelta;
				while (std::isgreaterequal(unit.MyVigilAccumulator, _kit.MyDpInterval - 1e-9) && std::islessequal(unit.MyVigilDp + _kit.MyDp, _kit.MyDpCap + 1e-9))
				{
					unit.MyVigilAccumulator -= _kit.MyDpInterval; unit.MyVigilDp += _kit.MyDp;
					if (unit.MyOwner != NoPlayer) (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyDp);
				}
			}
			else if (_event.MyKind == ContentEventKind::SKILL_ENDING && _event.MySkillReason == SkillReason::DURATION && std::isgreater(_kit.MyDpCap - unit.MyVigilDp, 1e-9))
			{
				if (unit.MyOwner != NoPlayer) (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyDpCap - unit.MyVigilDp);
				unit.MyVigilDp = _kit.MyDpCap;
			}
			return;
		}
		if (_event.MyKind != ContentEventKind::SKILL_START) return;
		if (unit.MyOwner != NoPlayer && std::isgreater(_kit.MyDp, 0)) (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyDp);
		const auto pack = unit.MyProfession.MyReinforcement;
		if (!WolfOnField(pack)) return;
		auto& wolf = _MyUnits[Index(pack)];
		if (_kit.MySkill == VigilSkillKind::CALL)
		{
			if (!wolf.MyAlive) (void)ReturnWolf(pack);
			else if (!AddWolfShadow(pack, _kit.MyMaxWolves)) wolf.MyHealth = wolf.MyStats.MyMaxHealth;
		}
		else
		{
			if (wolf.MyAlive && std::isgreater(_kit.MyGiftHeal, 0)) (void)Heal(pack, pack, wolf.MyStats.MyMaxHealth * _kit.MyGiftHeal, {.MySelf = true});
			wolf.MyWolfGift = WolfGift{.MyScale = _kit.MyGiftScale, .MyDp = _kit.MyGiftDp};
		}
	}

	void Battle::VigilTick(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		const auto& kit = std::get<VigilKit>(*unit.MyDefinition.MyOperatorKit);
		const auto pack = unit.MyProfession.MyReinforcement;
		if (unit.MyOperatorHooksReleased || kit.MySkill == VigilSkillKind::DIGNITY || !CanAutoSkill(unit) || unit.MySkill.MyActive || !WolfOnField(pack) ||
			(kit.MySkill == VigilSkillKind::GIFT && Unit(pack).MyWolfGift)) return;
		(void)ActivateSkill(_unit, false, SkillReason::TRIGGER);
	}

	void Battle::WolfBeforeAttack(CombatUnit& _unit)
	{
		if (!_unit.MyWolfGift || !_unit.MyOwnerUnit) return;
		const auto& owner = Unit(_unit.MyOwnerUnit); const auto* kit = VigilDefinition(owner);
		if (!kit || kit->MySkill != VigilSkillKind::GIFT || owner.MyOperatorHooksReleased || owner.MyProfession.MyReinforcement != _unit.MyId) return;
		_unit.MyWolfGiftActive = std::exchange(_unit.MyWolfGift, std::nullopt);
	}

	void Battle::NotifyWolfCombat(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && _event.MySource)
		{
			const auto* kit = WolfDefinition(Unit(_event.MyTarget));
			if (kit && Unit(_event.MySource).MyBlockedBy == _event.MyTarget) _event.MyDamage.MyMultiplier *= kit->MyBlockedReduction;
		}
		if (!_event.MySource) return;
		auto& source = _MyUnits[Index(_event.MySource)];
		const auto pack = WolfDefinition(source) ? source.MyId : source.MyProfession.MyReinforcement;
		if (!pack || !WolfDefinition(Unit(pack))) return;
		auto& wolf = _MyUnits[Index(pack)]; const auto& wolfKit = *WolfDefinition(wolf);
		const auto owner = wolf.MyOwnerUnit;
		const auto* vigil = owner ? VigilDefinition(Unit(owner)) : nullptr;
		const bool managed = owner && (Unit(owner).MyDefinition.MyOperatorKit || Unit(owner).MyDefinition.MyContent.MyTag == ContentTag::CUSTOM_OPERATOR);
		const bool hooks = vigil && !Unit(owner).MyOperatorHooksReleased;
		if (_event.MyKind == ContentEventKind::ATTACK)
		{
			if (hooks && vigil->MySkill == VigilSkillKind::GIFT && source.MyId == pack) wolf.MyWolfGiftActive.reset();
			return;
		}
		if (_event.MyKind == ContentEventKind::BEFORE_KILL && hooks && vigil->MySkill == VigilSkillKind::GIFT && source.MyId == pack &&
			wolf.MyWolfGiftActive && !wolf.MyWolfGiftActive->MyPaid && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
		{
			wolf.MyWolfGiftActive->MyPaid = true;
			if (Unit(owner).MyOwner != NoPlayer && std::isgreater(wolf.MyWolfGiftActive->MyDp, 0))
				(void)AddDp(_MyPlayers[Unit(owner).MyOwner].MyPlayerId, wolf.MyWolfGiftActive->MyDp);
		}
		if (!wolf.MyAlive || !_event.MyDamage.MyIsAttack || !_event.MyTarget) return;
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE)
		{
			if (hooks && vigil->MySkill == VigilSkillKind::GIFT && source.MyId == pack && wolf.MyWolfGiftActive) _event.MyDamage.MyMultiplier *= wolf.MyWolfGiftActive->MyScale;
			if (Unit(_event.MyTarget).MyBlockedBy == pack)
				_event.MyDamage.MyDefenseIgnoreFlat += hooks ? vigil->MyDefenseIgnore : managed ? 0 : wolfKit.MyDefenseIgnore;
		}
		if (_event.MyKind != ContentEventKind::DAMAGED || !owner || Unit(_event.MyTarget).MySide != UnitSide::ENEMY || !Unit(_event.MyTarget).MyAlive || Unit(_event.MyTarget).MyBlockedBy != pack) return;
		const auto bonus = hooks ? vigil->MyAdditionScale : managed ? 0 : wolfKit.MyAdditionScale;
		const auto& leader = Unit(owner);
		if (std::isgreater(bonus, 0) && leader.MySkill.MyActive && (hooks || (leader.MyAlive && IsTimedSkill(leader.MyDefinition.MySkill.MyKind))))
			(void)DealDamage(source.MyId, _event.MyTarget, {.MyAmount = leader.MyStats.MyAttack * bonus, .MyType = DamageType::ARTS,
				.MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
	}

	void Battle::WolfBlocked(UnitId _wolf, UnitId _enemy)
	{
		const auto& wolf = Unit(_wolf); const auto* kit = WolfDefinition(wolf);
		if (!kit || !wolf.MyOwnerUnit || !std::islessgreater(kit->MyTaunt, 0) || !wolf.MyAlive || !Unit(_enemy).MyAlive) return;
		auto& owner = _MyUnits[Index(wolf.MyOwnerUnit)];
		if (!VigilDefinition(owner) || owner.MyOperatorHooksReleased || !owner.MyAlive || owner.MyProfession.MyReinforcement != _wolf) return;
		(void)AddBuff(_enemy, {.MyKey = "token:pack_mark", .MySource = _wolf, .MyDuration = 0.25, .MyRefresh = BuffRefresh::EXTEND,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::TAUNT, .MyValue = kit->MyTaunt}}});
		if (!std::ranges::contains(owner.MyWolfMarked, _enemy)) owner.MyWolfMarked.push_back(_enemy);
	}

	void Battle::VigilMark(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (unit.MyOperatorHooksReleased) return;
		const auto pack = unit.MyProfession.MyReinforcement;
		const auto* kit = pack ? WolfDefinition(Unit(pack)) : nullptr;
		auto& scratch = AcquireAttackScratch();
		struct Guard
		{
			std::size_t& MyDepth;

			~Guard() { --MyDepth; }
		};
		const Guard guard{.MyDepth = _MyAttackDepth};
		if (kit && unit.MyAlive && Unit(pack).MyAlive && std::islessgreater(kit->MyTaunt, 0))
			for (const auto id : Unit(pack).MyBlocking)
				if (Unit(id).MyAlive && Unit(id).MySide == UnitSide::ENEMY && Unit(id).MyBlockedBy == pack) scratch.MyTargets.push_back(id);
		for (const auto id : scratch.MyTargets) WolfBlocked(pack, id);
		scratch.MySeen.assign(unit.MyWolfMarked.begin(), unit.MyWolfMarked.end());
		for (const auto id : scratch.MySeen) if (!std::ranges::contains(scratch.MyTargets, id)) (void)RemoveBuff(id, "token:pack_mark");
		unit.MyWolfMarked.assign(scratch.MyTargets.begin(), scratch.MyTargets.end());
	}
}
