#include <stronghold/domain/wave_generation.hpp>

namespace Stronghold
{
	std::size_t WaveGenerator::RouteByMotion(const WaveRecord& _wave, bool _flying, WaveSide _side) const
	{
		for (const bool preferSide : {true, false})
			for (std::size_t i = 0; i < _wave.MyRoutes.size(); ++i)
			{
				const auto& route = _wave.MyRoutes[i];
				if (route.MyFlying != _flying) continue;
				if (!preferSide || _side == WaveSide::ANY || (_side == WaveSide::LEFT ? std::isless(route.MyEnd.MyX, 10) : std::isgreater(route.MyEnd.MyX, 10))) return i;
			}
		return 0;
	}

	std::optional<std::size_t> WaveGenerator::Host(const WaveRecord* _wave, bool _flying, bool _token, bool _firstSpawnFallback) const
	{
		if (!_wave) return std::nullopt;
		const auto slot = _token ? (_flying ? WaveSlot::TF : WaveSlot::T) : (_flying ? WaveSlot::NF : WaveSlot::N);
		const auto wanted = std::ranges::find(_MyRules.MyTemplateSlots, slot, &WavePlaceholder::MySlot);
		std::optional<std::size_t> first;
		for (std::size_t i = 0; i < _wave->MySpawns.size(); ++i)
		{
			const auto& spawn = _wave->MySpawns[i];
			if (spawn.MyAction != WaveAction::SPAWN) continue;
			if (!first) first = i;
			if (wanted != _MyRules.MyTemplateSlots.end() && spawn.MyEnemyId == wanted->MyEnemyId) return i;
		}
		return _firstSpawnFallback ? first : _wave->MySpawns.empty() ? std::nullopt : std::optional<std::size_t>{0};
	}

	WavePreview WaveGenerator::Preview(const WaveRecord* _wave, std::string_view _enemy, std::size_t _route, std::optional<bool> _leader) const
	{
		const auto* enemy = Enemy(_enemy);
		const auto start = _wave && _route < _wave->MyRoutes.size() ? std::optional(_wave->MyRoutes[_route].MyStart) : std::nullopt;
		const bool leader = _leader.value_or(start && std::islessequal(start->MyY, 8));
		return WavePreview{.MyUpper = start && std::isgreaterequal(start->MyY + (leader ? 13 : 6), 18), .MyStart = start,
			.MyFlying = enemy && enemy->MyFlying, .MyElite = enemy && enemy->MyRank != EnemyRank::NORMAL};
	}

