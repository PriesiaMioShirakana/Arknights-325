#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		constexpr std::array<std::string_view, 7> Students{"char_196_sunbr", "char_115_headbr", "char_194_leto", "char_405_absin", "char_195_glassb", "char_197_poca", "char_1051_headb2"};
		constexpr std::array<RangeOffset, 2> FrontPair{{{.MyRow = 0}, {.MyColumn = 1}}};

		const DiyOperatorKit* TeamRules(const CombatUnit& _unit)
		{
			return _unit.MyDefinition.MyOperatorKit ? std::get_if<DiyOperatorKit>(_unit.MyDefinition.MyOperatorKit) : nullptr;
		}

		bool TeamUp(const CombatUnit& _unit)
		{
			return _unit.MyAlive && !_unit.MyHidden && !_unit.MyOperatorHooksReleased;
		}

		bool Minos(const CombatUnit& _unit)
		{
			return _unit.MyKind == UnitKind::OPERATOR && _unit.MyDefinition.MyIdentity.MyNationId == "minos";
		}

		struct TeamScratchGuard
		{
			std::size_t& MyDepth;

			~TeamScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::InstallDiyTeam(UnitId _unit, const DiyOperatorKit& _kit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_kit.MyKind == DiyOperatorKind::PALLAS)
		{
			_MyDiyPallases.push_back(_unit); unit.MyDiyBlessKey = "skill:pallas:blessing#" + std::to_string(_unit);
		}
		if (_kit.MyKind != DiyOperatorKind::POCA) return;
		unit.MyDiyStudents.reserve(_MyAllyIds.size()); unit.MyDiyLinks.reserve(_kit.MyLinkTargets);
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (ally.MyOwner != unit.MyOwner || ally.MyKind != UnitKind::OPERATOR || !std::ranges::contains(Students, ally.MyDefinition.MyIdentity.MyCharacterId)) continue;
			unit.MyDiyStudents.push_back(id);
			if (std::islessgreater(_kit.MyStudentAttack, 0)) (void)AddBuff(id, {.MyKey = "talent:poca:students",
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _kit.MyStudentAttack}}, .MyPersistent = true, .MyAllowDead = true});
		}
		if (std::isgreater(_kit.MyStudentPerSkill, 0))
			Schedule({.MyAt = Time() + 0.3, .MyKind = ScheduledKind::DIY_STUDENT_PULSE, .MySource = _unit, .MyInterval = 0.3});
	}

	void BattleCore::DiyStudentUpdate(UnitId _unit)
	{
		const auto& unit = Unit(_unit); const auto& kit = *TeamRules(unit);
		if (unit.MyOperatorHooksReleased || !std::isgreater(kit.MyStudentPerSkill, 0)) return;
		const auto count = std::ranges::count_if(unit.MyDiyStudents, [&](UnitId _id)
		{
			const auto& ally = Unit(_id);
			return TeamUp(ally) && ally.MySkill.MyActive && ally.MyDefinition.MySkill.MyKind != SkillKind::PASSIVE;
		});
		const auto value = std::min(kit.MyStudentSkillCap, kit.MyStudentPerSkill * static_cast<double>(count));
		for (const auto id : unit.MyDiyStudents)
		{
			const auto& buffs = Unit(id).MyBuffs;
			const auto current = std::ranges::find(buffs, std::string_view("talent:poca:studentSkills"), [](const auto& _buff) { return std::string_view(_buff.MyDefinition.MyKey); });
			if (!std::isgreater(value, 0)) { if (current != buffs.end()) (void)RemoveBuff(id, "talent:poca:studentSkills"); continue; }
			if (current != buffs.end() && current->MyDefinition.MyModifiers && !current->MyDefinition.MyModifiers->empty() &&
				!std::islessgreater(current->MyDefinition.MyModifiers->front().MyValue, value)) continue;
			(void)AddBuff(id, {.MyKey = "talent:poca:studentSkills", .MySource = _unit,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = value}}, .MyPersistent = true, .MyAllowDead = true});
		}
	}

	void BattleCore::DiyBlessUpdate(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto& kit = *TeamRules(unit);
		UnitId holder = 0;
		if (kit.MySkill == 3 && unit.MySkill.MyActive && TeamUp(unit))
		{
			holder = _unit;
			const auto point = RulePosition(unit); const auto forward = RotateOffset({.MyColumn = 1}, unit.MyFacing);
			const auto row = static_cast<int>(std::floor(point.MyY + 0.5)) + forward.MyRow, column = static_cast<int>(std::floor(point.MyX + 0.5)) + forward.MyColumn;
			if (row >= 0 && row < FieldRows && column >= 0 && column < FieldColumns && (!_MyGrid || _MyGrid->Tile(row, column).MyLow))
				for (const auto id : _MyAllyIds)
				{
					const auto& ally = Unit(id); const auto at = RulePosition(ally);
					if (id != _unit && ally.MyAlive && !ally.MyHidden && ally.MyKind == UnitKind::OPERATOR && ally.MyGround && !ally.MyStatuses.Has(CombatStatus::ISOLATED) &&
						static_cast<int>(std::floor(at.MyY + 0.5)) == row && static_cast<int>(std::floor(at.MyX + 0.5)) == column) { holder = id; break; }
				}
		}
		if (holder == unit.MyDiyBlessHolder) return;
		if (unit.MyDiyBlessHolder) (void)RemoveBuff(unit.MyDiyBlessHolder, unit.MyDiyBlessKey);
		unit.MyDiyBlessHolder = holder;
		if (holder) (void)AddBuff(holder, {.MyKey = unit.MyDiyBlessKey, .MySource = _unit,
			.MyModifiers = std::vector<AttributeChange>(kit.MyBlessModifiers.begin(), kit.MyBlessModifiers.end())});
	}

	void BattleCore::DiyPeakUpdate()
	{
		if (_MyDiyPallases.empty()) return;
		for (std::size_t i = 0, count = _MyAllyIds.size(); i < count; ++i)
		{
			const auto id = _MyAllyIds[i];
			const auto& ally = Unit(id);
			if (!ally.MyAlive || ally.MyKind == UnitKind::DEVICE) continue;
			double value = 0;
			for (const auto sourceId : _MyDiyPallases)
			{
				const auto& source = Unit(sourceId); const auto* kit = TeamRules(source);
				if (!kit || kit->MyKind != DiyOperatorKind::PALLAS || !TeamUp(source) || (id != sourceId && ally.MyStatuses.Has(CombatStatus::ISOLATED))) continue;
				const auto ratio = ally.MyHealth / ally.MyStats.MyMaxHealth;
				if (Minos(ally) && std::isgreater(ratio, kit->MyPeakHealthRatio)) value = std::max(value, kit->MyPeakAttack);
				if (source.MySkill.MyActive && source.MyDiyBlessHolder == id && std::isgreater(ratio, kit->MyBlessHealthRatio)) value = std::max(value, kit->MyBlessAttack);
			}
			const auto current = std::ranges::find(ally.MyBuffs, std::string_view("talent:pallas:peak"), [](const auto& _buff) { return std::string_view(_buff.MyDefinition.MyKey); });
			const auto previous = current != ally.MyBuffs.end() && current->MyDefinition.MyStrength ? current->MyDefinition.MyStrength->MyValue : 0;
			if (!std::islessgreater(value, previous)) continue;
			if (std::isgreater(value, 0)) (void)AddBuff(id, {.MyKey = "talent:pallas:peak",
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = value}},
				.MyStrength = BuffStrength{.MyValue = value, .MyAttribute = Attribute::ATTACK_PERCENT}});
			else (void)RemoveBuff(id, "talent:pallas:peak");
		}
	}

	void BattleCore::DiyLinkClear(UnitId _unit, UnitId _target)
	{
		if (Unit(_target).MyStatuses.MyBindSource == _unit) (void)RemoveStatus(_target, CombatStatus::BIND);
	}

	void BattleCore::DiyLinkStrike(UnitId _unit, std::uint64_t _activation)
	{
		auto& unit = _MyUnits[Index(_unit)]; const auto& kit = *TeamRules(unit);
		if (unit.MyOperatorHooksReleased || !unit.MyAlive || !unit.MySkill.MyActive || unit.MySkill.MyActivations != _activation || !unit.MyDiyLinkStrikes) return;
		auto& scratch = AcquireAttackScratch(); const TeamScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : unit.MyDiyLinks) if (Unit(id).MyAlive && !Unit(id).MyHidden) scratch.MyTargets.push_back(id);
		if (!scratch.MyTargets.empty() && !unit.MyHidden && !unit.MyStatuses.Has(CombatStatus::STUN))
		{
			--unit.MyDiyLinkStrikes; (void)ForceAttack(_unit, scratch.MyTargets);
		}
		if (unit.MyAlive && unit.MySkill.MyActive && unit.MySkill.MyActivations == _activation && unit.MyDiyLinkStrikes)
			Schedule({.MyAt = Time() + kit.MyLinkInterval, .MyKind = ScheduledKind::DIY_LINK_STRIKE, .MySource = _unit, .MyVersion = _activation});
	}

	void BattleCore::DiyTeamObserve(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::TICK) DiyPeakUpdate();
		if (_event.MyKind == ContentEventKind::SKILL_START && _event.MyUnit && _event.MySkillReason != SkillReason::PASSIVE)
			for (std::size_t i = 0, count = _MyDiyOperators.size(); i < count; ++i)
			{
				const auto source = _MyDiyOperators[i];
				if (std::ranges::contains(Unit(source).MyDiyStudents, _event.MyUnit)) DiyStudentUpdate(source);
			}
		const auto id = _event.MySource ? _event.MySource : _event.MyUnit;
		if (!id) return;
		auto& unit = _MyUnits[Index(id)]; const auto* kit = TeamRules(unit);
		if (!kit || unit.MyOperatorHooksReleased) return;
		if (kit->MyKind == DiyOperatorKind::POCA)
		{
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
			{
				auto& damage = _event.MyDamage;
				if (std::isgreaterequal(Unit(_event.MyTarget).MyStats.MyMass, kit->MyHeavyMass))
				{
					damage.MyDefenseIgnorePercent += kit->MyHeavyPenetration;
					if (damage.MyIsAttack && !damage.MyIsSplash) damage.MyAmount *= kit->MyHeavyScale;
				}
				if (damage.MyIsAttack && std::isgreater(kit->MyDistanceScale, 0))
					damage.MyMultiplier *= 1 + kit->MyDistanceScale * std::clamp((Distance(RulePosition(unit), Unit(_event.MyTarget).MyPosition) - kit->MyDistanceMinimum) / std::max(1e-6, kit->MyDistanceMaximum - kit->MyDistanceMinimum), 0.0, 1.0);
			}
			if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyTarget && _event.MyDamage.MyIsAttack && std::isgreater(kit->MyHeavyExtra, 0) &&
				Unit(_event.MyTarget).MySide == UnitSide::ENEMY && std::isgreater(Unit(_event.MyTarget).MyHealth, 0) && std::isgreaterequal(Unit(_event.MyTarget).MyStats.MyMass, kit->MyHeavyMass))
				(void)DealDamage(id, _event.MyTarget, {.MyAmount = unit.MyStats.MyAttack * kit->MyHeavyExtra, .MyTags = static_cast<DamageTags>(DamageTag::TALENT)});
			if (kit->MySkill != 3 || _event.MyUnit != id) return;
			if (_event.MyKind == ContentEventKind::SKILL_START)
			{
				auto& scratch = AcquireAttackScratch(); const TeamScratchGuard guard{.MyDepth = _MyAttackDepth};
				const AttackProfile filter{.MyCanHitFlying = true, .MyPriority = TargetPriority::HEAVIEST};
				for (const auto enemy : _MyEnemyIds) if (TargetableEnemy(Unit(enemy), filter) && InRuleRange(id, enemy)) scratch.MyTargets.push_back(enemy);
				SortOperatorTargets(id, scratch.MyTargets, kit->MyLinkTargets, &filter);
				unit.MyDiyLinks.assign(scratch.MyTargets.begin(), scratch.MyTargets.end()); unit.MyDiyLinkStrikes = kit->MyLinkStrikes;
				for (const auto enemy : scratch.MyTargets) (void)ApplyStatus(enemy, CombatStatus::BIND, unit.MySkill.MyTimeLeft, id);
				Schedule({.MyAt = Time(), .MyKind = ScheduledKind::DIY_LINK_STRIKE, .MySource = id, .MyVersion = unit.MySkill.MyActivations});
			}
			else if (_event.MyKind == ContentEventKind::SKILL_TICK)
			{
				if (unit.MyStatuses.Has(CombatStatus::STUN) || unit.MyStatuses.Has(CombatStatus::FREEZE) || unit.MyStatuses.Has(CombatStatus::SILENCE)) { EndSkill(id, SkillReason::ABNORMAL); return; }
				std::erase_if(unit.MyDiyLinks, [&](UnitId _enemy)
				{
					if (Unit(_enemy).MyAlive && !Unit(_enemy).MyHidden) return false;
					DiyLinkClear(id, _enemy); return true;
				});
				if (unit.MyDiyLinks.empty()) EndSkill(id, SkillReason::NO_TARGET);
			}
			else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
			{
				auto& scratch = AcquireAttackScratch(); const TeamScratchGuard guard{.MyDepth = _MyAttackDepth};
				scratch.MyTargets.assign(unit.MyDiyLinks.begin(), unit.MyDiyLinks.end()); unit.MyDiyLinks.clear(); unit.MyDiyLinkStrikes = 0;
				for (const auto enemy : scratch.MyTargets) DiyLinkClear(id, enemy);
			}
		}
		else if (kit->MyKind == DiyOperatorKind::PALLAS)
		{
			if (_event.MyUnit == id && (_event.MyKind == ContentEventKind::SKILL_START || _event.MyKind == ContentEventKind::SKILL_TICK || _event.MyKind == ContentEventKind::SKILL_ENDING)) DiyBlessUpdate(id);
			if (_event.MyKind != ContentEventKind::DAMAGED || !_event.MyTarget || Unit(_event.MyTarget).MySide != UnitSide::ENEMY || _event.MyDamage.MyType == DamageType::ELEMENTAL || !TeamUp(unit)) return;
			auto& scratch = AcquireAttackScratch(); const TeamScratchGuard guard{.MyDepth = _MyAttackDepth};
			for (const auto ally : _MyAllyIds)
			{
				const auto& target = Unit(ally);
				if (target.MyAlive && !target.MyHidden && target.MyKind != UnitKind::DEVICE && (ally == id || !target.MyStatuses.Has(CombatStatus::ISOLATED)) && OperatorInGrid(id, ally, FrontPair)) scratch.MyTargets.push_back(ally);
			}
			if (std::isgreater(kit->MyTalentHeal, 0)) for (const auto ally : scratch.MyTargets) (void)Heal(id, ally, kit->MyTalentHeal, {.MySelf = ally == id, .MyIgnoreHealFree = true});
			if (std::isgreater(kit->MyNationHeal, 0)) for (std::size_t i = 0, count = _MyAllyIds.size(); i < count; ++i)
			{
				const auto ally = _MyAllyIds[i]; const auto& target = Unit(ally);
				if (target.MyAlive && !target.MyHidden && Minos(target) && (ally == id || !target.MyStatuses.Has(CombatStatus::ISOLATED))) (void)Heal(id, ally, kit->MyNationHeal, {.MySelf = ally == id, .MyIgnoreHealFree = true});
			}
		}
	}
}
