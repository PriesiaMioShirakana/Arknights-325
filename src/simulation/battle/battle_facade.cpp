#include "battle_core.hpp"
#include <utility>

namespace Stronghold
{
	Battle::Battle(BattleInput _input)
		: _MyImplementation(std::make_unique<BattleCore>(std::move(_input))), _MyView(_MyImplementation.get())
	{}

	Battle::Battle(BattleCore& _core) noexcept : _MyView(&_core) {}

	Battle::~Battle() = default;

	Battle::Battle(Battle&& _other) noexcept
		: _MyImplementation(std::move(_other._MyImplementation)), _MyView(std::exchange(_other._MyView, nullptr))
	{}

	Battle& Battle::operator=(Battle&& _other) noexcept
	{
		if (this != &_other)
		{
			_MyImplementation = std::move(_other._MyImplementation);
			_MyView = std::exchange(_other._MyView, nullptr);
		}
		return *this;
	}

	void Battle::Start()
	{
		_MyView->Start();
	}

	void Battle::Step()
	{
		_MyView->Step();
	}

	void Battle::Advance(std::uint64_t _ticks)
	{
		_MyView->Advance(_ticks);
	}

	void Battle::ForceEnd(BattleEndReason _reason)
	{
		_MyView->ForceEnd(_reason);
	}

	double Battle::LoseTeamLife(double _amount)
	{
		return _MyView->LoseTeamLife(_amount);
	}

	void Battle::SetObstacle(int _row, int _column, bool _enabled, ObstacleKind _kind)
	{
		_MyView->SetObstacle(_row, _column, _enabled, _kind);
	}

	double Battle::Displace(UnitId _enemy, WorldPoint _direction, double _distance)
	{
		return _MyView->Displace(_enemy, _direction, _distance);
	}

	double Battle::Push(UnitId _enemy, double _force, const PushOptions& _options)
	{
		return _MyView->Push(_enemy, _force, _options);
	}

	double Battle::Pull(UnitId _enemy, double _force, const PullOptions& _options)
	{
		return _MyView->Pull(_enemy, _force, _options);
	}

	double Battle::PullToFront(UnitId _enemy, UnitId _source, double _force)
	{
		return _MyView->PullToFront(_enemy, _source, _force);
	}

	double Battle::PushDistance(UnitId _enemy, double _force, bool _effect) const
	{
		return _MyView->PushDistance(_enemy, _force, _effect);
	}

	bool Battle::Started() const noexcept
	{
		return _MyView->Started();
	}

	bool Battle::Finished() const noexcept
	{
		return _MyView->Finished();
	}

	double Battle::Time() const noexcept
	{
		return _MyView->Time();
	}

	std::uint64_t Battle::Tick() const noexcept
	{
		return _MyView->Tick();
	}

	std::uint32_t Battle::RandomState() const noexcept
	{
		return _MyView->RandomState();
	}

	const std::deque<CombatUnit>& Battle::Units() const noexcept
	{
		return _MyView->Units();
	}

	std::span<const BattlePlayerState> Battle::Players() const noexcept
	{
		return _MyView->Players();
	}

	const CombatUnit& Battle::Unit(UnitId _id) const
	{
		return _MyView->Unit(_id);
	}

	RulePositionMode Battle::PositionMode() const noexcept
	{
		return _MyView->PositionMode();
	}

	bool Battle::GarrisonEffectsAfterExit() const noexcept
	{
		return _MyView->GarrisonEffectsAfterExit();
	}

	bool Battle::RetainGrantedGarrisonsAfterExit() const noexcept
	{
		return _MyView->RetainGrantedGarrisonsAfterExit();
	}

	WorldPoint Battle::RulePosition(UnitId _unit) const
	{
		return _MyView->RulePosition(_unit);
	}

	const std::bitset<FieldTiles>& Battle::RuleRange(UnitId _unit) const
	{
		return _MyView->RuleRange(_unit);
	}

