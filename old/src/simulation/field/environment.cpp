#include "battle_core.hpp"
#include <stronghold/simulation/environment.hpp>

namespace Stronghold
{
	Unit& Environment::GetUnit(UnitId _unit) const
	{
		return _MyCore.UnitInterface(_unit);
	}

	const CombatUnit& Environment::State(UnitId _unit) const
	{
		return _MyCore.Unit(_unit);
	}

	SkillBase& Environment::Skill(UnitId _unit) const
	{
		return _MyCore.Skill(_unit);
	}

	std::span<const BattlePlayerState> Environment::Owners() const noexcept
	{
		return _MyCore.Owners();
	}

	const BattlePlayerState& Environment::Owner(UnitId _unit) const
	{
		return _MyCore.UnitOwner(_unit);
	}

	PlayerTarget Environment::Player(std::size_t _index) const
	{
		(void)_MyCore._MyPlayers.at(_index);
		return PlayerTarget{const_cast<Environment&>(*this), _index};
	}

	TerrainTarget Environment::Tile(FieldPoint _point) const
	{
		if (!FieldGrid::InBounds(_point.MyRow, _point.MyColumn) ||
			(_MyCore._MyGrid && !_MyCore._MyGrid->InRect(_point.MyRow, _point.MyColumn)))
			throw std::out_of_range("terrain target outside field");
		return TerrainTarget{const_cast<Environment&>(*this), _point};
	}

	double Environment::Time() const noexcept
	{
		return _MyCore.Time();
	}

	bool Environment::Started() const noexcept
	{
		return _MyCore.Started();
	}

	bool Environment::Finished() const noexcept
	{
		return _MyCore.Finished();
	}

	WorldPoint Environment::RulePosition(UnitId _unit) const
	{
		return _MyCore.RulePosition(_unit);
	}

	const std::bitset<FieldTiles>& Environment::RuleRange(UnitId _unit) const
	{
		return _MyCore.RuleRange(_unit);
	}

	void Environment::Select(const EffectContext& _context, const SelectorDefinition& _definition, std::vector<EffectTarget>& _output) const
	{
		EffectExecutor::Select(*this, _context, _definition, _output);
	}

	void Environment::Execute(const EffectContext& _context, const EffectProgram& _program)
	{
		EffectExecutor::Execute(*this, _context, _program);
	}

	Battle& Environment::BattleView() const noexcept
	{
		return _MyCore._MyView;
	}

	const BattlePlayerState& PlayerTarget::State() const
	{
		return _MyEnvironment._MyCore._MyPlayers.at(_MyIndex);
	}

	double PlayerTarget::AddDp(double _amount)
	{
		return _MyEnvironment._MyCore.AddDp(State().MyPlayerId, _amount);
	}

	double PlayerTarget::AddCoins(double _amount)
	{
		return _MyEnvironment._MyCore.AddCoins(State().MyPlayerId, _amount);
	}

	double PlayerTarget::AddBondLayers(std::string_view _bond, double _amount, LayerGainOptions _options)
	{
		return _MyEnvironment._MyCore.AddBondLayers(State().MyPlayerId, _bond, _amount, _options);
	}

	FieldTile TerrainTarget::State() const
	{
		const auto& grid = _MyEnvironment._MyCore._MyGrid;
		return grid ? grid->Tile(_MyPoint.MyRow, _MyPoint.MyColumn) : FieldTile{};
	}

	void TerrainTarget::SetObstacle(bool _enabled, ObstacleKind _kind)
	{
		_MyEnvironment._MyCore.SetObstacle(_MyPoint.MyRow, _MyPoint.MyColumn, _enabled, _kind);
	}
}