	WavePlan WaveGenerator::WithBounties(const WavePlan& _plan, unsigned _round, std::span<const WaveBounty> _bounties,
		std::string_view _player, WaveSide _side, bool _retimeOwn) const
	{
		struct Group
		{
			std::optional<std::size_t> MyHost{};
			std::vector<std::size_t> MyItems{}; // 悬赏输入下标，每个单位一项。
			std::vector<std::size_t> MyMarkers{}; // 0 是原动作单位，下标 + 1 是悬赏单位。
			double MyTime{};
			double MyStep{};
			bool MyRetime{};
		};
		const auto* wave = Template(_plan.MyTemplateId);
		std::vector<Group> groups;
		groups.reserve(2); // 普通与飞行两种宿主；仍可自然扩容，不截断输入。
		for (std::size_t i = 0; i < _bounties.size(); ++i)
		{
			const auto* enemy = Enemy(_bounties[i].MyEnemyId);
			if (!enemy) continue;
			const auto host = Host(wave, enemy->MyFlying);
			auto it = std::ranges::find(groups, host, &Group::MyHost);
			if (it == groups.end()) { groups.emplace_back(Group{.MyHost = host}); it = groups.end() - 1; }
			it->MyItems.insert(it->MyItems.end(), std::clamp(_bounties[i].MyCount, 1U, 20U), i);
		}
		for (auto& group : groups)
		{
			const auto* host = group.MyHost ? &wave->MySpawns[*group.MyHost] : nullptr;
			group.MyTime = host ? std::max(0.0, host->MyTime) : 4;
			const double window = host ? std::max(1U, host->MyCount) * std::max(0.0, host->MyInterval) : 0;
			const auto action = group.MyHost ? std::ranges::find(_plan.MyActions, *group.MyHost, &WavePlannedAction::MyIndex) : _plan.MyActions.end();
			const unsigned own = action != _plan.MyActions.end() && action->MyValid && action->MyServer ? action->MyCount : 0;
			group.MyRetime = own > 0;
			group.MyMarkers.reserve(own + group.MyItems.size());
			group.MyMarkers.resize(own, 0);
			for (std::size_t i = 0; i < group.MyItems.size(); ++i)
			{
				// 原算法以每次插入时的当前长度取整；不能先算最终长度上的等分位置。
				const auto at = own ? static_cast<std::size_t>(std::floor(static_cast<double>(i + 1) * static_cast<double>(group.MyMarkers.size()) / static_cast<double>(group.MyItems.size() + 1))) : group.MyMarkers.size();
				group.MyMarkers.emplace(group.MyMarkers.begin() + static_cast<std::ptrdiff_t>(at), group.MyItems[i] + 1);
			}
			group.MyStep = std::isgreater(window, 0) ? std::max(window / static_cast<double>(group.MyMarkers.size()), 0.05 * window) : 0;
		}
		WavePlan output{.MyTemplateId = _plan.MyTemplateId, .MyTimeLimit = _plan.MyTimeLimit, .MyPick = _plan.MyPick, .MyActions = _plan.MyActions};
		std::size_t capacity = _plan.MySpawns.size();
		for (const auto& group : groups) capacity += group.MyMarkers.size();
		output.MySpawns.reserve(capacity);
		const auto runs = [](const Group& _group, auto _visit)
		{
			for (std::size_t start = 0; start < _group.MyMarkers.size();)
			{
				auto end = start + 1;
				while (end < _group.MyMarkers.size() && _group.MyMarkers[end] == _group.MyMarkers[start]) ++end;
				_visit(_group.MyMarkers[start], start, end - start);
				start = end;
			}
		};
		for (const auto& spawn : _plan.MySpawns)
		{
			const auto group = std::ranges::find_if(groups, [&](const Group& _group)
			{ return !spawn.MyBounty && _group.MyRetime && _group.MyHost == spawn.MyActionIndex; });
			if (!_retimeOwn || group == groups.end()) { output.MySpawns.emplace_back(spawn); continue; }
			runs(*group, [&](std::size_t _marker, std::size_t _start, std::size_t _length)
			{
				if (_marker) return;
				auto copy = spawn;
				copy.MyTime = group->MyTime + static_cast<double>(_start) * group->MyStep;
				copy.MyCount = static_cast<unsigned>(_length);
				copy.MyInterval = _length > 1 ? group->MyStep : 0;
				output.MySpawns.emplace_back(std::move(copy));
			});
		}
		const auto* round = Round(_round);
		for (const auto& group : groups)
			runs(group, [&](std::size_t _marker, std::size_t _start, std::size_t _length)
			{
				if (!_marker) return;
				const auto& bounty = _bounties[_marker - 1];
				const auto& enemy = *Enemy(bounty.MyEnemyId);
				const auto* host = group.MyHost ? &wave->MySpawns[*group.MyHost] : nullptr;
				auto route = host ? host->MyRoute : wave ? RouteByMotion(*wave, enemy.MyFlying, _side) : 0;
				if (wave && (route >= wave->MyRoutes.size() || wave->MyRoutes[route].MyFlying != enemy.MyFlying ||
					(_side != WaveSide::ANY && !(_side == WaveSide::LEFT ? std::isless(wave->MyRoutes[route].MyEnd.MyX, 10) : std::isgreater(wave->MyRoutes[route].MyEnd.MyX, 10)))))
					route = RouteByMotion(*wave, enemy.MyFlying, _side);
				output.MySpawns.emplace_back(WavePlannedSpawn{.MyTime = group.MyTime + static_cast<double>(_start) * group.MyStep, .MyEnemyId = enemy.MyId,
					.MyRoute = route, .MyCount = static_cast<unsigned>(_length), .MyInterval = _length > 1 ? group.MyStep : 0,
					.MyScale = round ? round->MyScale : WaveScale{},
					.MySlot = enemy.MyFlying ? (enemy.MyRank == EnemyRank::ELITE ? WaveSlot::EF : WaveSlot::NF) : (enemy.MyRank == EnemyRank::ELITE ? WaveSlot::E : WaveSlot::N),
					.MyTag = EnemySpawnTag::BOUNTY, .MyActionIndex = group.MyHost.value_or(std::numeric_limits<std::size_t>::max()),
					.MyPreview = Preview(wave, enemy.MyId, route, wave && (wave->MyKind == WaveKind::BOSS || wave->MyKind == WaveKind::HIDDEN)),
					.MyBounty = true, .MyBountyId = bounty.MyId, .MyOwnerPlayer = std::string(_player),
					.MyCoins = bounty.MyPerfect ? 0 : std::max(std::int64_t{0}, bounty.MyCoins), .MyRewardOwner = std::string(_player)});
			});
		return output;
	}

