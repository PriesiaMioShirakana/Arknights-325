#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		struct RangedScratchGuard
		{
			std::size_t& MyDepth;

			~RangedScratchGuard() { --MyDepth; }
		};
	}

	void Battle::HainiSkill(UnitId _unit, const HainiKit& _kit, const ContentEvent& _event)
	{
		if (!_kit.MySlow) return;
		const auto& unit = Unit(_unit);
		if (_event.MyKind == ContentEventKind::SKILL_START || _event.MyKind == ContentEventKind::SKILL_ENDING)
			_MyOperatorAuras[unit.MyTalentAura].MyScale = 1;
		else if (_event.MyKind != ContentEventKind::SKILL_TICK) return;
		RefreshOperatorAura(unit.MySkillAura);
	}

	void Battle::HainiKill(UnitId _victim)
	{
		const auto& victim = Unit(_victim);
		if (victim.MySide != UnitSide::ENEMY || victim.MyDefinition.MyElite || victim.MyDefinition.MyLeader || victim.MySpawnTag == EnemySpawnTag::BOSS) return;
		for (const auto id : _MyHainis)
		{
			const auto& unit = Unit(id);
			if (!unit.MyAlive || !unit.MySkill.MyActive || unit.MyOperatorHooksReleased || !InRuleRange(id, _victim)) continue;
			const auto& kit = std::get<HainiKit>(*unit.MyDefinition.MyOperatorKit);
			auto& aura = _MyOperatorAuras[unit.MyTalentAura];
			aura.MyScale = std::min(kit.MyMaxMultiplier, aura.MyScale + kit.MyKillStep);
		}
	}

	void Battle::PinecnSkill(UnitId _unit, const PinecnKit& _kit, ContentEvent& _event)
	{
		const auto& unit = Unit(_unit);
		if (_event.MyKind == ContentEventKind::DEPLOY)
			(void)AddBuff(_unit, {.MyKey = "talent:pinecn_power", .MyDuration = _kit.MySpDuration,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::SP_RECOVERY_FLAT, .MyValue = _kit.MySpRecovery}}});
		else if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			if (_kit.MySpike) (void)ForceAttack(_unit);
			else if (!_kit.MyAttackSteps.empty())
			{
				const auto index = std::min<std::uint64_t>(_kit.MyAttackSteps.size() - 1, unit.MySkill.MyActivations ? unit.MySkill.MyActivations - 1 : 0);
				(void)AddBuff(_unit, {.MyKey = "skill:pinecn_atk", .MyModifiers = std::vector<AttributeChange>{
					{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _kit.MyAttackSteps[static_cast<std::size_t>(index)]}}});
			}
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING && !_kit.MySpike) (void)RemoveBuff(_unit, "skill:pinecn_atk");
		else if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _kit.MySpike && std::isgreater(_kit.MyDefenseIgnore, 0) && HasTag(_event.MyDamage.MyTags, DamageTag::PINECN_SPIKE))
			_event.MyDamage.MyDefenseIgnoreFlat += _kit.MyDefenseIgnore;
	}

	void Battle::SnhuntSkill(UnitId _unit, const SnhuntKit& _kit)
	{
		const auto& unit = Unit(_unit);
		auto& scratch = AcquireAttackScratch(); const RangedScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyEnemyIds)
			if (TargetableEnemy(Unit(id), unit.MyDefinition.MyAttack) && InRuleRange(_unit, id)) scratch.MyTargets.push_back(id);
		SortOperatorTargets(_unit, scratch.MyTargets, 1, &unit.MyDefinition.MyAttack);
		if (scratch.MyTargets.empty()) return;
		const auto target = scratch.MyTargets.front();
		if (_kit.MyVolley) LaunchSkillProjectile(_unit, target, 16, {.MyScale = _kit.MyMovingScale, .MyStillScale = _kit.MyStillScale,
			.MyHits = _kit.MyShots, .MyTags = DamageTag::SKILL | DamageTag::SNHUNT});
		(void)DealDamage(_unit, target, {.MyAmount = unit.MyStats.MyAttack * _kit.MyBeastScale, .MyType = DamageType::PHYSICAL,
			.MyTags = DamageTag::TALENT | DamageTag::CLOUDBEAST, .MyIsSkill = true});
		if (Unit(target).MyAlive) (void)ApplyStatus(target, CombatStatus::COLD, _kit.MyCold, _unit);
	}

	void Battle::ShotstShred(UnitId _unit, UnitId _target, const ShotstKit& _kit)
	{
		if (!_target || !Unit(_target).MyAlive || Unit(_target).MySide != UnitSide::ENEMY) return;
		(void)AddBuff(_target, {.MyKey = "skill:shotst_shred", .MySource = _unit, .MyDuration = _kit.MyShredDuration,
			.MyRefresh = BuffRefresh::EXTEND, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::DEFENSE_PERCENT, .MyValue = _kit.MyShred}}});
	}

	void Battle::ShotstBurst(UnitId _unit, const ShotstKit& _kit)
	{
		const auto& unit = Unit(_unit);
		auto& scratch = AcquireAttackScratch(); const RangedScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyEnemyIds)
			if (TargetableEnemy(Unit(id), unit.MyDefinition.MyAttack) && InRuleRange(_unit, id)) scratch.MyTargets.push_back(id);
		SortOperatorTargets(_unit, scratch.MyTargets, _kit.MyTargets, &unit.MyDefinition.MyAttack);
		for (const auto id : scratch.MyTargets)
		{
			const auto flying = Unit(id).Flying() ? unit.MyDefinition.MyAttack.MyConditionalScale : 1;
			(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * _kit.MySkillScale * flying,
				.MyType = DamageType::PHYSICAL, .MyTags = DamageTag::SKILL | DamageTag::BURST, .MyIsSkill = true});
			ShotstShred(_unit, id, _kit);
		}
	}

	void Battle::SnhuntTick(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<SnhuntKit>(*unit.MyDefinition.MyOperatorKit);
		if (kit.MyVolley && !std::isgreater(unit.MyProfession.MyAmmo, 0) && CanAutoSkill(unit))
		{
			const auto& range = unit.MyBaseTriggerMask;
			for (const auto id : _MyEnemyIds)
			{
				const auto& enemy = Unit(id);
				if (!TargetableEnemy(enemy, unit.MyDefinition.MyAttack)) continue;
				const auto tiles = BodyTiles(enemy); bool inside = false;
				for (int row = tiles.MyFirstRow; row <= tiles.MyLastRow && !inside; ++row)
					for (int column = tiles.MyFirstColumn; column <= tiles.MyLastColumn && !inside; ++column)
						if (row >= 0 && row < FieldRows && column >= 0 && column < FieldColumns) inside = range.test(static_cast<std::size_t>(FieldGrid::Key(row, column)));
				if (inside) { (void)ActivateSkill(_unit, false, SkillReason::TRIGGER); break; }
			}
		}
		if (std::isgreater(kit.MyReloadExtra, 0))
		{
			if (unit.MyPreviousAmmo && !std::islessgreater(*unit.MyPreviousAmmo, 0) && !std::islessgreater(unit.MyProfession.MyAmmo, 1))
				unit.MyProfession.MyAmmo = std::min(std::max(1.0, unit.MyDefinition.MyProfession.MyAmmoMax), unit.MyProfession.MyAmmo + kit.MyReloadExtra);
			unit.MyPreviousAmmo = unit.MyProfession.MyAmmo;
		}
	}
}
