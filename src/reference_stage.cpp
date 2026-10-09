#include <stronghold/adapters/reference_stage.hpp>

namespace Stronghold
{
	StageSetup StageRecord::Prepare(FieldRect _rect, std::span<const BattlePlayerInput> _players, std::span<const DeviceOverride> _overrides) const
	{
		if (MyTiles.size() != FieldTiles || _players.empty() || !FieldGrid::InBounds(_rect.MyFirstRow, _rect.MyFirstColumn) ||
			!FieldGrid::InBounds(_rect.MyLastRow, _rect.MyLastColumn) || _rect.MyFirstRow > _rect.MyLastRow || _rect.MyFirstColumn > _rect.MyLastColumn)
			throw std::invalid_argument("invalid stage setup");
		StageSetup setup;
		std::ranges::copy(MyTiles, setup.MyField.MyTiles.begin());
		setup.MyField.MyRect = _rect;
		setup.MyField.MyTerrainRules = MyRules;
		setup.MyDevices.reserve(MyDevices.size());
		setup.MyTurrets.reserve(MyDevices.size());
		const auto inside = [&](FieldPoint _point)
		{ return _point.MyRow >= _rect.MyFirstRow && _point.MyRow <= _rect.MyLastRow && _point.MyColumn >= _rect.MyFirstColumn && _point.MyColumn <= _rect.MyLastColumn; };
		for (const auto& device : MyDevices)
		{
			const auto owner = _players.size() == 1 ? _players.begin() : std::ranges::find_if(_players, [&](const BattlePlayerInput& _player) { return _player.MyRightHalf.value_or(_player.MyMirrorDeployment) == (device.MyPosition.MyColumn >= 11); });
			const auto& player = owner == _players.end() ? _players.front() : *owner;
			std::optional<bool> mine, global;
			if (device.MyAlias.find('#') != std::string_view::npos)
				for (const auto& setting : _overrides)
					if (setting.MyAlias == device.MyAlias)
					{
						if (setting.MyPlayerId.empty()) global = setting.MyActive;
						else if (setting.MyPlayerId == player.MyPlayerId) mine = setting.MyActive;
					}
			const auto override = mine ? mine : global;
			const bool active = override.value_or(device.MyActive);
			if ((device.MyRole == "platform" || device.MyRole == "mound") && inside(device.MyPosition))
			{
				const auto key = static_cast<std::size_t>(FieldGrid::Key(device.MyPosition.MyRow, device.MyPosition.MyColumn));
				setup.MyField.MyTiles[key].MyElevated = device.MyActive;
				if (override && *override != device.MyActive) setup.MyField.MyTiles[key].MyDeploymentElevation = *override;
				if (active) setup.MyField.MyInitialObstacles[key] |= static_cast<unsigned char>(ObstacleKind::BLOCK);
			}
			if (!active && !(device.MyRole == "crate" && device.MyActive)) continue;
			if (device.MyRole == "blower" && device.MyAirflow)
			{
				for (const auto point : device.MyRange)
					if (inside(point)) setup.MyField.MyAirflow[static_cast<std::size_t>(FieldGrid::Key(point.MyRow, point.MyColumn))] = device.MyAirflow;
			}
			else if (device.MyRole == "crate" && inside(device.MyPosition))
			{
				const auto health = device.MyActive && device.MyId == "trap_1105_accrate" ? 100 : device.MyStats.MyMaxHealth;
				setup.MyDevices.emplace_back(DeviceSpawn{.MyId = std::string(device.MyId), .MyPosition = WorldPoint{
					.MyX = static_cast<double>(device.MyPosition.MyColumn), .MyY = static_cast<double>(device.MyPosition.MyRow)},
					.MyHealth = health, .MyObstacle = true, .MyRemoveOnDeploy = !active, .MyAfterDeployment = !device.MyActive});
			}
			else if (device.MyRole == "turret")
			{
				std::bitset<FieldTiles> range;
				for (const auto point : device.MyRange)
					if (inside(point)) range.set(static_cast<std::size_t>(FieldGrid::Key(point.MyRow, point.MyColumn)));
				if (range.none()) continue;
				const auto& stats = device.MyStats;
				setup.MyTurrets.emplace_back(TurretSpawn{.MyAlias = std::string(device.MyAlias), .MyDevice = DeviceSpawn{
					.MyId = std::string(device.MyId), .MyPosition = WorldPoint{.MyX = static_cast<double>(device.MyPosition.MyColumn), .MyY = static_cast<double>(device.MyPosition.MyRow)},
					.MyHealth = stats.MyMaxHealth, .MyDefense = stats.MyDefense, .MyResistance = stats.MyResistance, .MyBlockCount = 0,
					.MyAttack = stats.MyAttack, .MyAttackTime = stats.MyBaseAttackTime, .MyAttackSpeed = stats.MyAttackSpeed},
					.MyPlayerId = player.MyPlayerId, .MyRange = range, .MyAttackSpeedPerLayer = device.MySkill.Number("attack_speed_per_stack"),
					.MyMaxAttackSpeedBonus = device.MySkill.Number("max_attack_speed", std::numeric_limits<double>::infinity()),
					.MyFragilityPerLayer = device.MySkill.Number("damage_scale_per_stack"),
					.MyMaxDamageScale = device.MySkill.Number("max_damage_scale", std::numeric_limits<double>::infinity()),
					.MyFragilityDuration = device.MyFragilityDuration});
			}
			// 水上平台、沙尘暴、草丛、封闭地块在原 sim 中也没有战斗动作，保留原始元数据。
		}
		return setup;
	}

	void StageRecord::Apply(BattleInput& _input, FieldRect _rect, std::span<const DeviceOverride> _overrides) const
	{
		auto setup = Prepare(_rect, _input.MyPlayers, _overrides);
		_input.MyField = std::move(setup.MyField);
		_input.MyDevices = std::move(setup.MyDevices);
		_input.MyTurrets = std::move(setup.MyTurrets);
	}
}
