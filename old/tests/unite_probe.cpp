#include <iomanip>
#include <iostream>
#include <stronghold/domain/unite.hpp>

namespace
{
	using namespace Stronghold;
	void String(std::string_view _s) { std::cout << '"' << _s << '"'; }
	void Strings(std::span<const std::string> _values)
	{
		std::cout << '[';
		for (std::size_t i = 0; i < _values.size(); ++i) { if (i) std::cout << ','; String(_values[i]); }
		std::cout << ']';
	}
	void Mods(const std::optional<EnemySpawnModifiers>& _m)
	{
		if (!_m) { std::cout << "null"; return; }
		std::cout << '[' << _m->MyHealth.value_or(1) << ',';
		String(_m->MyBountyId); std::cout << ',' << _m->MyBountyCoins.value_or(0) << ']';
	}
	void Losses(std::span<const UniteLoss> _losses)
	{
		std::cout << '[';
		for (std::size_t i = 0; i < _losses.size(); ++i)
		{
			if (i) std::cout << ',';
			std::cout << '['; String(_losses[i].MyPlayerId); std::cout << ',' << _losses[i].MyCount << ']';
		}
		std::cout << ']';
	}
	void Plan(const std::optional<UnitePlan>& _plan)
	{
		if (!_plan) { std::cout << "null"; return; }
		const auto& p = *_plan;
		std::cout << '['; Strings(p.MyHelpers); std::cout << ','; Strings(p.MyLeakers); std::cout << ',';
		Losses(p.MyNotReentered); std::cout << ',' << p.MyRelayRound << ','; Strings(p.MyRelayCandidates); std::cout << ",[";
		for (std::size_t i = 0; i < p.MyLeaks.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& l = p.MyLeaks[i]; std::cout << '['; String(l.MyEnemyId); std::cout << ','; String(l.MySourcePlayer);
			std::cout << ','; Mods(l.MyModifiers); std::cout << ',' << l.MyBounty << ',' << l.MyCoins << ',';
			String(l.MyCoins > 0 ? std::string_view(l.MyRewardOwner) : std::string_view{}); std::cout << ']';
		}
		std::cout << "]]";
	}
}

