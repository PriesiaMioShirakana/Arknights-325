#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		std::size_t StatusIndex(CombatStatus _status)
		{
			const auto index = static_cast<std::size_t>(_status);
			if (index >= static_cast<std::size_t>(CombatStatus::COUNT)) throw std::invalid_argument("invalid combat status");
			return index;
		}

		constexpr bool Resisted(CombatStatus _status)
		{
			switch (_status)
			{
			case CombatStatus::STUN: case CombatStatus::FREEZE: case CombatStatus::COLD: case CombatStatus::SLEEP:
			case CombatStatus::FEAR: case CombatStatus::TREMBLE: case CombatStatus::ATTRACT: case CombatStatus::LEVITATE:
			case CombatStatus::BIND: case CombatStatus::SILENCE: case CombatStatus::DISARM: case CombatStatus::SLUGGISH:
			case CombatStatus::SLOW: case CombatStatus::GROUNDBIND: return true;
			default: return false;
			}
		}

		constexpr bool Valued(CombatStatus _status)
		{
			switch (_status)
			{
			case CombatStatus::SLOW: case CombatStatus::FRAGILE: case CombatStatus::ARTS_FRAGILE:
			case CombatStatus::PHYSICAL_FRAGILE: case CombatStatus::ELEMENTAL_FRAGILE: case CombatStatus::WEAKEN:
			case CombatStatus::ATTACK_SPEED_DOWN: case CombatStatus::DEFENSE_DOWN: case CombatStatus::RESISTANCE_DOWN:
			case CombatStatus::RESIST: return true;
			default: return false;
			}
		}

		constexpr double DefaultValue(CombatStatus _status)
		{
			switch (_status)
			{
			case CombatStatus::SLOW: case CombatStatus::RESIST: return 0.5;
			case CombatStatus::FRAGILE: case CombatStatus::ARTS_FRAGILE: case CombatStatus::PHYSICAL_FRAGILE:
			case CombatStatus::WEAKEN: case CombatStatus::DEFENSE_DOWN: return 0.3;
			case CombatStatus::ELEMENTAL_FRAGILE: return 0.2;
			case CombatStatus::ATTACK_SPEED_DOWN: return -30;
			case CombatStatus::RESISTANCE_DOWN: return 20;
			default: return 1;
			}
		}
	}

	bool Battle::ApplyStatus(UnitId _target, CombatStatus _status, double _seconds, UnitId _source, bool _force)
	{
		if (!std::isfinite(_seconds) || std::isless(_seconds, 0) || std::isgreater(_seconds, 3600))
			throw std::invalid_argument("invalid status duration");
		return ApplyStatus(_target, _status, StatusApplication{.MyDuration = _seconds, .MySource = _source, .MyForce = _force});
	}

	bool Battle::ApplyStatus(UnitId _target, CombatStatus _status, const StatusApplication& _application)
	{
		auto application = _application;
		const auto index = StatusIndex(_status);
		auto& target = _MyUnits[Index(_target)];
		if (application.MySource) (void)Index(application.MySource);
		auto duration = application.MyDuration;
		if (std::isnan(duration) || std::isless(duration, 0) ||
			(application.MyValue && !std::isfinite(*application.MyValue)) ||
			(application.MyStackAs && !std::isfinite(*application.MyStackAs)) ||
			(application.MyPoint && (!std::isfinite(application.MyPoint->MyX) || !std::isfinite(application.MyPoint->MyY))))
			throw std::invalid_argument("invalid status application");
		if (!_MyStarted || Finished() || !target.MyAlive || !std::isgreater(duration, 0)) return false;
		auto& statuses = target.MyStatuses;
		if (application.MySource && Unit(application.MySource).MySide != target.MySide &&
			statuses.Has(CombatStatus::INVULNERABLE) && statuses.Has(CombatStatus::UNTARGETABLE)) return false;
		if (!application.MyForce && (target.MyDefinition.MyImmunities[index] ||
			(_status == CombatStatus::STUN && target.MyDefinition.MyStunImmune))) return false;
		if (_status == CombatStatus::LEVITATE && ((target.MyDefinition.MyFlying && !statuses.Has(CombatStatus::GROUNDBIND)) || statuses.Has(CombatStatus::LEVITATE))) return false;
		// 切换动画的免控是职业规则，即使强制施加也拒绝；无需分配自定义监听器。
		if (target.MyProfession.MyDollSwitching && (_status == CombatStatus::STUN ||
			_status == CombatStatus::FREEZE || _status == CombatStatus::SLEEP)) return false;
		if (!_MyContentInstances.empty())
		{
			ContentEvent event{.MyKind = ContentEventKind::BEFORE_STATUS, .MySource = application.MySource,
				.MyTarget = _target, .MyStatus = _status, .MyApplication = application};
			NotifyContent(event);
			if (event.MyCancel || Finished() || !target.MyAlive) return false;
			application = event.MyApplication;
			duration = application.MyDuration;
			if (!std::isgreater(duration, 0) || (application.MyValue && !std::isfinite(*application.MyValue)) ||
				(application.MyStackAs && !std::isfinite(*application.MyStackAs)) ||
				(application.MyPoint && (!std::isfinite(application.MyPoint->MyX) || !std::isfinite(application.MyPoint->MyY)))) return false;
			if (application.MySource) (void)Index(application.MySource);
		}
		if (Resisted(_status) && !application.MyResistApplied && statuses.Has(CombatStatus::RESIST))
			duration *= 1 - std::clamp(statuses.MyValues[StatusIndex(CombatStatus::RESIST)], 0.0, 0.95);
		if ((_status == CombatStatus::LEVITATE || _status == CombatStatus::GROUNDBIND) && std::isgreater(target.MyStats.MyMass, 3)) duration /= 2;
		if (_status == CombatStatus::COLD && std::isgreater(statuses.MyRemaining[index], 0) && !target.MyDefinition.MyImmunities[StatusIndex(CombatStatus::FREEZE)])
		{
			const auto freezeFor = std::max(statuses.MyRemaining[index], duration);
			const auto frozen = ApplyStatus(_target, CombatStatus::FREEZE, StatusApplication{
				.MyDuration = freezeFor, .MySource = application.MySource, .MyForce = application.MyForce, .MyResistApplied = true});
			if (frozen && target.MySide == UnitSide::ENEMY)
			{
				(void)RemoveStatus(_target, CombatStatus::COLD);
				return true;
			}
		}
		auto& remaining = statuses.MyRemaining[index];
		auto& oldValue = statuses.MyValues[index];
		auto& oldStrength = statuses.MyStrengths[index];
		auto& tail = statuses.MyTails[index];
		const auto value = application.MyValue.value_or(DefaultValue(_status));
		const auto strength = std::abs(application.MyStackAs.value_or(value));
		if (Valued(_status) && std::isgreater(remaining, 0))
		{
			const auto oldEnd = Time() + remaining;
			const auto newEnd = Time() + duration;
			if (std::isgreater(strength, oldStrength + 1e-12))
			{
				if (tail && std::islessequal(tail->MyEnd, newEnd)) tail.reset();
				if (std::isgreater(oldEnd, newEnd) && (!tail || std::isgreaterequal(oldEnd, tail->MyEnd)))
					tail = StatusTail{.MyEnd = oldEnd, .MyValue = oldValue, .MyStrength = oldStrength};
				remaining = duration;
				oldValue = value;
				oldStrength = strength;
			}
			else if (std::isless(strength, oldStrength - 1e-12))
			{
				if (std::isgreater(newEnd, oldEnd) && (!tail || std::isgreater(newEnd, tail->MyEnd)))
					tail = StatusTail{.MyEnd = newEnd, .MyValue = value, .MyStrength = strength};
			}
			else remaining = std::max(remaining, duration);
		}
		else
		{
			remaining = _status == CombatStatus::PALSY ? duration : std::max(remaining, duration);
			oldValue = _status == CombatStatus::PALSY ? std::clamp(oldValue + std::max(1.0, std::floor(value)), 1.0, 3.0) : value;
			oldStrength = strength;
		}
		if (_status == CombatStatus::FEAR)
		{
			const bool self = application.MySource == 0 || application.MySource == _target;
			statuses.MyFear = FearStamp{.MySequence = ++_MyFearSequence, .MyHit = target.MyPosition,
				.MySource = self ? target.MyPosition : Unit(application.MySource).MyPosition, .MySelf = self};
		}
		if (_status == CombatStatus::ATTRACT)
		{
			if (application.MyPoint) statuses.MyAttractPoint = application.MyPoint;
			else if (application.MySource) statuses.MyAttractPoint = Unit(application.MySource).MyPosition;
			if (statuses.MyAttractPoint)
			{
				target.MyInducedMode = CombatStatus::COUNT;
				const auto rect = _MyGrid ? _MyGrid->Rect() : FieldRect{};
				statuses.MyAttractPoint->MyX = std::clamp(std::floor(statuses.MyAttractPoint->MyX + 0.5), static_cast<double>(rect.MyFirstColumn), static_cast<double>(rect.MyLastColumn));
				statuses.MyAttractPoint->MyY = std::clamp(std::floor(statuses.MyAttractPoint->MyY + 0.5), static_cast<double>(rect.MyFirstRow), static_cast<double>(rect.MyLastRow));
			}
		}
		Recalculate(target);
		Emit(BattleEventKind::STATUS_APPLIED, application.MySource, _target, duration, _status);
		ContentEvent event{.MyKind = ContentEventKind::STATUS_APPLIED, .MySource = application.MySource,
			.MyTarget = _target, .MyAmount = duration, .MyStatus = _status, .MyApplication = application};
		NotifyContent(event);
		return true;
	}

	bool Battle::RemoveStatus(UnitId _target, CombatStatus _status)
	{
		const auto index = StatusIndex(_status);
		auto& target = _MyUnits[Index(_target)];
		auto& remaining = target.MyStatuses.MyRemaining[index];
		if (!_MyStarted || Finished() || !target.MyAlive || !std::isgreater(remaining, 0)) return false;
		remaining = 0;
		if (_status == CombatStatus::FEAR) target.MyStatuses.MyFear.reset();
		target.MyStatuses.MyValues[index] = 0;
		target.MyStatuses.MyTails[index].reset();
		Recalculate(target);
		Emit(BattleEventKind::STATUS_REMOVED, 0, _target, 0, _status);
		return true;
	}

	void Battle::TickStatuses()
	{
		for (std::size_t unitIndex = 0; unitIndex < _MyUnits.size(); ++unitIndex)
		{
			auto& unit = _MyUnits[unitIndex];
			if (!unit.MyAlive) continue;
			bool changed = false;
			auto& statuses = unit.MyStatuses;
			for (std::size_t i = 0; i < statuses.MyRemaining.size(); ++i)
			{
				auto& remaining = statuses.MyRemaining[i];
				if (!std::isgreater(remaining, 0)) continue;
				remaining -= BattleClock::StepSeconds;
				if (std::isgreater(remaining, 1e-9)) continue;
				remaining = 0;
				if (static_cast<CombatStatus>(i) == CombatStatus::FEAR) statuses.MyFear.reset();
				changed = true;
				Emit(BattleEventKind::STATUS_REMOVED, 0, unit.MyId, 0, static_cast<CombatStatus>(i));
				const auto tail = statuses.MyTails[i];
				statuses.MyTails[i].reset();
				if (tail && std::isgreater(tail->MyEnd - Time(), 1e-6))
				{
					remaining = tail->MyEnd - Time();
					statuses.MyValues[i] = tail->MyValue;
					statuses.MyStrengths[i] = tail->MyStrength;
					Emit(BattleEventKind::STATUS_APPLIED, 0, unit.MyId, remaining, static_cast<CombatStatus>(i));
				}
				else statuses.MyValues[i] = 0;
			}
			if (std::isgreater(statuses.MyRemaining[StatusIndex(CombatStatus::RESIST)], 0))
			{
				statuses.MyResistAccumulator += BattleClock::StepSeconds;
				if (std::isgreaterequal(statuses.MyResistAccumulator, 5 - 1e-9))
				{
					statuses.MyResistAccumulator -= 5;
					auto& stacks = statuses.MyValues[StatusIndex(CombatStatus::PALSY)];
					if (std::isgreater(stacks, 0) && std::islessequal(--stacks, 0)) (void)RemoveStatus(unit.MyId, CombatStatus::PALSY);
				}
			}
			if (changed) Recalculate(unit);
		}
	}

	void Battle::ReleaseBlocked(CombatUnit& _ally)
	{
		if (_ally.MyBlocking.empty()) return;
		auto& scratch = AcquireAttackScratch();
		struct ScratchGuard { std::size_t& MyDepth; ~ScratchGuard() { --MyDepth; } } guard{.MyDepth = _MyAttackDepth};
		scratch.MyTargets.assign(_ally.MyBlocking.begin(), _ally.MyBlocking.end());
		_ally.MyBlocking.clear();
		for (const auto id : scratch.MyTargets)
		{
			auto& enemy = _MyUnits[Index(id)];
			if (enemy.MyBlockedBy != _ally.MyId) continue;
			enemy.MyBlockedBy = 0;
			SwitchStealth(enemy);
		}
	}
}
