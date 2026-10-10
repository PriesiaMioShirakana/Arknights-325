#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::NotifyCoreBonds(ContentEvent& _event, bool _late)
	{
		if (_late)
		{
			if (_event.MyKind != ContentEventKind::LAYER_GAIN && _event.MyKind != ContentEventKind::SKILL_START) return;
		}
		else if (_event.MyKind != ContentEventKind::BATTLE_START && _event.MyKind != ContentEventKind::DEPLOY && _event.MyKind != ContentEventKind::SKILL_START &&
			_event.MyKind != ContentEventKind::AMMO_USED && _event.MyKind != ContentEventKind::DAMAGED && _event.MyKind != ContentEventKind::BEFORE_DAMAGE) return;
		for (std::size_t index = 0; index < _MyCoreBonds.size(); ++index)
		{
			auto& state = _MyCoreBonds[index];
			const auto& player = _MyInput.MyPlayers[state.MyPlayer];
			const auto& effect = player.MyCoreBonds[state.MyEffect]; const auto& p = effect.MyParameters;
			const auto layers = [&] { return BondLayers(player.MyPlayerId, CoreBondIds[static_cast<unsigned>(effect.MyKind)]); };
			const auto member = [&](UnitId _id) { return _id && std::ranges::contains(state.MyMembers, _id); };
			if (_late)
			{
				if (_event.MyKind == ContentEventKind::LAYER_GAIN && _event.MyPlayer == state.MyPlayer &&
					(effect.MyKind == CoreBondKind::YAN || effect.MyKind == CoreBondKind::VICTORIA || effect.MyKind == CoreBondKind::KAZIMIERZ || effect.MyKind == CoreBondKind::EGIR) &&
					_event.MyBondId == CoreBondIds[static_cast<unsigned>(effect.MyKind)] && !state.MyPending)
				{
					state.MyPending = true;
					Schedule({.MyAt = Time(), .MyKind = ScheduledKind::CORE_BOND_REFRESH, .MyHandle = index});
				}
				if (_event.MyKind == ContentEventKind::SKILL_START && effect.MyKind == CoreBondKind::LATERANO && member(_event.MyUnit))
				{
					auto& unit = _MyUnits[Index(_event.MyUnit)];
					if (unit.MyDefinition.MySkill.MyKind != SkillKind::AMMO || !(unit.MySkill.MyAmmoLeft > 0)) continue;
					const auto ammo = std::floor(unit.MySkill.MyAmmoLeft * (1 + p.MyAmmoPercent + p.MyAmmoPerLayer * layers()) + 1e-9);
					if (std::isfinite(ammo) && ammo > unit.MySkill.MyAmmoLeft) unit.MySkill.MyAmmoLeft = ammo;
				}
				continue;
			}
			switch (effect.MyKind)
			{
			case CoreBondKind::YAN:
				if (_event.MyKind == ContentEventKind::BATTLE_START && state.MyPower) SpawnBondYanyou(index);
				break;
			case CoreBondKind::SIRACUSA:
				if (_event.MyKind == ContentEventKind::DEPLOY && member(_event.MyUnit))
				{
					const auto duration = p.MyDuration + p.MyDurationPerLayer * layers();
					if (!(duration > 0)) continue;
					(void)AddBuff(_event.MyUnit, BuffDefinition{.MyKey = "bond:siracusa", .MyDuration = duration,
						.MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_SPEED, p.MyAttackSpeed + p.MyAttackSpeedPerLayer * layers()}}});
					if (!state.MyPower) continue;
					_MyUnits[Index(_event.MyUnit)].MySiracusaStealthEnd = std::numeric_limits<double>::infinity();
					StatusFlags flags; flags.set(static_cast<unsigned>(CombatStatus::STEALTH));
					(void)AddBuff(_event.MyUnit, BuffDefinition{.MyKey = "bond:siracusa:stealth", .MyDuration = duration,
						.MyFlags = flags, .MyBuiltin = BuiltinBuff::SIRACUSA_STEALTH, .MyStatus = CombatStatus::STEALTH});
				}
				if (_event.MyKind == ContentEventKind::DAMAGED && state.MyPower && p.MyPrdStep > 0 && member(_event.MySource) && _event.MyTarget)
				{
					const auto& target = Unit(_event.MyTarget); const auto& source = Unit(_event.MySource);
					const auto& damage = _event.MyDamage;
					constexpr auto excluded = DamageTag::DOT | DamageTag::PERIODIC | DamageTag::ADDITION | DamageTag::ITEM |
						DamageTag::HP_LOSS | DamageTag::BOND | DamageTag::STEAD_SHARE | DamageTag::STEAD_THORN;
					if (target.MySide != UnitSide::ENEMY || !target.MyAlive || _event.MyElement || damage.MyType == DamageType::ELEMENTAL ||
						damage.MyIsSplash || damage.MySourceless || (damage.MyTags & excluded)) continue;
					if (!source.MyStatuses.Has(CombatStatus::STEALTH) && (!std::isfinite(source.MySiracusaStealthEnd) ||
						Time() > source.MySiracusaStealthEnd + p.MyStealthTail + 1e-9)) continue;
					++state.MyCount;
					if (!(_MyRandom.Next() < std::min(1.0, p.MyPrdStep * static_cast<double>(state.MyCount)))) continue;
					state.MyCount = 0;
					const auto amount = p.MyBaseDamage + p.MyDamagePerLayer * layers();
					if (amount > 0) (void)DealDamage(source.MyId, target.MyId, DamageInfo{.MyAmount = amount, .MyType = DamageType::TRUE_DAMAGE,
						.MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::BOND)});
					if (target.MyAlive && p.MyFear > 0) (void)ApplyStatus(target.MyId, CombatStatus::FEAR, p.MyFear, source.MyId);
				}
				break;
			case CoreBondKind::EGIR:
				if (_event.MyKind == ContentEventKind::BATTLE_START)
				{
					_MyEgirDevouring = true;
					DevourEgir(index);
					_MyEgirDevouring = false;
					if (--_MyEgirPasses == 0) FlushEgirRevives();
				}
				break;
			case CoreBondKind::SARGON:
				if (_event.MyKind == ContentEventKind::SKILL_START && member(_event.MyUnit))
				{
					const auto duration = p.MyDuration + p.MyDurationPerLayer * layers();
					if (duration > 0) for (const auto id : state.MyMembers) if (Unit(id).MyAlive && !Unit(id).MyRemoved)
					{
						(void)AddBuff(id, BuffDefinition{.MyKey = "bond:sargon", .MyDuration = duration, .MyMaxStacks = p.MyMaxStacks,
							.MyRefresh = BuffRefresh::INDEPENDENT, .MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_SPEED, p.MyAttackSpeed}}, .MyBuiltin = BuiltinBuff::SARGON_STACK});
						SyncSargon(id);
					}
					if (!effect.MyShareEquipment || !state.MyPower || Unit(_event.MyUnit).MyDefinition.MyIdentity.MyItems.empty()) continue;
					const auto lendDuration = p.MyLendDuration + p.MyLendDurationPerLayer * layers();
					if (!(lendDuration > 0)) continue;
					const auto& source = Unit(_event.MyUnit);
					const auto origin = RulePosition(source);
					constexpr std::array<RangeOffset, 8> neighbors{{{1, -1}, {1, 0}, {1, 1}, {0, -1}, {0, 1}, {-1, -1}, {-1, 0}, {-1, 1}}};
					// 先固定本次邻格收件人；借出安装的部署效果可能重入内容代码。
					std::array<UnitId, 8> recipients{}; unsigned count = 0;
					for (const auto offset : neighbors)
					{
						const auto rotated = RotateOffset(offset, source.MyFacing);
						for (const auto id : _MyAllyIds)
						{
							const auto& target = Unit(id);
							const auto point = RulePosition(target);
							if (target.MyAlive && target.MyKind == UnitKind::OPERATOR && target.MyOwner == state.MyPlayer &&
								point.MyX == origin.MyX + rotated.MyColumn && point.MyY == origin.MyY + rotated.MyRow)
							{ recipients[count++] = id; break; }
						}
					}
					for (unsigned i = 0; i < count; ++i) (void)LendEquipment(source.MyId, recipients[i], p.MyLendMaximumTier, lendDuration);
				}
				break;
			case CoreBondKind::KJERAG:
				if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && member(_event.MySource) && _event.MyTarget)
				{
					const auto& target = Unit(_event.MyTarget);
					if (target.MySide != UnitSide::ENEMY || !target.MyAlive || (!target.MyStatuses.Has(CombatStatus::COLD) && !target.MyStatuses.Has(CombatStatus::FREEZE))) continue;
					const auto multiplier = p.MyColdMultiplier + p.MyColdPerLayer * layers();
					if (p.MyDamageMultiplier > 0 && multiplier > 0) _event.MyDamage.MyMultiplier *= multiplier / p.MyDamageMultiplier;
				}
				break;
			case CoreBondKind::LATERANO:
				if (_event.MyKind == ContentEventKind::AMMO_USED && state.MyPower && member(_event.MyUnit) && p.MyAttack > 0 && state.MyBonus < p.MyAttackMaximum)
				{
					++state.MyCount;
					state.MyBonus = std::min(p.MyAttackMaximum, static_cast<double>(state.MyCount) * p.MyAttack);
					for (const auto id : state.MyMembers) (void)AddBuff(id, BuffDefinition{.MyKey = "bond:laterano:ammo",
						.MyModifiers = std::vector<AttributeChange>{{Attribute::ATTACK_PERCENT, state.MyBonus}}, .MyPersistent = true, .MyAllowDead = true});
				}
				break;
			case CoreBondKind::KAZIMIERZ:
				if (_event.MyKind == ContentEventKind::DEPLOY && _event.MyUnit && Unit(_event.MyUnit).MyOwner == state.MyPlayer && Unit(_event.MyUnit).MyKind == UnitKind::OPERATOR)
				{
					++state.MyCount; RefreshCoreBond(index);
				}
				if (_event.MyKind == ContentEventKind::DAMAGED && state.MyPower && p.MyUnblockedAttackScale > 0 && member(_event.MySource) &&
					_event.MyTarget && _event.MyDamage.MyIsAttack && !HasTag(_event.MyDamage.MyTags, DamageTag::BOND) &&
					Unit(_event.MyTarget).MySide == UnitSide::ENEMY && Unit(_event.MyTarget).MyAlive && Unit(_event.MySource).MyBlocking.empty())
					(void)DealDamage(_event.MySource, _event.MyTarget, DamageInfo{.MyAmount = p.MyUnblockedAttackScale * Unit(_event.MySource).MyStats.MyAttack,
						.MyType = DamageType::TRUE_DAMAGE, .MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::BOND)});
				break;
			case CoreBondKind::VICTORIA: case CoreBondKind::COUNT: break;
			}
		}
	}
}
