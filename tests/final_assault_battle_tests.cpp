#include <iostream>
#include <stronghold/domain/final_assault.hpp>
#include <stronghold/simulation/battle.hpp>
#include <stronghold/simulation/boss_battle_group.hpp>

namespace
{
	using namespace Stronghold;
	void Check(bool _condition) { if (!_condition) throw std::runtime_error("final assault integration assertion failed"); }

	BattleInput Input(FinalAssault& _fight, std::string _player, std::optional<int> _leak = {})
	{
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = _player, .MyUnits = {AllyDeployment{.MyPieceUid = 1,
			.MyDefinition = CombatDefinition{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyBlockCount = 0},
				.MyAttack = AttackProfile{.MyDisabled = true}}, .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}}}}},
			.MyTimeLimit = std::numeric_limits<double>::infinity(), .MyBossBattle = true, .MyFinalAssault = std::ref(_fight)};
		input.MySpawns.emplace_back(EnemySpawn{.MyOwnerId = _player, .MyDefinition = CombatDefinition{.MyId = "boss",
			.MyStats = CombatStats{.MyMaxHealth = 1000, .MyMoveSpeed = 0}, .MyAttack = AttackProfile{.MyDisabled = true}, .MyLeader = true},
			.MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = 8, .MyY = 9}, .MyEnd = WorldPoint{.MyX = 0, .MyY = 9}}, .MyCounted = false, .MyTag = EnemySpawnTag::BOSS});
		if (_leak) input.MySpawns.emplace_back(EnemySpawn{.MyOwnerId = _player, .MyDefinition = CombatDefinition{.MyId = "leaker",
			.MyStats = CombatStats{.MyMaxHealth = 1000, .MyMoveSpeed = 1}, .MyAttack = AttackProfile{.MyDisabled = true}},
			.MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = 8, .MyY = 9}, .MyEnd = WorldPoint{.MyX = 8, .MyY = 9}},
			.MyLifeCost = *_leak, .MyCounted = false});
		return input;
	}
}

int main()
{
	using namespace Stronghold;
	try
	{
		const std::array players{BossParticipant{.MyPlayerId = "left", .MySeat = 0, .MyLife = 10}, BossParticipant{.MyPlayerId = "right", .MySeat = 1, .MyLife = 10}};
		{
			FinalAssault fight(FinalAssaultRules{}, players, 1000); Battle left(Input(fight, "left")), right(Input(fight, "right", 0));
			Check(!std::isgreater(left.LoseTeamLife(5), 0)); left.Step(); right.Step();
			Check(std::isunordered(fight.TeamLife(), 20) == false && !std::islessgreater(fight.TeamLife(), 20));
			(void)left.DealDamage(1, 2, 200, DamageType::TRUE_DAMAGE);
			(void)left.DealDamage(1, 2, 300, DamageType::TRUE_DAMAGE);
			(void)left.DealDamage(1, 2, 300, DamageType::TRUE_DAMAGE);
			Check(fight.Hits().size() == 3); // 多次攻击即使同一帧也逐次触发 20/50/80%，不能合并到 DrainEvents。
			Check(std::isgreater(right.LoseTeamLife(20), 0));
			Check(fight.Outcome() == BossOutcome::DEFEAT);
			Check(!std::isgreater(left.DealDamage(1, 2, 200, DamageType::TRUE_DAMAGE), 0));
			Check(!std::islessgreater(fight.Pool().Health(), 200));
			left.ForceEnd(); right.ForceEnd();
		}
		{
			FinalAssault fight(FinalAssaultRules{}, players, 1000); Battle left(Input(fight, "left")), right(Input(fight, "right", 20));
			left.Step(); right.Start(); (void)left.DealDamage(1, 2, 1000, DamageType::TRUE_DAMAGE); right.Step(); left.Step();
			Check(fight.Outcome() == BossOutcome::VICTORY && !std::islessgreater(fight.TeamLife(), 20));
			Check(left.Finished() && right.Finished());
		}
		{
			FinalAssault fight(FinalAssaultRules{}, players, 1000); Battle battle(Input(fight, "left", 7)); battle.Step();
			Check(!std::islessgreater(fight.TeamLife(), 13)); // 未计入通关数量的敌人仍按 lpr 扣血。
			battle.Advance(7200); Check(!battle.Finished()); // 120 实秒的 HUD 倒计时不结束首领战。
			fight.Advance(302); Check(!std::islessgreater(fight.TeamLife(), 12));
			battle.ForceEnd(); Check(!std::isgreater(battle.LoseTeamLife(1), 0));
		}
		{
			FinalAssault fight(FinalAssaultRules{}, players, 1000); SharedBossPool other(1000);
			auto input = Input(fight, "left"); input.MySharedBoss = std::ref(other);
			bool rejected = false; try { Battle invalid(std::move(input)); } catch (const std::invalid_argument&) { rejected = true; }
			Check(rejected);
		}
		{
			const std::array team{BossParticipant{.MyPlayerId = "a", .MyLife = 10}, BossParticipant{.MyPlayerId = "b", .MySeat = 1, .MyLife = 10},
				BossParticipant{.MyPlayerId = "c", .MySeat = 2, .MyLife = 10}, BossParticipant{.MyPlayerId = "d", .MySeat = 3, .MyLife = 10}};
			FinalAssault fight(FinalAssaultRules{}, team, 1000);
			std::vector<BattleInput> fields; fields.reserve(2);
			for (const auto& pair : fight.Fields())
			{
				auto input = Input(fight, pair.MyPlayers[0]);
				auto right = Input(fight, pair.MyPlayers[1]);
				right.MyPlayers[0].MyUnits[0].MyPosition.MyX = 13;
				input.MyPlayers.emplace_back(std::move(right.MyPlayers[0])); fields.emplace_back(std::move(input));
			}
			BossBattleGroup group(fight, std::move(fields));
			group.Advance(9060); Check(!group.Finished() && !std::islessgreater(fight.TeamLife(), 39));
			(void)group.Field(0).DealDamage(1, 3, 1000, DamageType::TRUE_DAMAGE);
			group.Step(); Check(group.Finished() && fight.Outcome() == BossOutcome::VICTORY);
			const auto results = group.Results(); Check(results.size() == 2);
			for (const auto& result : results) Check(result.MyReason != BattleEndReason::RUNNING);
			group.Step(); Check(!std::islessgreater(fight.TeamLife(), 39));
		}
		std::cout << "final assault battle integration passed\n";
	}
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
