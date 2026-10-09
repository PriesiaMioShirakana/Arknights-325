#include <stronghold/adapters/reference_ally.hpp>

namespace Stronghold
{
	CombatDefinition AllyRecord::MakeKitDefinition(bool _genericTalents) const
	{
		if (!MyOperatorKit) return MakeGenericDefinition(_genericTalents);
		const auto& kit = *MyOperatorKit;
		auto definition = MakeDefinitionWithSkill(kit.MySkill ? kit.MySkill : MyGenericSkill, false);
		definition.MyOperatorKit = &kit.MyRules;
		const auto base = [&](AttackProfile& attack)
		{
			if (kit.MyBaseSluggish) { attack.MyOnHitStatus = CombatStatus::SLUGGISH; attack.MyOnHitApplication.MyDuration = *kit.MyBaseSluggish; }
			if (kit.MyBasePriority) attack.MyPriority = *kit.MyBasePriority;
		};
		base(definition.MyAttack);
		if (definition.MySkill.MyAttack) base(*definition.MySkill.MyAttack);
		if (kit.MySkillNoHeal) definition.MySkill.MyFlags.set(static_cast<std::size_t>(CombatStatus::NO_HEAL));
		if (kit.MyPriority || kit.MySluggish)
		{
			if (!definition.MySkill.MyAttack) definition.MySkill.MyAttack = definition.MyAttack;
			auto& attack = *definition.MySkill.MyAttack;
			if (kit.MyPriority) attack.MyPriority = *kit.MyPriority;
			if (kit.MySluggish) { attack.MyOnHitStatus = CombatStatus::SLUGGISH; attack.MyOnHitApplication.MyDuration = *kit.MySluggish; }
		}
		if (kit.MyChainNoFalloff && definition.MySkill.MyAttack) definition.MySkill.MyAttack->MyChainFalloff = 0;
		if (kit.MyHealing)
		{
			if (!definition.MySkill.MyAttack) definition.MySkill.MyAttack = definition.MyAttack;
			definition.MySkill.MyAttack->MyHealing = true;
			const auto* sunbr = std::get_if<SunbrKit>(&kit.MyRules);
			if (sunbr) definition.MySkill.MyAttack->MyRanged = false;
			if (!sunbr || !sunbr->MyCooking)
			{
				definition.MySkill.MyHealSkill = true;
				if (sunbr && MySkill && MySkill->MyHasRange)
				{
					definition.MySkill.MyTrigger = SkillTrigger::SKILL_RANGE;
					definition.MySkill.MyTriggerAllies = true;
					definition.MySkill.MyTriggerRange.assign(MySkill->MyRange.begin(), MySkill->MyRange.end());
				}
			}
		}
		if (const auto* yak = std::get_if<YakKit>(&kit.MyRules); yak && yak->MyResistance != 0)
			definition.MyInitialBuffs.push_back({.MyKey = "talent:yak", .MyModifiers = std::vector<AttributeChange>{{Attribute::RESISTANCE_FLAT, yak->MyResistance}},
				.MyPersistent = true, .MyAllowDead = true});
		return definition;
	}
}
