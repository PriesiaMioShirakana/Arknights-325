#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::InstallProfession(CombatUnit& _unit)
	{
		const auto& definition = _unit.MyDefinition.MyProfession;
		_unit.MyProfession.MyFunnelScale = definition.MyFunnelInitial;
		_unit.MyProfession.MyAmmo = definition.MyAmmoMax;
		if (definition.MyKind == ProfessionTrait::BARD) _unit.MyProfession.MyAuraKey = "trait:bard:" + std::to_string(_unit.MyId);
		if (definition.MyKind == ProfessionTrait::BARD || definition.MyKind == ProfessionTrait::GEEK ||
			definition.MyKind == ProfessionTrait::LIBRATOR || definition.MyKind == ProfessionTrait::MERCHANT)
		{
			const auto interval = definition.MyKind == ProfessionTrait::BARD ? 0.25 : definition.MyKind == ProfessionTrait::MERCHANT
				? std::max(BattleClock::StepSeconds, definition.MyMerchantInterval) : 1.0;
			Schedule(ScheduledAction{.MyAt = Time() + interval, .MyKind = ScheduledKind::PROFESSION, .MySource = _unit.MyId, .MyInterval = interval});
		}
		if (definition.MyKind == ProfessionTrait::SKYWALKER && (!_unit.MyDefinition.MyOperatorKit || !std::holds_alternative<TippiKit>(*_unit.MyDefinition.MyOperatorKit)))
		{
			StatusFlags flags;
			flags.set(static_cast<std::size_t>(CombatStatus::BLOCK_FLYING));
			(void)AddBuff(_unit.MyId, BuffDefinition{.MyKey = "trait:skywalker", .MyFlags = flags,
				.MyPersistent = true, .MyAllowDead = true});
		}
		if (definition.MyKind == ProfessionTrait::STALKER)
			(void)AddBuff(_unit.MyId, BuffDefinition{.MyKey = "trait:stalker",
				.MyModifiers = std::vector{AttributeChange{.MyAttribute = Attribute::PHYSICAL_DODGE, .MyValue = definition.MyDodge},
					AttributeChange{.MyAttribute = Attribute::ARTS_DODGE, .MyValue = definition.MyDodge},
					AttributeChange{.MyAttribute = Attribute::TAUNT, .MyValue = -1}}, .MyPersistent = true, .MyAllowDead = true});
	}

	void Battle::NotifyProfession(const ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::TICK)
		{
			for (const auto id : _MyAllyIds)
			{
				auto& unit = _MyUnits[Index(id)];
				if (!unit.MyAlive || unit.MyDefinition.MyProfession.MyKind != ProfessionTrait::HUNTER) continue;
				auto& state = unit.MyProfession;
				// 原实现精确按一秒累积，不增加误差阈值；停手与重新部署均保留尚未完成的装填进度。
				if (std::isgreaterequal(Time() - unit.MyLastAttackAt, 1) && std::isless(state.MyAmmo, unit.MyDefinition.MyProfession.MyAmmoMax))
				{
					state.MyReloadAccumulator += _event.MyDelta;
					if (std::isgreaterequal(state.MyReloadAccumulator, 1))
					{
						state.MyReloadAccumulator -= 1;
						state.MyAmmo += 1;
					}
				}
				else state.MyReloadAccumulator = 0;
			}
			return;
		}
		if (_event.MySource && _event.MyTarget)
		{
			auto& source = _MyUnits[Index(_event.MySource)];
			const auto& target = Unit(_event.MyTarget);
			const auto& definition = source.MyDefinition.MyProfession;
			if (_event.MyKind == ContentEventKind::BEFORE_KILL && definition.MyKind == ProfessionTrait::CHARGER &&
				target.MySide == UnitSide::ENEMY && source.MyOwner != NoPlayer)
				(void)AddDp(_MyPlayers[source.MyOwner].MyPlayerId, definition.MyDpOnKill);
			if (_event.MyKind == ContentEventKind::DAMAGED && definition.MyKind == ProfessionTrait::INCANTATION && source.MyAlive &&
				target.MySide == UnitSide::ENEMY && std::isgreater(_event.MyAmount, 0) && !_event.MyElement && _event.MyDamage.MyType != DamageType::ELEMENTAL)
			{
				const auto ally = _event.MyDamage.MyTraitAlly ? _event.MyDamage.MyTraitAlly : LowestHpAllyInRange(source);
				if (ally) (void)Heal(source.MyId, ally, _event.MyAmount * definition.MyHealRatio);
			}
		}
		if (_event.MyUnit && _event.MyKind == ContentEventKind::DEATH)
		{
			auto& unit = _MyUnits[Index(_event.MyUnit)];
			unit.MyProfession.MyDoll = unit.MyProfession.MyDollSwitching = false;
		}
		if (!_event.MyUnit || (_event.MyKind != ContentEventKind::DEPLOY && _event.MyKind != ContentEventKind::SKILL_START &&
			_event.MyKind != ContentEventKind::SKILL_END)) return;
		auto& unit = _MyUnits[Index(_event.MyUnit)];
		const auto& definition = unit.MyDefinition.MyProfession;
		switch (definition.MyKind)
		{
		case ProfessionTrait::TACTICIAN:
			if (const auto* vigil = unit.MyDefinition.MyOperatorKit ? std::get_if<VigilKit>(unit.MyDefinition.MyOperatorKit) : nullptr)
			{
				if (_event.MyKind == ContentEventKind::DEPLOY) VigilDeploy(unit.MyId, *vigil);
				break;
			}
			if (_event.MyKind == ContentEventKind::DEPLOY && unit.MyAlive &&
				(!unit.MyProfession.MyReinforcement || !Unit(unit.MyProfession.MyReinforcement).MyAlive))
			{
				if (const auto point = FindTacticalPoint(unit.MyId))
				{
					// 援军使用本体基础值，不继承本体当时的 Buff；单位自身仍通过普通召唤生命周期管理。
					const auto& stats = unit.MyDefinition.MyStats;
					unit.MyProfession.MyReinforcement = SpawnToken(TokenSpawn{
						.MyDefinition = CombatDefinition{.MyId = "token_tactician_reinforce",
							.MyStats = CombatStats{.MyMaxHealth = std::max(1.0, stats.MyMaxHealth * 0.7),
								.MyAttack = stats.MyAttack * 0.5, .MyDefense = stats.MyDefense,
								.MyBaseAttackTime = 1.2, .MyBlockCount = 1, .MyRedeploySeconds = 999},
							.MyRange = {RangeOffset{}}}, .MyPosition = *point, .MyOwnerUnit = unit.MyId});
				}
			}
			break;
		case ProfessionTrait::LOOPSHOOTER:
			if (_event.MyKind == ContentEventKind::DEPLOY) unit.MyProfession.MyBoomerangsOut = 0;
			break;
		case ProfessionTrait::LIBRATOR:
			if (_event.MyKind == ContentEventKind::DEPLOY) unit.MyProfession.MyRamp = std::min(definition.MyRampMax, definition.MyRampInitial);
			else if (_event.MyKind == ContentEventKind::SKILL_END) unit.MyProfession.MyRamp = 0;
			ApplyLibrator(unit, _event.MyKind != ContentEventKind::SKILL_START);
			break;
		case ProfessionTrait::HUNTER:
			if (_event.MyKind == ContentEventKind::DEPLOY) unit.MyProfession.MyAmmo = definition.MyAmmoMax;
			break;
		case ProfessionTrait::MYSTIC:
			if (_event.MyKind == ContentEventKind::DEPLOY && !_event.MyMove) unit.MyProfession.MyStored = 0;
			break;
		case ProfessionTrait::PHALANX:
			if (unit.MySkill.MyActive) (void)RemoveBuff(unit.MyId, "trait:phalanxGuard");
			else (void)AddBuff(unit.MyId, BuffDefinition{.MyKey = "trait:phalanxGuard",
				.MyModifiers = std::vector{AttributeChange{.MyAttribute = Attribute::DEFENSE_PERCENT, .MyValue = definition.MyGuardDefense},
					AttributeChange{.MyAttribute = Attribute::RESISTANCE_FLAT, .MyValue = definition.MyGuardResistance}}});
			break;
		case ProfessionTrait::BEARER:
			if (_event.MyKind == ContentEventKind::SKILL_START)
			{
				(void)AddBuff(unit.MyId, BuffDefinition{.MyKey = "trait:bearer",
					.MyModifiers = std::vector{AttributeChange{.MyAttribute = Attribute::BLOCK_COUNT, .MyValue = -99}}});
				ReleaseBlocked(unit);
			}
			else if (_event.MyKind == ContentEventKind::SKILL_END) (void)RemoveBuff(unit.MyId, "trait:bearer");
			break;
		default: break;
		}
	}

	bool Battle::ProfessionCanAttack(const CombatUnit& _unit) const noexcept
	{
		return (_unit.MyDefinition.MyProfession.MyKind != ProfessionTrait::HUNTER || std::isgreater(_unit.MyProfession.MyAmmo, 0)) &&
			(_unit.MyDefinition.MyProfession.MyKind != ProfessionTrait::LOOPSHOOTER || _unit.MyProfession.MyBoomerangsOut == 0);
	}

	void Battle::StoreEnergy(CombatUnit& _unit)
	{
		if (_unit.MyDefinition.MyProfession.MyKind == ProfessionTrait::MYSTIC &&
			_unit.MyProfession.MyStored < _unit.MyDefinition.MyProfession.MyStoreMax)
		{
			++_unit.MyProfession.MyStored;
			_unit.MyAttackCooldown = std::max(_unit.MyAttackCooldown, _unit.MyStats.AttackInterval());
		}
	}
}