	std::span<const int> Battle::RuleRangeKeys(UnitId _unit) const
	{
		return _MyView->RuleRangeKeys(_unit);
	}

	bool Battle::InRuleRange(UnitId _source, UnitId _target) const
	{
		return _MyView->InRuleRange(_source, _target);
	}

	BattleResult Battle::Result() const
	{
		return _MyView->Result();
	}

	std::span<const ContentError> Battle::ContentErrors() const noexcept
	{
		return _MyView->ContentErrors();
	}

	std::vector<BattleEvent> Battle::DrainEvents()
	{
		return _MyView->DrainEvents();
	}

	double Battle::DealDamage(UnitId _source, UnitId _target, double _amount, DamageType _type)
	{
		return _MyView->DealDamage(_source, _target, _amount, _type);
	}

	double Battle::DealDamage(UnitId _source, UnitId _target, const DamageInfo& _damage)
	{
		return _MyView->DealDamage(_source, _target, _damage);
	}

	double Battle::Heal(UnitId _source, UnitId _target, double _amount, HealOptions _options)
	{
		return _MyView->Heal(_source, _target, _amount, _options);
	}

	void Battle::AddShield(UnitId _target, Shield _shield)
	{
		_MyView->AddShield(_target, _shield);
	}

	double Battle::DealElement(UnitId _source, UnitId _target, ElementHit _hit)
	{
		return _MyView->DealElement(_source, _target, _hit);
	}

	double Battle::ReduceElement(UnitId _target, double _amount, std::optional<Element> _element)
	{
		return _MyView->ReduceElement(_target, _amount, _element);
	}

	void Battle::BurstElement(UnitId _source, UnitId _target, Element _element)
	{
		_MyView->BurstElement(_source, _target, _element);
	}

	std::optional<ElementDisplay> Battle::ElementView(UnitId _unit) const
	{
		return _MyView->ElementView(_unit);
	}

	bool Battle::ApplyStatus(UnitId _target, CombatStatus _status, double _seconds, UnitId _source, bool _force)
	{
		return _MyView->ApplyStatus(_target, _status, _seconds, _source, _force);
	}

	bool Battle::ApplyStatus(UnitId _target, CombatStatus _status, const StatusApplication& _application)
	{
		return _MyView->ApplyStatus(_target, _status, _application);
	}

	bool Battle::RemoveStatus(UnitId _target, CombatStatus _status)
	{
		return _MyView->RemoveStatus(_target, _status);
	}

	std::uint64_t Battle::AddBuff(UnitId _target, BuffDefinition _definition)
	{
		return _MyView->AddBuff(_target, std::move(_definition));
	}

	bool Battle::ApplyStrongest(UnitId _target, std::string _key, double _duration, BuffStrength _strength, UnitId _source)
	{
		return _MyView->ApplyStrongest(_target, std::move(_key), _duration, _strength, _source);
	}

	std::size_t Battle::RemoveBuff(UnitId _target, std::string_view _key)
	{
		return _MyView->RemoveBuff(_target, _key);
	}

	bool Battle::RemoveBuff(UnitId _target, std::uint64_t _buff)
	{
		return _MyView->RemoveBuff(_target, _buff);
	}

	double Battle::LoseHealth(UnitId _source, UnitId _target, double _amount, bool _silent)
	{
		return _MyView->LoseHealth(_source, _target, _amount, _silent);
	}

	double Battle::AddDp(std::string_view _playerId, double _amount)
	{
		return _MyView->AddDp(_playerId, _amount);
	}

	double Battle::AddCoins(std::string_view _playerId, double _amount)
	{
		return _MyView->AddCoins(_playerId, _amount);
	}

	double Battle::AddBondLayers(std::string_view _playerId, std::string_view _bondId, double _amount, LayerGainOptions _options)
	{
		return _MyView->AddBondLayers(_playerId, _bondId, _amount, _options);
	}

	std::size_t Battle::LendEquipment(UnitId _from, UnitId _to, unsigned _maximumTier, double _duration)
	{
		return _MyView->LendEquipment(_from, _to, _maximumTier, _duration);
	}

