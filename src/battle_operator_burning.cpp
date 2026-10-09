#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		struct BurningScratchGuard
		{
			std::size_t& MyDepth;

			~BurningScratchGuard() { --MyDepth; }
		};
	}

	void Battle::ArtsAndElement(UnitId _unit, UnitId _target, double _scale, double _elementScale, Element _element)
	{
		const auto dealt = DealDamage(_unit, _target, {.MyAmount = Unit(_unit).MyStats.MyAttack * _scale, .MyType = DamageType::ARTS,
			.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
		if (std::isgreater(dealt, 0) && Unit(_target).MyAlive)
			(void)DealElement(_unit, _target, {.MyElement = _element, .MyAmount = dealt * _elementScale, .MyTags = static_cast<DamageTags>(DamageTag::SKILL)});
	}

	void Battle::BurstSpAura(UnitId _unit, std::string_view _key, double _sp)
	{
		const AttackProfile profile{.MyCanHitFlying = true};
		if (std::isgreater(_sp, 0) && std::ranges::any_of(_MyEnemyIds, [&](UnitId _id)
			{ return TargetableEnemy(Unit(_id), profile) && InRuleRange(_unit, _id) && Unit(_id).MyStatuses.Has(CombatStatus::BURST_LOCK); }))
			(void)AddBuff(_unit, {.MyKey = std::string(_key), .MyDuration = 0.5, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::SP_RECOVERY_FLAT, .MyValue = _sp}}});
	}

	void Battle::Blaze2Observe(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::ELEMENT_BURST && _event.MyElement == Element::BURN && _event.MyTarget)
		{
			for (std::size_t i = 0, count = _MyBlazes.size(); i < count; ++i)
			{
				const auto& unit = Unit(_MyBlazes[i]);
				if (!unit.MyAlive || unit.MyOperatorHooksReleased) continue;
				const auto& kit = std::get<Blaze2Kit>(*unit.MyDefinition.MyOperatorKit);
				if (Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
				{
					if (std::isgreater(kit.MyMeltdownScale, 0)) (void)DealDamage(unit.MyId, _event.MyTarget, {.MyAmount = unit.MyStats.MyAttack * kit.MyMeltdownScale,
						.MyType = DamageType::ELEMENTAL, .MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::TALENT)});
					if (!unit.MyBlazeDowned) (void)Heal(unit.MyId, unit.MyId, unit.MyStats.MyMaxHealth * kit.MyMeltdownHeal, {.MySelf = true});
				}
				if (kit.MySkill == Blaze2SkillKind::FURNACE && unit.MySkill.MyActive && unit.MyDefinition.MySkill.MyKind == SkillKind::AMMO)
				{
					const auto ammo = std::min(kit.MyAmmoRefill, unit.MyDefinition.MySkill.MyAmmo - unit.MySkill.MyAmmoLeft);
					if (std::isgreater(ammo, 0)) (void)AddSkillAmmo(unit.MyId, ammo);
				}
			}
		}
		if (_event.MyKind != ContentEventKind::BUFF_TICK || !_event.MyUnit) return;
		auto& target = _MyUnits[Index(_event.MyUnit)];
		const auto found = std::ranges::find(target.MyBuffs, _event.MyBuff, &CombatBuff::MyId);
		if (found == target.MyBuffs.end()) return;
		const auto source = found->MyDefinition.MySource;
		const bool down = found->MyDefinition.MyKey == "blaze2:downed";
		if (!source || (!down && !found->MyDefinition.MyKey.starts_with("blaze2:aid:"))) return;
		const auto* kit = Unit(source).MyDefinition.MyOperatorKit ? std::get_if<Blaze2Kit>(Unit(source).MyDefinition.MyOperatorKit) : nullptr;
		if (!kit) return;
		auto& scratch = AcquireAttackScratch(); const BurningScratchGuard guard{.MyDepth = _MyAttackDepth};
		if (down)
		{
			if (target.MyDeploySequence != target.MyBlazeDownDeployment || std::isless(target.MyHealth, target.MyStats.MyMaxHealth - 1e-6)) return;
			(void)RemoveBuff(target.MyId, "blaze2:downed"); target.MyBlazeDowned = false;
			FoesInRadius(RulePosition(target), 1.7, scratch.MyTargets);
			for (const auto id : scratch.MyTargets) (void)ApplyStatus(id, CombatStatus::STUN, kit->MyReviveStun, target.MyId);
		}
		else
		{
			FoesInRadius(RulePosition(target), kit->MyRadius, scratch.MyTargets);
			for (const auto id : scratch.MyTargets) ArtsAndElement(source, id, kit->MyArtsScale, kit->MyElementScale, Element::BURN);
		}
	}

	void Battle::Blaze2Fatal(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::FATAL || !_event.MyUnit || _event.MyPrevented) return;
		auto& unit = _MyUnits[Index(_event.MyUnit)];
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<Blaze2Kit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || unit.MyOperatorHooksReleased || unit.MyBlazeDowned) return;
		_event.MyPrevented = true; unit.MyBlazeDowned = true;
		if (unit.MySkill.MyActive) EndSkill(unit.MyId, SkillReason::DOWNED);
		ReleaseBlocked(unit); unit.MyBlazeDownDeployment = unit.MyDeploySequence;
		StatusFlags flags; flags.set(static_cast<std::size_t>(CombatStatus::DISARM)); flags.set(static_cast<std::size_t>(CombatStatus::NO_HEAL));
		(void)AddBuff(unit.MyId, {.MyKey = "blaze2:downed", .MySource = unit.MyId,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALTH_REGEN_RATIO, .MyValue = kit->MyDownRegen}, {.MyAttribute = Attribute::BLOCK_COUNT, .MyValue = -99}},
			.MyFlags = flags, .MyInterval = 0.2, .MyShield = {.MyHealth = kit->MyDownShield}, .MyNotifyTick = true});
	}

	void Battle::Blaze2Ground(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (!unit.MyAlive || unit.MyOperatorHooksReleased || !unit.MySkill.MyActive || unit.MyBurnTiles.none()) return;
		const auto& kit = std::get<Blaze2Kit>(*unit.MyDefinition.MyOperatorKit);
		unit.MyBlazeAccumulator += 0.25;
		const bool damage = std::isgreaterequal(unit.MyBlazeAccumulator, 1 - 1e-9);
		if (damage) unit.MyBlazeAccumulator -= 1;
		const auto key = "blaze2:ground:" + std::to_string(_unit);
		auto& scratch = AcquireAttackScratch(); const BurningScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyEnemyIds)
			if (Unit(id).MyAlive && !Unit(id).MyHidden && !Unit(id).Flying() && BodyInRange(Unit(id), unit.MyBurnTiles)) scratch.MyTargets.push_back(id);
		for (const auto id : scratch.MyTargets)
		{
			(void)AddBuff(id, {.MyKey = key, .MyDuration = 0.5, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::MOVE_MULTIPLIER, .MyValue = kit.MyMoveMultiplier}}});
			if (damage) ArtsAndElement(_unit, id, kit.MyArtsScale, kit.MyElementScale, Element::BURN);
		}
	}

	void Battle::Blaze2Skill(UnitId _unit, const Blaze2Kit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY) unit.MyBlazeDowned = false;
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
		{
			const auto& enemy = Unit(_event.MyTarget);
			if (enemy.MyStatuses.Has(CombatStatus::BURST_LOCK) && std::isgreater(_kit.MyBurstMultiplier, 1)) _event.MyDamage.MyMultiplier *= _kit.MyBurstMultiplier;
			if (_kit.MySkill == Blaze2SkillKind::FURNACE && !_event.MyCancel && _event.MyDamage.MyIsAttack && _event.MyDamage.MyIsSkill && enemy.MyAlive && std::isgreater(_kit.MyBurnBonus, 0) &&
				std::ranges::any_of(enemy.MyBuffs, [](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == "burnBurst"; }))
				(void)DealDamage(_unit, enemy.MyId, {.MyAmount = unit.MyStats.MyAttack * _kit.MyBurnBonus, .MyType = DamageType::ELEMENTAL, .MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::SKILL)});
		}
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			unit.MyBlazeAccumulator = 0; unit.MyBurnTiles.reset();
			if (_kit.MySkill == Blaze2SkillKind::AID)
			{
				auto& scratch = AcquireAttackScratch(); const BurningScratchGuard guard{.MyDepth = _MyAttackDepth};
				OperatorAlliesInRange(_unit, scratch.MyTargets);
				UnitId target = 0;
				for (const auto id : scratch.MyTargets)
					if (Unit(id).MyKind == UnitKind::OPERATOR && (!target || std::isgreater(Unit(id).MyStats.MyMaxHealth, Unit(target).MyStats.MyMaxHealth) ||
						(!std::islessgreater(Unit(id).MyStats.MyMaxHealth, Unit(target).MyStats.MyMaxHealth) && id < target))) target = id;
				if (target) (void)AddBuff(target, {.MyKey = "blaze2:aid:" + std::to_string(_unit), .MySource = _unit, .MyDuration = _kit.MyAidDuration, .MyInterval = _kit.MyAidInterval, .MyNotifyTick = true});
			}
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING) unit.MyBurnTiles.reset();
		else if (_event.MyKind == ContentEventKind::ATTACK && unit.MySkill.MyActive && _kit.MySkill == Blaze2SkillKind::GROUND)
			(void)LoseHealth(_unit, _unit, unit.MyStats.MyMaxHealth * _kit.MyAttackLoss);
		else if (_event.MyKind == ContentEventKind::SKILL_TICK && _kit.MySkill == Blaze2SkillKind::FURNACE && std::isgreater(_kit.MyLoss, 0))
		{
			PeriodicHealthLoss(_unit, unit.MyBlazeAccumulator, _event.MyDelta, 1, _kit.MyLoss, false);
		}
	}
}
