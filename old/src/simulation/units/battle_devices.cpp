#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::SetBondLayers(std::string_view _playerId, std::string_view _bondId, double _layers)
	{
		if (_bondId.empty() || !std::isfinite(_layers) || std::isless(_layers, 0)) throw std::invalid_argument("invalid bond layers");
		auto& player = _MyPlayers[Owner(_playerId)];
		if (Finished()) return;
		const auto found = std::ranges::find(player.MyBonds, _bondId, &BondLayer::MyId);
		if (found == player.MyBonds.end()) player.MyBonds.emplace_back(std::string(_bondId), _layers);
		else found->MyLayers = _layers;
		// 修改时重算最高层数，炮台热路径只读取缓存。层数降低也必须更新。
		player.MyTopBondLayers = 0;
		for (const auto& bond : player.MyBonds) player.MyTopBondLayers = std::max(player.MyTopBondLayers, bond.MyLayers);
	}

	double BattleCore::BondLayers(std::string_view _playerId, std::string_view _bondId) const
	{
		const auto& bonds = _MyPlayers[Owner(_playerId)].MyBonds;
		const auto found = std::ranges::find(bonds, _bondId, &BondLayer::MyId);
		return found == bonds.end() ? 0 : found->MyLayers;
	}

	UnitId BattleCore::SpawnTurret(TurretSpawn _spawn)
	{
		const auto owner = Owner(_spawn.MyPlayerId);
		if (_spawn.MyAlias.empty() || !std::isfinite(_spawn.MyAttackSpeedPerLayer) || !std::isfinite(_spawn.MyFragilityPerLayer) ||
			std::isnan(_spawn.MyMaxAttackSpeedBonus) || std::isnan(_spawn.MyMaxDamageScale) ||
			!std::isfinite(_spawn.MyFragilityDuration) || std::isless(_spawn.MyFragilityDuration, 0) ||
			!std::isfinite(_spawn.MyDevice.MyPosition.MyX) || !std::isfinite(_spawn.MyDevice.MyPosition.MyY))
			throw std::invalid_argument("invalid turret definition");
		if (!_MyStarted || Finished()) return 0;
		const auto found = std::ranges::find(_MyTurrets, _spawn.MyAlias, [](const TurretState& _state) -> const std::string& { return _state.MyDefinition.MyAlias; });
		if (found != _MyTurrets.end()) return found->MyUnit;
		if (!_MyGrid) _MyGrid.emplace();
		const auto& rect = _MyGrid->Rect();
		for (int key = 0; key < FieldTiles; ++key)
			if (!_MyGrid->InRect(key / FieldColumns, key % FieldColumns)) _spawn.MyRange.reset(static_cast<std::size_t>(key));
		if (_spawn.MyRange.none()) return 0;
		std::optional<WorldPoint> best;
		std::array<double, 4> bestScore{};
		for (int row = rect.MyFirstRow; row <= rect.MyLastRow; ++row)
			for (int col = rect.MyFirstColumn; col <= rect.MyLastColumn; ++col)
			{
				const auto tile = WorldPoint{.MyX = static_cast<double>(col), .MyY = static_cast<double>(row)};
				if (ReservedTile(tile)) continue;
				const bool exact = !std::islessgreater(tile.MyX, _spawn.MyDevice.MyPosition.MyX) && !std::islessgreater(tile.MyY, _spawn.MyDevice.MyPosition.MyY);
				if (!exact && _MyGrid->Tile(row, col).MyBuild != FieldBuild::NONE) continue;
				const std::array score{exact ? 0.0 : 1.0, !exact && _MyGrid->GroundPassable(row, col, true) ? 1.0 : 0.0,
					Distance(tile, _spawn.MyDevice.MyPosition), static_cast<double>(FieldGrid::Key(row, col))};
				bool better = !best;
				if (best)
					for (std::size_t i = 0; i < score.size(); ++i)
					{
						if (std::isless(score[i], bestScore[i] - 1e-9)) { better = true; break; }
						if (std::isgreater(score[i], bestScore[i] + 1e-9)) break;
					}
				if (better) { best = tile; bestScore = score; }
			}
		if (!best) return 0;
		_spawn.MyDevice.MyPosition = *best;
		_spawn.MyDevice.MyBlockCount = 0;
		_spawn.MyDevice.MyObstacle = false;
		_spawn.MyDevice.MyPlayerId = _spawn.MyPlayerId;
		const auto id = SpawnDevice(_spawn.MyDevice);
		if (!id) return 0;
		auto& unit = _MyUnits[Index(id)];
		unit.MyOwner = owner;
		unit.MyFacing = _MyInput.MyPlayers[owner].MyMirrorDeployment ? Facing::LEFT : Facing::RIGHT;
		unit.MyDefinition.MyAttack = AttackProfile{.MyDamageType = DamageType::ARTS, .MyCanHitFlying = true,
			.MyRanged = true, .MyProjectileSpeed = 11, .MyNoHeal = true};
		unit.MyRangeMask = _spawn.MyRange;
		_MyTurrets.emplace_back(std::move(_spawn), id);
		return id;
	}

	void BattleCore::TickTurrets()
	{
		// 原 devices.tickTurret 在普通攻击和弹道阶段之后行动；同帧新发射的弹道下一帧开始移动。
		for (std::size_t i = 0; i < _MyTurrets.size() && !Finished(); ++i)
		{
			auto& state = _MyTurrets[i];
			auto& unit = _MyUnits[Index(state.MyUnit)];
			if (!unit.MyAlive) continue;
			state.MyCooldown -= BattleClock::StepSeconds;
			if (std::isgreater(state.MyCooldown, 0) || unit.MyStatuses.Has(CombatStatus::STUN)) continue;
			const auto& definition = state.MyDefinition;
			const auto layers = _MyPlayers[unit.MyOwner].MyTopBondLayers;
			const auto aspd = definition.MyDevice.MyAttackSpeed + std::min(layers * definition.MyAttackSpeedPerLayer, definition.MyMaxAttackSpeedBonus);
			if (std::isgreater(std::abs(unit.MyDefinition.MyStats.MyAttackSpeed - aspd), 1e-9))
			{
				unit.MyDefinition.MyStats.MyAttackSpeed = aspd;
				Recalculate(unit);
			}
			unit.MyRangeMask = definition.MyRange;
			const auto targets = AllyTargets(unit);
			if (targets.empty()) { state.MyCooldown = 0; continue; }
			const auto fragile = std::min(1 + layers * definition.MyFragilityPerLayer, definition.MyMaxDamageScale) - 1;
			auto& profile = unit.MyDefinition.MyAttack;
			profile.MyOnHitStatus.reset();
			if (std::isgreater(fragile, 0) && std::isgreater(definition.MyFragilityDuration, 0))
			{
				profile.MyOnHitStatus = CombatStatus::FRAGILE;
				profile.MyOnHitApplication = StatusApplication{.MyDuration = definition.MyFragilityDuration, .MyValue = fragile};
			}
			Attack(unit, targets);
			state.MyCooldown = unit.MyStats.AttackInterval();
		}
	}
}
