#include <iomanip>
#include <iostream>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;

	// 与 JS 的部署赠送、拥有者带入召唤物和开场再部署修正完全独立地实现相同输入。
	struct CarryHooks final : CustomBond<CarryHooks>
	{
		void OnDeploy(Battle& _battle, ContentEvent& _event)
		{
			_battle.GainSp(_event.MyUnit, 20);
			if (_event.MyUnit != 1 || !_event.MyInitial) return;
			const auto& token = _battle.Unit(3);
			if (token.MyDeferred && token.MyCarry->MyDown) (void)_battle.Redeploy(3, true);
			(void)_battle.SpawnToken(TokenSpawn{
				.MyDefinition = CombatDefinition{.MyId = "extra", .MyStats = CombatStats{.MyMaxHealth = 1000, .MyBlockCount = 0}, .MyAttack = AttackProfile{.MyDisabled = true}},
				.MyPosition = WorldPoint{.MyX = 7, .MyY = 7}, .MyOwnerUnit = 1});
		}

		void OnBattleStart(Battle& _battle, ContentEvent&)
		{
			for (const auto id : {1U, 2U, 4U, 5U})
				(void)_battle.AddBuff(id, BuffDefinition{.MyKey = "redeploy", .MyModifiers = std::vector<AttributeChange>{
					AttributeChange{.MyAttribute = Attribute::REDEPLOY_MULTIPLIER, .MyValue = 0.5}}, .MyPersistent = true, .MyAllowDead = true});
		}
	};

	CombatDefinition Definition(unsigned _scene, bool _token = false)
	{
		CombatDefinition definition{.MyId = _token ? "token" : "ally",
			.MyStats = CombatStats{.MyMaxHealth = 1000, .MyBlockCount = 0, .MyRedeploySeconds = 2, .MyDeploymentCost = 10},
			.MyAttack = AttackProfile{.MyDisabled = true},
			.MySkill = SkillDefinition{.MyKind = SkillKind::CHARGES, .MyTrigger = SkillTrigger::NEVER, .MySpCost = 10, .MyInitialSp = 2, .MyMaxCharges = 3, .MyManual = false}};
		if (_scene == 1) { definition.MySkill.MyKind = SkillKind::DURATION; definition.MySkill.MyDuration = 0.7; definition.MySkill.MyActivateOnDeploy = true; }
		if (_scene == 2) definition.MySkill.MyKind = SkillKind::PASSIVE;
		if (_scene == 3) definition.MySkill.MyKind = SkillKind::NONE;
		return definition;
	}

	void Number(double _value)
	{
		if (std::isfinite(_value)) std::cout << _value; else std::cout << "null";
	}

	void Snapshot(const Battle& _battle)
	{
		std::cout << '[' << _battle.Players()[0].MyDeaths << ',' << _battle.Players()[0].MyDp << ",[";
		for (std::size_t i = 0; i < _battle.Units().size(); ++i)
		{
			const auto& unit = _battle.Units()[i];
			if (i) std::cout << ',';
			std::cout << '[' << unit.MyHealth << ',' << unit.MyAlive << ',' << unit.MyRemoved << ',' << unit.MyDeploySequence << ',' << unit.MyAggroSequence << ',';
			Number(unit.MyRespawnAt); std::cout << ',' << _battle.SpTotal(unit.MyId) << ',' << unit.MySkill.MyActive << ',' << unit.MySkill.MyActivations << ',' << unit.MyOwnerUnit << ',';
			Number(unit.MySkill.MyOperationReadyAt); std::cout << ']';
		}
		std::cout << "],[";
		const auto result = _battle.Result();
		for (std::size_t p = 0; p < result.MyPlayers.size(); ++p)
		{
			if (p) std::cout << ',';
			std::cout << '[';
			const auto& units = result.MyPlayers[p].MyUnitsEnd;
			for (std::size_t i = 0; i < units.size(); ++i)
			{
				const auto& unit = units[i];
				if (i) std::cout << ',';
				std::cout << '[' << unit.MyPieceUid << ',' << unit.MyId << ',' << unit.MyHealthRatio << ',' << unit.MySp << ',' << unit.MySkillActive << ',' << unit.MyAlive << ']';
			}
			std::cout << ']';
		}
		std::cout << "]]";
	}
}

int main()
{
	ContentRegistry registry;
	const auto hooks = registry.Register<CarryHooks>("carry"); registry.Seal();
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 8; ++scene)
	{
		BattleInput input{.MyAutoFinish = false, .MyContentRegistry = std::cref(registry), .MyContentBindings = {ContentBinding{.MyContent = hooks, .MyPlayerId = "one"}}};
		auto& player = input.MyPlayers.emplace_back(BattlePlayerInput{.MyPlayerId = "one"});
		player.MyUnits.reserve(4);
		for (unsigned i = 0; i < 4; ++i)
			player.MyUnits.emplace_back(AllyDeployment{.MyPieceUid = i + 1, .MyDefinition = Definition(scene, i == 2),
				.MyPosition = WorldPoint{.MyX = static_cast<double>(i == 2 ? 4 : 5 + i), .MyY = 9}, .MyKind = i == 2 ? UnitKind::TOKEN : UnitKind::OPERATOR,
				.MyCarry = CarryState{.MyHealthRatio = i == 0 ? 0.4 : i == 1 ? -1.0 : 2.0, .MySp = i == 0 ? 25.126 : i == 1 ? -10.0 : 15.0, .MyDown = i == 1 || (scene == 4 && i == 2)},
				.MyDeferred = i == 2 && (scene == 4 || scene == 5), .MyOwnerPieceUid = i == 2 ? 1U : 0U});
		input.MyPlayers.emplace_back(BattlePlayerInput{.MyPlayerId = "two", .MyUnits = {AllyDeployment{.MyPieceUid = 9, .MyDefinition = Definition(scene), .MyPosition = WorldPoint{.MyX = 15, .MyY = 9}}}, .MyMirrorDeployment = true});
		if (scene == 6) input.MyPlayers[0].MyUnits[0].MyCarry = CarryState{.MyHealthRatio = std::numeric_limits<double>::quiet_NaN(), .MySp = std::numeric_limits<double>::infinity()};
		if (scene == 7) input.MyInitialDp = 0;
		Battle battle(std::move(input)); battle.Start();
		if (scene) std::cout << ',';
		std::cout << '['; Snapshot(battle);
		for (unsigned tick = 0; tick < 150; ++tick)
		{
			if (tick == 10 && scene == 5) (void)battle.Redeploy(3, true);
			if (tick == 40) { battle.Retreat(1); battle.Retreat(3); }
			if (tick == 75) (void)battle.LoseHealth(0, 4, 100000);
			battle.Step(); std::cout << ','; Snapshot(battle);
		}
		if (!battle.ContentErrors().empty()) return 1;
		battle.ForceEnd();
		// 下一场输入直接接收结果转换的值，不借用结束战斗的内存。
		const auto result = battle.Result();
		BattleInput next{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one"}}, .MyAutoFinish = false};
		for (const auto& state : result.MyPlayers[0].MyUnitsEnd)
		{
			const auto& old = battle.Unit(state.MyId);
			next.MyPlayers[0].MyUnits.emplace_back(AllyDeployment{.MyPieceUid = state.MyPieceUid, .MyDefinition = old.MyDefinition,
				.MyPosition = old.MyHome, .MyKind = old.MyKind, .MyCarry = state.Carry()});
		}
		Battle continuation(std::move(next)); continuation.Start(); std::cout << ','; Snapshot(continuation);
		std::cout << ']';
	}
	std::cout << ']';
}
