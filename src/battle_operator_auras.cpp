#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		struct OperatorAuraScratchGuard
		{
			std::size_t& MyDepth;

			~OperatorAuraScratchGuard() { --MyDepth; }
		};
	}

	bool Battle::OperatorInGrid(UnitId _source, UnitId _target, std::span<const RangeOffset> _grid) const
	{
		const auto& source = Unit(_source); const auto origin = RulePosition(source);
		const int row = static_cast<int>(std::floor(origin.MyY + 0.5)), column = static_cast<int>(std::floor(origin.MyX + 0.5));
		for (const auto offset : _grid)
		{
			const auto local = RotateOffset(offset, source.MyFacing); const int r = row + local.MyRow, c = column + local.MyColumn;
			if (r >= 0 && r < FieldRows && c >= 0 && c < FieldColumns && BodyOnTile(Unit(_target), r, c)) return true;
		}
		return false;
	}

	void Battle::NotifyOperatorObservers(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget)
		{
			const auto* rules = Unit(_event.MyTarget).MyDefinition.MyOperatorKit;
			if (const auto* kit = rules ? std::get_if<UtageKit>(rules) : nullptr) UtageProtect(_event.MyTarget, *kit);
		}
		if (_MyTinmanWither && _event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY &&
			(HasTag(_event.MyDamage.MyTags, DamageTag::DOT) || HasTag(_event.MyDamage.MyTags, DamageTag::NECROSIS) || HasTag(_event.MyDamage.MyTags, DamageTag::APOPTOSIS)))
		{
			const auto& buffs = Unit(_event.MyTarget).MyBuffs;
			const auto found = std::ranges::find(buffs, "tinman:wither", [](const auto& buff) -> const auto& { return buff.MyDefinition.MyKey; });
			if (found != buffs.end() && found->MyDefinition.MyStrength) _event.MyDamage.MyAmount *= found->MyDefinition.MyStrength->MyValue;
		}
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && _event.MyDamage.MyType == DamageType::PHYSICAL)
		{
			const auto& target = Unit(_event.MyTarget); const auto* kit = target.MyDefinition.MyOperatorKit;
			if (const auto* estell = kit ? std::get_if<EstellKit>(kit) : nullptr; estell && target.MyHealth / target.MyStats.MyMaxHealth > estell->MyHealthThreshold)
				_event.MyDamage.MyMultiplier *= 1 - estell->MyPhysicalReduction;
		}
		if (_event.MyKind == ContentEventKind::DEATH && _event.MyRemovalReason == RemovalReason::KILLED && Unit(_event.MyUnit).MySide == UnitSide::ENEMY)
		{
			for (std::size_t i = 0, count = _MyEstells.size(); i < count; ++i)
			{
				const auto& unit = Unit(_MyEstells[i]); if (!unit.MyAlive) continue;
				const auto& kit = std::get<EstellKit>(*unit.MyDefinition.MyOperatorKit); const auto origin = RulePosition(unit);
				const bool near = kit.MyHasRange ? OperatorInGrid(unit.MyId, _event.MyUnit, kit.MyRange) :
					BodyTileReach(Unit(_event.MyUnit), static_cast<int>(std::floor(origin.MyY + 0.5)), static_cast<int>(std::floor(origin.MyX + 0.5))) <= 1;
				if (near) (void)Heal(unit.MyId, unit.MyId, unit.MyStats.MyMaxHealth * kit.MyHealRatio, {.MySelf = true});
			}
		}
		if (_event.MyKind != ContentEventKind::TICK) return;
		for (std::size_t i = 0, count = _MyUtages.size(); i < count; ++i) UtageTick(_MyUtages[i]);
		for (std::size_t i = 0, count = _MyPodegos.size(); i < count; ++i)
		{
			const auto& unit = Unit(_MyPodegos[i]);
			if (!CanAutoSkill(unit) || unit.MySkill.MyActive) continue;
			for (const auto id : _MyAllyIds)
			{
				const auto& ally = Unit(id);
				if (!ally.MyAlive || ally.MyHidden || ally.MyKind == UnitKind::DEVICE || !(ally.MyHealth < ally.MyStats.MyMaxHealth - 1e-6) ||
					(id != unit.MyId && (ally.MyStatuses.Has(CombatStatus::NO_HEAL) || ally.MyDefinition.MyAttack.MyNoHeal))) continue;
				const auto point = RulePosition(ally); const int r = static_cast<int>(std::floor(point.MyY + 0.5)), c = static_cast<int>(std::floor(point.MyX + 0.5));
				if (r < 0 || r >= FieldRows || c < 0 || c >= FieldColumns || !unit.MyBaseTriggerMask.test(static_cast<std::size_t>(FieldGrid::Key(r, c)))) continue;
				(void)ActivateSkill(unit.MyId, false, SkillReason::TRIGGER); break;
			}
		}
	}

	void Battle::UtageProtect(UnitId _unit, const UtageKit& _kit)
	{
		const auto& unit = Unit(_unit);
		if (unit.MyAlive && _kit.MyProtection > 0 && unit.MyHealth / unit.MyStats.MyMaxHealth < _kit.MyProtectThreshold)
			(void)ApplyStrongest(_unit, "protect", 1.5 * BattleClock::StepSeconds,
				{.MyValue = _kit.MyProtection, .MyAttribute = Attribute::PHYSICAL_TAKEN_MULTIPLIER, .MyScale = -1, .MyOffset = 1,
					.MySecondAttribute = Attribute::ARTS_TAKEN_MULTIPLIER}, _unit);
	}

	void Battle::UtageTick(UnitId _unit)
	{
		const auto& unit = Unit(_unit); if (!unit.MyAlive) return;
		const auto& kit = std::get<UtageKit>(*unit.MyDefinition.MyOperatorKit);
		if (kit.MyMaxAttackSpeed > 0 && kit.MyMinHealthRatio < 1)
		{
			const auto value = kit.MyMaxAttackSpeed * std::clamp((1 - unit.MyHealth / unit.MyStats.MyMaxHealth) / (1 - kit.MyMinHealthRatio), 0.0, 1.0);
			const auto found = std::ranges::find(unit.MyBuffs, "utage:serious", [](const auto& buff) -> const auto& { return buff.MyDefinition.MyKey; });
			const bool existing = found != unit.MyBuffs.end();
			const auto previous = existing && found->MyDefinition.MyStrength ? found->MyDefinition.MyStrength->MyValue : 0;
			if (!existing || std::abs(previous - value) >= 0.5)
			{
				if (value < 0.5) { if (existing) (void)RemoveBuff(_unit, "utage:serious"); }
				else (void)AddBuff(_unit, {.MyKey = "utage:serious", .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_SPEED, value}},
					.MyStrength = BuffStrength{.MyValue = value, .MyAttribute = Attribute::ATTACK_SPEED}});
			}
		}
		UtageProtect(_unit, kit);
	}

	void Battle::PodegoAura(UnitId _unit)
	{
		const auto& source = Unit(_unit); if (!source.MyAlive) return;
		const auto value = std::get<PodegoKit>(*source.MyDefinition.MyOperatorKit).MyAuraAttack;
		auto& scratch = AcquireAttackScratch(); const OperatorAuraScratchGuard guard{_MyAttackDepth};
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (!ally.MyAlive || ally.MyKind != UnitKind::OPERATOR || ally.MyDefinition.MyOperatorProfession != OperatorProfession::SUPPORT ||
				(ally.MyId != _unit && ally.MyStatuses.Has(CombatStatus::ISOLATED))) continue;
			scratch.MyTargets.push_back(id);
		}
		for (const auto id : scratch.MyTargets)
		{
			const auto& ally = Unit(id);
			const auto found = std::ranges::find(ally.MyBuffs, "talent:podego", [](const auto& buff) -> const auto& { return buff.MyDefinition.MyKey; });
			if (found != ally.MyBuffs.end() && found->MyDefinition.MySource != _unit && found->MyDefinition.MyStrength &&
				found->MyDefinition.MyStrength->MyValue > value && found->MyRemaining > 0.05) continue;
			(void)AddBuff(ally.MyId, {.MyKey = "talent:podego", .MySource = _unit, .MyDuration = 0.6,
				.MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, value}},
				.MyStrength = BuffStrength{.MyValue = value, .MyAttribute = Attribute::ATTACK_PERCENT}});
		}
	}

	void Battle::PodegoStartZone(UnitId _unit, const PodegoKit& _kit)
	{
		const auto& source = Unit(_unit);
		auto& scratch = AcquireAttackScratch(); const OperatorAuraScratchGuard guard{_MyAttackDepth};
		GenericEnemies(_unit, scratch.MyTargets, 1);
		const auto origin = RulePosition(source); const auto forward = RotateOffset({0, 1}, source.MyFacing);
		const auto point = scratch.MyTargets.empty() ? WorldPoint{origin.MyX + forward.MyColumn, origin.MyY + forward.MyRow} : Unit(scratch.MyTargets.front()).MyPosition;
		// 位置和施放时攻击力随定时动作保存，来源退场不取消区域，也不重新取攻击力。
		Schedule({.MyAt = Time(), .MyKind = ScheduledKind::PODEGO_ZONE, .MySource = _unit, .MyInterval = 1, .MyPoint = point,
			.MyAmount = source.MyStats.MyAttack * _kit.MyZoneScale, .MyRemaining = static_cast<unsigned>(std::max(1.0, std::floor(_kit.MyZoneDuration + 0.5)))});
	}

	void Battle::PodegoZonePulse(UnitId _unit, WorldPoint _point, double _damage)
	{
		// 原区域在一次脉冲开始时采集敌人；回调新生成的敌人从下一次脉冲参与。
		auto& scratch = AcquireAttackScratch(); const OperatorAuraScratchGuard guard{_MyAttackDepth};
		const AttackProfile filter{.MyCanHitFlying = true, .MyHitSleep = true};
		for (const auto id : _MyEnemyIds)
			if (TargetableEnemy(Unit(id), filter) && BodyDistance(Unit(id), _point) <= 0.9 + 1e-9) scratch.MyTargets.push_back(id);
		for (const auto id : scratch.MyTargets)
		{
			(void)ApplyStatus(id, CombatStatus::SLUGGISH, 1.05, _unit); (void)ApplyStatus(id, CombatStatus::SILENCE, 1.05, _unit);
			(void)DealDamage(_unit, id, {.MyAmount = _damage, .MyType = DamageType::ARTS, .MyCanDodge = false, .MyTags = DamageTag::DOT | DamageTag::ZONE, .MyIsSkill = true});
		}
	}
}
