#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::PrepareOwners()
	{
		for (const auto& player : _MyInput.MyPlayers)
			if (static_cast<unsigned>(player.MyKind) > static_cast<unsigned>(BattlePlayerKind::VIRTUAL))
				throw std::invalid_argument("invalid battle player kind");
		std::stable_partition(_MyInput.MyPlayers.begin(), _MyInput.MyPlayers.end(), [](const BattlePlayerInput& _player)
		{
			return _player.MyKind == BattlePlayerKind::PARTICIPANT;
		});
		_MyParticipantCount = std::ranges::count(_MyInput.MyPlayers, BattlePlayerKind::PARTICIPANT, &BattlePlayerInput::MyKind);
		_MyEnvironmentPlayer = "@environment";
		while (std::ranges::any_of(_MyInput.MyPlayers, [&](const BattlePlayerInput& _player)
		{
			return _player.MyPlayerId == _MyEnvironmentPlayer;
		})) _MyEnvironmentPlayer += ':';
		_MyInput.MyPlayers.emplace_back(BattlePlayerInput{
			.MyPlayerId = _MyEnvironmentPlayer,
			.MyKind = BattlePlayerKind::VIRTUAL});
		_MyPlayers.reserve(_MyInput.MyPlayers.size());
	}

	void BattleCore::ResolveOwnerPrincipals()
	{
		for (std::size_t i = 0; i < _MyPlayers.size(); ++i)
		{
			const auto& input = _MyInput.MyPlayers[i];
			if (input.MyPrincipalId.empty()) continue;
			const auto principal = Owner(input.MyPrincipalId);
			if (input.MyKind != BattlePlayerKind::VIRTUAL || principal >= _MyParticipantCount)
				throw std::invalid_argument("virtual player principal must be a participant");
			_MyPlayers[i].MyPrincipal = principal;
			_MyPlayers[i].MyTeam = _MyPlayers[principal].MyTeam;
		}
	}

	const BattlePlayerState& BattleCore::UnitOwner(UnitId _unit) const
	{
		return _MyPlayers.at(Unit(_unit).MyOwner);
	}

	bool BattleCore::SameOwner(UnitId _left, UnitId _right) const
	{
		return Unit(_left).MyOwner == Unit(_right).MyOwner;
	}

	bool BattleCore::Teammates(std::size_t _left, std::size_t _right) const
	{
		return _MyPlayers.at(_left).MyTeam == _MyPlayers.at(_right).MyTeam;
	}
}
