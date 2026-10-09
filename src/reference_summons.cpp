#include <stronghold/adapters/reference_ally.hpp>
#include <stronghold/adapters/reference_summons.hpp>

namespace Stronghold
{
	CombatDefinition MakeYanyouDefinition()
	{
		const auto& token = ReferenceToken("enemy_9012_acloon");
		const auto& body = token.MyUnowned.MyBody;
		const auto& enemy = ReferenceEnemy(token.MyId);
		const auto& talent = body.MyTalents.front().MyBlackboard;
		auto attack = body.MyBaseAttack;
		attack.MyDamageType = DamageType::ARTS; attack.MyDisabled = false; attack.MyHealing = false;
		attack.MyRanged = true; attack.MyCanHitFlying = true; attack.MyProjectileSpeed = 11;
		attack.MyMaxTargets = static_cast<std::size_t>(std::max(1.0, std::floor(talent.Number("1.attack@max_target", 1))));
		auto definition = body.MakeDefinition(attack);
		definition.MyFlying = true; definition.MyStats.MyBlockCount = 0;
		if (!(definition.MyStats.MySpRecovery > 0)) definition.MyStats.MySpRecovery = 1;
		definition.MyYanyou = YanyouKitDefinition{.MyRangeRadius = enemy.MyAttack.MyEnemyRange > 0 ? enemy.MyAttack.MyEnemyRange : 2,
			.MyMoveSpeed = body.MyStats.MyMoveSpeed * 0.5, .MyBurnRatio = talent.Number("2.ep_damage_ratio"),
			.MyFragileMultiplier = talent.Number("2.damage_scale", 1), .MyDeployLimit = static_cast<unsigned>(std::max(1.0, std::floor(token.MyDeployLimit)))};
		const auto skill = std::ranges::find(enemy.MySkills, std::string_view("Skill_2"), &EnemySkillRecord::MyId);
		if (skill != enemy.MySkills.end() && skill->MyCooldown > 0 && skill->MyBlackboard.Number("hit_duration") > 0)
		{
			definition.MySkill = {.MyKind = SkillKind::DURATION, .MySpType = SpType::TIME, .MyTrigger = SkillTrigger::DEFAULT,
				.MySpCost = skill->MyCooldown, .MyInitialSp = std::max(0.0, skill->MyCooldown - skill->MyInitialCooldown),
				.MyDuration = skill->MyBlackboard.Number("hit_duration")};
			definition.MySkill.MyAttack = attack; definition.MySkill.MyAttack->MyDisabled = true;
			definition.MyYanyou->MyFlameScale = skill->MyBlackboard.Number("atk_scale");
			definition.MyYanyou->MyFlameRadius = skill->MyBlackboard.Number("range_radius");
		}
		if (body.MyStartingFlags) definition.MyInitialBuffs.push_back({.MyKey = "token:starting",
			.MyFlags = StatusFlags(body.MyStartingFlags), .MyPersistent = true, .MyAllowDead = true});
		return definition;
	}
}