	std::vector<Battle::LentEquipmentView> Battle::LentEquipment(UnitId _unit) const
	{
		return _MyView->LentEquipment(_unit);
	}

	double Battle::GainSp(UnitId _unit, double _amount, SpReason _reason, bool _silent)
	{
		return _MyView->GainSp(_unit, _amount, _reason, _silent);
	}

	void Battle::SetSpTotal(UnitId _unit, double _total)
	{
		_MyView->SetSpTotal(_unit, _total);
	}

	void Battle::SetSpCostMultiplier(UnitId _unit, double _multiplier)
	{
		_MyView->SetSpCostMultiplier(_unit, _multiplier);
	}

	double Battle::SpCost(UnitId _unit) const
	{
		return _MyView->SpCost(_unit);
	}

	double Battle::SpTotal(UnitId _unit) const
	{
		return _MyView->SpTotal(_unit);
	}

	bool Battle::ActivateSkill(UnitId _unit, bool _free, SkillReason _reason)
	{
		return _MyView->ActivateSkill(_unit, _free, _reason);
	}

	void Battle::EndSkill(UnitId _unit, SkillReason _reason)
	{
		_MyView->EndSkill(_unit, _reason);
	}

	bool Battle::ForceAttack(UnitId _unit, std::span<const UnitId> _targets, bool _noAmmo)
	{
		return _MyView->ForceAttack(_unit, _targets, _noAmmo);
	}

	void Battle::AddSkillAmmo(UnitId _unit, double _amount)
	{
		_MyView->AddSkillAmmo(_unit, _amount);
	}

	void Battle::ExtendSkill(UnitId _unit, double _seconds)
	{
		_MyView->ExtendSkill(_unit, _seconds);
	}

	void Battle::AddSkillCharge(UnitId _unit, int _amount)
	{
		_MyView->AddSkillCharge(_unit, _amount);
	}

	void Battle::SetSkillTrigger(UnitId _unit, SkillTrigger _rule, std::vector<RangeOffset> _grid)
	{
		_MyView->SetSkillTrigger(_unit, _rule, std::move(_grid));
	}

	std::uint64_t Battle::AddSkillTriggerRange(UnitId _unit, SkillTriggerArea _area)
	{
		return _MyView->AddSkillTriggerRange(_unit, _area);
	}

	bool Battle::RemoveSkillTriggerRange(UnitId _unit, std::uint64_t _range)
	{
		return _MyView->RemoveSkillTriggerRange(_unit, _range);
	}

	bool Battle::UpdateSkillTriggerRange(UnitId _unit, std::uint64_t _range, SkillTriggerArea _area)
	{
		return _MyView->UpdateSkillTriggerRange(_unit, _range, _area);
	}

	UnitId Battle::SpawnToken(TokenSpawn _spawn)
	{
		return _MyView->SpawnToken(std::move(_spawn));
	}

	UnitId Battle::ReleaseSkillSummon(UnitId _owner, std::string_view _token, unsigned _cap)
	{
		return _MyView->ReleaseSkillSummon(_owner, _token, _cap);
	}

	unsigned Battle::SkillSummonStock(UnitId _owner, std::string_view _token) const
	{
		return _MyView->SkillSummonStock(_owner, _token);
	}

	UnitId Battle::SpawnDevice(DeviceSpawn _spawn)
	{
		return _MyView->SpawnDevice(std::move(_spawn));
	}

	UnitId Battle::SpawnTurret(TurretSpawn _spawn)
	{
		return _MyView->SpawnTurret(std::move(_spawn));
	}

	void Battle::SetBondLayers(std::string_view _playerId, std::string_view _bondId, double _layers)
	{
		_MyView->SetBondLayers(_playerId, _bondId, _layers);
	}

	double Battle::BondLayers(std::string_view _playerId, std::string_view _bondId) const
	{
		return _MyView->BondLayers(_playerId, _bondId);
	}

