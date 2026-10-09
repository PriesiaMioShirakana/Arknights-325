#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::Schedule(ScheduledAction _action)
	{
		if (!_action.MySequence) _action.MySequence = ++_MyScheduleSequence;
		_MyScheduled.emplace_back(_action);
		std::push_heap(_MyScheduled.begin(), _MyScheduled.end(), ScheduledAction::Later);
	}

	std::uint64_t Battle::StartColdWind(ColdWindDefinition _definition)
	{
		if (!std::isfinite(_definition.MyInterval) || !std::isgreater(_definition.MyInterval, 0) ||
			!std::isfinite(_definition.MyBaseDuration) || !std::isfinite(_definition.MyDurationPerLayer) ||
			(_definition.MyFirstDelay && !std::isfinite(*_definition.MyFirstDelay)) ||
			(!_definition.MyBondId.empty() && _definition.MyPlayerId.empty()))
			throw std::invalid_argument("invalid cold wind definition");
		const auto owner = _definition.MyPlayerId.empty() ? NoPlayer : Owner(_definition.MyPlayerId);
		if (Finished()) return 0;
		const auto first = std::max(0.0, _definition.MyFirstDelay.value_or(_definition.MyInterval));
		_MyColdWinds.emplace_back(ColdWindState{.MyDefinition = std::move(_definition), .MyOwner = owner});
		const auto handle = static_cast<std::uint64_t>(_MyColdWinds.size());
		Schedule(ScheduledAction{.MyAt = Time() + first, .MyKind = ScheduledKind::COLD_WIND, .MyHandle = handle});
		return handle;
	}

	bool Battle::CancelColdWind(std::uint64_t _handle)
	{
		if (!_handle) return false;
		auto& state = _MyColdWinds.at(static_cast<std::size_t>(_handle - 1));
		if (state.MyCancelled) return false;
		state.MyCancelled = true;
		return true;
	}

	std::uint64_t Battle::ColdWindGusts(std::uint64_t _handle) const
	{
		if (!_handle) return 0;
		return _MyColdWinds.at(static_cast<std::size_t>(_handle - 1)).MyGusts;
	}

	void Battle::Gust(ColdWindState& _state)
	{
		const auto& definition = _state.MyDefinition;
		const auto layers = definition.MyBondId.empty() ? 0 : BondLayers(definition.MyPlayerId, definition.MyBondId);
		const auto duration = definition.MyBaseDuration + definition.MyDurationPerLayer * layers;
		// 对齐 devices.js 的 num()：非有限或非正持续时间不吹风，也不增加阵数。
		if (!std::isfinite(duration) || !std::isgreater(duration, 0)) return;
		// 状态扩展回调可能召唤敌人；按下标遍历可增长 ID 表，不持有 vector 迭代器。
		for (std::size_t i = 0; i < _MyEnemyIds.size(); ++i)
		{
			const auto& unit = Unit(_MyEnemyIds[i]);
			if (!unit.MyAlive || unit.MyHidden ||
				(definition.MyOwnerOnly && _state.MyOwner != NoPlayer && unit.MyOwner != _state.MyOwner)) continue;
			(void)ApplyStatus(unit.MyId, CombatStatus::COLD, StatusApplication{.MyDuration = duration});
		}
		++_state.MyGusts;
	}

	void Battle::TickScheduled()
	{
		// 先提取本阶段快照。回调中新建的零延迟动作必须等下一帧，和 JS _runScheduled 一致。
		// 容量跨帧复用；动作是值，不依赖队列节点地址或 Battle 的物理地址。
		const auto now = Time() + 1e-9;
		_MyDueActions.clear();
		while (!_MyScheduled.empty() && std::islessequal(_MyScheduled.front().MyAt, now))
		{
			std::pop_heap(_MyScheduled.begin(), _MyScheduled.end(), ScheduledAction::Later);
			_MyDueActions.emplace_back(_MyScheduled.back());
			_MyScheduled.pop_back();
		}
		for (auto action : _MyDueActions)
		{
			if (Finished()) break;
			switch (action.MyKind)
			{
			case ScheduledKind::OPERATOR_AURA:
			case ScheduledKind::OPERATOR_GROUND_ASPD:
				if (action.MyKind == ScheduledKind::OPERATOR_AURA) RefreshOperatorAura(static_cast<std::size_t>(action.MyHandle));
				else RefreshGroundAttackSpeed(static_cast<std::size_t>(action.MyHandle));
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::WOLF_GROW:
			{
				const auto& unit = Unit(action.MySource);
				if (!unit.MyAlive || unit.MyDeploySequence != action.MyVersion || !unit.MyDefinition.MyTokenKit || !unit.MyDefinition.MyTokenKit->MyWolf) break;
				(void)AddWolfShadow(action.MySource, unit.MyDefinition.MyTokenKit->MyWolf->MyMaxShadows);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished()) Schedule(action);
				break;
			}
			case ScheduledKind::WOLF_RETURN:
				if (!Unit(action.MySource).MyWolfTactical || Unit(action.MySource).MyWolfFormSequence != action.MyVersion) break;
				if (!ReturnWolf(action.MySource) && !Finished()) { action.MyAt = Time() + 0.25; Schedule(action); }
				break;
			case ScheduledKind::VIGIL_MARK:
				VigilMark(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::LISA_AURA:
			case ScheduledKind::DEMKNI_SUIT:
				if (action.MyKind == ScheduledKind::LISA_AURA) LisaAura(action.MySource); else DemkniSuit(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::REED2_BURST:
				Reed2Burst(action.MySource, action.MyPoint);
				break;
			case ScheduledKind::ROSMON_STABLE:
				RosmonStable(action.MySource);
				break;
			case ScheduledKind::CELLO_PULSE:
			case ScheduledKind::REED2_MODULE:
				if (action.MyKind == ScheduledKind::CELLO_PULSE) CelloPulse(action.MySource, static_cast<unsigned>(action.MyHandle)); else Reed2Module(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyOperatorHooksReleased) Schedule(action);
				break;
			case ScheduledKind::HALO2_PULSE:
			case ScheduledKind::AGOAT2_PULSE:
				if (action.MyKind == ScheduledKind::HALO2_PULSE) Halo2Pulse(action.MySource); else Agoat2Pulse(action.MySource, action.MyHandle != 0);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyOperatorHooksReleased) Schedule(action);
				break;
			case ScheduledKind::SIEGE2_PULSE:
				Siege2Pulse(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyOperatorHooksReleased) Schedule(action);
				break;
			case ScheduledKind::SBELL2_MODULE:
				Sbell2Module(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyOperatorHooksReleased) Schedule(action);
				break;
			case ScheduledKind::YU_PULSE:
				YuPulse(action.MySource, static_cast<unsigned>(action.MyHandle));
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyOperatorHooksReleased) Schedule(action);
				break;
			case ScheduledKind::PASNGR_STORM:
				if (const auto& unit = Unit(action.MySource); unit.MyAlive && !unit.MyHidden && !unit.MyOperatorHooksReleased && unit.MyDeploySequence == action.MyVersion)
				{
					PasngrStorm(action.MySource, action.MyPoint);
					if (action.MyRemaining > 1 && !Finished()) { --action.MyRemaining; action.MyAt = Time() + action.MyInterval; Schedule(action); }
				}
				break;
			case ScheduledKind::QIUBAI_BURST:
				if (const auto& unit = Unit(action.MySource); unit.MyAlive && !unit.MyOperatorHooksReleased && unit.MyDeploySequence == action.MyVersion) QiubaiBurst(unit.MyId, action.MyTarget);
				break;
			case ScheduledKind::LEMUEN_WANTED:
				LemuenWanted();
				if (!Finished()) { action.MyAt = Time() + action.MyInterval; Schedule(action); }
				break;
			case ScheduledKind::LEMUEN_EXTRADITION:
				if (const auto& unit = Unit(action.MySource); unit.MyAlive && !unit.MyHidden && !unit.MyOperatorHooksReleased && unit.MyDeploySequence == action.MyVersion)
				{
					const auto attack = std::get<LemuenKit>(*unit.MyDefinition.MyOperatorKit).MyExtraditionAttack;
					if (std::islessgreater(attack, 0)) (void)AddBuff(unit.MyId, {.MyKey = "lemuen:extradition", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = attack}}});
				}
				break;
			case ScheduledKind::LEMUEN_FIRE:
				LemuenFire(static_cast<std::size_t>(action.MyHandle), action.MyRemaining);
				break;
			case ScheduledKind::LEMUEN_BLAST:
				if (!Unit(action.MySource).MyOperatorHooksReleased) LemuenBlast(action.MySource, action.MyPoint, action.MyAmount);
				break;
			case ScheduledKind::THORN2_VISION:
			case ScheduledKind::THORN2_ZONES:
				if (action.MyKind == ScheduledKind::THORN2_VISION) Thorn2Vision(action.MySource); else Thorn2Zones(action.MySource);
				if (!Finished() && !Unit(action.MySource).MyOperatorHooksReleased)
				{ action.MyAt = Time() + action.MyInterval; Schedule(action); }
				break;
			case ScheduledKind::SNTLLA_TALENT:
				SntllaTalent(action.MySource);
				if (!Finished() && !Unit(action.MySource).MyOperatorHooksReleased)
				{ action.MyAt = Time() + action.MyInterval; Schedule(action); }
				break;
			case ScheduledKind::SNTLLA_IMPACT:
				if (!Unit(action.MySource).MyOperatorHooksReleased) SntllaImpact(action.MySource, action.MyPoint);
				break;
			case ScheduledKind::SVASH2_SNOW:
				Svash2Snow(action.MySource);
				if (!Finished() && !Unit(action.MySource).MyOperatorHooksReleased)
				{ action.MyAt = Time() + action.MyInterval; Schedule(action); }
				break;
			case ScheduledKind::GHOST2_SLOW:
			case ScheduledKind::GHOST2_DAMAGE:
				Ghost2Pulse(action.MySource, action.MyKind == ScheduledKind::GHOST2_DAMAGE);
				if (!Finished() && !Unit(action.MySource).MyOperatorHooksReleased)
				{ action.MyAt = Time() + action.MyInterval; Schedule(action); }
				break;
			case ScheduledKind::HORN_FLARES:
				HornFlares(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyOperatorHooksReleased) Schedule(action);
				break;
			case ScheduledKind::SURTR_RETREAT:
				if (Unit(action.MySource).MyAlive && Unit(action.MySource).MyDeploySequence == action.MyVersion) Retreat(action.MySource, false, RemovalReason::RETREAT, true);
				break;
			case ScheduledKind::BLAZE_GROUND:
				Blaze2Ground(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::TITI_DREAM:
			case ScheduledKind::TITI_VIGOR:
				TitiPulse(action.MySource, action.MyKind == ScheduledKind::TITI_DREAM);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::EXCU2_ATTACK:
				Excu2ExtraAttack(action.MySource, action.MyVersion);
				break;
			case ScheduledKind::CETSYR_MOTES:
				CetsyrMotes(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::OPERATOR_MODULE:
				OperatorModuleTick(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::TEXAS2_RAIN:
				if (!Unit(action.MySource).MyAlive || !Unit(action.MySource).MySkill.MyActive || Unit(action.MySource).MyTexas2RainVersion != action.MyVersion) break;
				Texas2Rain(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished()) Schedule(action);
				break;
			case ScheduledKind::MUDROK_LAYERS:
			{
				const auto& unit = Unit(action.MySource);
				if (unit.MyDeploySequence != action.MyVersion || unit.MyRemoved) break;
				const auto& kit = std::get<MudrokKit>(*unit.MyDefinition.MyOperatorKit);
				if (unit.MyAlive && unit.MyMudrokLayers < kit.MyMaxLayers) MudrokLayers(unit.MyId, std::min(kit.MyMaxLayers, unit.MyMudrokLayers + kit.MyLayerGain));
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished()) Schedule(action);
				break;
			}
			case ScheduledKind::GNOSIS_AURA:
			case ScheduledKind::GNOSIS_RESIST:
			case ScheduledKind::LIONHD_PRESENCE:
				if (action.MyKind == ScheduledKind::LIONHD_PRESENCE) LionhdPresence(action.MySource);
				else GnosisAura(action.MySource, action.MyKind == ScheduledKind::GNOSIS_RESIST);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::CATHY_FORGE:
				if (Unit(action.MySource).MyDeploySequence != action.MyVersion) break;
				RefreshOperatorAura(action.MyHandle);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::CAT_SHIELD:
				CatShieldTick(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::MIZUKI_PRESENCE:
			case ScheduledKind::AROMA_LANDING:
				if (action.MyKind == ScheduledKind::MIZUKI_PRESENCE) MizukiPresence(action.MySource); else AromaLanding(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::INES_SENTRY:
				InesSentry(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::INES_FIRST_RETREAT:
				if (Unit(action.MySource).MyAlive)
				{ Retreat(action.MySource); _MyUnits[Index(action.MySource)].MyRespawnAt = Time(); }
				break;
			case ScheduledKind::RMIXER_SHIELD:
				RmixerShield(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::ARCHET_TACTICS:
				ArchetTactics(action.MySource);
				action.MyAt += action.MyInterval;
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::ANGEL_BLESS:
				AngelBless(action.MySource);
				break;
			case ScheduledKind::DOCKED_SUMMON_RETRY:
				_MyUnits[Index(action.MyTarget)].MySummonRetry = false;
				(void)DeployDockedSummon(action.MyTarget);
				break;
			case ScheduledKind::TOKEN_KIT_EXPIRE:
				if (Unit(action.MyTarget).MyAlive && Unit(action.MyTarget).MyDeploySequence == action.MyVersion)
					Retreat(action.MyTarget, true, RemovalReason::EXPIRED);
				break;
			case ScheduledKind::TINMAN_ZONE:
				TinmanZonePulse(action.MySource, action.MyPoint, action.MyAmount, action.MyHandle, action.MyVersion % 4 == 0);
				++action.MyVersion;
				action.MyAt += action.MyInterval;
				if (action.MyAt <= now) action.MyAt = Time() + action.MyInterval;
				if (--action.MyRemaining)
				{
					if (!Finished()) Schedule(action);
				}
				else --_MyUnits[Index(action.MySource)].MyTinmanZones;
				break;
			case ScheduledKind::PODEGO_ZONE:
				PodegoZonePulse(action.MySource, action.MyPoint, action.MyAmount);
				action.MyAt += action.MyInterval;
				if (action.MyAt <= now) action.MyAt = Time() + action.MyInterval;
				if (--action.MyRemaining && !Finished()) Schedule(action);
				break;
			case ScheduledKind::INSIDER_AMMO:
				GrantInsiderAmmo(action.MySource, action.MyVersion);
				break;
			case ScheduledKind::OPERATOR_REVEAL:
				OperatorReveal(action.MySource);
				action.MyAt += action.MyInterval;
				if (action.MyAt <= now) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			case ScheduledKind::GARRISON_REFRESH:
				_MyGarrisonPending = false; RefreshGarrisons();
				break;
			case ScheduledKind::CORE_BOND_REFRESH:
				_MyCoreBonds[static_cast<std::size_t>(action.MyHandle)].MyPending = false;
				RefreshCoreBond(static_cast<std::size_t>(action.MyHandle));
				break;
			case ScheduledKind::CORE_BOND_PULSE:
				CoreBondPulse(static_cast<std::size_t>(action.MyHandle));
				action.MyAt += action.MyInterval;
				if (action.MyAt <= now) action.MyAt = Time() + action.MyInterval;
				if (!Finished()) Schedule(action);
				break;
			case ScheduledKind::BOND_REFRESH:
				_MyAddonBonds[static_cast<std::size_t>(action.MyHandle)].MyPending = false;
				RefreshAddonBonds(static_cast<std::size_t>(action.MyHandle));
				UpdateBondAura(static_cast<std::size_t>(action.MyHandle));
				break;
			case ScheduledKind::BOND_AURA:
			case ScheduledKind::BOND_RAID:
				for (std::size_t i = 0; i < _MyAddonBonds.size(); ++i)
					if (action.MyKind == ScheduledKind::BOND_AURA) UpdateBondAura(i); else PollRaidBond(i);
				action.MyAt += 0.25;
				if (action.MyAt <= now) action.MyAt = Time() + 0.25;
				if (!Finished()) Schedule(action);
				break;
			case ScheduledKind::EQUIPMENT_EXPIRE:
				if (_MyEquipmentLends[static_cast<std::size_t>(action.MyHandle)].MyVersion == action.MyVersion)
					ExpireEquipmentLend(static_cast<std::size_t>(action.MyHandle));
				break;
			case ScheduledKind::HAMMER:
			{
				auto& hammer = _MyHammers[static_cast<std::size_t>(action.MyHandle)];
				if (Unit(action.MySource).MyRemoved || !hammer.MyReferences || hammer.MyGeneration != action.MyVersion) break;
				RefreshHammer(hammer);
				action.MyAt += 0.5;
				if (action.MyAt <= now) action.MyAt = Time() + 0.5;
				if (!Finished()) Schedule(action);
				break;
			}
			case ScheduledKind::EQUIPMENT_RETREAT:
				if (Unit(action.MySource).MyAlive) (void)Retreat(action.MySource, false, RemovalReason::RETREAT, true);
				break;
			case ScheduledKind::EQUIPMENT:
			{
				if (Unit(action.MySource).MyRemoved || !EquipmentActive(_MyEquipment[static_cast<std::size_t>(action.MyHandle)])) break;
				unsigned catches = 0;
				while (action.MyAt <= now && !Finished() && catches++ < 8)
				{
					EquipmentPeriodic(static_cast<std::size_t>(action.MyHandle));
					action.MyAt += action.MyInterval;
				}
				if (action.MyAt <= now) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
				break;
			}
			case ScheduledKind::AFTERSHOCK:
				Aftershock(action.MySource, action.MyPoint);
				break;
			case ScheduledKind::PROFESSION:
			{
				auto& unit = _MyUnits[Index(action.MySource)];
				if (unit.MyRemoved) break;
				unsigned catches = 0;
				while (std::islessequal(action.MyAt, now) && !Finished() && catches++ < 8)
				{
					ProfessionPeriodic(unit);
					action.MyAt += action.MyInterval;
				}
				if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !unit.MyRemoved) Schedule(action);
				break;
			}
			case ScheduledKind::CRATE_BREAK:
			{
				const auto& enemy = Unit(action.MySource);
				auto& crate = _MyUnits[Index(action.MyTarget)];
				if (crate.MyAlive && enemy.MyAlive && enemy.MyBlockedBy == crate.MyId) Kill(crate, enemy.MyId);
				break;
			}
			case ScheduledKind::DUSK_EXPIRE:
				if (Unit(action.MyTarget).MyAlive && Unit(action.MyTarget).MyDeploySequence == action.MyVersion)
				{
					const auto until = Unit(action.MyTarget).MyDuskUntil;
					if (std::isgreater(until - Time(), 1e-6)) { action.MyAt = until; Schedule(action); }
					else Retreat(action.MyTarget, true, RemovalReason::EXPIRED);
				}
				break;
			case ScheduledKind::TOKEN_EXPIRE:
				if (Unit(action.MyTarget).MyAlive && Unit(action.MyTarget).MyDeploySequence == action.MyVersion) Retreat(action.MyTarget, true, RemovalReason::EXPIRED);
				break;
			case ScheduledKind::COLD_WIND:
			{
				auto& state = _MyColdWinds[static_cast<std::size_t>(action.MyHandle - 1)];
				if (state.MyCancelled) break;
				if (!std::isgreater(action.MyInterval, 0))
				{
					Gust(state);
					// 首次回调在实际执行时创建循环计时器，不能从取整前的首次 due 累加间隔。
					action.MyInterval = std::max(BattleClock::StepSeconds, state.MyDefinition.MyInterval);
					action.MyAt = Time() + action.MyInterval;
					action.MySequence = 0;
				}
				else
				{
					unsigned catches = 0;
					while (std::islessequal(action.MyAt, now) && !state.MyCancelled && !Finished() && catches++ < 8)
					{
						Gust(state);
						action.MyAt += action.MyInterval;
					}
					if (std::islessequal(action.MyAt, now)) action.MyAt = Time() + action.MyInterval;
				}
				if (!state.MyCancelled && !Finished()) Schedule(action);
				break;
			}
			}
		}
	}
}
