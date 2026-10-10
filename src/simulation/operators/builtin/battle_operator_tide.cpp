#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct TideScratchGuard
		{
			std::size_t& MyDepth;

			~TideScratchGuard() { --MyDepth; }
		};

		bool IsSeaborn(const CombatUnit& _unit, UnitId _owner, std::string_view _id)
		{
			return _unit.MyKind == UnitKind::TOKEN && _unit.MyOwnerUnit == _owner && _unit.MyDefinition.MyId == _id;
		}
	}

	bool BattleCore::Skadi2Covers(UnitId _source, UnitId _target) const
	{
		if (InRuleRange(_source, _target)) return true;
		const auto& kit = std::get<Skadi2Kit>(*Unit(_source).MyDefinition.MyOperatorKit);
		for (const auto id : _MyAllyIds)
			if (const auto& token = Unit(id); token.MyAlive && !token.MyHidden && IsSeaborn(token, _source, kit.MySeaborn) && InRuleRange(id, _target)) return true;
		return false;
	}

	void BattleCore::Skadi2Covered(UnitId _unit, std::vector<UnitId>& _allies, std::vector<UnitId>& _tokens) const
	{
		const auto& kit = std::get<Skadi2Kit>(*Unit(_unit).MyDefinition.MyOperatorKit);
		_allies.clear(); _tokens.clear();
		for (const auto id : _MyAllyIds)
			if (const auto& token = Unit(id); token.MyAlive && !token.MyHidden && IsSeaborn(token, _unit, kit.MySeaborn)) _tokens.push_back(id);
		// 按来源顺序取并集；同一接受者只获得一次回复，潮涌的各范围伤害则分别结算。
		const auto append = [&](UnitId _source)
		{
			for (const auto id : _MyAllyIds)
			{
				const auto& ally = Unit(id);
				if (ally.MyAlive && !ally.MyHidden && ally.MyKind != UnitKind::DEVICE && (id == _source || !ally.MyStatuses.Has(CombatStatus::ISOLATED)) &&
					InRuleRange(_source, id) && !std::ranges::contains(_tokens, id) && !std::ranges::contains(_allies, id)) _allies.push_back(id);
			}
		};
		append(_unit);
		for (const auto id : _tokens) append(id);
	}

	void BattleCore::Skadi2Pulse(UnitId _unit, bool _predator)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased || (!_predator && unit.MyStatuses.Has(CombatStatus::STUN))) return;
		const auto& kit = std::get<Skadi2Kit>(*unit.MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch();
		const TideScratchGuard guard{.MyDepth = _MyAttackDepth};
		if (!_predator)
		{
			for (const auto& buff : unit.MyBuffs)
				if (buff.MyDefinition.MyKey == "inspire" || buff.MyDefinition.MyKey.starts_with("inspire:") || buff.MyDefinition.MyKey.ends_with(":inspire")) scratch.MyBuffIds.push_back(buff.MyId);
			for (const auto id : scratch.MyBuffIds) (void)RemoveBuff(_unit, id);
		}
		Skadi2Covered(_unit, scratch.MyTargets, scratch.MySeen);
		if (_predator)
		{
			constexpr std::array<std::string_view, 5> Abyssal{"char_143_ghost", "char_263_skadi", "char_474_glady", "char_4145_ulpia", "char_1023_ghost2"};
			unsigned covered = 0, own = 0;
			bool abyssal = false;
			for (const auto id : scratch.MyTargets)
			{
				const auto& ally = Unit(id);
				if (id == _unit || ally.MyKind != UnitKind::OPERATOR) continue;
				++covered;
				abyssal |= std::ranges::contains(Abyssal, ally.MyDefinition.MyIdentity.MyCharacterId);
				if (InRuleRange(_unit, id)) ++own;
			}
			const auto attack = (abyssal ? kit.MyAbyssalAttack : covered ? kit.MyPredatorAttack : 0) + (std::isgreaterequal(static_cast<double>(own), kit.MyModuleCount) ? kit.MyModuleAttack : 0);
			const auto defense = covered ? kit.MyPredatorDefense : 0;
			if (std::isgreater(attack, 0) || std::isgreater(defense, 0)) (void)AddBuff(_unit, {.MyKey = "skadi2:predator", .MyDuration = 0.75,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = attack}, {.MyAttribute = Attribute::DEFENSE_PERCENT, .MyValue = defense}}});
			return;
		}
		++unit.MySkadiPulses;
		const bool active = unit.MySkill.MyActive;
		if (active && (kit.MyDefault || kit.MySkill == Skadi2SkillKind::PRAYER))
		{
			const auto attack = unit.MyStats.MyAttack * kit.MyInspireAttack, defense = unit.MyStats.MyDefense * kit.MyInspireDefense;
			for (const auto id : scratch.MyTargets)
				if (id != _unit)
				{
					InspireAttribute(_unit, id, attack, Attribute::ATTACK_FLAT, 0.75, 0.1);
					if (!kit.MyDefault) InspireAttribute(_unit, id, defense, Attribute::DEFENSE_FLAT, 0.75, 0.1);
				}
		}
		if (active && kit.MyDefault)
		{
			if (unit.MySkadiPulses % 2) return;
			const auto damage = unit.MyStats.MyAttack * kit.MyTideScale;
			scratch.MySeen.insert(scratch.MySeen.begin(), _unit);
			for (const auto source : scratch.MySeen)
			{
				scratch.MyTargets.clear();
				GenericEnemies(source, scratch.MyTargets, 0, false);
				if (std::isgreater(damage, 0)) for (const auto id : scratch.MyTargets)
					(void)DealDamage(_unit, id, {.MyAmount = damage, .MyType = DamageType::TRUE_DAMAGE, .MyTags = DamageTag::SKILL | DamageTag::TIDE, .MyIsSkill = true});
			}
			const auto loss = unit.MyStats.MyMaxHealth * kit.MyHealthLoss;
			if (std::isgreater(loss, 0))
			{
				if (std::isgreaterequal(unit.MyHealth - loss, 1)) (void)LoseHealth(_unit, _unit, loss, true);
				else unit.MyHealth = std::min(unit.MyHealth, 1.0);
			}
			return;
		}
		const auto value = unit.MyStats.MyAttack * (active ? kit.MySkillRatio : unit.MyBardRatio.value_or(kit.MyBaseRatio));
		for (const auto id : scratch.MyTargets) BardRegen(_unit, id, value, 0.75);
	}

	void BattleCore::Skadi2Observe(ContentEvent& _event, bool _late)
	{
		if (!_late && _event.MyKind == ContentEventKind::DAMAGED && _event.MyDamage.MySkadiShareSource && std::isgreater(_event.MyAmount, 0))
		{
			const auto source = _event.MyDamage.MySkadiShareSource;
			if (Unit(source).MyAlive && !Unit(source).MyHidden && !Unit(source).MyOperatorHooksReleased)
				(void)DealDamage(_event.MySource, source, {.MyAmount = _event.MyAmount * _event.MyDamage.MySkadiShare / std::max(1e-6, 1 - _event.MyDamage.MySkadiShare),
					.MyType = DamageType::TRUE_DAMAGE, .MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::TRANSFER)});
			return;
		}
		if (!_late && _event.MyKind == ContentEventKind::DEPLOY && _event.MyUnit)
		{
			const auto& ally = Unit(_event.MyUnit);
			if (ally.MyKind != UnitKind::OPERATOR || ally.MySide != UnitSide::ALLY) return;
			for (const auto id : _MySkadi2s)
			{
				const auto& unit = Unit(id); const auto& kit = std::get<Skadi2Kit>(*unit.MyDefinition.MyOperatorKit);
				if (id != ally.MyId && unit.MyAlive && !unit.MyHidden && !unit.MyOperatorHooksReleased && std::isgreater(kit.MyDeploySp, 0) && Skadi2Covers(id, ally.MyId)) (void)GainSp(id, kit.MyDeploySp, SpReason::TALENT);
			}
			return;
		}
		if (!_late || _event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MyTarget || _event.MyElement) return;
		const auto& target = Unit(_event.MyTarget);
		if (target.MySide != UnitSide::ALLY || target.MyKind == UnitKind::DEVICE) return;
		for (const auto id : _MySkadi2s)
		{
			const auto& unit = Unit(id); const auto& kit = std::get<Skadi2Kit>(*unit.MyDefinition.MyOperatorKit);
			if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased || !unit.MySkill.MyActive || !Skadi2Covers(id, target.MyId)) continue;
			if (std::isgreater(kit.MyFlatReduction, 0))
			{
				if (_event.MyDamage.MyType == DamageType::PHYSICAL) _event.MyDamage.MyDefenseIgnoreFlat -= kit.MyFlatReduction;
				else if (_event.MyDamage.MyType == DamageType::ARTS) _event.MyDamage.MyAmount = std::max(0.0, _event.MyDamage.MyAmount - kit.MyFlatReduction / std::max(0.05, 1 - target.MyStats.MyResistance / 100));
			}
		}
		if (_event.MyDamage.MyType == DamageType::ELEMENTAL || _event.MyDamage.MySkadiShareSource || HasTag(_event.MyDamage.MyTags, DamageTag::TRANSFER)) return;
		for (const auto id : _MySkadi2s)
		{
			const auto& unit = Unit(id); const auto& kit = std::get<Skadi2Kit>(*unit.MyDefinition.MyOperatorKit);
			if (id == target.MyId || kit.MySkill != Skadi2SkillKind::SEPARATE || !std::isgreater(kit.MyShare, 0) || !unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased || !unit.MySkill.MyActive || !Skadi2Covers(id, target.MyId)) continue;
			_event.MyDamage.MySkadiShareSource = id; _event.MyDamage.MySkadiShare = kit.MyShare;
			_event.MyDamage.MyMultiplier *= 1 - kit.MyShare;
			break;
		}
	}

	void BattleCore::SeabornDeploy(UnitId _unit)
	{
		auto& token = _MyUnits[Index(_unit)];
		token.MySeabornAccumulator = 0;
		if (!std::ranges::contains(_MySeaborns, _unit)) _MySeaborns.push_back(_unit);
		if (!token.MyOwnerUnit) return;
		const auto& owner = Unit(token.MyOwnerUnit);
		const auto* kit = owner.MyDefinition.MyOperatorKit ? std::get_if<Skadi2Kit>(owner.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit) return;
		if (!std::ranges::any_of(owner.MySkill.MyExtraRanges, [&](const SkillTriggerRange& _range) { return _range.MyArea.MySourceUnit == _unit; }))
			(void)AddSkillTriggerRange(owner.MyId, {.MySourceUnit = _unit});
		if (!token.MyDefinition.MyTokenKit->MyLifetimeSpecified) token.MyDefinition.MyTokenKit->MyLifetime = kit->MySeabornLifetime;
	}

	void BattleCore::SeabornExpired(UnitId _unit, std::uint64_t _version)
	{
		const auto& token = Unit(_unit);
		if (!token.MyAlive || token.MyDeploySequence != _version) return;
		const auto owner = token.MyOwnerUnit; const auto point = token.MyPosition;
		const auto delay = std::max(0.0, token.MyDefinition.MyStats.MyRedeploySeconds);
		const bool managed = owner && Unit(owner).MyDefinition.MyOperatorKit && std::holds_alternative<Skadi2Kit>(*Unit(owner).MyDefinition.MyOperatorKit);
		(void)Retreat(_unit, true, RemovalReason::EXPIRED);
		if (managed && !Finished()) Schedule({.MyAt = Time() + delay, .MyKind = ScheduledKind::SEABORN_RESPAWN, .MySource = owner, .MyTarget = _unit, .MyPoint = point});
	}

	void BattleCore::SeabornRespawn(UnitId _owner, UnitId _token, WorldPoint _point, unsigned _attempt)
	{
		if (Finished()) return;
		const auto& token = Unit(_token);
		const auto* kit = _owner && Unit(_owner).MyDefinition.MyOperatorKit ? std::get_if<Skadi2Kit>(Unit(_owner).MyDefinition.MyOperatorKit) : nullptr;
		if (!kit)
		{
			if (token.MyAlive || token.MyRemoved) return;
			if (!Redeploy(_token, false)) Schedule({.MyAt = Time() + 0.25, .MyKind = ScheduledKind::SEABORN_RESPAWN, .MySource = _owner, .MyTarget = _token});
			return;
		}
		const auto& owner = Unit(_owner);
		if (_attempt > 300 || owner.MyOperatorHooksReleased) return;
		const auto cost = token.MyDefinition.MyStats.MyDeploymentCost;
		bool spawned = false;
		if (owner.MyAlive && !owner.MyHidden && owner.MyOwner != NoPlayer && std::isgreaterequal(_MyPlayers[owner.MyOwner].MyDp + 1e-9, cost) && CanDeploy(_point, 0, false))
		{
			(void)AddDp(_MyPlayers[owner.MyOwner].MyPlayerId, -cost);
			const auto* body = FindTokenTemplate(_owner, kit->MySeaborn);
			spawned = body && SpawnToken({.MyDefinition = *body, .MyPosition = _point, .MyOwnerUnit = _owner});
			if (!spawned) (void)AddDp(_MyPlayers[owner.MyOwner].MyPlayerId, cost);
		}
		if (!spawned) Schedule({.MyAt = Time() + 1, .MyKind = ScheduledKind::SEABORN_RESPAWN, .MySource = _owner, .MyTarget = _token, .MyPoint = _point, .MyRemaining = _attempt + 1});
	}

	void BattleCore::SeabornTick(UnitId _unit, double _delta)
	{
		auto& token = _MyUnits[Index(_unit)];
		if (!token.MyAlive || token.MyHidden || token.MyStatuses.Has(CombatStatus::STUN)) return;
		const auto owner = token.MyOwnerUnit ? token.MyOwnerUnit : _unit;
		if (Unit(owner).MyDefinition.MyOperatorKit || Unit(owner).MyDefinition.MyContent.MyTag == ContentTag::CUSTOM_OPERATOR) return;
		token.MySeabornAccumulator += _delta;
		if (std::isless(token.MySeabornAccumulator, 1 - 1e-9)) return;
		token.MySeabornAccumulator -= 1;
		const auto& source = Unit(owner); const auto& kit = *token.MyDefinition.MyTokenKit;
		auto& scratch = AcquireAttackScratch(); const TideScratchGuard guard{.MyDepth = _MyAttackDepth};
		OperatorAlliesInRange(_unit, scratch.MyTargets);
		if (source.MyAlive && source.MySkill.MyActive && source.MyDefinition.MySkill.MyKind != SkillKind::PASSIVE)
		{
			for (const auto id : scratch.MyTargets)
				if (id != _unit && id != owner && std::isgreater(kit.MySeabornInspire, 0)) (void)AddBuff(id, {.MyKey = "inspire:" + std::to_string(owner), .MySource = _unit, .MyDuration = 1.25,
					.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_FLAT, .MyValue = source.MyStats.MyAttack * kit.MySeabornInspire}}});
			scratch.MyTargets.clear();
			for (const auto id : _MyEnemyIds) if (Unit(id).MyAlive && !Unit(id).MyHidden && InRuleRange(_unit, id)) scratch.MyTargets.push_back(id);
			for (const auto id : scratch.MyTargets) (void)DealDamage(_unit, id, {.MyAmount = source.MyStats.MyAttack * kit.MySeabornDamageScale, .MyType = DamageType::TRUE_DAMAGE, .MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::SUMMON), .MyIsSkill = true});
		}
		else for (const auto id : scratch.MyTargets)
			if (id != _unit && (!source.MyAlive || !InRuleRange(owner, id))) BardRegen(owner, id, source.MyStats.MyAttack * kit.MySeabornHealRatio, 1.25);
	}
}
