#include <algorithm>
#include <stronghold/simulation/battle.hpp>
#include <stronghold/domain/final_assault.hpp>

namespace Stronghold
{
	void Battle::ValidateBuff(const BuffDefinition& _definition)
	{
		if (_definition.MyKey.empty() || _definition.MyKey.size() > 256 ||
			(std::isnan(_definition.MyDuration) || std::isless(_definition.MyDuration, 0)) || _definition.MyStacks == 0 || _definition.MyStacks > 1000000 ||
			_definition.MyMaxStacks == 0 || _definition.MyMaxStacks > 1000000 ||
			static_cast<unsigned>(_definition.MyRefresh) > static_cast<unsigned>(BuffRefresh::KEEP) ||
			!std::isfinite(_definition.MyInterval) || _definition.MyInterval < 0 ||
			!std::isfinite(_definition.MyShield.MyHealth) || _definition.MyShield.MyHealth < 0 ||
			_definition.MyShield.MyHits < 0 || _definition.MyShield.MyTypeMask > 15)
			throw std::invalid_argument("invalid buff definition");
		if (static_cast<unsigned>(_definition.MyBuiltin) > static_cast<unsigned>(BuiltinBuff::SIRACUSA_STEALTH)) throw std::invalid_argument("invalid builtin buff");
		if (_definition.MyStatus && *_definition.MyStatus >= CombatStatus::COUNT) throw std::invalid_argument("invalid buff status");
		if (_definition.MyStealthRestore && (!std::isfinite(*_definition.MyStealthRestore) || std::isless(*_definition.MyStealthRestore, 0))) throw std::invalid_argument("invalid stealth restore time");
		if (_definition.MyElementBurst && *_definition.MyElementBurst >= Element::COUNT) throw std::invalid_argument("invalid burst element");
		if (_definition.MyStrength)
		{
			const auto& strength = *_definition.MyStrength;
			if (!std::isfinite(strength.MyValue) || !std::isfinite(strength.MyScale) || !std::isfinite(strength.MyOffset) ||
				strength.MyAttribute >= Attribute::COUNT || (strength.MySecondAttribute && (*strength.MySecondAttribute >= Attribute::COUNT || *strength.MySecondAttribute == strength.MyAttribute)) ||
				(strength.MyTail && (!std::isfinite(strength.MyTail->MyValue) || std::isnan(strength.MyTail->MyUntil))))
				throw std::invalid_argument("invalid valued buff");
		}
		if (_definition.MyModifiers)
		{
			AttributeModifiers check;
			check.Add(*_definition.MyModifiers, _definition.MyStacks);
		}
		for (const auto* effects : {&_definition.MyTickEffects, &_definition.MyExpireEffects, &_definition.MyRemoveEffects})
			for (const auto& effect : *effects)
				if (!std::isfinite(effect.MyAmount) || effect.MyAmount < 0 ||
					static_cast<unsigned>(effect.MyKind) > static_cast<unsigned>(BuffEffectKind::HEALTH_LOSS) ||
					static_cast<unsigned>(effect.MyDamageType) > static_cast<unsigned>(DamageType::ELEMENTAL))
					throw std::invalid_argument("invalid buff effect");
	}

