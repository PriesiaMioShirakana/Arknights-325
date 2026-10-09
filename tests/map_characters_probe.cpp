#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_map_characters.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	CombatDefinition Target()
	{
		return CombatDefinition{.MyId = "target", .MyStats = CombatStats{.MyMaxHealth = 10000, .MyAttack = 100, .MyBlockCount = 0, .MyRedeploySeconds = 70},
			.MyAttack = AttackProfile{.MyDisabled = true}, .MySkill = SkillDefinition{.MyKind = SkillKind::DURATION,
				.MyTrigger = SkillTrigger::NEVER, .MySpCost = 100000, .MyDuration = 1}};
	}

	void Snapshot(const Battle& _battle, bool _tokens)
	{
		std::cout << '['; bool first = true;
		for (const auto& u : _battle.Units())
		{
			if (_tokens && u.MyKind != UnitKind::TOKEN) continue;
			if (!first) std::cout << ',';
			first = false;
			std::cout << '[' << std::quoted(u.MyDefinition.MyId) << ',' << u.MyOwner << ',' << u.MyPosition.MyX << ',' << u.MyPosition.MyY << ','
				<< static_cast<unsigned>(u.MyFacing) << ',' << u.MyHealth << ',' << u.MyStats.MyAttack << ',' << _battle.SpTotal(u.MyId) << ','
				<< u.MySkill.MyActive << ',' << u.MyTotals.MyAttacks << ',' << u.MyTotals.MyHealing << ',' << u.MyAlive << ',' << u.MyRemoved << ']';
		}
		std::cout << ']';
	}
}

int main()
{
	using namespace Stronghold;
	std::cout << std::setprecision(17);
	for (std::string command; std::cin >> command;)
	{
		if (command == "map")
		{
			std::string stageId; unsigned mode = 0, elites = 0, occupied = 0; std::cin >> stageId >> mode >> elites >> occupied;
			const bool boss = mode >= 2, multi = mode % 2 != 0;
			BattleInput input{.MyAutoFinish = false, .MyBossBattle = boss};
			for (unsigned owner = 0; owner < (multi ? 2U : 1U); ++owner)
			{
				auto& player = input.MyPlayers.emplace_back(BattlePlayerInput{.MyPlayerId = owner ? "q" : "p",
					.MyMirrorDeployment = boss && owner, .MyRightHalf = owner != 0});
				for (unsigned i = 0; i < 3; ++i)
				{
					auto def = Target(); def.MySkill = {}; def.MyIdentity.MyGolden = i < (owner ? 3 - elites : elites);
					const double column = i == 0 && occupied ? (owner ? (boss ? 18 : 10) : 2) : (owner ? 15.0 - i : 5.0 + i);
					player.MyUnits.push_back(AllyDeployment{.MyPieceUid = 1 + owner * 3 + i, .MyDefinition = std::move(def),
						.MyPosition = {column, boss ? 3.0 : 10.0},
						.MyCarry = i == 0 && occupied == 3 ? std::optional(CarryState{.MyDown = true}) : std::nullopt,
						.MyDeferred = i == 0 && occupied == 2});
				}
			}
			const auto& stage = ReferenceBattleStage(stageId);
			input.MyField = stage.Prepare(boss ? FieldRect{0, 5, 0, 20} : FieldRect{9, 12, 0, multi ? 20 : 10}, input.MyPlayers).MyField;
			input.MyMapCharacters = MakeMapCharacterVariants(stage);
			Battle battle(std::move(input)); battle.Start(); Snapshot(battle, true); std::cout << '\n';
			if (!battle.ContentErrors().empty()) return 1;
		}
		else
		{
			unsigned scene = 0; std::cin >> scene;
			BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "p"}, BattlePlayerInput{.MyPlayerId = "q"}}, .MyAutoFinish = false};
			constexpr std::array<WorldPoint, 4> positions{{{3, 10}, {4, 10}, {3, 11}, {3, 9}}};
			for (unsigned i = 0; i < 4; ++i)
			{
				auto pos = positions[i]; if (scene >= 8) pos.MyX += 3;
				input.MyPlayers[i == 3 ? 1 : 0].MyUnits.push_back(AllyDeployment{.MyPieceUid = i + 1, .MyDefinition = Target(), .MyPosition = pos});
			}
			Battle battle(std::move(input)); battle.Start();
			const auto token = scene % 2 ? "char_613_acmedc" : "char_605_cmedic";
			const auto medic = battle.SpawnToken(TokenSpawn{.MyDefinition = MakeMapMedicDefinition(ReferenceToken(token).Variant().MyBody), .MyPosition = {2, 10}, .MyPlayerId = "p"});
			std::cout << '['; Snapshot(battle, false);
			for (unsigned tick = 0; tick < 600; ++tick)
			{
				if (tick % 40 == 0) for (UnitId id = 1; id <= 4; ++id)
					(void)battle.LoseHealth(0, id, std::max(0.0, battle.Unit(id).MyHealth - (id == 1 ? 4900.0 : id == 2 ? 5000.0 : 500.0)));
				if (tick == 2 && scene < 8) (void)battle.GainSp(medic, 100);
				if (tick == 10) (void)battle.LoseHealth(0, 1, 100000);
				if (tick == 11) (void)battle.Redeploy(1);
				if (tick == 20) battle.Retreat(2, false, RemovalReason::RETREAT, scene % 4 >= 2);
				if (tick == 21) (void)battle.Redeploy(2);
				if (tick == 30) (void)battle.Heal(medic, 1, 100000);
				if (tick == 31) (void)battle.Heal(medic, 1, 100);
				if (tick == 32) (void)battle.Heal(medic, 1, 100, HealOptions{.MyRegen = true});
				if (tick == 40 && scene % 8 >= 4) (void)battle.ApplyStatus(3, CombatStatus::NO_HEAL, StatusApplication{.MyDuration = 3});
				if (tick == 50 && scene % 8 >= 4) (void)battle.ApplyStatus(4, CombatStatus::ISOLATED, StatusApplication{.MyDuration = 3});
				if (tick == 160 && scene % 8 >= 4) (void)battle.ApplyStatus(3, CombatStatus::HEAL_FREE, StatusApplication{.MyDuration = 3});
				if (tick == 110) battle.EndSkill(medic);
				if (tick == 111) (void)battle.LoseHealth(0, 1, 100000);
				if (tick == 112) (void)battle.Redeploy(1);
				if (tick == 113) battle.Retreat(2, false, RemovalReason::RETREAT, scene % 4 >= 2);
				if (tick == 114) (void)battle.Redeploy(2);
				if (tick == 115) (void)battle.LoseHealth(0, 4, 100000);
				if (tick == 116) (void)battle.Redeploy(4);
				if (tick == 120) battle.EndSkill(medic);
				if (tick == 121) (void)battle.GainSp(medic, 100);
				if (tick % 17 == 0 && scene < 8) (void)battle.ForceAttack(medic);
				battle.Step(); std::cout << ','; Snapshot(battle, false);
			}
			std::cout << "]\n";
			if (!battle.ContentErrors().empty()) return 2;
		}
	}
}
