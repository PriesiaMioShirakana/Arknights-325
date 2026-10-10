#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::InstallBandEffects()
	{
		std::size_t count = 0;
		for (const auto& player : _MyInput.MyPlayers) count += player.MyBandEffects.size();
		_MyBandEffects.reserve(count);
		for (std::size_t i = 0; i < _MyInput.MyPlayers.size(); ++i)
			for (std::size_t j = 0; j < _MyInput.MyPlayers[i].MyBandEffects.size(); ++j)
			{
				const auto& effect = _MyInput.MyPlayers[i].MyBandEffects[j];
				const auto& p = effect.MyParameters;
				if (effect.MyKey.empty() || effect.MyKey.size() > 256 || p.MyKind < BandBattleKind::ACTIVE_BOND_STATS ||
					p.MyKind > BandBattleKind::ELITE_STATS || !std::isfinite(p.MyValue) || !std::isfinite(p.MyHealth) ||
					p.MyMaximum > 1000000 || p.MyStepCount > p.MySteps.size()) throw std::invalid_argument("invalid battle strategy");
				for (unsigned k = 0; k < p.MyStepCount; ++k)
					if (!std::isfinite(p.MySteps[k].MyAttack) || !std::isfinite(p.MySteps[k].MyHealth))
						throw std::invalid_argument("invalid strategy stat milestone");
				_MyBandHitEffects |= p.MyKind == BandBattleKind::WEAKNESS;
				auto& runtime = _MyBandEffects.emplace_back(BandRuntime{.MyPlayer = i, .MyEffect = j});
				if (p.MyKind == BandBattleKind::FRONT_SHIELD) runtime.MyChosen.reserve(_MyInput.MyPlayers[i].MyUnits.size());
			}
	}

	void BattleCore::NotifyBands(ContentEvent& _event, bool _early)
	{
		for (auto& runtime : _MyBandEffects)
		{
			const auto& player = _MyInput.MyPlayers[runtime.MyPlayer];
			const auto& effect = player.MyBandEffects[runtime.MyEffect];
			const auto& p = effect.MyParameters;
			if (_early != (p.MyKind == BandBattleKind::REVIVE || p.MyKind == BandBattleKind::WEAKNESS)) continue;
			const auto own = [&](const CombatUnit& _unit)
				{ return _unit.MySide == UnitSide::ALLY && _unit.MyOwner == runtime.MyPlayer && _unit.MyKind == UnitKind::OPERATOR; };
			const auto ally = [&](const CombatUnit& _unit)
				{ return _unit.MySide == UnitSide::ALLY && _unit.MyOwner == runtime.MyPlayer && _unit.MyKind != UnitKind::DEVICE && _unit.MyAlive; };
			const auto passive = [&](UnitId _id, double _attack, double _health)
			{
				std::vector<AttributeChange> mods; mods.reserve(2);
				if (_attack != 0) mods.push_back({Attribute::ATTACK_PERCENT, _attack});
				if (_health != 0) mods.push_back({Attribute::HEALTH_PERCENT, _health});
				(void)AddBuff(_id, BuffDefinition{.MyKey = effect.MyKey, .MyModifiers = std::move(mods), .MyPersistent = true, .MyAllowDead = true});
			};
			switch (p.MyKind)
			{
			case BandBattleKind::ACTIVE_BOND_STATS:
				if (_event.MyKind == ContentEventKind::BATTLE_START)
				{
					const auto n = static_cast<unsigned>(std::ranges::count_if(player.MyBonds, &BondLayer::MyActive));
					double attack = 0, health = 0;
					for (unsigned i = 0; i < p.MyStepCount; ++i)
						if (n >= p.MySteps[i].MyCount) { attack = p.MySteps[i].MyAttack; health = p.MySteps[i].MyHealth; }
					if (attack > 0 || health > 0)
						for (std::size_t i = 0; i < _MyAllyIds.size(); ++i) if (own(Unit(_MyAllyIds[i]))) passive(_MyAllyIds[i], attack, health);
				}
				break;
			case BandBattleKind::REVIVE:
				if (_event.MyKind == ContentEventKind::DEATH && _event.MyUnit && _event.MyRemovalReason == RemovalReason::KILLED &&
					runtime.MyUsed < p.MyMaximum && own(Unit(_event.MyUnit)) && Redeploy(_event.MyUnit)) { ++runtime.MyUsed; _event.MyRevived = true; }
				break;
			case BandBattleKind::DEATH_LAYERS:
				if (_event.MyKind == ContentEventKind::DEATH && _event.MyUnit && _event.MyRemovalReason == RemovalReason::KILLED)
				{
					const auto& unit = Unit(_event.MyUnit);
					if (own(unit) && std::ranges::contains(unit.MyDefinition.MyIdentity.MyBonds, effect.MyBond))
						(void)AddBondLayers(player.MyPlayerId, effect.MyBond, p.MyByTier ? unit.MyDefinition.MyIdentity.MyTier : p.MyValue,
							LayerGainOptions{.MySource = unit.MyId, .MyReason = "band"});
				}
				break;
			case BandBattleKind::DEPLOY_COOLDOWN:
				if (_event.MyKind == ContentEventKind::DEPLOY && _event.MyUnit && own(Unit(_event.MyUnit)) && p.MyValue != 1)
					(void)AddBuff(_event.MyUnit, BuffDefinition{.MyKey = effect.MyKey, .MyMaxStacks = p.MyMaximum,
						.MyRefresh = BuffRefresh::STACK, .MyModifiers = std::vector<AttributeChange>{{Attribute::REDEPLOY_MULTIPLIER, std::max(0.0, p.MyValue)}},
						.MyPersistent = true, .MyAllowDead = true});
				break;
			case BandBattleKind::FRONT_SHIELD:
				if (!(p.MyValue > 0)) break;
				if (_event.MyKind == ContentEventKind::BATTLE_START)
				{
					double front = -std::numeric_limits<double>::infinity();
					const auto column = [&](const CombatUnit& _unit) { return player.MyMirrorDeployment ? -_unit.MyPosition.MyX : _unit.MyPosition.MyX; };
					for (const auto id : _MyAllyIds) if (ally(Unit(id))) front = std::max(front, column(Unit(id)));
					for (const auto id : _MyAllyIds) if (ally(Unit(id)) && column(Unit(id)) == front) runtime.MyChosen.push_back(id);
				}
				else if (_event.MyKind == ContentEventKind::ATTACK && _event.MySource &&
					std::ranges::contains(runtime.MyChosen, _event.MySource) && Unit(_event.MySource).MyAlive &&
					(p.MyValue >= 1 || _MyRandom.Next() < p.MyValue))
				{
					const auto key = effect.MyKey + ":shield";
					const auto& buffs = Unit(_event.MySource).MyBuffs;
					const auto found = std::ranges::find(buffs, key, [](const CombatBuff& _buff) -> const std::string& { return _buff.MyDefinition.MyKey; });
					if (found == buffs.end() || found->MyDefinition.MyShield.MyHits < 1)
						(void)AddBuff(_event.MySource, BuffDefinition{.MyKey = key, .MyShield = Shield{.MyHits = 1}});
				}
				break;
			case BandBattleKind::SKILL_END_SP:
				if (_event.MyKind == ContentEventKind::SKILL_END && _event.MyUnit && _event.MySkillReason != SkillReason::DEATH && p.MyValue > 0)
				{
					const auto& unit = Unit(_event.MyUnit);
					if (!own(unit) || !unit.MyAlive || !unit.MyDefinition.MyIdentity.MyMeleePosition) break;
					constexpr std::array<RangeOffset, 4> offsets{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
					std::array<UnitId, 4> candidates{}; unsigned n = 0;
					for (const auto offset : offsets)
					{
						const auto rotated = RotateOffset(offset, unit.MyFacing);
						for (const auto id : _MyAllyIds)
						{
							const auto& candidate = Unit(id);
							if (own(candidate) && candidate.MyAlive && candidate.MyPosition.MyX == unit.MyPosition.MyX + rotated.MyColumn &&
								candidate.MyPosition.MyY == unit.MyPosition.MyY + rotated.MyRow && candidate.MyDefinition.MySkill.MyKind != SkillKind::NONE &&
								candidate.MyDefinition.MySkill.MyKind != SkillKind::PASSIVE) { candidates[n++] = id; break; }
						}
					}
					if (n) (void)GainSp(candidates[_MyRandom.Index(n)], p.MyValue);
				}
				break;
			case BandBattleKind::DEATH_ATTACK:
				if (_event.MyKind == ContentEventKind::DEATH && _event.MyUnit && _event.MyRemovalReason == RemovalReason::KILLED &&
					own(Unit(_event.MyUnit)) && p.MyValue > 0)
					for (std::size_t i = 0; i < _MyAllyIds.size(); ++i)
						if (_MyAllyIds[i] != _event.MyUnit && ally(Unit(_MyAllyIds[i])))
							(void)AddBuff(_MyAllyIds[i], BuffDefinition{.MyKey = effect.MyKey, .MyMaxStacks = std::max(1U, p.MyMaximum),
								.MyRefresh = BuffRefresh::STACK, .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, p.MyValue}}});
				break;
			case BandBattleKind::WEAKNESS:
				if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MySource && _event.MyTarget &&
					own(Unit(_event.MySource)) && Unit(_event.MyTarget).MySide == UnitSide::ENEMY &&
					(_event.MyDamage.MyType == DamageType::PHYSICAL || _event.MyDamage.MyType == DamageType::ARTS))
					RetypeWeakness(_event.MyDamage, _event.MySource, _event.MyTarget);
				break;
			case BandBattleKind::SAME_NAME_ATTACK:
				if (_event.MyKind == ContentEventKind::BATTLE_START && p.MyValue != 0)
					for (std::size_t i = 0; i < _MyAllyIds.size(); ++i)
					{
						const auto& unit = Unit(_MyAllyIds[i]);
						if (!own(unit) || !unit.MyAlive) continue;
						const auto& base = unit.MyDefinition.MyIdentity.MyBaseChess;
						const bool twin = std::ranges::any_of(_MyAllyIds, [&](UnitId _other)
						{
							const auto& other = Unit(_other);
							return _other != unit.MyId && own(other) && other.MyAlive &&
								(base.empty() ? other.MyDefinition.MyId == unit.MyDefinition.MyId : other.MyDefinition.MyIdentity.MyBaseChess == base);
						});
						if (twin) passive(unit.MyId, p.MyValue, 0);
					}
				break;
			case BandBattleKind::ELITE_STATS:
				if (_event.MyKind == ContentEventKind::BATTLE_START && (p.MyValue > 0 || p.MyHealth > 0))
				{
					const auto elite = [&](UnitId _id) { const auto& unit = Unit(_id); return own(unit) && unit.MyAlive && unit.MyDefinition.MyIdentity.MyGolden; };
					const auto n = static_cast<double>(std::ranges::count_if(_MyAllyIds, elite));
					if (n > 0) for (std::size_t i = 0; i < _MyAllyIds.size(); ++i) if (elite(_MyAllyIds[i])) passive(_MyAllyIds[i], p.MyValue * n, p.MyHealth * n);
				}
				break;
			}
		}
		if (!_early && _event.MyKind == ContentEventKind::BATTLE_START) SpawnMapCharacters();
	}
}
