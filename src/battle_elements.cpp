#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		struct BurstRule
		{
			std::string_view MyKey{};
			double MyAllyDuration{};
			double MyEnemyDuration{};
			double MyAllyDamage{};
			double MyEnemyDamage{};
			DamageType MyAllyType{};
		};

		// 与原 constants.js 的阵营差异一致；necrosis 仅为原项目旧内容兼容，不能冒充凋亡。
		constexpr std::array BurstRules{
			BurstRule{.MyKey = "neuralBurst", .MyAllyDuration = 10, .MyEnemyDuration = 10, .MyAllyDamage = 1000, .MyEnemyDamage = 6000, .MyAllyType = DamageType::TRUE_DAMAGE},
			BurstRule{.MyKey = "erosionBurst", .MyAllyDuration = 10, .MyEnemyDuration = 8, .MyAllyDamage = 800, .MyEnemyDamage = 5000, .MyAllyType = DamageType::PHYSICAL},
			BurstRule{.MyKey = "burnBurst", .MyAllyDuration = 10, .MyEnemyDuration = 10, .MyAllyDamage = 1200, .MyEnemyDamage = 7000, .MyAllyType = DamageType::ARTS},
			BurstRule{.MyKey = "apoptosisBurst", .MyAllyDuration = 15, .MyEnemyDuration = 15},
			BurstRule{.MyKey = "necrosisBurst", .MyAllyDuration = 12, .MyEnemyDuration = 12}};

		bool Locked(const CombatUnit& _unit) noexcept
		{ return _unit.MyElements.MyBurstPending || _unit.MyStatuses.Has(CombatStatus::BURST_LOCK); }

		double GaugeMaximum(const CombatUnit& _unit) noexcept
		{ return _unit.MySide == UnitSide::ENEMY && (_unit.MyDefinition.MyLeader || _unit.MySpawnTag == EnemySpawnTag::BOSS) ? 2000 : 1000; }

		std::size_t ElementIndex(Element _element)
		{
			const auto index = static_cast<std::size_t>(_element);
			if (index >= BurstRules.size()) throw std::invalid_argument("invalid element");
			return index;
		}
	}

	double Battle::DealElement(UnitId _source, UnitId _target, ElementHit _hit)
	{
		(void)ElementIndex(_hit.MyElement);
		auto& target = _MyUnits[Index(_target)];
		if (_source) (void)Index(_source);
		if (!std::isfinite(_hit.MyAmount) || std::isless(_hit.MyAmount, 0) || !std::isfinite(_hit.MyMultiplier) || std::isless(_hit.MyMultiplier, 0))
			throw std::invalid_argument("invalid element hit");
		if (!_MyStarted || Finished() || !target.MyAlive || !std::isgreater(target.MyHealth, 0) || target.MyHidden || Locked(target) ||
			target.MyStatuses.Has(CombatStatus::INVULNERABLE) || (target.MyStatuses.Has(CombatStatus::SLEEP) && !_hit.MyHitSleep) ||
			(!_hit.MySourceless && !_hit.MyIgnoreSelect && _source && target.MySide == UnitSide::ALLY &&
				target.MyStatuses.Has(CombatStatus::LIFTOFF) && !Unit(_source).Flying())) return 0;
		ContentEvent before{.MyKind = ContentEventKind::ELEMENT_HIT, .MySource = _source, .MyTarget = _target, .MyElementHit = _hit};
		NotifyContent(before);
		_hit = before.MyElementHit;
		if (before.MyCancel || Finished() || !target.MyAlive || Locked(target) || _hit.MyElement >= Element::COUNT ||
			!std::isfinite(_hit.MyAmount) || !std::isgreater(_hit.MyAmount, 0) || !std::isfinite(_hit.MyMultiplier) || !std::isgreater(_hit.MyMultiplier, 0)) return 0;
		const auto amount = _hit.MyAmount * _hit.MyMultiplier * target.MyStats.MyElementTakenMultiplier *
			std::max(0.05, 1 - std::clamp(target.MyStats.MyElementResistance / 100, 0.0, 1.0));
		if (!std::isfinite(amount) || !std::isgreater(amount, 0)) return 0;
		auto& gauge = target.MyElements.MyGauges[ElementIndex(_hit.MyElement)];
		gauge = std::min(GaugeMaximum(target), gauge + amount);
		if (_source) _MyUnits[Index(_source)].MyTotals.MyElementDamage += amount;
		ContentEvent event{.MyKind = ContentEventKind::DAMAGED, .MySource = _source, .MyTarget = _target,
			.MyAmount = amount, .MyElement = _hit.MyElement};
		NotifyContent(event);
		// 元素损伤不触发受击 SP；爆发产生的 HP 伤害才会通过普通伤害管线触发。
		if (target.MyAlive && std::isgreaterequal(gauge, GaugeMaximum(target))) BurstElement(_source, _target, _hit.MyElement);
		return amount;
	}

	void Battle::BurstElement(UnitId _source, UnitId _target, Element _element)
	{
		const auto index = ElementIndex(_element);
		auto& target = _MyUnits[Index(_target)];
		if (_source) (void)Index(_source);
		if (!_MyStarted || Finished() || !target.MyAlive || Locked(target)) return;
		struct PendingGuard
		{
			bool& MyPending;
			explicit PendingGuard(bool& _pending) : MyPending(_pending) { MyPending = true; }
			~PendingGuard() { MyPending = false; }
		} guard(target.MyElements.MyBurstPending);
		target.MyElements.MyGauges[index] = GaugeMaximum(target);
		ContentEvent event{.MyKind = ContentEventKind::ELEMENT_BURST, .MySource = _source, .MyTarget = _target, .MyElement = _element};
		NotifyContent(event);
		if (!target.MyAlive || Finished()) { target.MyElements.MyGauges.fill(0); return; }
		const auto& rule = BurstRules[index];
		const bool enemy = target.MySide == UnitSide::ENEMY;
		StatusFlags flags;
		flags.set(static_cast<std::size_t>(CombatStatus::BURST_LOCK));
		BuffDefinition lock{.MyKey = std::string(rule.MyKey), .MySource = _source,
			.MyDuration = enemy ? rule.MyEnemyDuration : rule.MyAllyDuration,
			.MyFlags = flags, .MyElementBurst = _element};
		if (_element == Element::BURN)
			lock.MyModifiers = std::vector<AttributeChange>{AttributeChange{.MyAttribute = Attribute::RESISTANCE_FLAT, .MyValue = -20}};
		else if (_element == Element::NECROSIS || _element == Element::APOPTOSIS)
		{
			lock.MyInterval = 1;
			if (_element == Element::NECROSIS || enemy)
				lock.MyModifiers = std::vector<AttributeChange>{AttributeChange{.MyAttribute = Attribute::ATTACK_MULTIPLIER, .MyValue = _element == Element::NECROSIS ? 0.8 : 0.5}};
			else
			{
				lock.MyFlags->set(static_cast<std::size_t>(CombatStatus::NO_SP));
				lock.MyFlags->set(static_cast<std::size_t>(CombatStatus::SILENCE));
			}
		}
		(void)AddBuff(_target, std::move(lock));
		if (_element == Element::NEURAL)
		{
			if (enemy) (void)ApplyStatus(_target, CombatStatus::PALSY, StatusApplication{.MySource = _source, .MyValue = 3});
			else (void)ApplyStatus(_target, CombatStatus::STUN, StatusApplication{.MyDuration = 10, .MySource = _source, .MyForce = true});
		}
		else if (_element == Element::EROSION)
			(void)AddBuff(_target, BuffDefinition{.MyKey = "erosionDown", .MySource = _source, .MyMaxStacks = 1000000,
				.MyRefresh = BuffRefresh::STACK,
				.MyModifiers = std::vector<AttributeChange>{AttributeChange{.MyAttribute = Attribute::DEFENSE_FLAT, .MyValue = enemy ? -120.0 : -100.0}}});
		const auto amount = enemy ? rule.MyEnemyDamage : rule.MyAllyDamage;
		if (std::isgreater(amount, 0))
			(void)DealDamage(_source, _target, DamageInfo{.MyAmount = amount, .MyType = enemy ? DamageType::ELEMENTAL : rule.MyAllyType,
				.MyCanDodge = false, .MySourceless = true});
	}

	void Battle::TickElementBurst(CombatUnit& _unit, CombatBuff& _buff)
	{
		const auto element = *_buff.MyDefinition.MyElementBurst;
		if (element != Element::NECROSIS && element != Element::APOPTOSIS) return;
		const bool enemy = _unit.MySide == UnitSide::ENEMY;
		const auto source = _buff.MyDefinition.MySource;
		if (element == Element::APOPTOSIS)
		{
			if (enemy)
			{
				if (!_buff.MyDefinition.MyModifiers) _buff.MyDefinition.MyModifiers.emplace();
				auto& modifiers = *_buff.MyDefinition.MyModifiers;
				const auto value = 1 - 0.5 * std::max(0.0, _buff.MyRemaining) / 15;
				const auto found = std::ranges::find(modifiers, Attribute::ATTACK_MULTIPLIER, &AttributeChange::MyAttribute);
				if (found != modifiers.end()) found->MyValue = value;
				else modifiers.emplace_back(Attribute::ATTACK_MULTIPLIER, value);
				Recalculate(_unit);
			}
			else SetSpTotal(_unit.MyId, std::max(0.0, SpTotal(_unit.MyId) - 1));
		}
		// DealDamage 的内容回调可能删掉当前 Buff；从此处起不再读取 _buff。
		const auto type = element == Element::NECROSIS ? DamageType::TRUE_DAMAGE : enemy ? DamageType::ELEMENTAL : DamageType::ARTS;
		(void)DealDamage(source, _unit.MyId, DamageInfo{.MyAmount = element == Element::APOPTOSIS && enemy ? 800.0 : 100.0,
			.MyType = type, .MyCanDodge = false, .MySourceless = true});
	}

	double Battle::ReduceElement(UnitId _target, double _amount, std::optional<Element> _element)
	{
		auto& target = _MyUnits[Index(_target)];
		if (_element) (void)ElementIndex(*_element);
		if (!_MyStarted || Finished() || Locked(target) || !std::isfinite(_amount) || !std::isgreater(_amount, 0)) return 0;
		double removed = 0;
		for (std::size_t i = 0; i < target.MyElements.MyGauges.size(); ++i)
		{
			if (_element && static_cast<std::size_t>(*_element) != i) continue;
			auto& gauge = target.MyElements.MyGauges[i];
			const auto take = std::min(gauge, _amount);
			gauge -= take;
			removed += take;
		}
		return removed;
	}

	std::optional<ElementDisplay> Battle::ElementView(UnitId _unit) const
	{
		const auto& unit = Unit(_unit);
		if (unit.MyStatuses.Has(CombatStatus::BURST_LOCK))
		{
			const CombatBuff* longest = nullptr;
			for (const auto& buff : unit.MyBuffs)
				if (buff.MyDefinition.MyElementBurst && (!longest || std::isgreater(buff.MyRemaining, longest->MyRemaining))) longest = &buff;
			if (longest)
				return ElementDisplay{.MyElement = *longest->MyDefinition.MyElementBurst, .MyFill = 1,
					.MyCooldownEnd = std::floor((Time() + std::max(0.0, longest->MyRemaining)) * 100 + 0.5) / 100,
					.MyCooldown = std::floor(longest->MyDefinition.MyDuration * 100 + 0.5) / 100};
		}
		std::optional<Element> best;
		double maximum = 0;
		for (std::size_t i = 0; i < unit.MyElements.MyGauges.size(); ++i)
			if (std::isgreater(unit.MyElements.MyGauges[i], maximum))
			{
				maximum = unit.MyElements.MyGauges[i];
				best = static_cast<Element>(i);
			}
		if (!best) return std::nullopt;
		return ElementDisplay{.MyElement = *best, .MyFill = std::clamp(std::floor(maximum / GaugeMaximum(unit) * 100 + 1e-9) / 100, 0.01, 0.99)};
	}
}