	std::uint64_t Battle::StartColdWind(ColdWindDefinition _definition)
	{
		return _MyView->StartColdWind(std::move(_definition));
	}

	bool Battle::CancelColdWind(std::uint64_t _handle)
	{
		return _MyView->CancelColdWind(_handle);
	}

	std::uint64_t Battle::ColdWindGusts(std::uint64_t _handle) const
	{
		return _MyView->ColdWindGusts(_handle);
	}

	UnitId Battle::SpawnEnemy(EnemySpawn _spawn)
	{
		return _MyView->SpawnEnemy(std::move(_spawn));
	}

	void Battle::Retreat(UnitId _unit, bool _permanent, RemovalReason _reason, bool _dying)
	{
		_MyView->Retreat(_unit, _permanent, _reason, _dying);
	}

	bool Battle::Redeploy(UnitId _unit, bool _free, std::optional<WorldPoint> _tile, bool _keepSp)
	{
		return _MyView->Redeploy(_unit, _free, _tile, _keepSp);
	}

	bool Battle::CanDeploy(WorldPoint _position, UnitId _excludedUnit, bool _includeDown) const
	{
		return _MyView->CanDeploy(_position, _excludedUnit, _includeDown);
	}

	bool Battle::IsDown(UnitId _unit) const
	{
		return _MyView->IsDown(_unit);
	}

	WorldPoint Battle::RestPosition(UnitId _unit) const
	{
		return _MyView->RestPosition(_unit);
	}

	bool Battle::ReservedTile(WorldPoint _position) const
	{
		return _MyView->ReservedTile(_position);
	}

	std::optional<WorldPoint> Battle::FindTacticalPoint(UnitId _unit)
	{
		return _MyView->FindTacticalPoint(_unit);
	}

	const std::bitset<FieldTiles>& Battle::GroundPathTiles()
	{
		return _MyView->GroundPathTiles();
	}

	bool Battle::Relocate(UnitId _unit, WorldPoint _position)
	{
		return _MyView->Relocate(_unit, _position);
	}

	bool Battle::MoveRedeploy(UnitId _unit, WorldPoint _position, bool _clearSp)
	{
		return _MyView->MoveRedeploy(_unit, _position, _clearSp);
	}

	void Battle::SetDownAtHome(UnitId _unit, bool _enabled)
	{
		_MyView->SetDownAtHome(_unit, _enabled);
	}

	bool Battle::EnterDoll(UnitId _unit)
	{
		return _MyView->EnterDoll(_unit);
	}

	double Battle::RemainingDistance(UnitId _enemy) const
	{
		return _MyView->RemainingDistance(_enemy);
	}
	std::span<const BattlePlayerState> Battle::Owners() const noexcept
	{
		return _MyView->Owners();
	}

	const BattlePlayerState& Battle::UnitOwner(UnitId _unit) const
	{
		return _MyView->UnitOwner(_unit);
	}

	bool Battle::SameOwner(UnitId _left, UnitId _right) const
	{
		return _MyView->SameOwner(_left, _right);
	}

	bool Battle::Teammates(std::size_t _left, std::size_t _right) const
	{
		return _MyView->Teammates(_left, _right);
	}

	SkillBase& Battle::Skill(UnitId _unit)
	{
		return _MyView->Skill(_unit);
	}

	OperatorBase& Battle::Operator(UnitId _unit)
	{
		return _MyView->Operator(_unit);
	}

	std::uint64_t Battle::AttachMechanism(UnitId _unit, MechanismDefinition _definition, std::string_view _playerId)
	{
		return _MyView->AttachMechanism(_unit, std::move(_definition), _playerId);
	}

	std::uint64_t Battle::AttachMechanism(UnitId _unit, ComponentReference _reference, std::string_view _playerId)
	{
		return _MyView->AttachMechanism(_unit, _reference, _playerId);
	}

	bool Battle::RemoveMechanism(std::uint64_t _handle)
	{
		return _MyView->RemoveMechanism(_handle);
	}

}
