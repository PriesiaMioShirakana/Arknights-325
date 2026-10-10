#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct MistScratchGuard
		{
			std::size_t& MyDepth;

			~MistScratchGuard() { --MyDepth; }
		};

		bool HasElementLoad(const CombatUnit& _unit)
		{
			return std::ranges::any_of(_unit.MyElements.MyGauges, [](double _value) { return std::isgreater(_value, 0); });
		}
	}

	void BattleCore::AbsorbElement(ContentEvent& _event, double& _pool, bool _multiplier)
	{
		const auto effective = _event.MyElementHit.MyAmount * _event.MyElementHit.MyMultiplier;
		if (!std::isgreater(effective, 0) || !std::isgreater(_pool, 0)) return;
		const auto absorbed = std::min(_pool, effective); _pool -= absorbed;
		if (std::isgreaterequal(absorbed, effective - 1e-9)) _event.MyCancel = true;
		else (_multiplier ? _event.MyElementHit.MyMultiplier : _event.MyElementHit.MyAmount) *= (effective - absorbed) / effective;
	}

	void BattleCore::Agoat2BeforeAttack(CombatUnit& _unit, const Agoat2Kit& _kit, std::vector<UnitId>& _targets)
	{
		if (!_unit.MySkill.MyActive || _targets.empty() || _targets.size() >= _kit.MyShots) return;
		const auto count = _targets.size(); _targets.reserve(_kit.MyShots);
		for (std::size_t i = count; i < _kit.MyShots; ++i) _targets.push_back(_targets[i % count]);
	}

	void BattleCore::Agoat2Pulse(UnitId _unit, bool _ash)
	{
		const auto& unit = Unit(_unit); const auto& kit = std::get<Agoat2Kit>(*unit.MyDefinition.MyOperatorKit);
		if (!unit.MyAlive || unit.MyHidden || unit.MyOperatorHooksReleased) return;
		auto& scratch = AcquireAttackScratch(); const MistScratchGuard guard{.MyDepth = _MyAttackDepth};
		OperatorAlliesInRange(_unit, scratch.MyTargets);
		if (!_ash)
		{
			if (std::ranges::any_of(scratch.MyTargets, [&](UnitId _id) { return HasElementLoad(Unit(_id)); }))
				(void)AddBuff(_unit, {.MyKey = "agoat2:linger", .MyDuration = 0.4, .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = kit.MyModuleSpeed}}});
			return;
		}
		const auto scale = unit.MySkill.MyActive ? kit.MyTalentScale : 1;
		for (const auto id : scratch.MyTargets) (void)AddBuff(id, {.MyKey = "agoat2:ash", .MySource = _unit, .MyDuration = 0.75,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALTH_PERCENT, .MyValue = kit.MyHealth * scale}},
			.MyStrength = BuffStrength{.MyValue = std::min(1.0, kit.MyElementCut * scale)}});
	}

	void BattleCore::Agoat2Ash(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::ELEMENT_HIT || !_event.MyTarget) return;
		const auto& target = Unit(_event.MyTarget);
		const auto ash = std::ranges::find(target.MyBuffs, std::string_view("agoat2:ash"), [](const auto& _buff) -> std::string_view { return _buff.MyDefinition.MyKey; });
		if (ash == target.MyBuffs.end() || !ash->MyDefinition.MySource || !ash->MyDefinition.MyStrength || Unit(ash->MyDefinition.MySource).MyOperatorHooksReleased) return;
		const auto cut = ash->MyDefinition.MyStrength->MyValue;
		if (std::isgreater(cut, 0)) _event.MyElementHit.MyMultiplier *= std::max(0.0, 1 - cut);
	}

	void BattleCore::Agoat2Observe(ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::BUFF_TICK && _event.MyUnit && _event.MyBuff)
		{
			const auto& target = Unit(_event.MyUnit); const auto buff = std::ranges::find(target.MyBuffs, _event.MyBuff, &CombatBuff::MyId);
			if (buff == target.MyBuffs.end() || !buff->MyDefinition.MyKey.starts_with("agoat2:mist:") || !buff->MyDefinition.MySource) return;
			const auto& source = Unit(buff->MyDefinition.MySource);
			if (source.MyOperatorHooksReleased) return;
			const auto& kit = std::get<Agoat2Kit>(*source.MyDefinition.MyOperatorKit);
			const auto attack = buff->MyDefinition.MyStrength ? buff->MyDefinition.MyStrength->MyValue : source.MyStats.MyAttack;
			const auto amount = attack * kit.MyMistScale * std::max(1U, buff->MyDefinition.MyStacks);
			(void)ReduceElement(target.MyId, amount * source.MyDefinition.MyAttack.MyElementHealRatio);
			if (std::isless(target.MyHealth, target.MyStats.MyMaxHealth)) (void)Heal(source.MyId, target.MyId, amount, {.MyHot = true});
			return;
		}
		if (_event.MyKind != ContentEventKind::BEFORE_HEAL || !_event.MySource || !_event.MyTarget || _event.MyHealOptions.MyHot || _event.MyHealOptions.MyAura || _event.MyHealOptions.MyRegen || _event.MyHealOptions.MySkillHeal) return;
		const auto& source = Unit(_event.MySource); const auto* kit = source.MyDefinition.MyOperatorKit ? std::get_if<Agoat2Kit>(source.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || source.MyOperatorHooksReleased || !std::isgreater(kit->MyMistScale, 0)) return;
		const auto id = AddBuff(_event.MyTarget, {.MyKey = source.MyAgoatMistKey, .MySource = source.MyId, .MyDuration = kit->MyMistDuration,
			.MyMaxStacks = kit->MyMaxStacks, .MyRefresh = BuffRefresh::STACK, .MyInterval = 1, .MyNotifyTick = true});
		auto& buffs = _MyUnits[Index(_event.MyTarget)].MyBuffs; const auto buff = std::ranges::find(buffs, id, &CombatBuff::MyId);
		if (buff != buffs.end()) buff->MyDefinition.MyStrength = BuffStrength{.MyValue = source.MyStats.MyAttack};
	}

	void BattleCore::Agoat2Skill(UnitId _unit, const Agoat2Kit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_kit.MySkill == Agoat2SkillKind::DRIZZLE)
		{
			if (_event.MyKind == ContentEventKind::SKILL_START) unit.MyAgoatAccumulator = 0;
			if (_event.MyKind != ContentEventKind::SKILL_TICK) return;
			unit.MyAgoatAccumulator += _event.MyDelta;
			if (std::isless(unit.MyAgoatAccumulator + 1e-9, 1)) return;
			unit.MyAgoatAccumulator -= 1;
			if (!std::isgreater(_kit.MyRecovery, 0)) return;
			auto& scratch = AcquireAttackScratch(); const MistScratchGuard guard{.MyDepth = _MyAttackDepth};
			OperatorAlliesInRange(_unit, scratch.MyTargets);
			const auto amount = unit.MyStats.MyAttack * _kit.MyRecovery;
			for (const auto id : scratch.MyTargets) (void)ReduceElement(id, amount);
			return;
		}
		if (_kit.MySkill != Agoat2SkillKind::VEIL || _event.MyKind != ContentEventKind::SKILL_START) return;
		auto& scratch = AcquireAttackScratch(); const MistScratchGuard guard{.MyDepth = _MyAttackDepth};
		OperatorAlliesInRange(_unit, scratch.MyTargets);
		for (const auto id : scratch.MyTargets)
		{
			if (std::isgreater(unit.MyDefinition.MyAttack.MyElementHealRatio, 0)) (void)ReduceElement(id, unit.MyStats.MyAttack * unit.MyDefinition.MyAttack.MyElementHealRatio);
			if (std::isless(Unit(id).MyHealth, Unit(id).MyStats.MyMaxHealth)) (void)Heal(_unit, id, unit.MyStats.MyAttack, {.MySkillHeal = true});
		}
		_MyElementVeils.push_back({.MyKeys = RuleRange(unit), .MyPool = unit.MyStats.MyAttack * _kit.MyVeilScale, .MyUntil = Time() + _kit.MyVeilDuration, .MySource = _unit});
	}

	void BattleCore::Agoat2Veil(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::ELEMENT_HIT || !_event.MyTarget || _event.MyCancel || Unit(_event.MyTarget).MySide != UnitSide::ALLY) return;
		std::erase_if(_MyElementVeils, [&](const auto& _veil) { return !std::isgreater(_veil.MyPool, 1e-9) || !std::isless(Time(), _veil.MyUntil); });
		const auto point = RulePosition(Unit(_event.MyTarget)); const auto row = static_cast<int>(std::floor(point.MyY + 0.5)), column = static_cast<int>(std::floor(point.MyX + 0.5));
		if (!FieldGrid::InBounds(row, column)) return;
		for (auto& veil : _MyElementVeils)
		{
			if (!veil.MyKeys.test(static_cast<std::size_t>(FieldGrid::Key(row, column)))) continue;
			AbsorbElement(_event, veil.MyPool, true);
			if (_event.MyCancel) return;
		}
	}
}
