#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::UpdateBlockingDefense(UnitId _unit, std::string_view _key, double _defense)
	{
		const auto& unit = Unit(_unit);
		if (unit.MyOperatorHooksReleased || !std::islessgreater(_defense, 0)) return;
		const bool wanted = unit.MyAlive && !unit.MyBlocking.empty();
		const bool present = std::ranges::any_of(unit.MyBuffs, [&](const auto& _buff) { return _buff.MyDefinition.MyKey == _key; });
		if (wanted && !present) (void)AddBuff(_unit, {.MyKey = std::string(_key), .MyModifiers = std::vector<AttributeChange>{
			{.MyAttribute = Attribute::DEFENSE_PERCENT, .MyValue = _defense}}});
		else if (!wanted && present) (void)RemoveBuff(_unit, _key);
	}

	void Battle::BubbleTick(UnitId _unit)
	{
		UpdateBlockingDefense(_unit, "bubble:guard", std::get<BubbleKit>(*Unit(_unit).MyDefinition.MyOperatorKit).MyBlockingDefense);
	}

	void Battle::BubbleDamaged(ContentEvent& _event, const BubbleKit& _kit)
	{
		const auto& unit = Unit(_event.MyTarget);
		const auto& damage = _event.MyDamage;
		if (!unit.MyAlive || unit.MyOperatorHooksReleased || !_event.MySource || Unit(_event.MySource).MySide != UnitSide::ENEMY || damage.MySourceless ||
			_event.MyElement || HasTag(damage.MyTags, DamageTag::HP_LOSS) || HasTag(damage.MyTags, DamageTag::COUNTER) || HasTag(damage.MyTags, DamageTag::REFLECT)) return;
		if (_kit.MyCounter && unit.MySkill.MyActive && Unit(_event.MySource).MyAlive)
			(void)DealDamage(unit.MyId, _event.MySource, {.MyAmount = unit.MyStats.MyDefense * _kit.MyCounterScale, .MyType = DamageType::PHYSICAL,
				.MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::COUNTER), .MyIsSkill = true});
		if (std::isless(_kit.MyAttackDebuff, 0) && Unit(_event.MySource).MyAlive)
			(void)AddBuff(_event.MySource, {.MyKey = "bubble:spike", .MySource = unit.MyId, .MyDuration = _kit.MyDebuffDuration,
				.MyRefresh = BuffRefresh::EXTEND, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _kit.MyAttackDebuff}}});
	}

	void Battle::HumusPeakBuff(UnitId _unit, const HumusKit& _kit)
	{
		const auto& unit = Unit(_unit);
		const auto ratio = unit.MyHealth / unit.MyStats.MyMaxHealth;
		const auto peak = std::ranges::find_if(_kit.MyPeaks, [&](const auto& _peak) { return std::isgreater(ratio, _peak.MyHealthRatio); });
		const auto value = peak == _kit.MyPeaks.end() ? 0 : peak->MyAttack;
		const auto found = std::ranges::find(unit.MyBuffs, "humus:peak", [](const auto& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
		const auto previous = found != unit.MyBuffs.end() && found->MyDefinition.MyStrength ? found->MyDefinition.MyStrength->MyValue : 0;
		if (!std::islessgreater(previous, value)) return;
		if (std::isgreater(value, 0)) (void)AddBuff(_unit, {.MyKey = "humus:peak", .MyModifiers = std::vector<AttributeChange>{
			{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = value}}, .MyStrength = BuffStrength{.MyValue = value, .MyAttribute = Attribute::ATTACK_PERCENT}});
		else (void)RemoveBuff(_unit, "humus:peak");
	}

	void Battle::RockrTick(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<RockrKit>(*unit.MyDefinition.MyOperatorKit);
		if (!std::isgreater(kit.MyStackInterval, 0) || !kit.MyMaxStacks || !std::islessgreater(kit.MyStackAttack, 0)) return;
		const auto stacks = static_cast<unsigned>(std::clamp(std::floor((Time() - unit.MyDeployedAt + 1e-9) / kit.MyStackInterval), 0.0, static_cast<double>(kit.MyMaxStacks)));
		const auto found = std::ranges::find(unit.MyBuffs, "rockr:rock", [](const auto& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
		if ((found == unit.MyBuffs.end() ? 0U : found->MyDefinition.MyStacks) == stacks) return;
		if (stacks) (void)AddBuff(_unit, {.MyKey = "rockr:rock", .MyStacks = stacks, .MyMaxStacks = kit.MyMaxStacks,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyStackAttack}}});
		else (void)RemoveBuff(_unit, "rockr:rock");
	}

	void Battle::RockrSkill(UnitId _unit, const RockrKit& _kit, const ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			unit.MyRockrOverAt = Time() + unit.MyDefinition.MySkill.MyDuration / 2;
			unit.MyRockrOver = false;
			unit.MyRockrLock = 0;
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK && !unit.MyRockrOver && !std::isless(Time() + 1e-9, unit.MyRockrOverAt))
		{
			unit.MyRockrOver = true;
			(void)AddBuff(_unit, {.MyKey = "rockr:overload", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _kit.MyOverloadAttack}}});
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			const auto over = unit.MyRockrOver ? Time() - unit.MyRockrOverAt : 0;
			(void)RemoveBuff(_unit, "rockr:overload");
			unit.MyRockrOver = false;
			unit.MyRockrLock = 0;
			if (std::isgreater(over, 0) && unit.MyAlive && _event.MySkillReason != SkillReason::DEATH) (void)ApplyStatus(_unit, CombatStatus::STUN, over, _unit);
		}
	}
}
