#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct ChargeScratchGuard
		{
			std::size_t& MyDepth;

			~ChargeScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::LowHealthHealBonus(ContentEvent& _event, double _threshold, double _scale, bool _inclusive, bool _self, bool _skipRegen)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_HEAL || !_event.MyTarget || (!_self && _event.MyTarget == _event.MySource) || (_skipRegen && (_event.MyHealOptions.MyRegen || Unit(_event.MyTarget).MySide != UnitSide::ALLY)) || !std::isgreater(_threshold, 0) || !std::isgreater(_scale, 0)) return;
		const auto& target = Unit(_event.MyTarget); const auto ratio = target.MyHealth / target.MyStats.MyMaxHealth;
		if (_inclusive ? std::islessequal(ratio, _threshold + 1e-9) : std::isless(ratio, _threshold)) _event.MyAmount *= _scale;
	}

	void BattleCore::OperatorAlliesInRange(UnitId _unit, std::vector<UnitId>& _targets) const
	{
		_targets.clear(); _targets.reserve(_MyAllyIds.size());
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (ally.MyAlive && !ally.MyHidden && ally.MyKind != UnitKind::DEVICE && InRuleRange(_unit, id) &&
				(id == _unit || !ally.MyStatuses.Has(CombatStatus::ISOLATED))) _targets.push_back(id);
		}
	}

	void BattleCore::BillroSkill(UnitId _unit, const BillroKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			unit.MyBillroCharged = unit.MyDefinition.MySkill.MyMaxCharges > 1 && unit.MySkill.MyCharges + 1 >= unit.MyDefinition.MySkill.MyMaxCharges;
			if (_kit.MySkill == BillroSkillKind::GUARD && unit.MyBillroCharged)
				(void)AddBuff(_unit, {.MyKey = "billro:s1guard", .MyModifiers = std::vector<AttributeChange>{
					{.MyAttribute = Attribute::DEFENSE_PERCENT, .MyValue = unit.MyDefinition.MyProfession.MyGuardDefense},
					{.MyAttribute = Attribute::RESISTANCE_FLAT, .MyValue = unit.MyDefinition.MyProfession.MyGuardResistance}}});
			else if (_kit.MySkill == BillroSkillKind::CHAINS && unit.MyBillroCharged) SetOperatorAttribute(_unit, "billro:charged", true, Attribute::ATTACK_PERCENT, _kit.MyAttack);
			else if (_kit.MySkill == BillroSkillKind::DEVOUR)
			{
				unit.MyBillroRamp = 0; unit.MyBillroMarked.clear();
				SetOperatorAttribute(_unit, "billro:s3atk", true, Attribute::ATTACK_PERCENT, 0);
			}
			(void)Heal(_unit, _unit, unit.MyStats.MyMaxHealth * (unit.MyBillroCharged ? _kit.MyChargedHeal : _kit.MyHeal), {.MySelf = true});
			if ((_kit.MySkill != BillroSkillKind::GUARD || !unit.MyBillroCharged) && (std::islessgreater(_kit.MyKeepDefense, 0) || std::islessgreater(_kit.MyKeepResistance, 0)))
				(void)AddBuff(_unit, {.MyKey = "billro:keep", .MyModifiers = std::vector<AttributeChange>{
					{.MyAttribute = Attribute::DEFENSE_PERCENT, .MyValue = _kit.MyKeepDefense}, {.MyAttribute = Attribute::RESISTANCE_FLAT, .MyValue = _kit.MyKeepResistance}}});
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK && _kit.MySkill == BillroSkillKind::DEVOUR)
		{
			unit.MyBillroRamp += _event.MyDelta;
			const auto value = _kit.MyAttack * std::min(20.0, std::floor(unit.MyBillroRamp + 1e-9)) / 20;
			const auto found = std::ranges::find(unit.MyBuffs, "billro:s3atk", [](const CombatBuff& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
			if (found != unit.MyBuffs.end() && found->MyDefinition.MyModifiers && !found->MyDefinition.MyModifiers->empty() && std::islessgreater(found->MyDefinition.MyModifiers->front().MyValue, value))
			{ found->MyDefinition.MyModifiers->front().MyValue = value; Recalculate(unit); }
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			unit.MyBillroCharged = false;
			for (const auto key : {"billro:s1guard", "billro:charged", "billro:s3atk"}) (void)RemoveBuff(_unit, key);
			for (const auto id : unit.MyBillroMarked)
				if (std::ranges::any_of(Unit(id).MyBuffs, [&](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == "billro:mark" && _buff.MyDefinition.MySource == _unit; })) (void)RemoveBuff(id, "billro:mark");
			unit.MyBillroMarked.clear();
		}
		else if (_event.MyKind == ContentEventKind::SKILL_END) (void)RemoveBuff(_unit, "billro:keep");
		if (!_event.MyTarget || Unit(_event.MyTarget).MySide != UnitSide::ENEMY) return;
		if (_event.MyKind == ContentEventKind::DAMAGED && _kit.MySkill == BillroSkillKind::CHAINS && _event.MyDamage.MyIsAttack && unit.MySkill.MyActive && Unit(_event.MyTarget).MyAlive)
			(void)ApplyStatus(_event.MyTarget, unit.MyBillroCharged ? CombatStatus::BIND : CombatStatus::SLUGGISH, unit.MyBillroCharged ? _kit.MyBind : _kit.MySluggish, _unit);
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE) return;
		if (std::isgreater(_kit.MyEnemyScale, 0))
		{
			const auto count = std::ranges::count_if(_MyEnemyIds, [&](UnitId _id) { return Unit(_id).MyAlive && !Unit(_id).MyHidden && InRuleRange(_unit, _id); });
			_event.MyDamage.MyMultiplier *= 1 + _kit.MyEnemyScale * std::min(_kit.MyEnemyCap, static_cast<double>(count));
		}
		if (_kit.MySkill != BillroSkillKind::DEVOUR) return;
		auto& enemy = _MyUnits[Index(_event.MyTarget)];
		auto mark = std::ranges::find(enemy.MyBuffs, "billro:mark", [](const CombatBuff& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
		if (_event.MyDamage.MyIsAttack && unit.MySkill.MyActive && unit.MyBillroCharged)
		{
			if (mark != enemy.MyBuffs.end()) (void)AddBuff(enemy.MyId, {.MyKey = "billro:mark", .MyDuration = mark->MyRemaining, .MyMaxStacks = 5, .MyRefresh = BuffRefresh::STACK});
			else if (AddBuff(enemy.MyId, {.MyKey = "billro:mark", .MySource = _unit, .MyDuration = std::max(0.1, unit.MySkill.MyTimeLeft + 0.1), .MyMaxStacks = 5, .MyRefresh = BuffRefresh::STACK})) unit.MyBillroMarked.push_back(enemy.MyId);
			mark = std::ranges::find(enemy.MyBuffs, "billro:mark", [](const CombatBuff& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
		}
		if (mark != enemy.MyBuffs.end() && mark->MyDefinition.MySource == _unit) _event.MyDamage.MyMultiplier *= 1 + _kit.MyMarkScale * mark->MyDefinition.MyStacks;
	}

	void BattleCore::BldskEarly(ContentEvent& _event)
	{
		const auto id = _event.MyKind == ContentEventKind::SP_GAIN ? _event.MyUnit : _event.MyKind == ContentEventKind::BEFORE_HEAL ? _event.MySource : UnitId{};
		if (!id) return;
		auto& unit = _MyUnits[Index(id)];
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<BldskKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || !kit->MyMergedHeal || unit.MyOperatorHooksReleased) return;
		if (_event.MyKind == ContentEventKind::SP_GAIN && _event.MySpReason == SpReason::ATTACK && unit.MyBandageSkipSp)
		{ unit.MyBandageSkipSp = false; _event.MyAmount = 0; }
		if (_event.MyKind == ContentEventKind::BEFORE_HEAL && !_event.MyHealOptions.MyRegen && unit.MyBandageBonus && _event.MyTarget == unit.MyBandageBonus)
		{
			unit.MyBandageBonus = 0;
			const auto& target = Unit(_event.MyTarget);
			_event.MyAmount += target.MyStats.MyMaxHealth * kit->MyBandageHeal * unit.MyStats.MyHealingDealtMultiplier * target.MyStats.MyHealingTakenMultiplier;
		}
	}

	void BattleCore::BldskBeforeAttack(CombatUnit& _unit, const BldskKit& _kit, std::span<const UnitId> _targets)
	{
		if (_kit.MyMergedHeal) _unit.MyBandageSkipSp = false;
		if (_targets.empty() || !_unit.MySkill.MyCharges || _unit.MyStatuses.Has(CombatStatus::SILENCE)) return;
		const auto& target = Unit(_targets.front());
		if (target.MySide != UnitSide::ALLY || !std::isless(target.MyHealth / target.MyStats.MyMaxHealth, 0.5)) return;
		if (_kit.MyMergedHeal) _unit.MyBandageBonus = target.MyId;
		if (ActivateSkill(_unit.MyId, false, SkillReason::TRIGGER))
		{
			if (_kit.MyMergedHeal) _unit.MyBandageSkipSp = true;
			else _unit.MyBandageTarget = target.MyId;
		}
		else if (_kit.MyMergedHeal) _unit.MyBandageBonus = 0;
	}

	void BattleCore::BldskPlasma(UnitId _unit, UnitId _target, const BldskKit& _kit)
	{
		(void)AddBuff(_target, {.MyKey = "bldsk:plasma", .MySource = _unit, .MyDuration = _kit.MyDuration + 1e-6,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _kit.MyAttack}}, .MyInterval = _kit.MyInterval,
			.MyTickEffects = {{.MyKind = BuffEffectKind::HEALTH_LOSS, .MyAmount = _kit.MyHealthLoss, .MySourceless = true, .MyTargetMaxHealthScale = true}}});
	}

	void BattleCore::BldskSkill(UnitId _unit, const BldskKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::BEFORE_HEAL) LowHealthHealBonus(_event, _kit.MyHealthRatio, _kit.MyMergedHeal && !std::isgreater(_kit.MyHealScale, 1) ? 0 : _kit.MyHealScale, true, _kit.MyMergedHeal, _kit.MyMergedHeal);
		if (!_kit.MyBandage && _event.MyKind == ContentEventKind::SKILL_START)
		{
			BldskPlasma(_unit, _unit, _kit);
			auto& scratch = AcquireAttackScratch(); const ChargeScratchGuard guard{.MyDepth = _MyAttackDepth};
			OperatorAlliesInRange(_unit, scratch.MyTargets); std::erase(scratch.MyTargets, _unit);
			if (!scratch.MyTargets.empty()) BldskPlasma(_unit, scratch.MyTargets[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MyTargets.size()))], _kit);
		}
		if (!_kit.MyBandage || _kit.MyMergedHeal) return;
		if (_event.MyKind == ContentEventKind::ATTACK && unit.MyBandageTarget)
		{
			const auto target = std::exchange(unit.MyBandageTarget, UnitId{}); unit.MyBandageSkipSp = true;
			if (Unit(target).MyAlive) (void)Heal(_unit, target, Unit(target).MyStats.MyMaxHealth * _kit.MyBandageHeal);
		}
		else if (_event.MyKind == ContentEventKind::SP_GAIN && _event.MySpReason == SpReason::ATTACK && unit.MyBandageSkipSp)
		{ unit.MyBandageSkipSp = false; _event.MyAmount = 0; }
	}

	void BattleCore::BldskDeath(const ContentEvent& _event)
	{
		if (!_event.MyUnit || Unit(_event.MyUnit).MySide != UnitSide::ENEMY) return;
		for (std::size_t i = 0, count = _MyBldsks.size(); i < count; ++i)
		{
			const auto& unit = Unit(_MyBldsks[i]);
			if (!unit.MyAlive || unit.MyOperatorHooksReleased || !InRuleRange(unit.MyId, _event.MyUnit)) continue;
			const auto& kit = std::get<BldskKit>(*unit.MyDefinition.MyOperatorKit);
			if (kit.MyMergedHeal ? _event.MyKind != ContentEventKind::BEFORE_KILL : (_event.MyKind != ContentEventKind::DEATH || _event.MyRemovalReason != RemovalReason::KILLED)) continue;
			(void)GainSp(unit.MyId, kit.MySelfSp);
			auto& scratch = AcquireAttackScratch(); const ChargeScratchGuard guard{.MyDepth = _MyAttackDepth};
			OperatorAlliesInRange(unit.MyId, scratch.MyTargets);
			std::erase_if(scratch.MyTargets, [&](UnitId _id)
			{
				const auto& ally = Unit(_id); const auto kind = ally.MyDefinition.MySkill.MyKind;
				return _id == unit.MyId || kind == SkillKind::NONE || kind == SkillKind::PASSIVE || (!kit.MyMergedHeal && ally.MySkill.MyActive && IsTimedSkill(kind));
			});
			if (!scratch.MyTargets.empty()) (void)GainSp(scratch.MyTargets[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MyTargets.size()))], kit.MyAllySp);
		}
	}
}
