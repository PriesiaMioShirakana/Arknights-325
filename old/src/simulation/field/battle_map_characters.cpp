#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::SpawnMapCharacters()
	{
		if (_MyInput.MyMapCharacters.empty()) return;
		const bool multi = _MyDefenderCount > 1;
		for (std::size_t owner = 0; owner < _MyParticipantCount; ++owner)
		{
			if (_MyPlayers[owner].MyRole != BattlePlayerRole::DEFENDER) continue;
			const auto elites = static_cast<unsigned>(std::ranges::count_if(_MyAllyIds, [&](UnitId _id)
			{
				const auto& unit = Unit(_id);
				return unit.MyOwner == owner && unit.MyKind == UnitKind::OPERATOR && unit.MyAlive && unit.MyDefinition.MyIdentity.MyGolden;
			}));
			const auto variant = std::ranges::find_if(_MyInput.MyMapCharacters, [&](const MapCharacterVariant& _variant)
				{ return elites >= _variant.MyMinimumElites && elites <= _variant.MyMaximumElites; });
			if (variant == _MyInput.MyMapCharacters.end()) continue;
			const auto& player = _MyInput.MyPlayers[owner];
			const bool right = player.MyRightHalf.value_or(player.MyMirrorDeployment);
			const int first = right ? 10 : 2, last = right ? 18 : 10;
			for (const auto& character : variant->MyCharacters)
			{
				bool spawned = false;
				// 稳定四档排序：本侧优先，然后非 multi_only；不在开战时复制或排序位置表。
				for (int priority = 0; priority < 4 && !spawned; ++priority)
					for (const auto& slot : character.MyPositions)
					{
						if (slot.MyMultiOnly && !multi) continue;
						const auto tile = slot.MyPosition;
						const bool half = !multi || (tile.MyColumn >= first && tile.MyColumn <= last);
						if ((half ? 0 : 2) + (slot.MyMultiOnly ? 1 : 0) != priority) continue;
						const WorldPoint position{static_cast<double>(tile.MyColumn), static_cast<double>(tile.MyRow)};
						if (ReservedTile(position)) continue;
						if (SpawnToken(TokenSpawn{.MyDefinition = character.MyDefinition, .MyPosition = position,
							.MyPlayerId = player.MyPlayerId, .MyFacing = slot.MyFacing})) { spawned = true; break; }
					}
			}
		}
	}

	void BattleCore::InstallUnitEffects(CombatUnit& _unit)
	{
		EnsureUnitComponents(_unit.MyId);
		for (const auto& mechanism : _unit.MyDefinition.MyMechanisms) (void)AttachMechanism(_unit.MyId, mechanism);
		InstallTokenKit(_unit);
		for (const auto& buff : _unit.MyDefinition.MyInitialBuffs) (void)AddBuff(_unit.MyId, buff);
		if (_unit.MyDefinition.MyMedic) _MyMedics.push_back(_unit.MyId);
		InstallEquipmentEffects(_unit);
		InstallYanyou(_unit);
	}

	void BattleCore::NotifyMedics(ContentEvent& _event, bool _late)
	{
		if (_event.MyKind == ContentEventKind::BEFORE_HEAL && _event.MySource && !_event.MyHealOptions.MyRegen)
		{
			const auto& source = Unit(_event.MySource);
			if (!source.MyDefinition.MyMedic || source.MyRemoved) return;
			const auto& medic = *source.MyDefinition.MyMedic;
			const auto& target = Unit(_event.MyTarget);
			if (!_late && source.MySkill.MyActive && medic.MySkillHpRatio > 0 &&
				target.MyHealth / target.MyStats.MyMaxHealth < medic.MySkillHpRatio - 1e-9)
				_event.MyAmount *= medic.MySkillHealMultiplier;
			if (_late && medic.MyHealSp > 0 && target.MyKind != UnitKind::DEVICE && target.MyDefinition.MySkill.MyKind != SkillKind::NONE)
				(void)GainSp(target.MyId, medic.MyHealSp);
		}
		else if (!_late && _event.MyKind == ContentEventKind::DEATH &&
			(_event.MyRemovalReason == RemovalReason::KILLED || (_event.MyRemovalReason == RemovalReason::RETREAT && _event.MyDying)))
		{
			const auto& dead = Unit(_event.MyUnit);
			if (dead.MyKind != UnitKind::OPERATOR) return;
			for (std::size_t i = 0; i < _MyMedics.size(); ++i)
			{
				const auto& medic = Unit(_MyMedics[i]);
				if (!medic.MyAlive || medic.MyOwner != dead.MyOwner || medic.MyId == dead.MyId) continue;
				const auto key = FieldGrid::Key(static_cast<int>(dead.MyPosition.MyY), static_cast<int>(dead.MyPosition.MyX));
				if (key >= 0 && key < FieldTiles && medic.MyRangeMask.test(static_cast<std::size_t>(key)))
					(void)GainSp(medic.MyId, medic.MyDefinition.MyMedic->MyDeathSp);
			}
		}
	}

	void BattleCore::MedicHealAttack(UnitId _source, UnitId _target, double _amount)
	{
		auto& source = _MyUnits[Index(_source)];
		if (!source.MyDefinition.MyMedic || !source.MySkill.MyActive || !(_amount > 0)) return;
		const auto extra = source.MyDefinition.MyMedic->MySkillExtraHeal;
		if (!(extra > 0) || source.MyMedicExtraAttack == source.MyTotals.MyAttacks) return;
		source.MyMedicExtraAttack = source.MyTotals.MyAttacks;
		const auto& main = Unit(_target);
		const auto center = RulePosition(main);
		auto best = _target;
		for (const auto id : _MyAllyIds)
		{
			const auto& unit = Unit(id);
			if (id == _target || !unit.MyAlive || unit.MyKind == UnitKind::DEVICE || unit.MyHidden ||
				unit.MyStatuses.Has(CombatStatus::NO_HEAL) ||
				unit.MyDefinition.MyAttack.MyNoHeal || (id != _source && unit.MyStatuses.Has(CombatStatus::ISOLATED))) continue;
			const auto point = RulePosition(unit);
			if (std::abs(point.MyX - center.MyX) + std::abs(point.MyY - center.MyY) != 1) continue;
			if (unit.MyHealth / unit.MyStats.MyMaxHealth < Unit(best).MyHealth / Unit(best).MyStats.MyMaxHealth - 1e-9) best = id;
		}
		(void)Heal(_source, best, _amount * extra);
	}
}
