#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::NymphSoul(UnitId _unit, UnitId _target, double _scale)
	{
		if (!std::isgreater(_scale, 0)) return;
		auto& target = _MyUnits[Index(_target)];
		const auto& kit = std::get<NymphKit>(*Unit(_unit).MyDefinition.MyOperatorKit);
		const auto burst = std::ranges::find(target.MyBuffs, "apoptosisBurst", [](const auto& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
		if (burst == target.MyBuffs.end()) return;
		const auto key = std::string("nymph:soul:") + std::to_string(_unit);
		const auto soul = std::ranges::find(target.MyBuffs, key, [](const auto& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
		if (soul != target.MyBuffs.end())
		{
			if (soul->MyDefinition.MyStrength) soul->MyDefinition.MyStrength->MyValue = std::max(soul->MyDefinition.MyStrength->MyValue, _scale);
			if (kit.MyHiddenVariant) soul->MyRemaining = std::max(soul->MyRemaining, burst->MyRemaining);
			return;
		}
		(void)AddBuff(_target, {.MyKey = key, .MySource = _unit, .MyDuration = std::max(0.1, burst->MyRemaining), .MyInterval = 1,
			.MyStrength = BuffStrength{.MyValue = _scale}, .MyNotifyTick = true});
	}

	void Battle::NymphObserve(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::BUFF_TICK && _event.MyUnit)
		{
			const auto& target = Unit(_event.MyUnit);
			const auto buff = std::ranges::find(target.MyBuffs, _event.MyBuff, &CombatBuff::MyId);
			if (buff == target.MyBuffs.end() || !buff->MyDefinition.MyKey.starts_with("nymph:soul:") || !buff->MyDefinition.MyStrength || !buff->MyDefinition.MySource) return;
			const auto& source = Unit(buff->MyDefinition.MySource);
			const bool hidden = std::get<NymphKit>(*source.MyDefinition.MyOperatorKit).MyHiddenVariant;
			if (!std::ranges::any_of(target.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "apoptosisBurst"; }))
			{ if (!hidden) (void)RemoveBuff(target.MyId, buff->MyId); return; }
			(void)DealDamage(source.MyId, target.MyId, {.MyAmount = source.MyStats.MyAttack * buff->MyDefinition.MyStrength->MyValue,
				.MyType = DamageType::ELEMENTAL, .MyCanDodge = false, .MyTags = static_cast<DamageTags>(hidden ? DamageTag::TALENT : DamageTag::ELEMENTAL)});
		}
		if (_event.MyKind != ContentEventKind::ELEMENT_BURST || _event.MyElement != Element::APOPTOSIS || !_event.MyTarget || Unit(_event.MyTarget).MySide != UnitSide::ENEMY) return;
		for (std::size_t n = 0, count = _MyNymphs.size(); n < count; ++n)
		{
			auto& unit = _MyUnits[Index(_MyNymphs[n])];
			const auto& kit = std::get<NymphKit>(*unit.MyDefinition.MyOperatorKit);
			if (!unit.MyAlive || (kit.MyHiddenVariant && unit.MyHidden) || unit.MyOperatorHooksReleased || (!(kit.MyHiddenVariant && kit.MyFieldWideGrowth) && !InRuleRange(unit.MyId, _event.MyTarget))) continue;
			if (kit.MyHiddenVariant)
			{
				unit.MyNymphKeys = std::min(kit.MyMaxStacks, unit.MyNymphKeys + 1);
				(void)AddBuff(unit.MyId, {.MyKey = "nymph:key", .MyModifiers = std::vector<AttributeChange>{
					{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyGrowth * unit.MyNymphKeys},
					{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = kit.MyFieldWideGrowth && unit.MyNymphKeys >= kit.MyMaxStacks ? kit.MyMaxAttackSpeed : 0}}});
				continue;
			}
			(void)AddBuff(unit.MyId, {.MyKey = "nymph:key", .MyMaxStacks = kit.MyMaxStacks, .MyRefresh = BuffRefresh::STACK,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyGrowth}}});
		}
	}

	void Battle::NymphSkill(UnitId _unit, const NymphKit& _kit, ContentEvent& _event)
	{
		if (_kit.MyHiddenVariant && _event.MyKind == ContentEventKind::DEPLOY) _MyUnits[Index(_unit)].MyNymphKeys = 0;
		if (!_event.MyTarget || Unit(_event.MyTarget).MySide != UnitSide::ENEMY) return;
		const auto& target = Unit(_event.MyTarget); const auto& unit = Unit(_unit);
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && std::isgreater(_kit.MyBurstMultiplier, 1) && target.MyStatuses.Has(CombatStatus::BURST_LOCK))
			_event.MyDamage.MyMultiplier *= _kit.MyBurstMultiplier;
		if (_event.MyKind != ContentEventKind::DAMAGED || _event.MyElement || !target.MyAlive) return;
		if (_kit.MyHiddenVariant)
		{
			if (!_event.MyDamage.MyIsAttack) return;
			NymphSoul(_unit, target.MyId, _event.MyDamage.MyIsSkill ? _kit.MySkillSoulScale : _kit.MySoulScale);
			if (_event.MyDamage.MyIsSkill && target.MyAlive) (void)DealElement(_unit, target.MyId, {.MyElement = Element::APOPTOSIS, .MyAmount = _event.MyAmount * _kit.MyElementRatio});
			return;
		}
		const bool skill = _event.MyDamage.MyIsSkill || (_kit.MySkill == NymphSkillKind::LASH && unit.MySkill.MyActive);
		if (skill && _event.MyDamage.MyIsAttack && std::isgreater(_kit.MyElementRatio, 0) && std::isgreater(_event.MyAmount, 0) && std::isgreater(target.MyHealth, 0))
			(void)DealElement(_unit, target.MyId, {.MyElement = Element::APOPTOSIS, .MyAmount = _event.MyAmount * _kit.MyElementRatio, .MyTags = static_cast<DamageTags>(DamageTag::SKILL)});
		if (_event.MyDamage.MyIsAttack && target.MyAlive) NymphSoul(_unit, target.MyId, _event.MyDamage.MyIsSkill ? std::max(_kit.MySoulScale, _kit.MySkillSoulScale) : _kit.MySoulScale);
		if (_kit.MySkill == NymphSkillKind::LASH && unit.MySkill.MyActive && _event.MyDamage.MyIsAttack && _event.MyDamage.MyType != DamageType::ELEMENTAL && target.MyAlive && std::isgreater(_kit.MyExtraScale, 0) &&
			std::ranges::any_of(target.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "apoptosisBurst"; }))
			(void)DealDamage(_unit, target.MyId, {.MyAmount = unit.MyStats.MyAttack * _kit.MyExtraScale, .MyType = DamageType::ELEMENTAL,
				.MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::ELEMENTAL)});
	}
}
