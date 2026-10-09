#include <iomanip>
#include <iostream>
#include <source_location>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	void Check(bool _condition, const std::source_location _where = std::source_location::current())
	{
		if (!_condition) throw std::runtime_error(std::string(_where.file_name()) + ":" + std::to_string(_where.line()));
	}

	SkillDefinition Definition(unsigned _case)
	{
		SkillDefinition skill{
			.MyKind = SkillKind::DURATION, .MySpCost = 2, .MyInitialSp = 2, .MyDuration = 0.7,
			.MyAmmo = 3, .MyManual = false,
			.MyAttack = AttackProfile{.MyAttackScale = 2},
			.MyModifiers = {AttributeChange{.MyAttribute = Attribute::ATTACK_FLAT, .MyValue = 5}}};
		switch (_case)
		{
		case 1: skill.MyKind = SkillKind::AMMO; skill.MyDuration = 0; skill.MySpType = SpType::ATTACK; break;
		case 2: skill.MyKind = SkillKind::INSTANT; skill.MySpType = SpType::ATTACK; break;
		case 3: skill.MyKind = SkillKind::CHARGES; skill.MyManual = true; skill.MyMaxCharges = 3; skill.MyInitialSp = 6; skill.MyTrigger = SkillTrigger::SP_FULL; break;
		case 4: skill.MyKind = SkillKind::PASSIVE; break;
		case 5: skill.MyKind = SkillKind::TOGGLE; break;
		case 6: skill.MyKind = SkillKind::INSTANT; skill.MySpType = SpType::HURT; skill.MyTrigger = SkillTrigger::TAKE_DAMAGE; break;
		case 7: skill.MyTrigger = SkillTrigger::CUSTOM_RANGE; break;
		case 8: skill.MyTrigger = SkillTrigger::SKILL_RANGE; break;
		case 9: skill.MyTrigger = SkillTrigger::ACTIVE_RANGE; break;
		case 10: skill.MyTrigger = SkillTrigger::SEARCH; break;
		case 11: skill.MyTrigger = SkillTrigger::GLOBAL; break;
		case 12: skill.MyKind = SkillKind::INSTANT; skill.MyTriggerAllies = true; skill.MyHealSkill = true; skill.MyTriggerHpAtMost = 0.7; skill.MyAttack->MyHealing = true; break;
		case 13: skill.MySpCost = 0; skill.MyInitialSp = 0; skill.MyTrigger = SkillTrigger::SP_FULL; break;
		case 14: skill.MyKind = SkillKind::NONE; break;
		case 15: skill.MyKind = SkillKind::AMMO; skill.MyDuration = 0.2; break;
		case 16: skill.MyKind = SkillKind::INSTANT; skill.MySpType = SpType::ATTACK; skill.MyAttack->MyRanged = true; skill.MyAttack->MyProjectileSpeed = 12; skill.MyAttack->MyDamageType = DamageType::ARTS; break;
		case 17: skill.MyKind = SkillKind::CHARGES; skill.MyMaxCharges = 3; skill.MyTrigger = SkillTrigger::NEVER; break;
		}
		if (_case >= 7 && _case <= 9)
		{
			skill.MyTriggerRange = {RangeOffset{.MyColumn = 3}};
			skill.MyRange = skill.MyTriggerRange;
		}
		return skill;
	}

	BattleInput Input(unsigned _case)
	{
		CombatDefinition ally{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyAttack = 10, .MyBaseAttackTime = 0.2, .MyRedeploySeconds = 1}};
		ally.MySkill = Definition(_case);
		ally.MyAttack.MyDisabled = _case >= 7 && _case <= 11;
		ally.MyAttack.MyHealing = _case == 12;
		CombatDefinition enemy{.MyId = "enemy", .MyStats = CombatStats{.MyMaxHealth = 1000000, .MyAttack = 0, .MyMoveSpeed = 0}, .MyAttack = AttackProfile{.MyDisabled = true}};
		const auto position = WorldPoint{.MyX = (_case >= 7 && _case <= 9) || _case == 11 ? 8.0 : 6.0, .MyY = 9};
		return BattleInput{
			.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {AllyDeployment{.MyPieceUid = 1, .MyDefinition = std::move(ally), .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}}}}},
			.MySpawns = {EnemySpawn{.MyOwnerId = "one", .MyDefinition = std::move(enemy), .MyRoute = CombatRoute{.MyStart = position, .MyEnd = WorldPoint{.MyY = 9}}}},
			.MyAutoFinish = false};
	}

	void Number(double _value)
	{
		if (std::isfinite(_value)) std::cout << _value;
		else std::cout << "null";
	}

	// 帧轨迹是给原 JS 的输入对照，而不是把实现细节重写成另一套预期计算。
	void Trace()
	{
		std::cout << std::setprecision(17) << '[';
		for (unsigned caseIndex = 0; caseIndex < 18; ++caseIndex)
		{
			if (caseIndex) std::cout << ',';
			std::cout << '[';
			Battle battle(Input(caseIndex));
			battle.Start();
			for (unsigned tick = 0; tick < 420; ++tick)
			{
				if (tick == 1 && caseIndex == 8) (void)battle.ApplyStatus(2, CombatStatus::UNTARGETABLE, 10);
				if (tick % 17 == 1) (void)battle.DealDamage(2, 1, 20, DamageType::TRUE_DAMAGE);
				if (tick == 80) (void)battle.ApplyStatus(1, CombatStatus::NO_SP, 1);
				if (tick == 100) (void)battle.ApplyStatus(1, CombatStatus::STUN, 1);
				if (tick == 140) (void)battle.ApplyStatus(1, CombatStatus::SILENCE, 0.5);
				if (tick == 180) (void)battle.ActivateSkill(1);
				if (tick == 220) battle.SetSpTotal(1, 5);
				if (tick == 250) battle.SetSpCostMultiplier(1, 0.5);
				if (tick == 280) battle.AddSkillAmmo(1, 2);
				if (tick == 290) battle.ExtendSkill(1, 0.4);
				if (tick == 300) battle.EndSkill(1);
				if (tick == 330) (void)battle.LoseHealth(0, 1, 100000);
				battle.Step();
				const auto& unit = battle.Unit(1);
				const auto& skill = unit.MySkill;
				if (tick) std::cout << ',';
				std::cout << '[' << skill.MySp << ',' << skill.MyCharges << ',' << skill.MyActive << ',' << skill.MyPending << ',';
				Number(skill.MyTimeLeft);
				std::cout << ',' << skill.MyAmmoLeft << ',' << skill.MyAmmoMax << ',' << skill.MyActivations << ',' << unit.MyStats.MyAttack << ',' << battle.Unit(2).MyHealth << ',' << unit.MyHealth << ',' << unit.MyTotals.MyAttacks << ',' << battle.SpTotal(1) << ',';
				Number(skill.MyOperationReadyAt);
				std::cout << ']';
			}
			Check(battle.ContentErrors().empty());
			std::cout << ']';
		}
		std::cout << ']';
	}

	struct RestartHandler final : CustomOperator<RestartHandler>
	{
		void OnSkillEnding(Battle& _battle, ContentEvent& _event)
		{
			Check(std::isgreaterequal(_battle.Unit(_event.MyUnit).MyStats.MyAttack, 15));
			if (_battle.Unit(_event.MyUnit).MySkill.MyActivations == 1)
				Check(_battle.ActivateSkill(_event.MyUnit, true));
		}
	};

	void Lifecycle()
	{
		ContentRegistry registry;
		const auto ref = registry.Register<RestartHandler>("restart");
		registry.Seal();
		auto input = Input(0);
		input.MyContentRegistry = registry;
		input.MyPlayers[0].MyUnits[0].MyDefinition.MyContent = ref;
		Battle battle(std::move(input));
		battle.Start();
		Check(battle.ActivateSkill(1));
		battle.EndSkill(1);
		Check(battle.Unit(1).MySkill.MyActive && battle.Unit(1).MySkill.MyActivations == 2);
		Check(std::islessequal(std::abs(battle.Unit(1).MyStats.MyAttack - 15), 1e-9));
		Check(battle.ContentErrors().empty());
		battle.EndSkill(1);
		Check(!battle.Unit(1).MySkill.MyActive && std::islessequal(std::abs(battle.Unit(1).MyStats.MyAttack - 10), 1e-9));
	}
}

int main(int _argc, char**)
{
	try
	{
		if (_argc > 1) Trace();
		else { Lifecycle(); std::cout << "skill callback lifecycle passed\n"; }
		return 0;
	}
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
