#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;
	BattleInput Input(SharedBossPool& _pool, unsigned _field, unsigned _scene)
	{
		const std::string player = _field == 0 ? "one" : "two";
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = player, .MyUnits = {AllyDeployment{.MyPieceUid = 1,
			.MyDefinition = CombatDefinition{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 1000}, .MyAttack = AttackProfile{.MyDisabled = true}}, .MyPosition = WorldPoint{.MyX = 5, .MyY = 9}}}}},
			.MyTimeLimit = _scene == 2 ? 1.0 : 60.0, .MyBossBattle = true, .MySharedBoss = std::ref(_pool)};
		for (unsigned enemy = 0; enemy < (_scene == 1 ? 1U : 3U); ++enemy)
			input.MySpawns.emplace_back(EnemySpawn{.MyOwnerId = player,
				.MyDefinition = CombatDefinition{.MyId = enemy == 0 ? "boss" : enemy == 1 ? "part" : "minion", .MyStats = CombatStats{.MyMaxHealth = 100000, .MyMoveSpeed = _scene == 1 ? 1.0 : 0.0, .MyHealthRegen = 100},
					.MyAttack = AttackProfile{.MyDisabled = true}, .MyLeader = enemy != 2},
				.MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = 8.0 + enemy, .MyY = 9}, .MyEnd = WorldPoint{.MyX = _scene == 1 ? 8.0 : 0.0, .MyY = 9}},
				.MyLifeCost = 5, .MyCounted = enemy == 2, .MyTag = enemy == 0 ? EnemySpawnTag::BOSS : enemy == 1 ? EnemySpawnTag::PART : EnemySpawnTag::NONE});
		return input;
	}

	void Effects(Battle& _battle, unsigned _tick, unsigned _field)
	{
		if (!_battle.Started() || _battle.Finished() || !_battle.Unit(2).MyAlive) return;
		if (_tick == 1) (void)_battle.DealDamage(1, 2, 100 + 10 * _field, DamageType::TRUE_DAMAGE);
		if (_tick == 2) (void)_battle.AddBuff(2, BuffDefinition{.MyKey = "hp", .MyModifiers = std::vector<AttributeChange>{AttributeChange{.MyAttribute = Attribute::HEALTH_PERCENT, .MyValue = 0.5}}});
		if (_tick == 3 && std::isgreater(_battle.Heal(1, 2, 10000), 0)) throw std::runtime_error("pooled boss healed");
		if (_tick == 4) (void)_battle.DealDamage(1, 2, 299999.01, DamageType::TRUE_DAMAGE);
		if (_tick == 5) { _battle.AddShield(2, Shield{.MyHealth = 100}); (void)_battle.DealDamage(1, 2, 60, DamageType::TRUE_DAMAGE); }
		if (_tick == 6) (void)_battle.LoseHealth(1, 2, 10);
		if (_tick == 7) (void)_battle.DealDamage(2, 2, 50, DamageType::TRUE_DAMAGE);
		if (_tick == 8) (void)_battle.DealDamage(1, 2, DamageInfo{.MyAmount = 60, .MyType = DamageType::TRUE_DAMAGE, .MySourceless = true});
		if (_tick == 15)
		{
			(void)_battle.LoseHealth(1, 2, 299999.01);
			(void)_battle.DealDamage(1, 3, 300000, DamageType::TRUE_DAMAGE); // 普通 BOSS 阶级的部件不受首领限伤。
		}
	}
}

// 同一个值类型血池被两场战斗借用；检查命中归属、外部扣血、镜像同步、漏怪留场与结果快照。
int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 5; ++scene)
	{
		SharedBossPool pool(3000);
		if (scene == 3) (void)pool.Damage("outside", 3000);
		Battle left(Input(pool, 0, scene)), right(Input(pool, 1, scene));
		if (scene) std::cout << ',';
		std::cout << '[';
		for (unsigned tick = 0; tick < 90; ++tick)
		{
			if (tick == 10) (void)pool.Damage("outside", 100);
			if (tick == 50) (void)pool.Damage("outside", pool.Health() - 0.5);
			if (tick == 20 && scene == 4) right.ForceEnd();
			Effects(left, tick, 0); left.Step();
			Effects(right, tick, 1); right.Step();
			if (tick) std::cout << ',';
			std::cout << '[' << pool.Health();
			for (const auto player : {"one", "two", "outside"})
			{
				const auto credit = pool.Credits().find(player);
				std::cout << ',' << (credit == pool.Credits().end() ? 0 : credit->second);
			}
			for (const auto& battle : {std::cref(left), std::cref(right)})
			{
				const auto& b = battle.get(); const auto& boss = b.Unit(2); const auto result = b.Result(); const auto& player = b.Players()[0];
				std::cout << ",[" << b.Time() << ',' << static_cast<unsigned>(result.MyReason) << ',' << *result.MyBossHealthLeft << ',' << boss.MyHealth << ','
					<< boss.MyStats.MyMaxHealth << ',' << boss.MyAlive << ',' << boss.MyTotals.MyTaken << ',' << player.MyBossDamage << ',' << player.MyDamage << ',' << player.MyLifeLost << ','
					<< result.MyTotal << ',' << result.MyKilled << ',' << result.MyLeaked << ',' << b.Unit(1).MyTotals.MyKills << ']';
			}
			std::cout << ']';
		}
		std::cout << ']';
	}
	std::cout << ']';
}
