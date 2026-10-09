#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	double Battle::AddDp(std::string_view _playerId, double _amount)
	{
		if (!std::isfinite(_amount) || Finished()) return 0;
		for (auto& player : _MyPlayers)
			if (player.MyPlayerId == _playerId)
			{
				player.MyDp = std::clamp(player.MyDp + _amount, 0.0, _MyInput.MyMaxDp);
				return player.MyDp;
			}
		return 0;
	}

	UnitId Battle::LowestHpAllyInRange(const CombatUnit& _unit) const
	{
		UnitId best = 0;
		double lowest = std::numeric_limits<double>::infinity();
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (!ally.MyAlive || ally.MyHidden || ally.MyKind == UnitKind::DEVICE || !InRange(_unit, ally) ||
				(id != _unit.MyId && (ally.MyStatuses.Has(CombatStatus::NO_HEAL) || ally.MyDefinition.MyAttack.MyNoHeal)) ||
				!std::isless(ally.MyHealth, ally.MyStats.MyMaxHealth - 1e-6)) continue;
			const auto ratio = ally.MyHealth / ally.MyStats.MyMaxHealth;
			if (std::isless(ratio, lowest) || (!std::islessgreater(ratio, lowest) && best && ally.MyDeploySequence < Unit(best).MyDeploySequence))
			{ best = id; lowest = ratio; }
		}
		return best;
	}

	// 原职业回复钩子优先级为 -10；普通 CUSTOM 通知完成后执行，保留同帧技能伤害的次数上限。
	void Battle::NotifyProfessionLate(ContentEvent& _event)
	{
		// 原版 fatal 的 -100 优先级：先让技能／天赋自定义不死处理，再决定是否进入替身。
		if (_event.MyKind == ContentEventKind::FATAL && _event.MyUnit && !_event.MyPrevented && !Finished())
		{
			const auto& unit = Unit(_event.MyUnit);
			if (unit.MyDefinition.MyProfession.MyKind == ProfessionTrait::DOLLKEEPER)
				_event.MyPrevented = unit.MyProfession.MyDollSwitching || EnterDoll(unit.MyId);
		}
		if (!_event.MySource || (_event.MyKind != ContentEventKind::DAMAGED && _event.MyKind != ContentEventKind::ATTACK)) return;
		auto& unit = _MyUnits[Index(_event.MySource)];
		if (!unit.MyAlive || Finished()) return;
		const auto& definition = unit.MyDefinition.MyProfession;
		const bool reaper = definition.MyKind == ProfessionTrait::REAPER;
		if (!reaper && definition.MyKind != ProfessionTrait::MUSHA) return;
		double count = 1;
		if (_event.MyKind == ContentEventKind::ATTACK)
		{
			if (!reaper) return;
			count = std::min(static_cast<double>(_event.MyTargetCount), static_cast<double>(std::max(1, unit.MyStats.MyBlockCount)));
		}
		else
		{
			const auto& damage = _event.MyDamage;
			if (!_event.MyTarget || Unit(_event.MyTarget).MySide != UnitSide::ENEMY || HasTag(damage.MyTags, DamageTag::HP_LOSS) ||
				HasTag(damage.MyTags, DamageTag::TALENT) || HasTag(damage.MyTags, DamageTag::DOT) || HasTag(damage.MyTags, DamageTag::PERIODIC)) return;
			if (reaper)
			{
				if (damage.MyIsAttack) return; // 普攻在一次攻击完成后按目标数统一治疗，多段不重复计算。
				auto& state = unit.MyProfession;
				if (state.MySelfHealTick != Tick()) { state.MySelfHealTick = Tick(); state.MySelfHealCount = 0; }
				if (state.MySelfHealCount >= static_cast<unsigned>(std::max(1, unit.MyStats.MyBlockCount))) return;
				++state.MySelfHealCount;
			}
			else if (_event.MyElement) return;
		}
		if (std::isgreater(count, 0)) (void)Heal(unit.MyId, unit.MyId, unit.MyReaperHeal.value_or(definition.MySelfHeal) * count,
			HealOptions{.MySelf = true, .MyIgnoreHealFree = true});
	}

	void Battle::Aftershock(UnitId _source, WorldPoint _point)
	{
		const auto& unit = Unit(_source);
		const auto radius = std::isgreater(unit.MyDefinition.MyAttack.MySplashRadius, 0) ? unit.MyDefinition.MyAttack.MySplashRadius : 1.0;
		// 余震按落点中心判定，攻击力在各次余震发生时读取；来源离场不取消已预约的余震。
		for (std::size_t i = 0, count = _MyEnemyIds.size(); i < count && !Finished(); ++i)
		{
			const auto& target = Unit(_MyEnemyIds[i]);
			if (!target.MyAlive || target.MyHidden || target.Flying() || target.MyStatuses.Has(CombatStatus::UNTARGETABLE) || EnemyStealthed(target) ||
				std::isgreater(Distance(target.MyPosition, _point), radius + 1e-9)) continue;
			(void)DealDamage(_source, target.MyId, DamageInfo{.MyAmount = unit.MyStats.MyAttack * unit.MyDefinition.MyProfession.MyShockScale,
				.MyTags = static_cast<DamageTags>(DamageTag::AFTERSHOCK)});
		}
	}

	void Battle::ApplyLibrator(CombatUnit& _unit, bool _ramp)
	{
		if (_ramp)
		{
			if (std::isgreater(_unit.MyProfession.MyRamp, 0))
				(void)AddBuff(_unit.MyId, BuffDefinition{.MyKey = "trait:libratorRamp",
					.MyModifiers = std::vector{AttributeChange{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _unit.MyProfession.MyRamp}}});
			else (void)RemoveBuff(_unit.MyId, "trait:libratorRamp");
		}
		if (_unit.MySkill.MyActive) (void)RemoveBuff(_unit.MyId, "trait:libratorBlock");
		else
		{
			(void)AddBuff(_unit.MyId, BuffDefinition{.MyKey = "trait:libratorBlock",
				.MyModifiers = std::vector{AttributeChange{.MyAttribute = Attribute::BLOCK_COUNT, .MyValue = -99}}});
			ReleaseBlocked(_unit);
		}
	}

	void Battle::ProfessionPeriodic(CombatUnit& _unit)
	{
		if (!_unit.MyAlive) return;
		const auto& definition = _unit.MyDefinition.MyProfession;
		switch (definition.MyKind)
		{
		case ProfessionTrait::GEEK:
		{
			const auto loss = _unit.MyStats.MyMaxHealth * definition.MyHpDrain;
			// 非致命流失不足一血时直接截断，不产生流失事件，也不触发致命伤害处理器。
			if (std::isless(_unit.MyHealth - loss, 1)) _unit.MyHealth = std::min(_unit.MyHealth, 1.0);
			else (void)LoseHealth(_unit.MyId, _unit.MyId, loss, true);
			break;
		}
		case ProfessionTrait::MERCHANT:
		{
			if (_unit.MyOwner == NoPlayer) break;
			if (const auto* kit = _unit.MyDefinition.MyOperatorKit ? std::get_if<Swire2Kit>(_unit.MyDefinition.MyOperatorKit) : nullptr)
			{
				Swire2Pay(_unit.MyId, *kit);
				break;
			}
			ContentEvent payment{.MyKind = ContentEventKind::MERCHANT_PAY, .MyUnit = _unit.MyId, .MyAmount = definition.MyMerchantCost};
			NotifyContent(payment);
			if (!_unit.MyAlive || payment.MyCancel || Finished()) break;
			const auto cost = std::isfinite(payment.MyAmount) ? std::max(0.0, payment.MyAmount) : definition.MyMerchantCost;
			auto& player = _MyPlayers[_unit.MyOwner];
			if (std::isgreaterequal(player.MyDp, cost)) (void)AddDp(player.MyPlayerId, -cost);
			else Retreat(_unit.MyId, false, RemovalReason::MERCHANT);
			break;
		}
		case ProfessionTrait::LIBRATOR:
			if (!_unit.MySkill.MyActive)
			{
				_unit.MyProfession.MyRamp = std::min(definition.MyRampMax, _unit.MyProfession.MyRamp + definition.MyRampMax / definition.MyRampTime);
				(void)AddBuff(_unit.MyId, BuffDefinition{.MyKey = "trait:libratorRamp",
					.MyModifiers = std::vector{AttributeChange{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _unit.MyProfession.MyRamp}}});
			}
			break;
		case ProfessionTrait::BARD:
		{
			if (_unit.MyHidden || _unit.MyStatuses.Has(CombatStatus::STUN)) break;
			const auto value = _unit.MyStats.MyAttack * _unit.MyBardRatio.value_or(definition.MyAuraRatio);
			if (!std::isgreater(value, 0)) break;
			// 回调可召唤新单位；这里固定本次选择的 ID 快照，不持有可失效的 vector 迭代器。
			auto& scratch = AcquireAttackScratch();
			struct Guard { std::size_t& MyDepth; ~Guard() { --MyDepth; } } guard{.MyDepth = _MyAttackDepth};
			for (const auto id : _MyAllyIds)
				if (const auto& ally = Unit(id); ally.MyAlive && !ally.MyHidden && ally.MyKind != UnitKind::DEVICE && InRange(_unit, ally) &&
					(id == _unit.MyId || !ally.MyStatuses.Has(CombatStatus::ISOLATED))) scratch.MyTargets.emplace_back(id);
			for (const auto id : scratch.MyTargets)
			{
				ContentEvent regen{.MyKind = ContentEventKind::BARD_REGEN, .MyUnit = _unit.MyId, .MySource = _unit.MyId, .MyTarget = id, .MyAmount = value};
				NotifyContent(regen);
				if (Finished()) break;
				if (std::isfinite(regen.MyAmount) && std::isgreater(regen.MyAmount, 0))
					(void)AddBuff(id, BuffDefinition{.MyKey = _unit.MyProfession.MyAuraKey, .MySource = _unit.MyId, .MyDuration = 0.5,
						.MyModifiers = std::vector{AttributeChange{.MyAttribute = Attribute::HEALTH_REGEN, .MyValue = regen.MyAmount}}});
			}
			break;
		}
		default: break;
		}
	}
}