	std::uint64_t Battle::AddBuff(UnitId _target, BuffDefinition _definition)
	{
		auto& unit = _MyUnits[Index(_target)];
		if (_definition.MySource) (void)Index(_definition.MySource);
		ValidateBuff(_definition);
		ValidateContent(_definition.MyContent, ContentTag::CUSTOM_BUFF);
		if (Finished() || (!unit.MyAlive && !_definition.MyAllowDead)) return 0;
		auto& buffs = unit.MyBuffs;
		auto found = std::ranges::find(buffs, _definition.MyKey, [](const CombatBuff& _buff) -> const std::string& { return _buff.MyDefinition.MyKey; });
		if (found != buffs.end() && _definition.MyRefresh != BuffRefresh::INDEPENDENT)
		{
			auto& old = found->MyDefinition;
			if (_definition.MyRefresh == BuffRefresh::KEEP) return found->MyId;
			if (_definition.MyRefresh == BuffRefresh::EXTEND || _definition.MyRefresh == BuffRefresh::STACK)
			{
				if (_definition.MyRefresh == BuffRefresh::STACK)
				{
					old.MyMaxStacks = std::max(old.MyMaxStacks, _definition.MyMaxStacks);
					old.MyStacks = std::min(old.MyStacks + _definition.MyStacks, old.MyMaxStacks);
					found->MyRemaining = old.MyDuration = _definition.MyDuration;
				}
				else
				{
					found->MyRemaining = std::max(found->MyRemaining, _definition.MyDuration);
					old.MyDuration = std::max(old.MyDuration, _definition.MyDuration);
					old.MyShield.MyHealth = std::max(old.MyShield.MyHealth, _definition.MyShield.MyHealth);
					old.MyShield.MyHits = std::max(old.MyShield.MyHits, _definition.MyShield.MyHits);
				}
				if (_definition.MyModifiers) old.MyModifiers = std::move(_definition.MyModifiers);
				if (_definition.MyFlags) old.MyFlags = _definition.MyFlags;
				if (_definition.MyStealthRestore) old.MyStealthRestore = _definition.MyStealthRestore;
			}
			else
			{
				RetireContent(_target, found->MyId);
				found->MyDefinition = std::move(_definition);
				found->MyRemaining = found->MyDefinition.MyDuration;
				found->MyAccumulator = 0;
				found->MyId = ++_MyBuffSequence;
				AttachContent(found->MyDefinition.MyContent, _target, found->MyId, unit.MyOwner);
			}
			const auto id = found->MyId;
			Recalculate(unit);
			ContentEvent event{.MyKind = ContentEventKind::BUFF_APPLIED, .MyUnit = _target, .MyBuff = id};
			NotifyContent(event);
			return id;
		}
		if (_definition.MyRefresh == BuffRefresh::INDEPENDENT)
		{
			std::size_t same = 0;
			std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
			for (const auto& buff : buffs)
				if (buff.MyDefinition.MyKey == _definition.MyKey)
				{
					++same;
					oldest = std::min(oldest, buff.MyId);
				}
			if (same >= _definition.MyMaxStacks) (void)RemoveBuff(_target, oldest);
		}
		const auto id = ++_MyBuffSequence;
		const auto duration = _definition.MyDuration;
		AttachContent(_definition.MyContent, _target, id, unit.MyOwner);
		buffs.emplace_back(std::move(_definition), id, duration);
		Recalculate(unit);
		ContentEvent event{.MyKind = ContentEventKind::BUFF_APPLIED, .MyUnit = _target, .MyBuff = id};
		NotifyContent(event);
		return id;
	}

	std::size_t Battle::RemoveBuff(UnitId _target, std::string_view _key)
	{
		const auto& buffs = _MyUnits[Index(_target)].MyBuffs;
		// 回调可能移除其他实例；先记编号，避免保存会失效的迭代器或下标。
		std::vector<std::uint64_t> ids;
		ids.reserve(buffs.size());
		for (auto it = buffs.rbegin(); it != buffs.rend(); ++it)
			if (it->MyDefinition.MyKey == _key) ids.emplace_back(it->MyId);
		std::size_t removed = 0;
		for (const auto id : ids) removed += RemoveBuff(_target, id) ? 1U : 0U;
		return removed;
	}

