#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct DeliveryScratchGuard
		{
			std::size_t& MyDepth;

			~DeliveryScratchGuard() { --MyDepth; }
		};
	}

	std::uint64_t BattleCore::DecayingShield(UnitId _unit, std::string _key, double _total, double _duration)
	{
		if (!std::isgreater(_total, 0)) return 0;
		const bool decay = std::isgreater(_duration, 0);
		return AddBuff(_unit, {.MyKey = std::move(_key), .MyDuration = decay ? _duration : std::numeric_limits<double>::infinity(),
			.MyInterval = decay ? 0.5 : 0, .MyShield = {.MyHealth = _total}, .MyBuiltin = BuiltinBuff::DECAYING_SHIELD,
			.MyStrength = BuffStrength{.MyValue = decay ? _total * 0.5 / _duration : 0}, .MyNotifyTick = decay});
	}

	void BattleCore::DecayingShieldTick(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BUFF_TICK || !_event.MyUnit || !_event.MyBuff) return;
		auto& buffs = _MyUnits[Index(_event.MyUnit)].MyBuffs;
		const auto found = std::ranges::find(buffs, _event.MyBuff, &CombatBuff::MyId);
		if (found != buffs.end() && found->MyDefinition.MyBuiltin == BuiltinBuff::DECAYING_SHIELD && found->MyDefinition.MyStrength)
			found->MyDefinition.MyShield.MyHealth = std::max(0.0, found->MyDefinition.MyShield.MyHealth - found->MyDefinition.MyStrength->MyValue);
	}

	void BattleCore::Angel2Airstrike(UnitId _unit, WorldPoint _point, double _scale, DamageTags _tags, bool _center)
	{
		auto& scratch = AcquireAttackScratch(); const DeliveryScratchGuard guard{.MyDepth = _MyAttackDepth};
		FoesInRadius(_point, 1, scratch.MyTargets, _center);
		for (const auto id : scratch.MyTargets)
			if (Unit(id).MyAlive && !Unit(id).MyStatuses.Has(CombatStatus::UNTARGETABLE))
				(void)DealDamage(_unit, id, {.MyAmount = Unit(_unit).MyStats.MyAttack * _scale, .MyTags = _tags, .MyIsSplash = true, .MyIsSkill = true});
	}

	void BattleCore::Angel2Coordinate(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		const auto& kit = std::get<Angel2Kit>(*unit.MyDefinition.MyOperatorKit);
		unit.MyAngelCoordinate = false;
		for (const auto id : _MyAllyIds)
			if (const auto& token = Unit(id); token.MyAlive && token.MyKind == UnitKind::TOKEN && token.MyOwnerUnit == _unit && token.MyDefinition.MyId == kit.MyCoordinate) return;
		const auto* body = FindTokenTemplate(_unit, kit.MyCoordinate);
		if (body) if (const auto tile = TacticalSummonTile(_unit)) (void)SpawnToken({.MyDefinition = *body, .MyPosition = *tile, .MyOwnerUnit = _unit, .MyUntargetable = true});
	}

	void BattleCore::Angel2Observe(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::SKILL_START && _event.MyUnit)
		{
			auto& unit = _MyUnits[Index(_event.MyUnit)];
			const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<Angel2Kit>(unit.MyDefinition.MyOperatorKit) : nullptr;
			if (kit && kit->MySkill == Angel2SkillKind::SKY && !unit.MyOperatorHooksReleased) unit.MyAngelAmmoLeft = unit.MySkill.MyAmmoLeft;
			return;
		}
		if (_event.MyKind == ContentEventKind::TICK)
		{
			const AttackProfile filter{.MyCanHitFlying = true};
			for (const auto id : _MyAngel2s)
			{
				const auto& unit = Unit(id);
				if (!unit.MyAngelCoordinate || !unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) continue;
				if (std::ranges::any_of(_MyEnemyIds, [&](UnitId _target) { return TargetableEnemy(Unit(_target), filter) && InRuleRange(id, _target); })) Angel2Coordinate(id);
			}
			return;
		}
		if (_event.MyKind != ContentEventKind::AMMO_USED || !_event.MyUnit || Unit(_event.MyUnit).MySide != UnitSide::ALLY) return;
		for (std::size_t i = 0, count = _MyAngel2s.size(); i < count; ++i)
		{
			const auto id = _MyAngel2s[i]; const auto& unit = Unit(id);
			if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) continue;
			const auto& kit = std::get<Angel2Kit>(*unit.MyDefinition.MyOperatorKit);
			if (std::isgreater(kit.MyAmmoHeal, 0)) (void)Heal(id, id, unit.MyStats.MyMaxHealth * kit.MyAmmoHeal, {.MySelf = true, .MySilent = true});
			if (!std::isgreater(kit.MyAirstrikeChance, 0) || !std::isgreater(kit.MyAirstrikeScale, 0) || !std::isless(_MyRandom.Next(), kit.MyAirstrikeChance)) continue;
			auto& scratch = AcquireAttackScratch(); const DeliveryScratchGuard guard{.MyDepth = _MyAttackDepth};
			GenericEnemies(_event.MyUnit, scratch.MyTargets, 1, true);
			if (!scratch.MyTargets.empty()) Angel2Airstrike(id, Unit(scratch.MyTargets.front()).MyPosition, kit.MyAirstrikeScale, DamageTag::TALENT | DamageTag::AIRSTRIKE, true);
		}
	}

	void BattleCore::Angel2Pulse(UnitId _unit, bool _calm)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<Angel2Kit>(*unit.MyDefinition.MyOperatorKit);
		if (_calm)
		{
			if (std::isgreater(unit.MyHealth / unit.MyStats.MyMaxHealth, kit.MyCalmHealth)) (void)AddBuff(_unit, {.MyKey = "angel2:calm", .MyDuration = 0.4,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::SP_RECOVERY_FLAT, .MyValue = kit.MyCalmSp}}});
			return;
		}
		auto& scratch = AcquireAttackScratch(); const DeliveryScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyAllyIds)
			if (const auto& ally = Unit(id); ally.MyAlive && !ally.MyHidden && ally.MyKind == UnitKind::OPERATOR && ally.MyDefinition.MySkill.MyKind == SkillKind::AMMO) scratch.MyTargets.push_back(id);
		for (const auto id : scratch.MyTargets) (void)AddBuff(id, {.MyKey = "angel2:covenant", .MyDuration = 0.75,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyCovenantAttack * (std::ranges::contains(Unit(id).MyDefinition.MyIdentity.MyBonds, "lateranoShip") ? kit.MyCovenantMultiplier : 1)}}});
	}

	void BattleCore::Angel2Skill(UnitId _unit, const Angel2Kit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_kit.MySkill == Angel2SkillKind::SKY)
		{
			if (_event.MyKind == ContentEventKind::ATTACK && unit.MySkill.MyActive) unit.MyAngelAmmoLeft = unit.MySkill.MyAmmoLeft - 1;
			if (_event.MyKind != ContentEventKind::SKILL_ENDING) return;
			const auto left = static_cast<unsigned>(std::max(0.0, std::floor(std::exchange(unit.MyAngelAmmoLeft, 0))));
			if (_event.MySkillReason != SkillReason::STOPPED || !unit.MyAlive || !left) return;
			auto& scratch = AcquireAttackScratch(); const DeliveryScratchGuard guard{.MyDepth = _MyAttackDepth};
			for (unsigned i = 0; i < left; ++i)
			{
				GenericEnemies(_unit, scratch.MyTargets, 0, false);
				if (scratch.MyTargets.empty()) break;
				const auto target = scratch.MyTargets[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MyTargets.size()))];
				(void)DealDamage(_unit, target, {.MyAmount = unit.MyStats.MyAttack * _kit.MyAttackScale, .MyTags = DamageTag::SKILL | DamageTag::VOLLEY, .MyIsAttack = true, .MyIsSkill = true});
				ContentEvent ammo{.MyKind = ContentEventKind::AMMO_USED, .MyUnit = _unit, .MyAmount = static_cast<double>(left - i - 1)};
				NotifyContent(ammo);
			}
			return;
		}
		if (_kit.MySkill == Angel2SkillKind::ADDICTION)
		{
			if (_event.MyKind == ContentEventKind::SKILL_START)
			{
				auto& scratch = AcquireAttackScratch(); const DeliveryScratchGuard guard{.MyDepth = _MyAttackDepth};
				OperatorAlliesInRange(_unit, scratch.MyTargets);
				std::erase_if(scratch.MyTargets, [&](UnitId _id) { return _id == _unit || Unit(_id).MyKind != UnitKind::OPERATOR; });
				std::ranges::sort(scratch.MyTargets, [&](UnitId _left, UnitId _right)
				{
					const auto& left = Unit(_left); const auto& right = Unit(_right);
					return std::islessgreater(left.MyStats.MyTaunt, right.MyStats.MyTaunt) ? std::isgreater(left.MyStats.MyTaunt, right.MyStats.MyTaunt) :
						left.MyAggroSequence != right.MyAggroSequence ? left.MyAggroSequence > right.MyAggroSequence : _left < _right;
				});
				unit.MyAngelVictim = std::isgreater(_kit.MyStealSpeed, 0) && !scratch.MyTargets.empty() ? scratch.MyTargets.front() : 0;
				(void)DecayingShield(_unit, "angel2:barrier", unit.MyStats.MyMaxHealth * _kit.MyShieldRatio, _kit.MyShieldDuration);
				if (unit.MyAngelVictim)
				{
					const auto victim = unit.MyAngelVictim;
					(void)AddBuff(victim, {.MyKey = "angel2:stolen", .MySource = _unit, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = -_kit.MyStealSpeed}}});
					(void)AddBuff(_unit, {.MyKey = "angel2:steal", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = _kit.MyStealSpeed}}});
					(void)DecayingShield(victim, "angel2:barrier", Unit(victim).MyStats.MyMaxHealth * _kit.MyShieldRatio, _kit.MyShieldDuration);
					if (_kit.MyExtraAmmo > 0) (void)AddSkillAmmo(_unit, _kit.MyExtraAmmo);
				}
			}
			else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
			{
				const auto victim = std::exchange(unit.MyAngelVictim, 0);
				if (victim) (void)RemoveBuff(victim, "angel2:stolen");
				(void)RemoveBuff(_unit, "angel2:steal");
			}
			return;
		}
		if (_event.MyKind == ContentEventKind::DEPLOY)
		{
			unit.MyAngelCoordinate = !std::ranges::any_of(_MyAllyIds, [&](UnitId _id)
			{
				const auto& token = Unit(_id);
				return token.MyAlive && token.MyKind == UnitKind::TOKEN && token.MyOwnerUnit == _unit && token.MyDefinition.MyId == _kit.MyCoordinate;
			});
		}
		else if (_event.MyKind == ContentEventKind::ATTACK && unit.MySkill.MyActive)
		{
			for (unsigned i = 1; i < 5 && std::isgreater(unit.MySkill.MyAmmoLeft, 1); ++i)
			{
				--unit.MySkill.MyAmmoLeft;
				ContentEvent ammo{.MyKind = ContentEventKind::AMMO_USED, .MyUnit = _unit, .MyAmount = unit.MySkill.MyAmmoLeft};
				NotifyContent(ammo);
			}
		}
		else if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			if (unit.MyAngelCoordinate) Angel2Coordinate(_unit);
			UnitId coordinate = 0;
			for (const auto id : _MyAllyIds)
				if (const auto& token = Unit(id); token.MyAlive && token.MyKind == UnitKind::TOKEN && token.MyOwnerUnit == _unit && token.MyDefinition.MyId == _kit.MyCoordinate) { coordinate = id; break; }
			if (!coordinate) return;
			const auto point = RulePosition(Unit(coordinate));
			Angel2Airstrike(_unit, point, _kit.MyCannonScale, DamageTag::SKILL | DamageTag::DELIVERY, false);
			auto& scratch = AcquireAttackScratch(); const DeliveryScratchGuard guard{.MyDepth = _MyAttackDepth};
			for (const auto id : _MyAllyIds)
			{
				const auto& ally = Unit(id);
				if (id != _unit && ally.MyKind == UnitKind::OPERATOR && ally.MyOwner == unit.MyOwner && !ally.MyAlive && !ally.MyRemoved && ally.MyDefinition.MyIdentity.MyMeleePosition &&
					(!_MyGrid || _MyGrid->CanStand(static_cast<int>(point.MyY), static_cast<int>(point.MyX)))) scratch.MyTargets.push_back(id);
			}
			if (scratch.MyTargets.empty()) return;
			std::ranges::sort(scratch.MyTargets, [&](UnitId _left, UnitId _right)
			{
				const auto& left = Unit(_left); const auto& right = Unit(_right);
				if (std::islessgreater(left.MyRespawnAt, right.MyRespawnAt)) return std::isgreater(left.MyRespawnAt, right.MyRespawnAt);
				if (std::islessgreater(left.MyDefinition.MyStats.MyRedeploySeconds, right.MyDefinition.MyStats.MyRedeploySeconds)) return std::isgreater(left.MyDefinition.MyStats.MyRedeploySeconds, right.MyDefinition.MyStats.MyRedeploySeconds);
				return _left < _right;
			});
			const auto target = scratch.MyTargets.front();
			(void)Retreat(coordinate, true, RemovalReason::EXPIRED);
			if (Redeploy(target, true, point)) (void)GainSp(target, _kit.MyDeliverySp, SpReason::SKILL);
		}
	}
}
