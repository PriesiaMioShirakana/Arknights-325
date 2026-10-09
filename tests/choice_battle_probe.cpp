#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_choices.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;
	void Changes(std::span<const AttributeChange> _changes)
	{
		std::cout << '[';
		bool first = true;
		for (const auto change : _changes)
		{
			if (!first) std::cout << ',';
			first = false;
			std::cout << '[' << static_cast<unsigned>(change.MyAttribute) << ',' << change.MyValue << ']';
		}
		std::cout << ']';
	}

	void Snapshot(const Battle& _battle)
	{
		std::cout << '[';
		bool first = true;
		for (const auto& unit : _battle.Units())
		{
			if (!first) std::cout << ',';
			first = false;
			const auto& stats = unit.MyStats;
			std::cout << '[' << unit.MyHealth << ',' << unit.MyAlive << ',' << stats.MyMaxHealth << ',' << stats.MyAttack << ','
				<< stats.MyDefense << ',' << stats.MyResistance << ',' << stats.MyMoveSpeed << ',' << stats.MyAttackSpeed << ','
				<< stats.MyDefenseIgnorePercent << ',' << stats.MyResistanceIgnoreFlat << ',' << stats.MyRedeployMultiplier << ',';
			if (std::isfinite(unit.MyRespawnAt)) std::cout << unit.MyRespawnAt; else std::cout << "null";
			std::cout << ',' << std::ranges::count_if(unit.MyBuffs, [](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey.starts_with("choice:"); }) << ']';
		}
		std::cout << ']';
	}
}

int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17) << "{\"rules\":[";
	bool first = true;
	for (const auto& rule : ReferenceChoiceBattleRules())
	{
		if (!first) std::cout << ',';
		first = false;
		std::cout << '[' << std::quoted(std::string(rule.MyId)) << ',' << static_cast<unsigned>(rule.MyGate.MyKind) << ',' << rule.MyGate.MyCount << ',' << rule.MySelfHeal << ',';
		Changes(rule.MyOperatorModifiers); std::cout << ','; Changes(rule.MyFullHealthModifiers); std::cout << ",[";
		for (std::size_t i = 0; i < rule.MyEnemies.size(); ++i)
		{
			if (i) std::cout << ',';
			std::cout << '[' << static_cast<unsigned>(rule.MyEnemies[i].MyRank) << ','; Changes(rule.MyEnemies[i].MyModifiers); std::cout << ']';
		}
		std::cout << "]]";
	}
	std::cout << "],\"scenes\":[";
	unsigned scene{}, count{}, sceneIndex{};
	while (std::cin >> scene >> count)
	{
		ChoiceRewardView view;
		for (unsigned i = 0; i < count; ++i)
		{
			std::string id; std::cin >> id;
			const auto rule = std::ranges::find(ReferenceChoiceBattleRules(), id, &ChoiceBattleRule::MyId);
			if (rule == ReferenceChoiceBattleRules().end()) return 2;
			view.MyBattleEffects.emplace_back(ChoiceEffectState{.MyId = "choice:" + id + "#" + std::to_string(i), .MyRule = rule->MyId,
				.MyPreparationPassed = scene == 1 ? std::optional{false} : scene == 2 ? std::optional{true} : std::nullopt});
		}
		CombatDefinition ally{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyAttack = 100, .MyDefense = 80,
			.MyResistance = 30, .MyBlockCount = 0, .MyRedeploySeconds = 1}, .MyAttack = AttackProfile{.MyDisabled = true}};
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one"}, BattlePlayerInput{.MyPlayerId = "two"}}, .MyAutoFinish = false};
		for (unsigned i = 0; i < 4; ++i)
			input.MyPlayers[0].MyUnits.emplace_back(AllyDeployment{.MyPieceUid = i + 1, .MyDefinition = ally,
				.MyPosition = WorldPoint{.MyX = 3.0 + i, .MyY = scene == 3 ? 4.0 + i : 4},
				.MyKind = i == 3 ? UnitKind::TOKEN : UnitKind::OPERATOR,
				.MyCarry = CarryState{.MyDown = scene == 4 && i == 2}});
		input.MyPlayers[1].MyUnits.emplace_back(AllyDeployment{.MyPieceUid = 5, .MyDefinition = ally, .MyPosition = WorldPoint{.MyX = 15, .MyY = 4}});
		input.MyPlayers[0].MyChoiceEffects = MakeChoiceBattleEffects(view);
		if (scene != 5) input.MyPlayers[0].MyHandUnits = scene == 6 ? 0 : 10;
		// 第三批延迟出生，验证增益已安装后创建的敌人仍按所属半场及位阶匹配。
		for (unsigned owner = 0; owner < 2; ++owner)
			for (unsigned rank = 0; rank < 3; ++rank)
			{
				auto enemy = ally; enemy.MyId = "enemy"; enemy.MyElite = rank == 1; enemy.MyLeader = rank == 2;
				enemy.MyStats.MyRedeploySeconds = 0;
				input.MySpawns.emplace_back(EnemySpawn{.MyTime = rank == 2 ? 0.6 : 0, .MyOwnerId = owner ? "two" : "one",
					.MyDefinition = std::move(enemy), .MyRoute = CombatRoute{.MyStart = WorldPoint{.MyX = 10, .MyY = 10}, .MyEnd = WorldPoint{.MyX = 0, .MyY = 10}}});
			}
		Battle battle(std::move(input)); battle.Start();
		if (sceneIndex++) std::cout << ',';
		std::cout << '['; Snapshot(battle);
		for (unsigned tick = 0; tick < 100; ++tick)
		{
			for (const UnitId target : {1u, 4u, 5u})
			{
				if (tick == 0 || tick == 3 || tick == 8) (void)battle.DealDamage(0, target, 110, DamageType::TRUE_DAMAGE);
				if (tick == 1) (void)battle.LoseHealth(0, target, 110);
				if (tick == 2) (void)battle.DealElement(0, target, ElementHit{.MyElement = Element::NEURAL, .MyAmount = 10});
				if (tick == 3) (void)battle.ApplyStatus(target, CombatStatus::NO_HEAL, 0.1);
				if (tick == 4) (void)battle.DealDamage(0, target, 110, DamageType::TRUE_DAMAGE);
				if (tick == 5) (void)battle.DealDamage(0, target, 110, DamageType::ELEMENTAL);
				if (tick == 10 || tick == 31) (void)battle.Heal(0, target, 10000, HealOptions{.MySelf = true});
				if (tick == 12) (void)battle.DealDamage(0, target, 10000, DamageType::TRUE_DAMAGE);
			}
			if (tick == 60) battle.Retreat(1);
			if (tick == 63) (void)battle.Redeploy(1);
			// 同时记录调用后与 tick 后：无瑕在治疗后的下一 tick 更新，而受伤时立即更新。
			std::cout << ','; Snapshot(battle); battle.Step(); std::cout << ','; Snapshot(battle);
		}
		std::cout << ']';
		if (!battle.ContentErrors().empty()) return 3;
	}
	std::cout << "]}";
}