	bool Battle::RemoveBuff(UnitId _target, std::uint64_t _buff)
	{
		auto& unit = _MyUnits[Index(_target)];
		if (!_MyStarted || Finished()) return false;
		auto& buffs = unit.MyBuffs;
		const auto found = std::ranges::find(buffs, _buff, &CombatBuff::MyId);
		if (found == buffs.end()) return false;
		auto removed = std::move(*found);
		buffs.erase(found);
		if (removed.MyDefinition.MyElementBurst) unit.MyElements.MyGauges.fill(0);
		Recalculate(unit);
		if (removed.MyDefinition.MyBuiltin == BuiltinBuff::SARGON_STACK) SyncSargon(unit.MyId);
		if (removed.MyDefinition.MyBuiltin == BuiltinBuff::SIRACUSA_STEALTH) unit.MySiracusaStealthEnd = Time();
		EndProfessionBuff(unit, removed.MyDefinition.MyBuiltin, false);
		RunBuffEffects(removed.MyDefinition.MySource, _target, removed.MyDefinition.MyRemoveEffects, 1);
		ContentEvent event{.MyKind = ContentEventKind::BUFF_REMOVED, .MyUnit = _target, .MyBuff = _buff};
		NotifyContent(event);
		RetireContent(_target, _buff);
		return true;
	}

	std::bitset<399> Battle::RangeMask(WorldPoint _origin, Facing _facing, std::span<const RangeOffset> _grid, int _extend, std::vector<int>* _order) const
	{
		std::bitset<399> mask;
		if (_order) { _order->clear(); _order->reserve(FieldTiles); }
		std::array<int, 201> rowOrder; std::size_t rowCount = 0;
		std::array<int, 201> maxima;
		maxima.fill(std::numeric_limits<int>::min());
		const auto add = [&](RangeOffset _offset)
		{
			const auto rotated = RotateOffset(_offset, _facing);
			const auto row = static_cast<int>(std::floor(_origin.MyY + 0.5)) + rotated.MyRow;
			const auto column = static_cast<int>(std::floor(_origin.MyX + 0.5)) + rotated.MyColumn;
			if (row >= 0 && row < 19 && column >= 0 && column < 21)
			{
				const auto key = static_cast<std::size_t>(row * 21 + column);
				if (_order && !mask.test(key)) _order->push_back(static_cast<int>(key));
				mask.set(key);
			}
		};
		for (const auto offset : _grid)
		{
			add(offset);
			auto& maximum = maxima[static_cast<std::size_t>(offset.MyRow + 100)];
			if (maximum == std::numeric_limits<int>::min()) rowOrder[rowCount++] = offset.MyRow;
			maximum = std::max(maximum, offset.MyColumn);
		}
		// 攻击距离只从每行最远格向前延长，不补齐原范围内部的空洞。
		for (std::size_t i = 0; i < rowCount; ++i)
			for (int extend = 1; extend <= std::min(21, _extend); ++extend)
				add(RangeOffset{.MyRow = rowOrder[i], .MyColumn = maxima[static_cast<std::size_t>(rowOrder[i] + 100)] + extend});
		return mask;
	}

	void Battle::RefreshRange(CombatUnit& _unit)
	{
		++_unit.MyRangeRevision;
		_unit.MyTraitFrontMask = _unit.MyDefinition.MyTraitFrontRange ? RangeMask(_unit.MyPosition, _unit.MyFacing, *_unit.MyDefinition.MyTraitFrontRange, 0) : std::bitset<FieldTiles>{};
		const auto& definition = _unit.MyDefinition.MySkill;
		const bool active = _unit.MySkill.MyActive;
		const auto& grid = active && !definition.MyRange.empty() ? definition.MyRange : _unit.MyDefinition.MyRange;
		const auto extend = static_cast<int>(std::clamp<std::int64_t>((active && definition.MyNoRangeExtend ? 0LL : _unit.MyStats.MyRangeExtend) + (active ? definition.MyRangeExtend : 0LL), 0, FieldColumns));
		_unit.MyRangeMask = RangeMask(_unit.MyPosition, _unit.MyFacing, grid, extend, &_unit.MyRangeKeys);
		_unit.MyBaseRangeMask = RangeMask(_unit.MyPosition, _unit.MyFacing, _unit.MyDefinition.MyRange, _unit.MyStats.MyPermanentRangeExtend);
		const auto origin = RulePosition(_unit);
		if (UsesInitialPosition(_unit))
		{
			_unit.MyInitialTraitFrontMask = _unit.MyDefinition.MyTraitFrontRange ? RangeMask(origin, _unit.MyFacing, *_unit.MyDefinition.MyTraitFrontRange, 0) : std::bitset<FieldTiles>{};
			_unit.MyInitialRuleRangeMask = RangeMask(origin, _unit.MyFacing, grid, extend, &_unit.MyInitialRuleRangeKeys);
			_unit.MyBaseTriggerMask = RangeMask(origin, _unit.MyFacing, _unit.MyDefinition.MyRange, _unit.MyStats.MyPermanentRangeExtend);
		}
		else _unit.MyBaseTriggerMask = _unit.MyBaseRangeMask;
		const auto triggerExtend = definition.MyTrigger == SkillTrigger::ACTIVE_RANGE && !definition.MyNoRangeExtend ? _unit.MyStats.MyPermanentRangeExtend : 0;
		_unit.MySkill.MyTriggerMask = RangeMask(origin, _unit.MyFacing, definition.MyTriggerRange, triggerExtend);
	}