int main()
{
	constexpr std::array enemies{WaveEnemyParameters{.MyId = "enemy_a"}, WaveEnemyParameters{.MyId = "enemy_b", .MyFlying = true}};
	std::cout << std::setprecision(17) << '[';
	for (unsigned scene = 0; scene < 48; ++scene)
	{
		// 每个场景独立持有战报、卡片和盟约；查询视图只在这些值存活期间使用。
		std::array<std::string, 10> names;
		std::array<BattlePlayerState, 10> results;
		std::array<std::array<UniteBond, 2>, 10> bonds;
		const std::array<std::uint64_t, 4> uids{1, 2, 3, 4};
		const std::array cards{WaveBounty{.MyId = "kill", .MyEnemyId = "enemy_a", .MyCoins = 17}, WaveBounty{.MyId = "perfect", .MyEnemyId = "enemy_b", .MyCoins = 30, .MyPerfect = true}};
		std::vector<UniteParticipant> players; players.reserve(10);
		for (unsigned i = 0; i < (scene < 4 ? 4U : 10U); ++i)
		{
			names[i] = std::to_string(i); auto& result = results[i]; result.MyPlayerId = names[i];
			if (scene % 6)
				for (unsigned j = 0; j < 5; ++j) result.MyUnitsEnd.emplace_back(UnitEndState{.MyPieceUid = j + 1,
					.MyHealthRatio = j == 0 ? 0.0 : 0.7, .MySp = static_cast<double>(j * 3), .MyAlive = (i + j + scene) % 4 != 0});
			if (i == 0 || i == 3)
			{
				result.MyPerfect = false;
				for (unsigned j = 0; j < 6; ++j)
					result.MyLeaks.emplace_back(LeakedEnemy{.MyEnemyId = j == 4 ? "missing" : j == 1 || j == 2 ? "enemy_b" : "enemy_a",
						.MyModifiers = EnemySpawnModifiers{.MyHealth = 1.5, .MyBountyId = j < 2 ? "kill" : j == 2 ? "perfect" : "", .MyBountyCoins = j == 5 ? std::optional<std::int64_t>(12) : std::nullopt},
						.MySourcePlayer = "older", .MyTag = EnemySpawnTag::BOUNTY, .MyCounted = j != 3});
			}
			bonds[i] = {UniteBond{.MyActive = (i + scene) % 2 == 0, .MyLayers = static_cast<double>((i * 13 + scene) % 1000),
				.MyStoredLayers = i == 2 ? std::optional<double>(995) : std::optional<double>(0), .MyPendingGain = 20.7}, UniteBond{.MyLayers = 50}};
			players.emplace_back(UniteParticipant{.MyPlayerId = names[i], .MySeat = static_cast<int>(9 - i), .MyAlive = scene % 7 != 0 || i < 3,
				.MyLeft = scene % 9 == 0 && i == 8, .MyDeployCount = 3 + (i + scene) % 3, .MyOperatorUids = uids, .MyBonds = bonds[i], .MyBounties = cards,
				.MyResult = scene % 11 == 0 && i == 5 ? std::nullopt : std::optional(std::cref(result)), .MySynthetic = scene % 5 == 0 && i == 7});
		}
		if (scene) std::cout << ',';
		std::cout << "[[";
		for (std::size_t i = 0; i < players.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto m = MeasureUniteHelper(players[i]); std::cout << '[' << m.MyUnits << ',' << m.MyActiveBond << ',' << m.MyLayers << ',' << m.MyStanding << ']';
		}
		std::cout << "],"; Strings(SelectUniteHelpers(players)); std::cout << ',';
		const auto plan = PlanUnite(UniteRules{.MySolo = scene == 0, .MyCapacityExperiment = true, .MyNormalAliveCount = scene % 13 == 0 ? 7U : 10U, .MyMaxHelpers = scene % 4 == 0 ? 1U : 2U}, players, enemies);
		Plan(plan); std::cout << ',';
		BattleResult residual{.MyPlayers = {BattlePlayerState{.MyPlayerId = "helper"}}};
		residual.MyPlayers[0].MyLeaks = {
			LeakedEnemy{.MyEnemyId = "enemy_b", .MyModifiers = EnemySpawnModifiers{.MyBountyId = "kill"}, .MySourcePlayer = "0", .MyTag = EnemySpawnTag::BOUNTY, .MyCounted = true},
			LeakedEnemy{.MyEnemyId = "missing", .MySourcePlayer = "3", .MyCounted = true},
			LeakedEnemy{.MyEnemyId = "enemy_a", .MySourcePlayer = "0", .MyCounted = false}};
		std::array field{
			EnemySpawn{.MyTime = 8, .MyDefinition = CombatDefinition{.MyId = "enemy_a"}, .MyModifiers = EnemySpawnModifiers{.MyHealth = 2}, .MySourcePlayer = "0", .MyBounty = BountyReward{.MyCoins = 8, .MyOwnerId = "0"}},
			EnemySpawn{.MyTime = 9, .MyDefinition = CombatDefinition{.MyId = "enemy_a"}, .MyModifiers = EnemySpawnModifiers{.MyHealth = 3}, .MySourcePlayer = "0", .MyBounty = BountyReward{.MyCoins = 9, .MyOwnerId = "0"}}};
		residual.MyPendingEnemies = {PendingEnemy{.MyEnemyId = "enemy_a", .MyTime = 9, .MySourcePlayer = "0"}, PendingEnemy{.MyEnemyId = "enemy_a", .MyTime = 8, .MySourcePlayer = "0"}};
		const auto relay = plan ? PlanUniteRelay(*plan, residual, scene % 8 == 0, players, field, enemies, scene % 4 == 0 ? 1U : 2U) : std::nullopt;
		Plan(relay); std::cout << ',';
		if (plan) Losses(UniteSurvivors(*plan, residual)); else std::cout << "null";
		std::cout << ",[";
		BattlePlayerInput carry;
		for (unsigned i = 0; i < 5; ++i) carry.MyUnits.emplace_back(AllyDeployment{.MyPieceUid = i + 1, .MyKind = i >= 3 ? UnitKind::TOKEN : UnitKind::OPERATOR});
		ApplyUniteCarry(carry, results[0]);
		for (std::size_t i = 0; i < carry.MyUnits.size(); ++i)
		{
			if (i) std::cout << ',';
			const auto& c = carry.MyUnits[i].MyCarry;
			if (!c) { std::cout << "null"; continue; }
			std::cout << '['; if (c->MyHealthRatio) std::cout << *c->MyHealthRatio; else std::cout << "null";
			std::cout << ','; if (c->MySp) std::cout << *c->MySp; else std::cout << "null";
			std::cout << ',' << c->MyDown << ']';
		}
		std::cout << "]]";
	}
	std::cout << ']';
}
