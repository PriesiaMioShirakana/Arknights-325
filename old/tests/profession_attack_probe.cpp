#include <array>
#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	WorldPoint Point(double _forward, double _lateral, Facing _facing)
	{
		const auto forward = RotateOffset(RangeOffset{.MyColumn = 1}, _facing);
		const auto lateral = RotateOffset(RangeOffset{.MyRow = 1}, _facing);
		return {.MyX = 10 + _forward * forward.MyColumn + _lateral * lateral.MyColumn,
			.MyY = 9 + _forward * forward.MyRow + _lateral * lateral.MyRow};
	}

	BattleInput Input(unsigned _scene, Facing _facing)
	{
		constexpr std::array Scales{AttackScaling::FLYING, AttackScaling::UNBLOCKED, AttackScaling::DISTANT,
			AttackScaling::FRONT, AttackScaling::FRONT, AttackScaling::FRONT, AttackScaling::NONE, AttackScaling::NONE};
		AttackProfile attack{.MyHealing = _scene == 7, .MyCanHitFlying = true, .MyHits = 2,
			.MyAllInRange = _scene < 6, .MySplashRadius = _scene == 0 ? 1.1 : 0.0, .MySplashScale = 0.7,
			.MyChainCount = _scene == 1 ? 3U : 1U, .MyHealChainCount = _scene == 7 ? 3U : 1U,
			.MyHitAllBlocked = _scene == 6, .MyScaling = Scales[_scene], .MyConditionalScale = 1.7,
			.MyHealFarMultiplier = 0.6};
		CombatDefinition source{.MyId = "source", .MyStats = CombatStats{.MyMaxHealth = 1000000, .MyAttack = 100,
			.MyBaseAttackTime = 0.5, .MyBlockCount = 2}, .MyAttack = attack, .MyRange = {}};
		for (int r = -5; r <= 5; ++r)
			for (int c = -5; c <= 5; ++c) source.MyRange.emplace_back(RangeOffset{.MyRow = r, .MyColumn = c});
		if (_scene == 4) source.MyTraitFrontRange = std::vector{RangeOffset{.MyColumn = 1}, RangeOffset{.MyRow = 1, .MyColumn = 2}};
		if (_scene == 5) source.MyTraitFrontRange = std::vector<RangeOffset>{};
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {
			AllyDeployment{.MyPieceUid = 1, .MyDefinition = std::move(source), .MyPosition = Point(0, 0, _facing), .MyFacing = _facing}}}},
			.MyAutoFinish = false};
		if (_scene == 7)
			for (unsigned i = 0; i < 3; ++i)
				input.MyPlayers.front().MyUnits.emplace_back(AllyDeployment{.MyPieceUid = i + 2,
					.MyDefinition = CombatDefinition{.MyId = "patient", .MyStats = CombatStats{.MyMaxHealth = 1000000, .MyBlockCount = 0}, .MyAttack = AttackProfile{.MyDisabled = true}},
					.MyPosition = Point(1.0 + i, 1, _facing)});
		constexpr std::array Offsets{WorldPoint{.MyX = -0.2}, WorldPoint{.MyX = 0.4}, WorldPoint{.MyX = 1},
			WorldPoint{.MyX = 2.3, .MyY = 0.6}, WorldPoint{.MyX = 3, .MyY = -1}, WorldPoint{.MyX = -2}};
		for (unsigned i = 0; i < Offsets.size(); ++i)
		{
			CombatDefinition enemy{.MyId = "enemy", .MyStats = CombatStats{.MyMaxHealth = 1000000, .MyDefense = 30, .MyMoveSpeed = 0},
				.MyAttack = AttackProfile{.MyDisabled = true}, .MyFlying = i == 4};
			if (i == 3) enemy.MyHitArea = HitArea{.MyWidth = 2.2, .MyHeight = 2.2};
			input.MySpawns.emplace_back(EnemySpawn{.MyOwnerId = "one", .MyDefinition = std::move(enemy),
				.MyRoute = CombatRoute{.MyStart = Point(Offsets[i].MyX, Offsets[i].MyY, _facing), .MyEnd = WorldPoint{.MyY = 9}}});
		}
		return input;
	}
}

// 几何倍率与副伤害必须同时检查；只看总伤害会掩盖主目标倍率错误传播到溅射／连锁。
int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 8; ++scene)
		for (unsigned facing = 0; facing < 4; ++facing)
		{
			Battle battle(Input(scene, static_cast<Facing>(facing)));
			battle.Step();
			if (scene == 7)
				for (UnitId id = 2; id <= 4; ++id) (void)battle.LoseHealth(0, id, id * 10000);
			if (scene || facing) std::cout << ',';
			std::cout << '[';
			for (unsigned tick = 0; tick < 150; ++tick)
			{
				if (tick == 40 || tick == 90)
					(void)battle.AddBuff(1, BuffDefinition{.MyKey = "block", .MyModifiers = std::vector{
						AttributeChange{.MyAttribute = Attribute::BLOCK_COUNT, .MyValue = tick == 40 ? 2.0 : -8.0}}});
				if (tick == 20 || tick == 70)
					(void)battle.AddBuff(1, BuffDefinition{.MyKey = "targets", .MyModifiers = std::vector{
						AttributeChange{.MyAttribute = Attribute::EXTRA_TARGETS, .MyValue = tick == 20 ? -4.0 : 2.0}}});
				battle.Step();
				if (tick) std::cout << ',';
				std::cout << '[';
				for (std::size_t i = 0; i < battle.Units().size(); ++i)
				{
					if (i) std::cout << ',';
					const auto& unit = battle.Units()[i];
					std::cout << '[' << unit.MyHealth << ',' << unit.MyTotals.MyDamage << ',' << unit.MyTotals.MyHealing << ',' << unit.MyTotals.MyAttacks << ']';
				}
				std::cout << ']';
			}
			std::cout << ']';
		}
	std::cout << ']';
}
