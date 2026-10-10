#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct TargetMarkGuard
		{
			std::size_t& MyDepth;

			~TargetMarkGuard() { --MyDepth; }
		};
	}

	void BattleCore::MizukiPresence(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<MizukiKit>(*unit.MyDefinition.MyOperatorKit);
		const bool active = std::ranges::any_of(_MyEnemyIds, [&](UnitId _id)
		{
			const auto& enemy = Unit(_id);
			return enemy.MyAlive && !enemy.MyHidden && InRuleRange(_unit, _id) && std::isless(enemy.MyHealth / enemy.MyStats.MyMaxHealth, kit.MyHealthThreshold);
		});
		const bool present = std::ranges::any_of(unit.MyBuffs, [](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == "mizuki:t2"; });
		if (active && !present) (void)AddBuff(_unit, {.MyKey = "mizuki:t2", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyAttack}}});
		else if (!active && present) (void)RemoveBuff(_unit, "mizuki:t2");
	}

	void BattleCore::MizukiAttack(UnitId _unit, const MizukiKit& _kit, const ContentEvent& _event)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive) return;
		const bool active = unit.MySkill.MyActive && _kit.MySkill != MizukiSkillKind::AWAKEN;
		const auto count = static_cast<std::size_t>(_kit.MyTargets) + (active ? _kit.MyExtraTargets : 0U);
		const auto scale = _kit.MyTalentScale * (_kit.MySkill == MizukiSkillKind::AWAKEN && _event.MySkillAttack ? _kit.MyAwakenScale : 1);
		auto& scratch = AcquireAttackScratch(); const TargetMarkGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _event.MyTargets) if (Unit(id).MyAlive && Unit(id).MySide == UnitSide::ENEMY) scratch.MyTargets.push_back(id);
		std::ranges::sort(scratch.MyTargets, [&](UnitId _a, UnitId _b)
		{
			const auto a = Unit(_a).MyHealth, b = Unit(_b).MyHealth;
			return std::islessgreater(a, b) ? std::isless(a, b) : _a < _b;
		});
		if (scratch.MyTargets.size() > count) scratch.MyTargets.resize(count);
		for (const auto id : scratch.MyTargets)
		{
			(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * scale, .MyType = DamageType::ARTS, .MyTags = static_cast<DamageTags>(DamageTag::TALENT)});
			if (active && Unit(id).MyAlive) (void)ApplyStatus(id, _kit.MySkill == MizukiSkillKind::BIND ? CombatStatus::BIND : CombatStatus::STUN, _kit.MyStatusDuration, _unit);
		}
	}

	void BattleCore::AromaHit(ContentEvent& _event, const AromaKit& _kit)
	{
		if (!_event.MySource || !_event.MyTarget || !_event.MyDamage.MyIsAttack) return;
		auto& unit = _MyUnits[Index(_event.MySource)];
		const auto& target = Unit(_event.MyTarget);
		if (target.MySide == UnitSide::ENEMY && !std::ranges::any_of(target.MyBuffs, [](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == "aroma:bubbled"; }))
		{
			(void)AddBuff(target.MyId, {.MyKey = "aroma:bubbled", .MySource = unit.MyId});
			if (!std::ranges::contains(unit.MyAromaMarked, target.MyId)) unit.MyAromaMarked.push_back(target.MyId);
			_event.MyDamage.MyMultiplier *= _kit.MyFirstScale;
			(void)ApplyStatus(target.MyId, CombatStatus::LEVITATE, _kit.MyLevitate, unit.MyId);
		}
		if (std::isgreater(_kit.MyDistanceScale, 0))
		{
			const auto distance = Distance(RulePosition(unit), RulePosition(target));
			_event.MyDamage.MyMultiplier *= 1 + _kit.MyDistanceScale * std::clamp((distance - _kit.MyMinDistance) / std::max(1e-6, _kit.MyMaxDistance - _kit.MyMinDistance), 0.0, 1.0);
		}
	}

	void BattleCore::AromaObserve(const ContentEvent& _event)
	{
		if (_event.MyKind == ContentEventKind::STATUS_APPLIED && _event.MyStatus == CombatStatus::LEVITATE && _event.MyTarget && Unit(_event.MyTarget).MySide == UnitSide::ENEMY)
			for (const auto id : _MyAromas)
			{
				auto& unit = _MyUnits[Index(id)];
				if (!unit.MyOperatorHooksReleased && !std::ranges::contains(unit.MyAromaFloating, _event.MyTarget)) unit.MyAromaFloating.push_back(_event.MyTarget);
			}
		if (_event.MyKind != ContentEventKind::DEATH || !_event.MyUnit) return;
		auto& unit = _MyUnits[Index(_event.MyUnit)];
		if (unit.MyOperatorHooksReleased || !unit.MyDefinition.MyOperatorKit || !std::holds_alternative<AromaKit>(*unit.MyDefinition.MyOperatorKit)) return;
		auto& scratch = AcquireAttackScratch(); const TargetMarkGuard guard{.MyDepth = _MyAttackDepth};
		scratch.MyTargets.assign(unit.MyAromaMarked.begin(), unit.MyAromaMarked.end());
		for (const auto id : scratch.MyTargets)
		{
			const auto& buffs = Unit(id).MyBuffs;
			const auto found = std::ranges::find(buffs, "aroma:bubbled", [](const CombatBuff& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
			if (found != buffs.end() && found->MyDefinition.MySource == unit.MyId) (void)RemoveBuff(id, found->MyId);
		}
		unit.MyAromaMarked.clear();
	}

	void BattleCore::AromaLanding(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (unit.MyOperatorHooksReleased || unit.MyAromaFloating.empty()) return;
		const auto& kit = std::get<AromaKit>(*unit.MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch(); const TargetMarkGuard guard{.MyDepth = _MyAttackDepth};
		scratch.MyTargets.assign(unit.MyAromaFloating.begin(), unit.MyAromaFloating.end());
		for (const auto id : scratch.MyTargets)
		{
			const auto& enemy = Unit(id);
			if (enemy.MyAlive && enemy.MyStatuses.Has(CombatStatus::LEVITATE)) continue;
			std::erase(unit.MyAromaFloating, id);
			if (enemy.MyAlive && kit.MyLanding && unit.MyAlive && unit.MySkill.MyActive && InRuleRange(_unit, id))
				(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * kit.MyLandingScale, .MyType = DamageType::ARTS,
					.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
		}
	}

	void BattleCore::OperatorEachHit(UnitId _source, UnitId _target, const AttackProfile& _profile, bool _main)
	{
		if (auto* native = _MyComponents[Index(_source)].NativeOperator()) native->EachHit(_target, _profile, _main);
		StandinEachHit(_source, _target, _profile, _main);
		if (_main) DiyOperatorSkillHit(_source, _target, _profile);
		const auto& source = Unit(_source);
		if (_main && _target && source.MyDefinition.MyOperatorKit && std::holds_alternative<Ghost2Kit>(*source.MyDefinition.MyOperatorKit)) Ghost2EachHit(_source, _target);
		if (!_target || !Unit(_target).MyAlive || !source.MyDefinition.MyOperatorKit) return;
		if (const auto* whitw = std::get_if<Whitw2Kit>(source.MyDefinition.MyOperatorKit); whitw && _main &&
			whitw->MySkill == Whitw2SkillKind::HUNT && std::isgreater(whitw->MyFear, 0) && std::isless(_MyRandom.Next(), whitw->MyFearChance))
			(void)ApplyStatus(_target, CombatStatus::FEAR, whitw->MyFear, _source);
		if (const auto* rosmon = std::get_if<RosmonKit>(source.MyDefinition.MyOperatorKit); rosmon && rosmon->MySkill == RosmonSkillKind::THOUGHT)
			(void)DealDamage(_source, _target, {.MyAmount = source.MyStats.MyAttack * rosmon->MyExtraScale, .MyType = DamageType::ARTS,
				.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
		if (const auto* f12yin = std::get_if<F12yinKit>(source.MyDefinition.MyOperatorKit); f12yin && _main && Unit(_target).MySide == UnitSide::ENEMY) (void)Push(_target, f12yin->MyForce, {.MyFrom = RulePosition(source)});
		if (const auto* svash = std::get_if<Svash2Kit>(source.MyDefinition.MyOperatorKit); svash && std::isgreater(svash->MyFragile, 0) && Unit(_target).MySide == UnitSide::ENEMY)
			(void)ApplyStatus(_target, CombatStatus::FRAGILE, {.MyDuration = svash->MyFragileDuration, .MySource = _source, .MyValue = svash->MyFragile});
		if (std::holds_alternative<HornKit>(*source.MyDefinition.MyOperatorKit) && std::isgreater(_profile.MyOperatorBonusScale, 0) && Unit(_target).MySide == UnitSide::ENEMY)
			(void)DealDamage(_source, _target, {.MyAmount = source.MyStats.MyAttack * _profile.MyOperatorBonusScale, .MyType = DamageType::ARTS,
				.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSplash = true, .MyIsSkill = true});
		if (const auto* blaze = std::get_if<Blaze2Kit>(source.MyDefinition.MyOperatorKit); blaze && blaze->MySkill == Blaze2SkillKind::GROUND && source.MySkill.MyActive && Unit(_target).MySide == UnitSide::ENEMY)
		{
			const auto point = Unit(_target).MyPosition; const auto r = static_cast<int>(std::floor(point.MyY + 0.5)), c = static_cast<int>(std::floor(point.MyX + 0.5));
			if (r >= 0 && r < FieldRows && c >= 0 && c < FieldColumns) _MyUnits[Index(_source)].MyBurnTiles.set(static_cast<std::size_t>(r * FieldColumns + c));
		}
		if (const auto* excu = std::get_if<Excu2Kit>(source.MyDefinition.MyOperatorKit); excu && excu->MySkill == Excu2SkillKind::VERDICT && source.MySkill.MyActive && Unit(_target).MySide == UnitSide::ENEMY)
		{
			auto& targets = _MyUnits[Index(_source)].MyVerdictTargets;
			if (!std::ranges::contains(targets, _target)) targets.push_back(_target);
		}
		if (const auto* gvial = std::get_if<GvialKit>(source.MyDefinition.MyOperatorKit); gvial && gvial->MySkill == GvialSkillKind::PULL && Unit(_target).MyAlive && !Unit(_target).MyBlockedBy)
			(void)PullToFront(_target, _source, gvial->MyForce);
		if (const auto* glady = std::get_if<GladyKit>(source.MyDefinition.MyOperatorKit); glady && glady->MySkill == GladySkillKind::GRASP) GladyPull(_source, _target, *glady);
		if (const auto* aroma = std::get_if<AromaKit>(source.MyDefinition.MyOperatorKit); aroma && !aroma->MyLanding && Unit(_target).Flying())
			(void)DealDamage(_source, _target, {.MyAmount = source.MyStats.MyAttack * aroma->MyFlyingScale, .MyType = DamageType::ARTS,
				.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
	}
}
