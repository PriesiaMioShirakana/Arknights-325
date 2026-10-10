#include <stronghold/adapters/reference_ally.hpp>

namespace Stronghold
{
	void AppendPermanentTalents(CombatDefinition& _definition, std::span<const GenericTalentRecord> _talents)
	{
		_definition.MyInitialBuffs.reserve(_definition.MyInitialBuffs.size() + _talents.size());
		for (const auto& talent : _talents)
			_definition.MyInitialBuffs.push_back({.MyKey = std::string(talent.MyKey),
				.MyModifiers = std::vector<AttributeChange>(talent.MyModifiers.begin(), talent.MyModifiers.end()), .MyPersistent = true, .MyAllowDead = true});
	}

	CombatDefinition AllyRecord::MakeGenericDefinition(bool _talents) const
	{ return MakeDefinitionWithSkill(MyGenericSkill, _talents); }

	CombatDefinition AllyRecord::MakeDefinitionWithSkill(const GenericSkillRecord* _skill, bool _talents) const
	{
		auto definition = MakeDefinition(MyBaseAttack);
		if (MySkill && _skill)
		{
			const auto& raw = *MySkill; const auto& spec = *_skill;
			auto& skill = definition.MySkill;
			skill.MyKind = spec.MyKind;
			skill.MySpType = spec.MyActivateOnDeploy ? SpType::NONE : raw.MySpType;
			skill.MySpCost = spec.MyActivateOnDeploy ? 0 : raw.MySpCost;
			skill.MyInitialSp = raw.MyInitialSp;
			skill.MyMaxCharges = raw.MyMaxCharges;
			skill.MyDuration = spec.MyDuration.value_or(std::max(0.0, raw.MyDuration));
			skill.MyAmmo = spec.MyAmmo;
			skill.MyManual = raw.MyType == "MANUAL";
			skill.MyActivateOnDeploy = spec.MyActivateOnDeploy;
			skill.MyHealSkill = MyBaseAttack.MyHealing;
			skill.MyTriggerAllies = !spec.MyTrigger && raw.MyTriggerAllies;
			constexpr std::array<std::pair<std::string_view, SkillTrigger>, 10> triggers{{
				{"SP_FULL", SkillTrigger::SP_FULL}, {"SEARCH", SkillTrigger::SEARCH}, {"CUSTOM_RANGE", SkillTrigger::CUSTOM_RANGE},
				{"SKILL_RANGE", SkillTrigger::SKILL_RANGE}, {"ACTIVE_RANGE", SkillTrigger::ACTIVE_RANGE}, {"GLOBAL", SkillTrigger::GLOBAL},
				{"GDGLOW_SKILL_2", SkillTrigger::GLOBAL}, {"TAKE_DAMAGE", SkillTrigger::TAKE_DAMAGE}, {"NEVER", SkillTrigger::NEVER}, {"MANUAL", SkillTrigger::NEVER}}};
			if (spec.MyTrigger) skill.MyTrigger = *spec.MyTrigger;
			else for (const auto& [name, trigger] : triggers) if (raw.MyTrigger == name) { skill.MyTrigger = trigger; break; }
			skill.MyTriggerRange.assign(raw.MyTriggerRange.begin(), raw.MyTriggerRange.end());
			skill.MyRange.assign(spec.MyRange.begin(), spec.MyRange.end());
			skill.MyRangeExtend = spec.MyRangeExtend;
			skill.MyModifiers.assign(spec.MyModifiers.begin(), spec.MyModifiers.end());
			if (spec.MyHasAttack || spec.MyMaxTargets || spec.MyNoAttack)
			{
				auto attack = MyBaseAttack;
				if (spec.MyDamageType) { attack.MyDamageType = *spec.MyDamageType; attack.MyHealing = false; }
				if (spec.MyAttackScale) attack.MyAttackScale = *spec.MyAttackScale;
				if (spec.MyHealScale) attack.MyHealScale = *spec.MyHealScale;
				if (spec.MyMaxTargets) attack.MyMaxTargets = std::max(1U, *spec.MyMaxTargets);
				if (spec.MyHits) attack.MyHits = *spec.MyHits;
				if (spec.MySplashRadius) attack.MySplashRadius = *spec.MySplashRadius;
				attack.MyDisabled |= spec.MyNoAttack;
				attack.MyGenericHit = spec.MyOnHit;
				skill.MyAttack = std::move(attack);
			}
			definition.MyGenericSkill = &spec.MyEffects;
		}
		if (_talents) AppendPermanentTalents(definition, MyGenericTalents);
		return definition;
	}
}
