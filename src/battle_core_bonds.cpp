#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::InstallCoreBonds()
	{
		std::size_t count = 0;
		for (const auto& player : _MyInput.MyPlayers) count += player.MyCoreBonds.size();
		_MyCoreBonds.reserve(count);
		for (std::size_t player = 0; player < _MyPlayers.size(); ++player)
		{
			const auto& input = _MyInput.MyPlayers[player];
			std::bitset<static_cast<unsigned>(CoreBondKind::COUNT)> seen;
			for (std::size_t effectIndex = 0; effectIndex < input.MyCoreBonds.size(); ++effectIndex)
			{
				const auto& effect = input.MyCoreBonds[effectIndex];
				if (effect.MyKind >= CoreBondKind::COUNT) throw std::invalid_argument("invalid core bond kind");
				const auto slot = static_cast<unsigned>(effect.MyKind);
				if (seen.test(slot)) throw std::invalid_argument("duplicate core bond");
				seen.set(slot);
				const auto& p = effect.MyParameters;
				for (const auto value : {p.MyAttack, p.MyGoldenAttack, p.MyAttackMaximum, p.MyAttackMaximumPerLayer, p.MyDuration, p.MyDurationPerLayer,
					p.MyAttackSpeed, p.MyDamageMultiplier, p.MyDamagePerLayer, p.MyColdMultiplier, p.MyColdPerLayer, p.MyInterval, p.MyAmmoPercent,
					p.MyAmmoPerLayer, p.MyRadius, p.MyDamageAttackScale, p.MyUnblockedAttackScale, p.MyStun, p.MyLendDuration, p.MyLendDurationPerLayer, p.MyHealth, p.MyHealthPerLayer, p.MyDevourDamage,
					p.MyAttackSpeedPerLayer, p.MyBaseDamage, p.MyStealthTail, p.MyPrdStep, p.MyFear, p.MyAttackPerLayer, p.MySummonAttackMultiplier, p.MySummonDamageResistance, p.MySummonShare})
					if (!std::isfinite(value)) throw std::invalid_argument("invalid core bond parameter");
				if (!p.MyMaxStacks || p.MyMaxStacks > 1000000 || p.MyReviveMaximum > 1000000 || p.MyInterval < 0 || p.MyRadius < 0 || p.MyStun < 0 || p.MyPrdStep < 0 || p.MyPrdStep > 1 || p.MyFear < 0)
					throw std::invalid_argument("invalid core bond interval or limit");
				const auto bond = std::ranges::find(input.MyBonds, CoreBondIds[slot], &BondLayer::MyId);
				if (bond == input.MyBonds.end() || !bond->MyActive) continue;
				CoreBondRuntime state{.MyPlayer = player, .MyEffect = effectIndex, .MyPower = bond->MyCount >= p.MyPowerCount || bond->MyTier >= 2 || !p.MyPowerCount,
					.MyExPower = bond->MyCount >= p.MyExCount || bond->MyTier >= 3 || !p.MyExCount};
				state.MyMembers.reserve(input.MyUnits.size());
				for (const auto id : _MyAllyIds)
					if (Unit(id).MyOwner == player && EquipmentMember(Unit(id), CoreBondIds[slot])) state.MyMembers.push_back(id);
				if (state.MyMembers.empty()) continue;
				const auto index = _MyCoreBonds.size();
				auto& runtime = _MyCoreBonds.emplace_back(std::move(state));
				RefreshCoreBond(index);
				if (effect.MyKind == CoreBondKind::EGIR)
				{
					++_MyEgirPasses;
					runtime.MyKnocked.reserve(runtime.MyMembers.size()); runtime.MyTargets.reserve(runtime.MyMembers.size());
				}
				if (effect.MyKind == CoreBondKind::VICTORIA && runtime.MyPower)
					for (const auto id : runtime.MyMembers)
					{
						double bonus = 0;
						for (const auto& item : Unit(id).MyDefinition.MyIdentity.MyItems) bonus += p.MyAttack + (item.ends_with("_b") ? p.MyGoldenAttack : 0);
						if (bonus > 0) (void)AddBuff(id, BuffDefinition{.MyKey = "bond:victoria:atk", .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, bonus}}, .MyPersistent = true, .MyAllowDead = true});
					}
				if (effect.MyKind == CoreBondKind::KJERAG)
				{
					_MyBondHitEffects = true;
					for (const auto id : runtime.MyMembers) (void)AddBuff(id, BuffDefinition{.MyKey = "bond:kjerag",
						.MyModifiers = std::vector<AttributeChange>{{Attribute::DAMAGE_DEALT_MULTIPLIER, p.MyDamageMultiplier}}, .MyPersistent = true, .MyAllowDead = true});
					if (runtime.MyPower && p.MyInterval > 0) (void)StartColdWind({.MyPlayerId = input.MyPlayerId, .MyBondId = "kjeragShip", .MyInterval = p.MyInterval,
						.MyBaseDuration = p.MyDuration, .MyDurationPerLayer = p.MyDurationPerLayer});
				}
				if (effect.MyKind == CoreBondKind::KAZIMIERZ && runtime.MyPower && p.MyInterval > 0 && p.MyDamageAttackScale > 0)
				{
					runtime.MyTargets.reserve(_MyInput.MySpawns.size());
					Schedule({.MyAt = p.MyInterval, .MyKind = ScheduledKind::CORE_BOND_PULSE, .MyHandle = index, .MyInterval = p.MyInterval});
				}
			}
		}
	}

	void Battle::RefreshCoreBond(std::size_t _index)
	{
		auto& state = _MyCoreBonds[_index];
		const auto& player = _MyInput.MyPlayers[state.MyPlayer];
		const auto& effect = player.MyCoreBonds[state.MyEffect]; const auto& p = effect.MyParameters;
		const auto layers = BondLayers(player.MyPlayerId, CoreBondIds[static_cast<unsigned>(effect.MyKind)]);
		if (effect.MyKind == CoreBondKind::YAN)
			for (const auto id : state.MyMembers) (void)AddBuff(id, BuffDefinition{.MyKey = "bond:yan",
				.MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, p.MyAttack + p.MyAttackPerLayer * layers}}, .MyPersistent = true, .MyAllowDead = true});
		if (effect.MyKind == CoreBondKind::EGIR)
			for (const auto id : state.MyMembers) (void)AddBuff(id, BuffDefinition{.MyKey = "bond:egir",
				.MyModifiers = std::vector<AttributeChange>{{Attribute::HEALTH_PERCENT, p.MyHealth + p.MyHealthPerLayer * layers}}, .MyPersistent = true, .MyAllowDead = true});
		if (effect.MyKind == CoreBondKind::VICTORIA)
			for (const auto id : state.MyMembers) if (!Unit(id).MyDefinition.MyIdentity.MyItems.empty())
				(void)AddBuff(id, BuffDefinition{.MyKey = "bond:victoria", .MyModifiers = std::vector<AttributeChange>{{Attribute::DAMAGE_DEALT_MULTIPLIER, p.MyDamageMultiplier + p.MyDamagePerLayer * layers}}, .MyPersistent = true, .MyAllowDead = true});
		if (effect.MyKind == CoreBondKind::KAZIMIERZ)
		{
			const auto bonus = std::max(0.0, std::min(p.MyAttackMaximum + p.MyAttackMaximumPerLayer * layers, static_cast<double>(state.MyCount) * p.MyAttack));
			if (bonus > 0) for (const auto id : state.MyMembers)
				(void)AddBuff(id, BuffDefinition{.MyKey = "bond:kazimierz", .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, bonus}}, .MyPersistent = true, .MyAllowDead = true});
		}
	}

	void Battle::SyncSargon(UnitId _unit)
	{
		for (const auto& state : _MyCoreBonds)
		{
			const auto& effect = _MyInput.MyPlayers[state.MyPlayer].MyCoreBonds[state.MyEffect];
			if (effect.MyKind != CoreBondKind::SARGON || !state.MyPower || effect.MyShareEquipment || !std::ranges::contains(state.MyMembers, _unit)) continue;
			const auto& unit = Unit(_unit);
			const auto count = std::ranges::count_if(unit.MyBuffs, [](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == "bond:sargon"; });
			if (count > 0 && unit.MyAlive)
				(void)AddBuff(_unit, BuffDefinition{.MyKey = "bond:sargon:atk", .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, effect.MyParameters.MyAttack * static_cast<double>(count)}}});
			else (void)RemoveBuff(_unit, "bond:sargon:atk");
			return;
		}
	}

	void Battle::CoreBondPulse(std::size_t _index)
	{
		auto& state = _MyCoreBonds[_index];
		const auto& p = _MyInput.MyPlayers[state.MyPlayer].MyCoreBonds[state.MyEffect].MyParameters;
		for (const auto id : state.MyMembers)
		{
			const auto& unit = Unit(id);
			if (!unit.MyAlive || unit.MyRemoved || unit.MyBlocking.empty()) continue;
			auto& targets = state.MyTargets; targets.clear(); targets.reserve(_MyEnemyIds.size());
			for (const auto enemyId : _MyEnemyIds)
			{
				const auto& enemy = Unit(enemyId);
				const auto distance = BodyDistance(enemy, RulePosition(unit));
				if (enemy.MyAlive && !enemy.MyHidden && !enemy.MyStatuses.Has(CombatStatus::UNTARGETABLE) && !EnemyStealthed(enemy) && distance * distance <= p.MyRadius * p.MyRadius + 1e-9) targets.push_back(enemyId);
			}
			const auto amount = p.MyDamageAttackScale * unit.MyStats.MyAttack;
			for (const auto target : targets) if (Unit(target).MyAlive)
			{
				(void)DealDamage(id, target, DamageInfo{.MyAmount = amount, .MyType = DamageType::TRUE_DAMAGE, .MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::BOND)});
				if (Unit(target).MyAlive && p.MyStun > 0) (void)ApplyStatus(target, CombatStatus::STUN, p.MyStun, id);
			}
		}
	}
}
