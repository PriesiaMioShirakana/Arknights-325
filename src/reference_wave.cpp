#include <stronghold/adapters/reference_wave.hpp>

namespace Stronghold
{
	namespace
	{
		void ValidateRect(FieldRect _rect)
		{
			if (_rect.MyFirstRow < 0 || _rect.MyLastRow >= FieldRows || _rect.MyFirstColumn < 0 ||
				_rect.MyLastColumn >= FieldColumns || _rect.MyFirstRow > _rect.MyLastRow || _rect.MyFirstColumn > _rect.MyLastColumn)
				throw std::invalid_argument("invalid wave rectangle");
		}

		WorldPoint Clamp(WorldPoint _point, FieldRect _rect, double _margin = 0)
		{
			if (!std::isfinite(_point.MyX) || !std::isfinite(_point.MyY)) throw std::invalid_argument("non-finite wave point");
			return WorldPoint{.MyX = std::clamp(_point.MyX, _rect.MyFirstColumn - _margin, _rect.MyLastColumn + _margin),
				.MyY = std::clamp(_point.MyY, _rect.MyFirstRow - _margin, _rect.MyLastRow + _margin)};
		}

		double Multiplier(double _value, bool _positive = false)
		{
			return !std::isfinite(_value) || std::isless(_value, 0) || (_positive && !std::isgreater(_value, 0)) ? 1 : _value;
		}
	}

	CombatRoute WaveRoute::Compile(FieldRect _rect) const
	{
		ValidateRect(_rect);
		CombatRoute route{.MyStart = Clamp(MyStart, _rect, 0.5), .MyEnd = Clamp(MyEnd, _rect)};
		route.MySteps.reserve(MySteps.size());
		for (auto step : MySteps)
		{
			if (step.MyKind == RouteStepKind::MOVE || step.MyKind == RouteStepKind::APPEAR)
				step.MyPosition = Clamp(step.MyPosition, _rect);
			else step.MyPosition = {};
			step.MyWaitSeconds = std::isfinite(step.MyWaitSeconds) ? std::max(0.0, step.MyWaitSeconds) : 0;
			route.MySteps.emplace_back(step);
		}
		return route;
	}

	const EnemyRecord& WaveRecord::Enemy(std::string_view _id) const
	{
		const auto found = std::ranges::lower_bound(MyEnemyOverrides, _id, {}, &EnemyRecord::MyId);
		return found != MyEnemyOverrides.end() && found->MyId == _id ? *found : ReferenceEnemy(_id);
	}

	const WaveBranch& WaveRecord::Branch(std::string_view _id) const
	{
		const auto found = std::ranges::lower_bound(MyBranches, _id, {}, &WaveBranch::MyId);
		if (found == MyBranches.end() || found->MyId != _id) throw std::out_of_range("unknown wave branch");
		return *found;
	}

	std::vector<CombatRoute> WaveRecord::MakeGroundRoutes(std::span<const std::size_t> _usedRoutes, FieldRect _rect) const
	{
		ValidateRect(_rect);
		const bool selected = std::ranges::any_of(_usedRoutes, [&](std::size_t _index)
		{ return _index < MyRoutes.size() && !MyRoutes[_index].MyFlying; });
		std::vector<CombatRoute> result;
		result.reserve(MyRoutes.size());
		for (std::size_t i = 0; i < MyRoutes.size(); ++i)
			if (!MyRoutes[i].MyFlying && (!selected || std::ranges::find(_usedRoutes, i) != _usedRoutes.end()))
				result.emplace_back(MyRoutes[i].Compile(_rect));
		return result;
	}

	std::vector<EnemySpawn> WaveRecord::MakeSpawns(
		std::span<const BattlePlayerInput> _players,
		FieldRect _rect,
		EnemyMultipliers _multipliers
	) const
	{
		ValidateRect(_rect);
		if (_players.empty() || MyRoutes.empty()) throw std::invalid_argument("wave requires players and routes");
		std::size_t count = 0;
		for (const auto& group : MySpawns)
			if (group.MyAction == WaveAction::SPAWN) count += group.MyCount;
		std::vector<EnemySpawn> result;
		result.reserve(count);
		for (const auto& group : MySpawns)
		{
			if (group.MyAction != WaveAction::SPAWN) continue;
			const auto& route = MyRoutes[group.MyRoute < MyRoutes.size() ? group.MyRoute : 0];
			const auto player = std::ranges::find_if(_players, [&](const BattlePlayerInput& _player)
			{
				return _player.MyRightHalf.value_or(_player.MyMirrorDeployment) == std::isgreaterequal(route.MyStart.MyX, 11);
			});
			const auto& owner = player != _players.end() ? *player : _players.front();
			const auto& record = Enemy(group.MyEnemyId);
			auto definition = record.MakeDefinition();
			auto& stats = definition.MyStats;
			stats.MyMaxHealth *= Multiplier(_multipliers.MyHealth, true);
			stats.MyAttack *= Multiplier(_multipliers.MyAttack);
			stats.MyDefense *= Multiplier(_multipliers.MyDefense);
			stats.MyResistance *= Multiplier(_multipliers.MyResistance);
			stats.MyMoveSpeed *= Multiplier(_multipliers.MySpeed);
			const auto compiled = route.Compile(_rect);
			for (unsigned i = 0; i < group.MyCount; ++i)
				result.emplace_back(EnemySpawn{.MyTime = std::max(0.0, group.MyTime) + i * std::max(0.0, group.MyInterval),
					.MyOwnerId = owner.MyPlayerId, .MyDefinition = definition, .MyRoute = compiled, .MyLifeCost = record.MyLifeCost,
					.MyCounted = record.MyCounted && !group.MyUnharmful && group.MyTag == EnemySpawnTag::NONE, .MyTag = group.MyTag});
		}
		std::ranges::stable_sort(result, {}, &EnemySpawn::MyTime);
		return result;
	}
}
