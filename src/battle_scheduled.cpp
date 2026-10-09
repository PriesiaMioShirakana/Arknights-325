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
			case ScheduledKind::PODEGO_AURA:
				PodegoAura(action.MySource);
				action.MyAt += action.MyInterval;
				if (action.MyAt <= now) action.MyAt = Time() + action.MyInterval;
				if (!Finished() && !Unit(action.MySource).MyRemoved) Schedule(action);
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
			case ScheduledKind::TOKEN_EXPIRE:
				if (Unit(action.MyTarget).MyAlive) Retreat(action.MyTarget, true, RemovalReason::EXPIRED);
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
