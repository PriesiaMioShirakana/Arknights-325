#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		constexpr std::array RhineMembers{"char_108_silent", "char_128_plosis", "char_202_demkni", "char_249_mlyss", "char_1047_halo2"};
		constexpr std::array ManifoldNeighbours{RangeOffset{.MyRow = 1}, RangeOffset{.MyRow = -1}, RangeOffset{.MyColumn = 1}, RangeOffset{.MyColumn = -1}};
		constexpr std::array ManifoldAround{RangeOffset{.MyRow = 1, .MyColumn = -1}, RangeOffset{.MyRow = 1}, RangeOffset{.MyRow = 1, .MyColumn = 1},
			RangeOffset{.MyColumn = -1}, RangeOffset{.MyColumn = 1}, RangeOffset{.MyRow = -1, .MyColumn = -1}, RangeOffset{.MyRow = -1}, RangeOffset{.MyRow = -1, .MyColumn = 1}};

		struct ManifoldScratchGuard
		{
			std::size_t& MyDepth;

			~ManifoldScratchGuard() { --MyDepth; }
		};

		const MlyssKit* MlyssDefinition(const CombatUnit& _unit)
		{
			return _unit.MyDefinition.MyOperatorKit ? std::get_if<MlyssKit>(_unit.MyDefinition.MyOperatorKit) : nullptr;
		}

		bool ManagedManifold(const CombatUnit& _owner)
		{
			return _owner.MyDefinition.MyOperatorKit || _owner.MyDefinition.MyContent.MyTag == ContentTag::CUSTOM_OPERATOR;
		}

		bool RhineOperator(const CombatUnit& _unit)
		{
			return _unit.MyKind == UnitKind::OPERATOR && std::ranges::contains(RhineMembers, _unit.MyDefinition.MyIdentity.MyCharacterId);
		}

		void CopyManifoldStats(CombatStats& _target, const CombatStats& _source, double _scale)
		{
			_target.MyMaxHealth = std::max(1.0, _source.MyMaxHealth * _scale);
			_target.MyAttack = _source.MyAttack * _scale;
			_target.MyDefense = _source.MyDefense * _scale;
			_target.MyResistance = _source.MyResistance * _scale;
			_target.MyBlockCount = _source.MyBlockCount;
			_target.MyBaseAttackTime = _source.MyBaseAttackTime;
			_target.MyAttackSpeed = _source.MyAttackSpeed;
		}
	}

	void BattleCore::InstallMlyss(CombatUnit& _unit)
	{
		_MyMlysses.push_back(_unit.MyId);
		if (!std::ranges::contains(_MyRhineOwners, _unit.MyOwner)) _MyRhineOwners.push_back(_unit.MyOwner);
		Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::MLYSS_PULSE, .MySource = _unit.MyId, .MyInterval = 0.25});
	}

	void BattleCore::InstallManifold(CombatUnit& _unit)
	{
		_unit.MyManifold.emplace(ManifoldState{.MyDefaultStats = _unit.MyDefinition.MyStats, .MyDefaultAttack = _unit.MyDefinition.MyAttack,
			.MyDefaultRange = _unit.MyDefinition.MyRange, .MyBaseHits = _unit.MyDefinition.MyAttack.MyHits});
		_MyManifolds.push_back(_unit.MyId);
	}

	UnitId BattleCore::StandingManifold(UnitId _owner) const
	{
		for (const auto id : _MyManifolds)
		{
			const auto& token = Unit(id);
			if (token.MyOwnerUnit == _owner && token.MyAlive && !token.MyTokenCloneSource) return id;
		}
		return 0;
	}

	void BattleCore::MlyssDeploy(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (const auto standing = StandingManifold(_unit)) { unit.MyProfession.MyReinforcement = standing; return; }
		const auto& kit = *MlyssDefinition(unit);
		UnitId previous = 0;
		for (const auto id : _MyManifolds)
		{
			const auto& token = Unit(id);
			if (token.MyOwnerUnit != _unit || token.MyTokenCloneSource) continue;
			if (!token.MyAlive && !token.MyRemoved && Redeploy(id, true)) { unit.MyProfession.MyReinforcement = id; return; }
			if (!previous || (token.MyPieceUid && !Unit(previous).MyPieceUid) ||
				(static_cast<bool>(token.MyPieceUid) == static_cast<bool>(Unit(previous).MyPieceUid) && token.MyDeploySequence > Unit(previous).MyDeploySequence)) previous = id;
		}
		std::optional<WorldPoint> tile;
		if (previous)
		{
			const auto point = Unit(previous).MyHome;
			const auto r = static_cast<int>(point.MyY), c = static_cast<int>(point.MyX);
			if (FieldGrid::InBounds(r, c) && unit.MyBaseTriggerMask.test(static_cast<std::size_t>(FieldGrid::Key(r, c))) && !ReservedTile(point) &&
				(!_MyGrid || (_MyGrid->InRect(r, c) && _MyGrid->CanStand(r, c)))) tile = point;
		}
		if (!tile) tile = FindTacticalPoint(_unit);
		const auto* body = FindTokenTemplate(_unit, kit.MyManifold);
		if (tile && body) unit.MyProfession.MyReinforcement = SpawnToken({.MyDefinition = *body, .MyPosition = *tile, .MyOwnerUnit = _unit});
	}

	void BattleCore::BuffManifold(UnitId _unit, UnitId _owner)
	{
		const auto& owner = Unit(_owner); const auto* kit = MlyssDefinition(owner);
		if (!kit || !owner.MySkill.MyActive || !IsTimedSkill(owner.MyDefinition.MySkill.MyKind)) return;
		(void)AddBuff(_unit, {.MyKey = "mlyss:s3", .MyDuration = owner.MySkill.MyTimeLeft,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit->MyAttack},
				{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = kit->MyAttackSpeed}}});
	}

	void BattleCore::ManifoldDeploy(UnitId _unit)
	{
		auto& token = _MyUnits[Index(_unit)]; auto& state = *token.MyManifold;
		const auto& rule = *token.MyDefinition.MyTokenKit->MyManifold;
		const auto ownerId = token.MyOwnerUnit;
		const auto* managedKit = ownerId ? MlyssDefinition(Unit(ownerId)) : nullptr;
		if (token.MyTokenCloneSource)
		{
			CopyManifold(_unit, token.MyTokenCloneSource, 1, managedKit != nullptr, true);
			return;
		}
		state.MyRespawnHome = token.MyPosition;
		if (state.MyTriggerRange && ownerId) (void)RemoveSkillTriggerRange(ownerId, std::exchange(state.MyTriggerRange, 0));
		if (state.MyFrom)
		{
			CopyManifoldStats(token.MyDefinition.MyStats, state.MyDefaultStats, 1);
			token.MyDefinition.MyAttack = state.MyDefaultAttack;
			token.MyDefinition.MyRange.assign(state.MyDefaultRange.begin(), state.MyDefaultRange.end());
			(void)RemoveBuff(_unit, "mlyss:steal"); (void)RemoveBuff(_unit, "mlyss:stolen");
			Recalculate(token); token.MyHealth = token.MyStats.MyMaxHealth; RefreshRange(token);
		}
		state.MyFrom = 0; state.MyRanged = false; state.MyStolenAttack = state.MyStolenDefense = 0; state.MyAttacks = 0;
		if (!ownerId)
		{
			if (!state.MyFirstSpDone && std::isgreater(rule.MyFirstSp, 0))
			{
				state.MyFirstSpDone = true; (void)GainSp(_unit, rule.MyFirstSp, SpReason::TALENT);
			}
			return;
		}
		auto& owner = _MyUnits[Index(ownerId)];
		if (owner.MyDefinition.MyProfession.MyKind == ProfessionTrait::TACTICIAN) owner.MyProfession.MyReinforcement = _unit;
		if (managedKit && !owner.MyOperatorHooksReleased)
		{
			owner.MyMlyssPendingToken = 0; ++owner.MyMlyssRespawnVersion;
			if (!owner.MyMlyssFirst && std::isgreater(rule.MyFirstSp, 0)) (void)GainSp(_unit, rule.MyFirstSp, SpReason::TALENT);
			owner.MyMlyssFirst = true;
			BuffManifold(_unit, ownerId);
		}
		else if (!ManagedManifold(owner) && !state.MyFirstSpDone && std::isgreater(rule.MyFirstSp, 0))
		{
			state.MyFirstSpDone = true; (void)GainSp(_unit, rule.MyFirstSp, SpReason::TALENT);
		}
	}

	UnitId BattleCore::ManifoldCopyTarget(UnitId _unit) const
	{
		const auto& token = Unit(_unit); const auto point = RulePosition(token);
		UnitId best = 0; double nearest = std::numeric_limits<double>::infinity();
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (ally.MyKind != UnitKind::OPERATOR || !ally.MyAlive || ally.MyHidden || ally.MyRemoved || id == token.MyOwnerUnit || ally.MyOwner != token.MyOwner) continue;
			const auto pos = RulePosition(ally);
			const auto distance = std::max(std::abs(std::floor(pos.MyY + 0.5) - std::floor(point.MyY + 0.5)), std::abs(std::floor(pos.MyX + 0.5) - std::floor(point.MyX + 0.5)));
			if (!best || std::isless(distance, nearest) || (!std::islessgreater(distance, nearest) &&
				(std::isgreater(ally.MyDefinition.MyStats.MyAttack, Unit(best).MyDefinition.MyStats.MyAttack) ||
					(!std::islessgreater(ally.MyDefinition.MyStats.MyAttack, Unit(best).MyDefinition.MyStats.MyAttack) && id < best))))
			{ best = id; nearest = distance; }
		}
		return best;
	}

	void BattleCore::CopyManifold(UnitId _unit, UnitId _target, double _scale, bool _managed, bool _clone)
	{
		auto& token = _MyUnits[Index(_unit)]; auto& state = *token.MyManifold; const auto& source = Unit(_target);
		const auto& original = source.MyDefinition.MyAttack; auto& profile = token.MyDefinition.MyAttack;
		CopyManifoldStats(token.MyDefinition.MyStats, source.MyDefinition.MyStats, _scale);
		if (!source.MyDefinition.MyRange.empty()) token.MyDefinition.MyRange.assign(source.MyDefinition.MyRange.begin(), source.MyDefinition.MyRange.end());
		const auto ranged = _managed ? (_clone || !source.MyDefinition.MyIdentity.MyMeleePosition) : (!source.MyDefinition.MyIdentity.MyMeleePosition || original.MyRanged);
		profile.MyRanged = ranged;
		profile.MyProjectileSpeed = ranged ? (std::isgreater(original.MyProjectileSpeed, 0) && !original.MyHealing ? original.MyProjectileSpeed : 10) : 0;
		profile.MyCanHitFlying = ranged || original.MyCanHitFlying;
		if (!original.MyHealing && !original.MyDisabled && !original.MyOnlyDuringSkill) profile.MyDamageType = original.MyDamageType;
		profile.MyHealing = false; profile.MyDisabled = false; profile.MyOnlyDuringSkill = false;
		(void)RemoveBuff(_unit, _managed ? "mlyss:stolen" : "mlyss:steal");
		state.MyFrom = !_managed && _clone && source.MyManifold ? source.MyManifold->MyFrom : _target;
		state.MyRanged = ranged; state.MyStolenAttack = state.MyStolenDefense = 0; state.MyAttacks = 0;
		Recalculate(token); token.MyHealth = token.MyStats.MyMaxHealth; RefreshRange(token);
		if (_managed && token.MyOwnerUnit)
		{
			if (!state.MyTriggerRange) state.MyTriggerRange = AddSkillTriggerRange(token.MyOwnerUnit, {.MySourceUnit = _unit});
			const auto* kit = MlyssDefinition(Unit(token.MyOwnerUnit));
			if (!_clone && kit && std::isgreater(kit->MyRhineSp, 0) && RhineOperator(source)) (void)GainSp(token.MyOwnerUnit, kit->MyRhineSp, SpReason::TALENT);
		}
	}

	void BattleCore::ManifoldTargets(CombatUnit& _unit, std::vector<UnitId>& _targets)
	{
		if (!_unit.MyManifold || !_unit.MyManifold->MyFrom || !_unit.MyManifold->MyRanged || !_unit.MyOwnerUnit) return;
		const auto& owner = Unit(_unit.MyOwnerUnit); const auto* kit = MlyssDefinition(owner);
		if (!kit || owner.MyOperatorHooksReleased || !owner.MySkill.MyActive || kit->MySkill != MlyssSkillKind::ECOLOGY) return;
		auto& scratch = AcquireAttackScratch(); const ManifoldScratchGuard guard{.MyDepth = _MyAttackDepth};
		GenericEnemies(_unit.MyId, scratch.MyTargets, 0, false, &EffectiveAttack(_unit));
		if (!scratch.MyTargets.empty()) _targets.assign(1, scratch.MyTargets[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MyTargets.size()))]);
	}

	void BattleCore::StealManifold(UnitId _unit, std::span<const UnitId> _targets, bool _managed)
	{
		auto& token = _MyUnits[Index(_unit)]; auto& state = *token.MyManifold; const auto& rule = *token.MyDefinition.MyTokenKit->MyManifold;
		bool changed = false;
		for (const auto id : _targets)
		{
			auto& enemy = _MyUnits[Index(id)]; if (!enemy.MyAlive || enemy.MySide != UnitSide::ENEMY) continue;
			const auto attack = std::max(0.0, std::min(rule.MyStealAttack, rule.MyAttackCap - state.MyStolenAttack));
			const auto defense = std::max(0.0, std::min(rule.MyStealDefense, rule.MyDefenseCap - state.MyStolenDefense));
			if (!std::isgreater(attack, 0) && !std::isgreater(defense, 0)) break;
			state.MyStolenAttack += attack; state.MyStolenDefense += defense;
			auto& robbed = _managed ? enemy.MyMlyssRobbed : enemy.MyManifoldRobbed;
			robbed.MyAttack += attack; robbed.MyDefense += defense;
			(void)AddBuff(id, {.MyKey = _managed ? "mlyss:robbed" : "mlyss:stolen", .MySource = _unit,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_FLAT, .MyValue = -robbed.MyAttack}, {.MyAttribute = Attribute::DEFENSE_FLAT, .MyValue = -robbed.MyDefense}}});
			changed = true;
		}
		if (changed) (void)AddBuff(_unit, {.MyKey = _managed ? "mlyss:stolen" : "mlyss:steal", .MyModifiers = std::vector<AttributeChange>{
			{.MyAttribute = Attribute::ATTACK_FLAT, .MyValue = state.MyStolenAttack}, {.MyAttribute = Attribute::DEFENSE_FLAT, .MyValue = state.MyStolenDefense}}});
	}

	void BattleCore::SplitManifold(UnitId _unit, bool _managed)
	{
		const auto& token = Unit(_unit); const auto owner = token.MyOwnerUnit;
		if (!owner) return;
		auto& scratch = AcquireAttackScratch(); const ManifoldScratchGuard guard{.MyDepth = _MyAttackDepth};
		if (_managed)
		{
			const auto point = RulePosition(token);
			const auto row = static_cast<int>(std::floor(point.MyY + 0.5)), column = static_cast<int>(std::floor(point.MyX + 0.5));
			for (const auto offset : ManifoldNeighbours)
			{
				const auto r = row + offset.MyRow, c = column + offset.MyColumn;
				if (FieldGrid::InBounds(r, c) && !ReservedTile({.MyX = static_cast<double>(c), .MyY = static_cast<double>(r)}) &&
					(!_MyGrid || (_MyGrid->InRect(r, c) && _MyGrid->CanStand(r, c)))) scratch.MyBuffIds.push_back(static_cast<std::uint64_t>(FieldGrid::Key(r, c)));
			}
		}
		else FreeSummonTiles(_unit, ManifoldNeighbours, scratch.MyBuffIds, false);
		const auto tile = BestSummonTile(scratch.MyBuffIds); const auto* body = FindTokenTemplate(owner, token.MyDefinition.MyId);
		if (!tile || !body) return;
		auto definition = *body;
		definition.MySkill = {}; definition.MyGenericSkill = nullptr;
		(void)SpawnToken({.MyDefinition = std::move(definition), .MyPosition = *tile, .MyOwnerUnit = owner,
			.MyDuration = token.MyDefinition.MyTokenKit->MyManifold->MySplitLife, .MyCloneSource = _unit});
	}

	void BattleCore::MlyssPulse(UnitId _unit, bool _adaptation)
	{
		const auto& unit = Unit(_unit); const auto& kit = *MlyssDefinition(unit);
		if (unit.MyOperatorHooksReleased || (_adaptation && !unit.MySkill.MyActive)) return;
		auto& scratch = AcquireAttackScratch(); const ManifoldScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyManifolds) if (Unit(id).MyOwnerUnit == _unit) scratch.MySeen.push_back(id);
		for (const auto id : scratch.MySeen)
		{
			auto& token = _MyUnits[Index(id)]; auto& state = *token.MyManifold; const auto& rule = *token.MyDefinition.MyTokenKit->MyManifold;
			const bool live = token.MyAlive && !token.MyHidden;
			if (_adaptation)
			{
				if (!live || !state.MyFrom || state.MyRanged) continue;
				const auto activation = unit.MySkill.MyActivations;
				scratch.MyTargets.clear();
				for (const auto enemy : _MyEnemyIds) if (Unit(enemy).MyAlive && !Unit(enemy).MyHidden && OperatorInGrid(id, enemy, ManifoldAround)) scratch.MyTargets.push_back(enemy);
				for (const auto enemy : scratch.MyTargets)
				{
					if (!unit.MyAlive || !unit.MySkill.MyActive || unit.MySkill.MyActivations != activation) return;
					(void)Pull(enemy, rule.MyForce, {.MyTo = RulePosition(token), .MyCenter = RulePosition(token), .MyCenterUnit = id});
				}
				scratch.MyTargets.assign(token.MyBlocking.begin(), token.MyBlocking.end());
				for (const auto enemy : scratch.MyTargets) if (Unit(enemy).MyAlive) (void)ApplyStatus(enemy, CombatStatus::STUN, kit.MyPulseInterval + 0.1, _unit);
				continue;
			}
			if (kit.MySkill == MlyssSkillKind::ECOLOGY)
			{
				const bool on = unit.MyAlive && !unit.MyHidden && unit.MySkill.MyActive;
				token.MyDefinition.MyAttack.MyHits = state.MyBaseHits * (on && state.MyRanged && live ? 2 : 1);
				if (on && state.MyFrom && !state.MyRanged && live)
				{
					(void)AddBuff(id, {.MyKey = "mlyss:eco", .MyDuration = 0.4, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALTH_REGEN_RATIO, .MyValue = kit.MyRegen}}});
					if (std::isgreater(kit.MyProtection, 0)) (void)ApplyStrongest(id, "protect", 0.4,
						{.MyValue = kit.MyProtection, .MyAttribute = Attribute::PHYSICAL_TAKEN_MULTIPLIER, .MyScale = -1, .MyOffset = 1, .MySecondAttribute = Attribute::ARTS_TAKEN_MULTIPLIER}, _unit);
				}
			}
			if (live && std::isgreater(rule.MyBlockedTaunt, 0))
			{
				scratch.MyTargets.assign(token.MyBlocking.begin(), token.MyBlocking.end());
				for (const auto enemy : scratch.MyTargets) if (Unit(enemy).MyAlive) (void)AddBuff(enemy, {.MyKey = "mlyss:exposed", .MySource = id, .MyDuration = 0.4,
					.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::TAUNT, .MyValue = rule.MyBlockedTaunt}}});
			}
		}
	}

	void BattleCore::MlyssSkill(UnitId _unit, const MlyssKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			unit.MyMlyssDpAccumulator = 0; unit.MyMlyssDpCount = 0;
			if (_kit.MySkill != MlyssSkillKind::LUBRICATION) (void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyDp);
			auto& scratch = AcquireAttackScratch(); const ManifoldScratchGuard guard{.MyDepth = _MyAttackDepth};
			for (const auto id : _MyManifolds) if (Unit(id).MyOwnerUnit == _unit && Unit(id).MyAlive && !Unit(id).MyHidden) scratch.MySeen.push_back(id);
			bool ranged = false;
			for (const auto id : scratch.MySeen) { BuffManifold(id, _unit); ranged |= Unit(id).MyManifold->MyFrom && Unit(id).MyManifold->MyRanged; }
			if (_kit.MySkill == MlyssSkillKind::ADAPTATION)
			{
				if (ranged)
				{
					for (const auto id : scratch.MySeen) _MyUnits[Index(id)].MyHealth = Unit(id).MyStats.MyMaxHealth;
					if (unit.MyMlyssPendingToken) ManifoldRespawn(_unit, unit.MyMlyssPendingToken, 0, ++unit.MyMlyssRespawnVersion);
				}
				MlyssPulse(_unit, true);
				if (unit.MySkill.MyActive) Schedule({.MyAt = Time() + _kit.MyPulseInterval, .MyKind = ScheduledKind::MLYSS_ADAPTATION,
					.MySource = _unit, .MyInterval = _kit.MyPulseInterval, .MyVersion = unit.MySkill.MyActivations});
			}
		}
		else if (_kit.MySkill == MlyssSkillKind::LUBRICATION)
		{
			if (_event.MyKind == ContentEventKind::SKILL_TICK)
			{
				unit.MyMlyssDpAccumulator += _event.MyDelta;
				while (std::isgreaterequal(unit.MyMlyssDpAccumulator + 1e-9, _kit.MyDpInterval) && unit.MyMlyssDpCount < _kit.MyDpCount)
				{
					unit.MyMlyssDpAccumulator -= _kit.MyDpInterval; ++unit.MyMlyssDpCount;
					(void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, _kit.MyDp);
				}
			}
			else if (_event.MyKind == ContentEventKind::SKILL_ENDING && _event.MySkillReason == SkillReason::DURATION && unit.MyMlyssDpCount < _kit.MyDpCount)
				(void)AddDp(_MyPlayers[unit.MyOwner].MyPlayerId, (_kit.MyDpCount - unit.MyMlyssDpCount) * _kit.MyDp);
		}
	}

	void BattleCore::MlyssObserve(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::BATTLE_START)
		{
			auto& scratch = AcquireAttackScratch(); const ManifoldScratchGuard guard{.MyDepth = _MyAttackDepth};
			for (const auto owner : _MyRhineOwners)
			{
				const auto source = std::ranges::find_if(_MyMlysses, [&](UnitId _id) { return Unit(_id).MyOwner == owner; });
				if (source == _MyMlysses.end()) continue;
				const auto& kit = *MlyssDefinition(Unit(*source)); scratch.MyTargets.clear();
				for (const auto id : _MyAllyIds) if (Unit(id).MyOwner == owner && RhineOperator(Unit(id))) scratch.MyTargets.push_back(id);
				std::ranges::stable_sort(scratch.MyTargets, {}, [&](UnitId _id) { return Unit(_id).MyDeploySequence; });
				for (std::size_t i = 0; i < scratch.MyTargets.size(); ++i)
				{
					auto& ally = _MyUnits[Index(scratch.MyTargets[i])];
					ally.MyDefinition.MyStats.MyDeploymentCost = std::max(0.0, ally.MyDefinition.MyStats.MyDeploymentCost + kit.MyCostCut + (i == 0 ? kit.MyFirstCostCut : 0));
					Recalculate(ally);
				}
			}
			return;
		}
		if (_event.MyKind == ContentEventKind::DEATH && _event.MyUnit && MlyssDefinition(Unit(_event.MyUnit)))
		{
			auto& owner = _MyUnits[Index(_event.MyUnit)]; owner.MyMlyssPendingToken = 0; ++owner.MyMlyssRespawnVersion;
		}
		if (_event.MyKind != ContentEventKind::DEPLOY || _event.MyInitial || !_event.MyUnit || !RhineOperator(Unit(_event.MyUnit))) return;
		for (std::size_t i = 0, count = _MyMlysses.size(); i < count; ++i)
		{
			const auto& owner = Unit(_MyMlysses[i]); const auto& kit = *MlyssDefinition(owner);
			if (owner.MyOperatorHooksReleased || owner.MyOwner != Unit(_event.MyUnit).MyOwner || !std::isgreater(kit.MyRhineOtherSp, 0)) continue;
			if (std::ranges::any_of(_MyManifolds, [&](UnitId _id) { const auto& token = Unit(_id); return token.MyOwnerUnit == owner.MyId && token.MyAlive && !token.MyHidden && token.MyManifold->MyFrom == _event.MyUnit; }))
				(void)GainSp(_event.MyUnit, kit.MyRhineOtherSp, SpReason::TALENT);
		}
	}

	void BattleCore::ManifoldRespawn(UnitId _owner, UnitId _token, unsigned _attempt, std::uint64_t _version)
	{
		if (Finished()) return;
		auto& token = _MyUnits[Index(_token)];
		if (_owner && MlyssDefinition(Unit(_owner)))
		{
			auto& owner = _MyUnits[Index(_owner)];
			if (owner.MyMlyssRespawnVersion != _version || owner.MyMlyssPendingToken != _token) return;
			owner.MyMlyssPendingToken = 0;
			if (!owner.MyAlive || owner.MyHidden || owner.MyOperatorHooksReleased || StandingManifold(_owner) || _attempt > 120) return;
			const auto* body = FindTokenTemplate(_owner, token.MyDefinition.MyId);
			const auto home = token.MyManifold->MyRespawnHome;
			if (!ReservedTile(home) && body && SpawnToken({.MyDefinition = *body, .MyPosition = home, .MyOwnerUnit = _owner})) return;
			owner.MyMlyssPendingToken = _token;
			Schedule({.MyAt = Time() + 1, .MyKind = ScheduledKind::MANIFOLD_RESPAWN, .MySource = _owner, .MyTarget = _token, .MyVersion = _version, .MyRemaining = _attempt + 1});
			return;
		}
		if (token.MyAlive || token.MyRemoved || token.MyDeploySequence != _version || (_owner && !Unit(_owner).MyAlive)) return;
		if (Redeploy(_token, false)) return;
		Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::MANIFOLD_RESPAWN, .MySource = _owner, .MyTarget = _token, .MyVersion = _version});
	}

	void BattleCore::NotifyManifolds(ContentEvent& _event, bool _late)
	{
		if (!_late && _event.MyKind == ContentEventKind::TICK)
		{
			for (std::size_t i = 0, count = _MyManifolds.size(); i < count; ++i)
			{
				auto& token = _MyUnits[Index(_MyManifolds[i])];
				if (!token.MyTokenCloneSource && CanAutoSkill(token) && ManifoldCopyTarget(token.MyId)) (void)ActivateSkill(token.MyId, false, SkillReason::TRIGGER);
			}
			return;
		}
		if (!_late && _event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && _event.MySource)
		{
			const auto& target = Unit(_event.MyTarget); const auto& attacker = Unit(_event.MySource);
			if (target.MyManifold && attacker.MyBlockedBy == target.MyId) _event.MyDamage.MyMultiplier *= target.MyDefinition.MyTokenKit->MyManifold->MyBlockedScale;
			else if (attacker.MySide == UnitSide::ENEMY && attacker.MyBlockedBy == target.MyId)
				for (const auto id : _MyMlysses)
				{
					const auto& owner = Unit(id); const auto& kit = *MlyssDefinition(owner);
					if (!owner.MyOperatorHooksReleased && owner.MyProfession.MyReinforcement == target.MyId && std::isgreater(kit.MyReinforcementCut, 0)) _event.MyDamage.MyMultiplier *= 1 - kit.MyReinforcementCut;
				}
		}
		if (!_late && _event.MyKind == ContentEventKind::DEATH && _event.MyUnit && Unit(_event.MyUnit).MyManifold)
		{
			auto& token = _MyUnits[Index(_event.MyUnit)]; const auto ownerId = token.MyOwnerUnit;
			if (token.MyTokenCloneSource || _event.MyRemovalReason != RemovalReason::KILLED || Finished()) return;
			const auto& rule = *token.MyDefinition.MyTokenKit->MyManifold;
			if (ownerId && MlyssDefinition(Unit(ownerId)) && !Unit(ownerId).MyOperatorHooksReleased)
			{
				auto& owner = _MyUnits[Index(ownerId)];
				if (StandingManifold(ownerId)) return;
				owner.MyMlyssPendingToken = token.MyId; ++owner.MyMlyssRespawnVersion;
				Schedule({.MyAt = Time() + rule.MyRespawn, .MyKind = ScheduledKind::MANIFOLD_RESPAWN, .MySource = ownerId, .MyTarget = token.MyId, .MyVersion = owner.MyMlyssRespawnVersion});
			}
			else if ((!ownerId || !ManagedManifold(Unit(ownerId))) && std::isgreater(rule.MyRespawn, 0))
			{
				token.MyRemoved = false;
				Schedule({.MyAt = Time() + rule.MyRespawn, .MyKind = ScheduledKind::MANIFOLD_RESPAWN, .MySource = ownerId, .MyTarget = token.MyId, .MyVersion = token.MyDeploySequence});
			}
			return;
		}
		const auto sourceId = _event.MySource ? _event.MySource : _event.MyUnit;
		if (!sourceId || !Unit(sourceId).MyManifold) return;
		auto& token = _MyUnits[Index(sourceId)]; const auto ownerId = token.MyOwnerUnit;
		const auto* ownerKit = ownerId ? MlyssDefinition(Unit(ownerId)) : nullptr;
		const bool hooks = ownerKit && !Unit(ownerId).MyOperatorHooksReleased;
		const bool managed = ownerId && ManagedManifold(Unit(ownerId));
		const auto& rule = *token.MyDefinition.MyTokenKit->MyManifold;
		if (!_late && _event.MyKind == ContentEventKind::SKILL_START && !token.MyTokenCloneSource && (!managed || hooks))
		{
			if (const auto target = ManifoldCopyTarget(sourceId)) CopyManifold(sourceId, target, rule.MyScale, hooks);
			else if (hooks) { EndSkill(sourceId, SkillReason::NO_TARGET); (void)GainSp(sourceId, SpCost(sourceId)); }
		}
		if (!token.MyAlive || !token.MyManifold->MyFrom) return;
		if (_late && _event.MyKind == ContentEventKind::ATTACK && (!managed || hooks))
		{
			if (!token.MyManifold->MyRanged && hooks) StealManifold(sourceId, _event.MyTargets, true);
			else if (token.MyManifold->MyRanged && !token.MyTokenCloneSource && rule.MySplitEvery && ++token.MyManifold->MyAttacks % rule.MySplitEvery == 0) SplitManifold(sourceId, hooks);
		}
		if (!_late && _event.MyKind == ContentEventKind::DAMAGED && !_event.MyElement && _event.MyTarget && Unit(_event.MyTarget).MyAlive && _event.MyDamage.MyIsAttack)
		{
			if (!managed && !token.MyManifold->MyRanged) { const std::array targets{_event.MyTarget}; StealManifold(sourceId, targets, false); }
			if (hooks && token.MyManifold->MyRanged && Unit(ownerId).MySkill.MyActive && std::isgreater(ownerKit->MyBind, 0)) (void)ApplyStatus(_event.MyTarget, CombatStatus::BIND, ownerKit->MyBind, ownerId);
		}
	}
}