	WavePlan WaveGenerator::BuildUnite(std::span<const WaveLeak> _leaks, unsigned _helpers) const
	{
		if (_helpers < 1 || _helpers > 2) throw std::invalid_argument("unite requires one or two helpers");
		WavePlan output{.MyTemplateId = _MyRules.MyUniteTemplates[_helpers - 1]};
		const auto* wave = Template(output.MyTemplateId);
		struct Owner { std::string_view MyId{}; std::vector<std::size_t> MyLeaks{}; };
		struct Group { std::optional<std::size_t> MyHost{}; bool MyFlying{}; std::vector<Owner> MyOwners{}; };
		std::vector<Group> groups;
		groups.reserve(4);
		for (std::size_t i = 0; i < _leaks.size(); ++i)
		{
			const auto& leak = _leaks[i]; const auto* enemy = Enemy(leak.MyEnemyId);
			if (!enemy) continue;
			const auto host = Host(wave, enemy->MyFlying, leak.MyToken || enemy->MyTokenOnly, false);
			auto group = std::ranges::find(groups, host, &Group::MyHost);
			if (group == groups.end()) { groups.emplace_back(Group{.MyHost = host, .MyFlying = enemy->MyFlying}); group = groups.end() - 1; }
			auto owner = std::ranges::find(group->MyOwners, leak.MySourcePlayer, &Owner::MyId);
			if (owner == group->MyOwners.end()) { group->MyOwners.emplace_back(Owner{.MyId = leak.MySourcePlayer}); owner = group->MyOwners.end() - 1; }
			owner->MyLeaks.emplace_back(i);
		}
		output.MySpawns.reserve(_leaks.size());
		for (const auto& group : groups)
		{
			const auto* host = group.MyHost ? &wave->MySpawns[*group.MyHost] : nullptr;
			const double time = host ? std::max(0.0, host->MyTime) : 3;
			const double window = host ? std::max(1U, host->MyCount) * std::max(0.0, host->MyInterval) : 40;
			std::size_t most = 1;
			for (const auto& owner : group.MyOwners) most = std::max(most, owner.MyLeaks.size());
			const double step = std::min(std::max(window / static_cast<double>(most), 0.05 * window), 5.0);
			auto route = host ? host->MyRoute : wave ? RouteByMotion(*wave, group.MyFlying, WaveSide::ANY) : 0;
			if (wave && route >= wave->MyRoutes.size()) route = 0;
			for (std::size_t k = 0; k < group.MyOwners.size(); ++k)
			{
				const auto& owner = group.MyOwners[k];
				for (std::size_t i = 0; i < owner.MyLeaks.size(); ++i)
				{
					const auto& leak = _leaks[owner.MyLeaks[i]]; const auto& enemy = *Enemy(leak.MyEnemyId);
					const auto mods = leak.MyModifiers.value_or(EnemySpawnModifiers{});
					constexpr std::array<std::string_view, 9> Slots{"", "N", "E", "S", "NF", "EF", "SF", "T", "TF"};
					const auto slot = std::ranges::find(Slots, mods.MySlot);
					const auto originalSlot = slot == Slots.end() ? WaveSlot::NONE : static_cast<WaveSlot>(slot - Slots.begin());
					output.MySpawns.emplace_back(WavePlannedSpawn{.MyTime = time + static_cast<double>(k) * 0.5 + static_cast<double>(i) * step, .MyEnemyId = enemy.MyId,
						.MyRoute = route, .MyScale = WaveScale{.MyHealth = mods.MyHealth.value_or(1), .MyAttack = mods.MyAttack.value_or(1),
							.MySpeed = mods.MySpeed.value_or(1), .MyDefense = mods.MyDefense.value_or(1), .MyResistance = mods.MyResistance.value_or(1), .MySupplyHealth = mods.MySupplyHealth},
						.MySlot = leak.MyModifiers ? originalSlot : enemy.MyFlying ? (enemy.MyRank == EnemyRank::ELITE ? WaveSlot::EF : WaveSlot::NF) : (enemy.MyRank == EnemyRank::ELITE ? WaveSlot::E : WaveSlot::N),
						.MyTag = leak.MyBounty ? EnemySpawnTag::BOUNTY : EnemySpawnTag::NONE, .MyActionIndex = group.MyHost.value_or(std::numeric_limits<std::size_t>::max()), .MyPreview = Preview(wave, enemy.MyId, route),
						.MyBounty = leak.MyBounty, .MyBountyId = leak.MyBountyId, .MySourcePlayer = leak.MySourcePlayer,
						.MyCoins = std::max(std::int64_t{0}, leak.MyCoins), .MyRewardOwner = leak.MyRewardOwner.empty() ? leak.MySourcePlayer : leak.MyRewardOwner, .MyOriginalModifiers = leak.MyModifiers});
				}
			}
		}
		std::ranges::stable_sort(output.MySpawns, {}, &WavePlannedSpawn::MyTime);
		return output;
	}
}