	void Battle::Recalculate(CombatUnit& _unit)
	{
		AttributeModifiers modifiers;
		_unit.MyStatuses.MyBuffFlags.reset();
		for (const auto& buff : _unit.MyBuffs)
		{
			const auto& definition = buff.MyDefinition;
			if (definition.MyModifiers)
				modifiers.Add(*definition.MyModifiers, definition.MyStacks, definition.MyPersistent && std::isinf(definition.MyDuration));
			if (definition.MyFlags) _unit.MyStatuses.MyBuffFlags |= *definition.MyFlags;
		}
		auto& statuses = _unit.MyStatuses;
		const auto has = [&](CombatStatus _status) { return statuses.Has(_status); };
		const auto flag = [&](CombatStatus _status) { statuses.MyBuffFlags.set(static_cast<std::size_t>(_status)); };
		if (has(CombatStatus::FREEZE) || has(CombatStatus::SLEEP) || has(CombatStatus::LEVITATE)) flag(CombatStatus::STUN);
		if (has(CombatStatus::SLEEP)) flag(CombatStatus::NO_BLOCK);
		if (has(CombatStatus::SLEEP) || has(CombatStatus::LEVITATE) || has(CombatStatus::FEAR) || has(CombatStatus::ATTRACT)) flag(CombatStatus::UNBLOCKABLE);
		if (has(CombatStatus::BIND) || has(CombatStatus::GROUNDBIND) || has(CombatStatus::SELF_BOUND)) flag(CombatStatus::NO_MOVE);
		if (has(CombatStatus::LEVITATE)) flag(CombatStatus::NO_DISPLACE);
		const auto change = [&](Attribute _attribute, double _value)
		{
			const std::array changes{AttributeChange{.MyAttribute = _attribute, .MyValue = _value}};
			modifiers.Add(changes);
		};
		for (std::size_t i = 0; i < statuses.MyRemaining.size(); ++i)
		{
			if (!std::isgreater(statuses.MyRemaining[i], 0)) continue;
			const auto value = statuses.MyValues[i];
			switch (static_cast<CombatStatus>(i))
			{
			case CombatStatus::FREEZE:
				if (_unit.MySide == UnitSide::ENEMY) change(Attribute::RESISTANCE_FLAT, -15);
				break;
			case CombatStatus::COLD: change(Attribute::ATTACK_SPEED, -30); break;
			case CombatStatus::SLOW: change(Attribute::MOVE_MULTIPLIER, 1 - std::clamp(value, 0.0, 1.0)); break;
			case CombatStatus::SLUGGISH: change(Attribute::MOVE_MULTIPLIER, 0.2); break;
			case CombatStatus::BIND: case CombatStatus::GROUNDBIND: change(Attribute::MOVE_MULTIPLIER, 0); break;
			case CombatStatus::FRAGILE: change(Attribute::DAMAGE_TAKEN_MULTIPLIER, 1 + value); break;
			case CombatStatus::ARTS_FRAGILE: change(Attribute::ARTS_TAKEN_MULTIPLIER, 1 + value); break;
			case CombatStatus::PHYSICAL_FRAGILE: change(Attribute::PHYSICAL_TAKEN_MULTIPLIER, 1 + value); break;
			case CombatStatus::ELEMENTAL_FRAGILE: change(Attribute::ELEMENTAL_TAKEN_MULTIPLIER, 1 + value); break;
			case CombatStatus::TAUNT: change(Attribute::TAUNT, value); break;
			case CombatStatus::WEAKEN: change(Attribute::ATTACK_MULTIPLIER, 1 - std::clamp(value, 0.0, 1.0)); break;
			case CombatStatus::ATTACK_SPEED_DOWN: change(Attribute::ATTACK_SPEED, value); break;
			case CombatStatus::DEFENSE_DOWN: change(Attribute::DEFENSE_MULTIPLIER, 1 - std::clamp(value, 0.0, 1.0)); break;
			case CombatStatus::RESISTANCE_DOWN: change(Attribute::RESISTANCE_FLAT, -value); break;
			default: break;
			}
		}
		const auto previous = _unit.MyStats;
		_unit.MyStats = ResolveStats(_unit.MyDefinition.MyStats, modifiers);
		if (_unit.MyAlive && std::isgreater(previous.MyMaxHealth, 0) && std::isgreater(std::abs(previous.MyMaxHealth - _unit.MyStats.MyMaxHealth), 1e-9))
			_unit.MyHealth = std::clamp(_unit.MyHealth * (_unit.MyStats.MyMaxHealth / previous.MyMaxHealth), 0.0, _unit.MyStats.MyMaxHealth);
		if (_unit.MyStats.MyRangeExtend != previous.MyRangeExtend || _unit.MyStats.MyPermanentRangeExtend != previous.MyPermanentRangeExtend) RefreshRange(_unit);
		if (_unit.MySide == UnitSide::ALLY && (_unit.MyStatuses.Has(CombatStatus::STUN) || _unit.MyStatuses.Has(CombatStatus::NO_BLOCK)))
			ReleaseBlocked(_unit);
		if (_unit.MySide == UnitSide::ENEMY && _unit.MyStatuses.Has(CombatStatus::UNBLOCKABLE)) ReleaseBlock(_unit);
	}

