#include <stronghold/simulation/battle.hpp>
#include <stronghold/domain/bonds.hpp>

namespace Stronghold
{
	double Battle::AddBondLayers(std::string_view _playerId, std::string_view _bondId, double _amount, LayerGainOptions _options)
	{
		if (!_MyInput.MyLayerGainsEnabled.value_or(!_MyInput.MyBossBattle) || _bondId.empty() || !std::isfinite(_amount) || !std::isgreater(_amount, 0)) return 0;
		const auto player = std::ranges::find(_MyPlayers, _playerId, &BattlePlayerState::MyPlayerId);
		if (player == _MyPlayers.end()) return 0;
		const auto owner = static_cast<std::size_t>(player - _MyPlayers.begin());
		const auto before = std::ranges::find(player->MyBonds, _bondId, &BondLayer::MyId);
		const auto gainBefore = std::ranges::find(player->MyLayerGains, _bondId, &BondLayer::MyId);
		const double live = before != player->MyBonds.end() ? before->MyLayers : gainBefore != player->MyLayerGains.end() ? gainBefore->MyLayers : 0;
		if (!std::isgreater(LayerGainRoom(live, std::numeric_limits<double>::infinity()), 0)) return 0;
		auto tile = _options.MyTile;
		if (tile && (!std::isfinite(tile->MyX) || !std::isfinite(tile->MyY))) throw std::invalid_argument("invalid layer source tile");
		if (_options.MySource)
		{
			const auto& unit = Unit(_options.MySource);
			const bool justDied = !unit.MyAlive && !std::islessgreater(unit.MyRemovedAt, Time());
			const bool continuingGarrison = _MyInput.MyGarrisonEffectsAfterExit && _options.MyReason == "garrison" && GarrisonSourceActive(unit.MyId);
			if (!tile && ((unit.MyAlive && !unit.MyHidden) || justDied || continuingGarrison))
				tile = unit.MySide == UnitSide::ALLY ? (unit.MyDefinition.MyYanyou ? unit.MyHome : RulePosition(unit)) : WorldPoint{.MyX = std::floor(unit.MyPosition.MyX + 0.5), .MyY = std::floor(unit.MyPosition.MyY + 0.5)};
		}
		// 钩子可能创建新的盟约条目；复制小 ID，且不跨钩子持有 vector 迭代器或字符串视图。
		const std::string bondId(_bondId);
		ContentEvent event{.MyKind = ContentEventKind::LAYER_GAIN, .MySource = _options.MySource, .MyAmount = _amount,
			.MyPlayer = owner, .MyBondId = bondId, .MyReason = _options.MyReason, .MySourceTile = tile};
		NotifyContent(event);
		if (event.MyCancel || !std::isfinite(event.MyAmount) || !std::isgreater(event.MyAmount, 0)) return 0;
		const auto add = LayerGainRoom(live, event.MyAmount);
		if (!std::isgreater(add, 0)) return 0;
		auto& gains = player->MyLayerGains;
		auto gain = std::ranges::find(gains, bondId, &BondLayer::MyId);
		if (gain == gains.end()) { gains.emplace_back(BondLayer{.MyId = bondId}); gain = std::prev(gains.end()); }
		gain->MyLayers += add;
		const auto current = std::ranges::find(player->MyBonds, bondId, &BondLayer::MyId);
		if (current != player->MyBonds.end())
		{
			current->MyLayers += add;
			player->MyTopBondLayers = 0;
			for (const auto& bond : player->MyBonds) player->MyTopBondLayers = std::max(player->MyTopBondLayers, bond.MyLayers);
		}
		Emit(BattleEventKind::LAYERS_GAINED, _options.MySource, 0, add, std::nullopt, owner);
		_MyEvents.back().MyBondIndex = static_cast<std::size_t>(gain - gains.begin());
		return add;
	}

	void Battle::ValidateSpawnMetadata(const EnemySpawn& _spawn)
	{
		if (static_cast<unsigned>(_spawn.MyTag) > static_cast<unsigned>(EnemySpawnTag::DUCK))
			throw std::invalid_argument("invalid enemy spawn tag");
		if (_spawn.MyBounty && (!std::isfinite(_spawn.MyBounty->MyCoins) || std::isless(_spawn.MyBounty->MyCoins, 0)))
			throw std::invalid_argument("invalid bounty reward");
		if (_spawn.MyModifiers)
		{
			const auto& mods = *_spawn.MyModifiers;
			for (const auto value : {mods.MyHealth, mods.MyAttack, mods.MyDefense, mods.MyResistance, mods.MySpeed, mods.MySupplyHealth})
				if (value && (!std::isfinite(*value) || std::isless(*value, 0))) throw std::invalid_argument("invalid spawn modifier metadata");
		}
	}

	double Battle::AddCoins(std::string_view _playerId, double _amount)
	{
		if (!std::isfinite(_amount) || !std::isgreater(_amount, 0)) return 0;
		for (std::size_t i = 0; i < _MyPlayers.size(); ++i)
		{
			auto& player = _MyPlayers[i];
			if (player.MyPlayerId != _playerId || !std::isfinite(player.MyCoins + _amount)) continue;
			player.MyCoins += _amount;
			Emit(BattleEventKind::COINS_GAINED, 0, 0, _amount, std::nullopt, i);
			return _amount;
		}
		return 0;
	}

	void Battle::PayBounty(const CombatUnit& _unit, UnitId _killer)
	{
		const auto& reward = SpawnDefinition(_unit.MySpawnIndex).MyBounty;
		if (!reward || !std::isgreater(reward->MyCoins, 0)) return;
		if (_killer && Unit(_killer).MySide == UnitSide::ALLY && Unit(_killer).MyOwner != NoPlayer)
		{
			(void)AddCoins(_MyPlayers[Unit(_killer).MyOwner].MyPlayerId, reward->MyCoins);
			return;
		}
		const auto original = reward->MyOwnerId.empty() ? std::string_view(_MyPlayers[_unit.MyOwner].MyPlayerId) : std::string_view(reward->MyOwnerId);
		for (const auto& player : _MyPlayers)
			if (player.MyPlayerId == original) { (void)AddCoins(original, reward->MyCoins); return; }
		// 无来源伤害：原悬赏拥有者不在联防场时，交给击倒地点所在半场的玩家。
		const bool right = std::isgreaterequal(std::floor(_unit.MyPosition.MyX + 0.5), 11);
		for (std::size_t i = 0; i < _MyPlayers.size(); ++i)
			if (_MyInput.MyPlayers[i].MyRightHalf.value_or(_MyInput.MyPlayers[i].MyMirrorDeployment) == right)
			{ (void)AddCoins(_MyPlayers[i].MyPlayerId, reward->MyCoins); return; }
		(void)AddCoins(_MyPlayers.front().MyPlayerId, reward->MyCoins);
	}
}
