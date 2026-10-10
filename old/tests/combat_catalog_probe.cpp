#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_combat.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;
	// 数据包含中文文本，显式转义 JSON 控制字符；不依赖系统 locale。
	void String(std::string_view _value)
	{
		constexpr std::string_view Hex = "0123456789abcdef";
		std::cout << '"';
		for (const unsigned char ch : _value)
		{
			if (ch == '"' || ch == '\\') std::cout << '\\' << static_cast<char>(ch);
			else if (ch < 32) std::cout << "\\u00" << Hex[ch >> 4] << Hex[ch & 15];
			else std::cout << static_cast<char>(ch);
		}
		std::cout << '"';
	}

	void Board(Blackboard _board)
	{
		std::cout << '{';
		bool first = true;
		for (const auto& entry : _board.MyEntries)
		{
			if (!first) std::cout << ',';
			first = false;
			String(entry.MyKey);
			std::cout << ':';
			if (const auto* number = std::get_if<double>(&entry.MyValue)) std::cout << *number;
			else String(std::get<std::string_view>(entry.MyValue));
			if (_board.Find(entry.MyKey) != &entry) throw std::runtime_error("blackboard index failed");
		}
		std::cout << '}';
	}
}

int main()
{
	try
	{
		using namespace Stronghold;
		std::cout << std::setprecision(17) << '[';
		bool first = true;
		for (const auto& record : ReferenceEnemies())
		{
			if (&ReferenceEnemy(record.MyId) != &record) throw std::runtime_error("enemy index failed");
			if (!first) std::cout << ',';
			first = false;
			std::cout << '[';
			String(record.MyId); std::cout << ','; String(record.MyName); std::cout << ',';
			const auto& s = record.MyStats;
			std::cout << '[' << s.MyMaxHealth << ',' << s.MyAttack << ',' << s.MyDefense << ',' << s.MyResistance << ','
				<< s.MyAttackSpeed << ',' << s.MyBaseAttackTime << ',' << s.MyMoveSpeed << ',' << s.MyTaunt << ','
				<< s.MyMass << ',' << s.MyHealthRegen << ',' << s.MyElementResistance << ',' << s.MyElementalResistance << ','
				<< record.MyBlockWeight << ',' << record.MyLifeCost << ',' << record.MyCounted << ',' << record.MyFlying << ','
				<< record.MyStaticBody << ',' << static_cast<unsigned>(record.MyRank) << "],";
			Board(record.MyTalent); std::cout << ','; Board(record.MyTalentStrings); std::cout << ",[";
			bool firstSkill = true;
			for (const auto& skill : record.MySkills)
			{
				if (!firstSkill) std::cout << ',';
				firstSkill = false;
				std::cout << '['; String(skill.MyId);
				std::cout << ',' << skill.MyPriority << ',' << skill.MyCooldown << ',' << skill.MyInitialCooldown << ',' << skill.MySpCost << ',';
				Board(skill.MyBlackboard); std::cout << ','; Board(skill.MyStrings); std::cout << ']';
			}
			std::cout << "],[";
			auto definition = record.MakeDefinition();
			constexpr std::array ImmuneKinds{CombatStatus::STUN, CombatStatus::SILENCE, CombatStatus::SLEEP, CombatStatus::FREEZE,
				CombatStatus::LEVITATE, CombatStatus::FEAR, CombatStatus::ATTRACT, CombatStatus::DISARM, CombatStatus::PALSY};
			for (std::size_t i = 0; i < ImmuneKinds.size(); ++i)
			{ if (i) std::cout << ','; std::cout << definition.MyImmunities[static_cast<std::size_t>(ImmuneKinds[i])]; }
			std::cout << "],[";
			// 所有真实敌人的基础攻击/移动进行逐帧验证；内容脚本在双方均关闭。
			BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "one", .MyUnits = {AllyDeployment{
				.MyPieceUid = 1, .MyDefinition = CombatDefinition{.MyId = "ally", .MyStats = CombatStats{
					.MyMaxHealth = 100000, .MyDefense = 100, .MyBlockCount = 10}, .MyAttack = AttackProfile{.MyDisabled = true}},
				.MyPosition = WorldPoint{.MyX = 5, .MyY = 9}}}}}, .MyAutoFinish = false};
			input.MySpawns.emplace_back(0, "one", std::move(definition), CombatRoute{
				.MyStart = WorldPoint{.MyX = 10, .MyY = 9}, .MyEnd = WorldPoint{.MyX = 4, .MyY = 9}}, record.MyLifeCost, record.MyCounted);
			Battle battle(std::move(input));
			for (unsigned tick = 0; tick < 180; ++tick)
			{
				battle.Step();
				if (tick) std::cout << ',';
				const auto& e = battle.Unit(2);
				std::cout << '[' << e.MyHealth << ',' << e.MyAlive << ',' << e.MyPosition.MyX << ',' << e.MyPosition.MyY << ','
					<< e.MyBlockedBy << ',' << e.MyTotals.MyAttacks << ',' << battle.Unit(1).MyHealth << ']';
			}
			std::cout << "]]";
		}
		std::cout << ']';
		return 0;
	}
	catch (const std::exception& _error) { std::cerr << _error.what() << '\n'; return 1; }
}
