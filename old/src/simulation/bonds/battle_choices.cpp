#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::InstallChoiceEffects()
	{
		std::size_t count = 0;
		for (const auto& player : _MyInput.MyPlayers) count += player.MyChoiceEffects.size();
		_MyChoiceEffects.reserve(count);
		if (!count) return;
		_MyChoiceHealing.resize(_MyInput.MyPlayers.size());
		for (std::size_t owner = 0; owner < _MyInput.MyPlayers.size(); ++owner)
		{
			const auto& player = _MyInput.MyPlayers[owner];
			for (std::size_t i = 0; i < player.MyChoiceEffects.size(); ++i)
			{
				const auto& effect = player.MyChoiceEffects[i];
				if (effect.MyKey.empty() || effect.MyKey.size() > 256 || !std::isfinite(effect.MySelfHeal) || std::isless(effect.MySelfHeal, 0) ||
					effect.MyGate.MyKind < ChoiceGateKind::ALWAYS || effect.MyGate.MyKind > ChoiceGateKind::SAME_ROW)
					throw std::invalid_argument("invalid battle choice effect");
				AttributeModifiers validation;
				validation.Add(effect.MyOperatorModifiers); validation.Add(effect.MyFullHealthModifiers);
				for (const auto& enemy : effect.MyEnemies)
				{
					if (enemy.MyRank < ChoiceEnemyRank::ANY || enemy.MyRank > ChoiceEnemyRank::BOSS) throw std::invalid_argument("invalid choice enemy rank");
					validation.Add(enemy.MyModifiers);
				}
				bool enabled = effect.MyGate.MyKind == ChoiceGateKind::ALWAYS;
				if (effect.MyGate.MyKind == ChoiceGateKind::BENCH_AT_LEAST || effect.MyGate.MyKind == ChoiceGateKind::BENCH_AT_MOST)
					enabled = effect.MyPreparationPassed.value_or(player.MyHandUnits && (effect.MyGate.MyKind == ChoiceGateKind::BENCH_AT_LEAST
						? *player.MyHandUnits >= effect.MyGate.MyCount : *player.MyHandUnits <= effect.MyGate.MyCount));
				_MyChoiceEffects.emplace_back(ChoiceRuntime{.MyPlayer = owner, .MyEffect = i, .MyEnabled = enabled});
				if (enabled) _MyChoiceHealing[owner] += effect.MySelfHeal;
				_MyChoiceFullHealth |= !effect.MyFullHealthModifiers.empty();
			}
			if (!std::isfinite(_MyChoiceHealing[owner])) throw std::overflow_error("choice healing overflow");
		}
	}

	void BattleCore::SyncChoiceFullHealth(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (unit.MySide != UnitSide::ALLY || unit.MyKind != UnitKind::OPERATOR) return;
		const bool full = unit.MyAlive && std::isgreaterequal(unit.MyHealth, unit.MyStats.MyMaxHealth - 1e-6);
		for (const auto& runtime : _MyChoiceEffects)
		{
			if (!runtime.MyEnabled || runtime.MyPlayer != unit.MyOwner) continue;
			const auto& effect = _MyInput.MyPlayers[runtime.MyPlayer].MyChoiceEffects[runtime.MyEffect];
			if (effect.MyFullHealthModifiers.empty()) continue;
			const bool present = std::ranges::any_of(unit.MyBuffs, [&](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == effect.MyKey; });
			if (full && !present) (void)AddBuff(_unit, BuffDefinition{.MyKey = effect.MyKey, .MyModifiers = effect.MyFullHealthModifiers});
			else if (!full && present) (void)RemoveBuff(_unit, effect.MyKey);
		}
	}

	void BattleCore::NotifyChoices(const ContentEvent& _event)
	{
		if (_MyChoiceEffects.empty()) return;
		if (_event.MyKind == ContentEventKind::BATTLE_START)
		{
			for (auto& runtime : _MyChoiceEffects)
			{
				const auto& effect = _MyInput.MyPlayers[runtime.MyPlayer].MyChoiceEffects[runtime.MyEffect];
				if (effect.MyGate.MyKind == ChoiceGateKind::SAME_ROW)
				{
					// 初始阵容最多 36 格，逐个单位计数，无需为有限的开局查询分配哈希表。
					std::size_t best = 0;
					for (const auto id : _MyAllyIds)
					{
						const auto& unit = Unit(id);
						if (unit.MyKind != UnitKind::OPERATOR || !unit.MyAlive || unit.MyOwner != runtime.MyPlayer) continue;
						std::size_t count = 0;
						for (const auto other : _MyAllyIds)
						{
							const auto& candidate = Unit(other);
							if (candidate.MyKind == UnitKind::OPERATOR && candidate.MyAlive && candidate.MyOwner == runtime.MyPlayer &&
								static_cast<int>(candidate.MyPosition.MyY) == static_cast<int>(unit.MyPosition.MyY)) ++count;
						}
						best = std::max(best, count);
					}
					runtime.MyEnabled = best >= effect.MyGate.MyCount;
					if (runtime.MyEnabled)
					{
						_MyChoiceHealing[runtime.MyPlayer] += effect.MySelfHeal;
						if (!std::isfinite(_MyChoiceHealing[runtime.MyPlayer])) throw std::overflow_error("choice healing overflow");
					}
				}
				if (!runtime.MyEnabled || effect.MyOperatorModifiers.empty()) continue;
				for (std::size_t i = 0; i < _MyAllyIds.size(); ++i)
				{
					const auto& unit = Unit(_MyAllyIds[i]);
					if (unit.MyKind == UnitKind::OPERATOR && unit.MyOwner == runtime.MyPlayer)
						(void)AddBuff(unit.MyId, BuffDefinition{.MyKey = effect.MyKey, .MyModifiers = effect.MyOperatorModifiers, .MyPersistent = true, .MyAllowDead = true});
				}
			}
			if (_MyChoiceFullHealth) for (std::size_t i = 0; i < _MyAllyIds.size(); ++i) SyncChoiceFullHealth(_MyAllyIds[i]);
		}
		else if (_event.MyKind == ContentEventKind::TICK && _MyChoiceFullHealth)
		{
			for (std::size_t i = 0; i < _MyAllyIds.size(); ++i) SyncChoiceFullHealth(_MyAllyIds[i]);
		}
		else if (_event.MyKind == ContentEventKind::DAMAGED && _event.MyTarget)
		{
			const auto& unit = Unit(_event.MyTarget);
			if (unit.MySide != UnitSide::ALLY || (unit.MyKind != UnitKind::OPERATOR && unit.MyKind != UnitKind::TOKEN) || unit.MyOwner == NoPlayer) return;
			const auto heal = _MyChoiceHealing[unit.MyOwner];
			if (std::isgreater(heal, 0) && std::isgreater(_event.MyAmount, 0) && !_event.MyElement &&
				!HasTag(_event.MyDamage.MyTags, DamageTag::HP_LOSS) && unit.MyAlive && std::isgreater(unit.MyHealth, 0) && !unit.MyStatuses.Has(CombatStatus::NO_HEAL))
				(void)Heal(0, unit.MyId, heal, HealOptions{.MySelf = true});
			if (_MyChoiceFullHealth) SyncChoiceFullHealth(unit.MyId);
		}
		else if (_event.MyKind == ContentEventKind::DEPLOY && _event.MyUnit)
		{
			const auto& unit = Unit(_event.MyUnit);
			if (unit.MySide != UnitSide::ENEMY || !unit.MyAlive) return;
			const auto rank = unit.MyDefinition.MyLeader ? ChoiceEnemyRank::BOSS : unit.MyDefinition.MyElite ? ChoiceEnemyRank::ELITE : ChoiceEnemyRank::NORMAL;
			for (const auto& runtime : _MyChoiceEffects)
			{
				if (!runtime.MyEnabled || runtime.MyPlayer != unit.MyResponsiblePlayer) continue;
				const auto& effect = _MyInput.MyPlayers[runtime.MyPlayer].MyChoiceEffects[runtime.MyEffect];
				for (const auto& enemy : effect.MyEnemies)
					if (enemy.MyRank == ChoiceEnemyRank::ANY || enemy.MyRank == rank)
						(void)AddBuff(unit.MyId, BuffDefinition{.MyKey = effect.MyKey, .MyModifiers = enemy.MyModifiers, .MyPersistent = true});
			}
		}
	}
}