	void Battle::RunBuffEffects(UnitId _source, UnitId _target, std::span<const BuffEffect> _effects, double _delta)
	{
		for (const auto& effect : _effects)
		{
			if (!Unit(_target).MyAlive || Finished()) break;
			const auto amount = effect.MyAmount * (effect.MyPerSecond ? _delta : 1);
			switch (effect.MyKind)
			{
			case BuffEffectKind::DAMAGE: (void)DealDamage(_source, _target, DamageInfo{.MyAmount = amount, .MyType = effect.MyDamageType,
				.MyCanDodge = effect.MyCanDodge, .MySourceless = effect.MySourceless, .MyNoSp = effect.MyNoSp, .MyTags = effect.MyTags}); break;
			case BuffEffectKind::HEAL: (void)Heal(_source, _target, amount); break;
			case BuffEffectKind::HEALTH_LOSS: (void)LoseHealth(_source, _target, amount); break;
			}
		}
	}

	void Battle::TickBuffs()
	{
		for (std::size_t i = 0; i < _MyUnits.size(); ++i)
		{
			auto& unit = _MyUnits[i];
			if (!unit.MyAlive) continue;
			// IDs survive removals caused by lethal periodic effects; newly added instances wait until the next tick.
			_MyBuffTickIds.clear();
			for (const auto& buff : unit.MyBuffs) _MyBuffTickIds.emplace_back(buff.MyId);
			for (const auto id : _MyBuffTickIds)
			{
				if (!unit.MyAlive) break;
				auto found = std::ranges::find(unit.MyBuffs, id, &CombatBuff::MyId);
				if (found == unit.MyBuffs.end()) continue;
				const auto source = found->MyDefinition.MySource;
				const auto interval = found->MyDefinition.MyInterval;
				unsigned ticks = interval > 0 ? 0U : 1U;
				if (interval > 0)
				{
					found->MyAccumulator += BattleClock::StepSeconds;
					while (found->MyAccumulator >= interval - 1e-9 && ticks < 8)
					{
						found->MyAccumulator -= interval;
						++ticks;
					}
				}
				if (found->MyDefinition.MyElementBurst)
					for (unsigned tick = 0; tick < ticks && unit.MyAlive && !Finished(); ++tick)
					{
						found = std::ranges::find(unit.MyBuffs, id, &CombatBuff::MyId);
						if (found == unit.MyBuffs.end()) break;
						TickElementBurst(unit, *found);
					}
				found = std::ranges::find(unit.MyBuffs, id, &CombatBuff::MyId);
				if (found == unit.MyBuffs.end()) continue;
				const auto customTicks = ticks;
				const bool custom = IsCustom(found->MyDefinition.MyContent.MyTag) || found->MyDefinition.MyNotifyTick;
				if (ticks && !found->MyDefinition.MyTickEffects.empty())
				{
					_MyBuffTickEffects.assign(found->MyDefinition.MyTickEffects.begin(), found->MyDefinition.MyTickEffects.end());
					while (ticks-- && unit.MyAlive) RunBuffEffects(source, unit.MyId, _MyBuffTickEffects, interval > 0 ? interval : BattleClock::StepSeconds);
				}
				if (custom)
					for (unsigned tick = 0; tick < customTicks && unit.MyAlive && !Finished(); ++tick)
					{
						if (std::ranges::find(unit.MyBuffs, id, &CombatBuff::MyId) == unit.MyBuffs.end()) break;
						ContentEvent event{.MyKind = ContentEventKind::BUFF_TICK, .MyUnit = unit.MyId, .MyBuff = id, .MyDelta = interval > 0 ? interval : BattleClock::StepSeconds};
						NotifyContent(event);
					}
				found = std::ranges::find(unit.MyBuffs, id, &CombatBuff::MyId);
				if (found == unit.MyBuffs.end()) continue;
				found->MyRemaining -= BattleClock::StepSeconds;
				if (found->MyRemaining > 1e-9) continue;
				auto expired = std::move(*found);
				unit.MyBuffs.erase(found);
				if (expired.MyDefinition.MyElementBurst) unit.MyElements.MyGauges.fill(0);
				Recalculate(unit);
				if (const auto& strength = expired.MyDefinition.MyStrength; strength && strength->MyTail &&
					strength->MyTail->MyUntil - Time() > 1e-6 && unit.MyAlive)
				{
					auto resumed = *strength;
					resumed.MyValue = strength->MyTail->MyValue; resumed.MyTail.reset();
					(void)ApplyStrongest(unit.MyId, expired.MyDefinition.MyKey, strength->MyTail->MyUntil - Time(), resumed, source);
				}
				if (expired.MyDefinition.MyBuiltin == BuiltinBuff::SARGON_STACK) SyncSargon(unit.MyId);
				if (expired.MyDefinition.MyBuiltin == BuiltinBuff::SIRACUSA_STEALTH) unit.MySiracusaStealthEnd = Time();
				EndProfessionBuff(unit, expired.MyDefinition.MyBuiltin, true);
				RunBuffEffects(source, unit.MyId, expired.MyDefinition.MyExpireEffects, 1);
				ContentEvent event{.MyKind = ContentEventKind::BUFF_EXPIRED, .MyUnit = unit.MyId, .MyBuff = id};
				NotifyContent(event);
				RetireContent(unit.MyId, id);
			}
			if (unit.MyAlive && !unit.MyDefinition.MySharedBoss && std::isgreater(unit.MyStats.MyHealthRegen, 0) && std::isless(unit.MyHealth, unit.MyStats.MyMaxHealth))
			{
				unit.MyRegenAccumulator += unit.MyStats.MyHealthRegen * BattleClock::StepSeconds;
				if (std::isgreaterequal(unit.MyRegenAccumulator, 1) || std::isgreaterequal(unit.MyHealth + unit.MyRegenAccumulator, unit.MyStats.MyMaxHealth))
				{
					(void)Heal(unit.MyId, unit.MyId, unit.MyRegenAccumulator, HealOptions{.MySelf = true, .MyRegen = true, .MySilent = true});
					unit.MyRegenAccumulator = 0;
				}
			}
		}
	}

	double Battle::LoseHealth(UnitId _source, UnitId _target, double _amount, bool _silent)
	{
		if (!std::isfinite(_amount) || std::isless(_amount, 0)) throw std::invalid_argument("invalid health loss");
		if (_source) (void)Index(_source);
		if (LeaderHitCancelled(_amount, _MyInput.MyBossBattle, Unit(_target).MySpawnTag == EnemySpawnTag::BOSS)) return 0;
		return ApplyHealthLoss(_source, _target, _amount, false, DamageInfo{.MyAmount = _amount, .MyType = DamageType::TRUE_DAMAGE, .MyNoSp = true, .MyTags = static_cast<DamageTags>(DamageTag::HP_LOSS), .MySilent = _silent});
	}

	double Battle::ApplyHealthLoss(UnitId _source, UnitId _target, double _amount, bool _recoverSp, const DamageInfo& _damage)
	{
		auto& target = _MyUnits[Index(_target)];
		if (_source) (void)Index(_source);
		if (!std::isfinite(_amount) || _amount < 0) throw std::invalid_argument("invalid health loss");
		if (!_MyStarted || Finished() || !target.MyAlive || target.MyHealth <= 0) return 0;
		double amount = 0;
		const bool pooled = target.MyDefinition.MySharedBoss && _MyInput.MySharedBoss;
		if (pooled)
		{
			std::string_view credit;
			const auto owner = _source && Unit(_source).MySide == UnitSide::ALLY ? Unit(_source).MyOwner : NoPlayer;
			if (owner != NoPlayer) credit = _MyPlayers[owner].MyPlayerId;
			amount = _MyInput.MyFinalAssault ? _MyInput.MyFinalAssault->get().Damage(credit, _amount) : _MyInput.MySharedBoss->get().Damage(credit, _amount);
			if (owner != NoPlayer) _MyPlayers[owner].MyBossDamage += amount;
			SyncBossHealth(target);
		}
		else
		{
			const auto previous = target.MyHealth;
			target.MyHealth = std::max(0.0, previous - _amount);
			if (!std::isgreater(target.MyHealth, 0))
			{
				ContentEvent fatal{.MyKind = ContentEventKind::FATAL, .MyUnit = _target, .MySource = _damage.MySourceless ? 0 : _source,
					.MyTarget = _target, .MyAmount = _amount, .MyDamage = _damage, .MyCredit = _source};
				NotifyContent(fatal);
				if (fatal.MyPrevented && target.MyAlive && std::isless(target.MyHealth, 1)) target.MyHealth = std::min(1.0, target.MyStats.MyMaxHealth);
			}
			amount = std::max(0.0, previous - target.MyHealth);
		}
		target.MyTotals.MyTaken += amount;
		if (_source && Unit(_source).MySide != target.MySide)
		{
			auto& source = _MyUnits[Index(_source)];
			source.MyTotals.MyDamage += amount;
			if (source.MySide == UnitSide::ALLY && source.MyOwner != NoPlayer) _MyPlayers[source.MyOwner].MyDamage += amount;
		}
		if (!_damage.MySilent) Emit(BattleEventKind::DAMAGED, _source, _target, amount);
		ContentEvent event{.MyKind = ContentEventKind::DAMAGED, .MySource = _damage.MySourceless ? 0 : _source,
			.MyTarget = _target, .MyAmount = _amount, .MyDamage = _damage, .MyCredit = _source};
		NotifyContent(event);
		if (_recoverSp && target.MySide == UnitSide::ALLY) SkillDamaged(target);
		const bool dead = pooled ? !std::isgreater(_MyInput.MySharedBoss->get().Health(), 0) : std::islessequal(target.MyHealth, 0);
		if (target.MyAlive && dead) Kill(target, _source);
		return amount;
	}
}
