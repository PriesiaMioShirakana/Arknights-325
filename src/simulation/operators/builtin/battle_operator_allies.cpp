#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct AllyKitScratchGuard
		{
			std::size_t& MyDepth;

			~AllyKitScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::InstallGroundAttackSpeed(UnitId _source, std::string_view _key, double _amount, unsigned _count)
	{
		const auto handle = _MyGroundAttackSpeeds.size();
		_MyGroundAttackSpeeds.emplace_back(GroundAttackSpeedRuntime{.MySource = _source, .MyKey = _key, .MyAmount = _amount, .MyCount = std::max(1U, _count)});
		Schedule({.MyAt = Time() + 0.1, .MyKind = ScheduledKind::OPERATOR_GROUND_ASPD, .MySource = _source, .MyHandle = handle, .MyInterval = 0.1});
	}

	void BattleCore::RefreshGroundAttackSpeed(std::size_t _handle)
	{
		auto& state = _MyGroundAttackSpeeds[_handle];
		const auto& source = Unit(state.MySource);
		if (source.MyOperatorHooksReleased) return;
		unsigned count = 0;
		const AttackProfile filter{.MyCanHitFlying = true};
		if (source.MyAlive) for (const auto id : _MyEnemyIds)
			if (!Unit(id).Flying() && TargetableEnemy(Unit(id), filter) && InRuleRange(source.MyId, id) && ++count >= state.MyCount) break;
		const bool wanted = count >= state.MyCount;
		const bool present = std::ranges::any_of(source.MyBuffs, [&](const auto& _buff) { return _buff.MyDefinition.MyKey == state.MyKey; });
		if (wanted == state.MyApplied && (!wanted || present)) return;
		state.MyApplied = wanted;
		if (wanted) (void)AddBuff(source.MyId, {.MyKey = std::string(state.MyKey),
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = state.MyAmount}}});
		else (void)RemoveBuff(source.MyId, state.MyKey);
	}

	void BattleCore::AngelBless(UnitId _unit)
	{
		const auto& source = Unit(_unit);
		if (!source.MyAlive || source.MyOperatorHooksReleased) return;
		const auto& kit = std::get<AngelKit>(*source.MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch(); const AllyKitScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (id == _unit || !ally.MyAlive || ally.MyOwner != source.MyOwner || ally.MyKind != UnitKind::OPERATOR ||
				std::ranges::any_of(ally.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "talent:angel_bless_ally"; })) continue;
			scratch.MyTargets.push_back(id);
		}
		if (scratch.MyTargets.empty()) return;
		const auto target = scratch.MyTargets[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MyTargets.size()))];
		(void)AddBuff(target, {.MyKey = "talent:angel_bless_ally", .MySource = _unit, .MyModifiers = std::vector<AttributeChange>{
			{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyBlessAttack}, {.MyAttribute = Attribute::HEALTH_PERCENT, .MyValue = kit.MyBlessHealth}}, .MyPersistent = true});
	}

	void BattleCore::AyerAttack(UnitId _unit, const AyerKit& _kit)
	{
		const auto& source = Unit(_unit);
		const auto origin = RulePosition(source);
		const int row = static_cast<int>(std::floor(origin.MyY + 0.5)), column = static_cast<int>(std::floor(origin.MyX + 0.5));
		auto& scratch = AcquireAttackScratch(); const AllyKitScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (id == _unit || !ally.MyAlive || ally.MyKind == UnitKind::DEVICE || ally.MyBlocking.empty()) continue;
			const auto point = RulePosition(ally);
			if (std::abs(static_cast<int>(std::floor(point.MyY + 0.5)) - row) > 1 || std::abs(static_cast<int>(std::floor(point.MyX + 0.5)) - column) > 1) continue;
			for (const auto enemy : ally.MyBlocking)
				if (Unit(enemy).MyAlive && !std::ranges::contains(scratch.MyTargets, enemy)) scratch.MyTargets.push_back(enemy);
		}
		for (const auto target : scratch.MyTargets)
			(void)DealDamage(_unit, target, {.MyAmount = source.MyStats.MyAttack * _kit.MyBladeScale, .MyType = DamageType::ARTS,
				.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
	}

	void BattleCore::InstallSkadi(UnitId _unit, const SkadiKit& _kit)
	{
		const auto& source = Unit(_unit);
		constexpr std::array<std::string_view, 6> Abyssal{{"char_263_skadi", "char_143_ghost", "char_218_cuttle", "char_474_glady", "char_1023_ghost2", "char_4145_ulpia"}};
		auto& scratch = AcquireAttackScratch(); const AllyKitScratchGuard guard{.MyDepth = _MyAttackDepth};
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (ally.MyKind == UnitKind::OPERATOR && ally.MyOwner == source.MyOwner && std::ranges::contains(Abyssal, ally.MyDefinition.MyIdentity.MyCharacterId)) scratch.MyTargets.push_back(id);
		}
		for (const auto id : scratch.MyTargets) (void)AddBuff(id, {.MyKey = "talent:skadi_predator", .MySource = _unit,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _kit.MyTeamAttack}}, .MyPersistent = true, .MyAllowDead = true});
	}

	void BattleCore::NotifyOperatorLate(ContentEvent& _event)
	{
		StandinObserve(_event, true);
		Skadi2Observe(_event, true);
		if (_event.MyKind == ContentEventKind::SKILL_START) Angel2Observe(_event);
		LumenHealing(_event);
		Agoat2Veil(_event);
		if (_event.MyKind == ContentEventKind::DEPLOY && _event.MyUnit)
		{
			const auto& unit = Unit(_event.MyUnit);
			if (unit.MyKind == UnitKind::OPERATOR && unit.MyOwner != NoPlayer)
			{ _MyLastDeployedOperators.resize(_MyPlayers.size()); _MyLastDeployedOperators[unit.MyOwner] = unit.MyId; }
		}
		if (_event.MyKind == ContentEventKind::FATAL) { HornFatal(_event); RmixerObserve(_event); Blaze2Fatal(_event); SurtrFatal(_event); Sbell2Observe(_event, true); Nearl2Fatal(_event); }
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MySource && _event.MyTarget && _event.MyDamage.MyIsAttack &&
			(_event.MyDamage.MyType == DamageType::ARTS || _event.MyDamage.MyType == DamageType::PHYSICAL))
		{
			const auto& unit = Unit(_event.MySource);
			const auto* nymph = unit.MyDefinition.MyOperatorKit ? std::get_if<NymphKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
			if (nymph && nymph->MySkill == NymphSkillKind::BREAK && unit.MySkill.MyActive && !unit.MyOperatorHooksReleased &&
				std::ranges::any_of(Unit(_event.MyTarget).MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "apoptosisBurst"; }))
			{ _event.MyDamage.MyType = DamageType::ELEMENTAL; _event.MyDamage.MyCanDodge = false; }
		}
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE) EtlchiObserve(_event, true);
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE) { RosesaObserve(_event); GvialObserve(_event, true); }
		if (_event.MyKind == ContentEventKind::ATTACK && _event.MySource)
		{
			const auto& unit = Unit(_event.MySource);
			if (unit.MyDefinition.MyOperatorKit && std::holds_alternative<PepeKit>(*unit.MyDefinition.MyOperatorKit)) _MyUnits[Index(unit.MyId)].MyPepeBoost = false;
			const auto* swire = unit.MyDefinition.MyOperatorKit ? std::get_if<Swire2Kit>(unit.MyDefinition.MyOperatorKit) : nullptr;
			if (swire && swire->MySkill == Swire2SkillKind::HEAL && !unit.MyOperatorHooksReleased) Swire2Heal(unit.MyId, *swire);
		}
		DiyOperatorObserve(_event, true);
		if (_event.MyKind != ContentEventKind::FATAL || !_event.MyUnit || _event.MyPrevented || Finished()) return;
		auto& unit = _MyUnits[Index(_event.MyUnit)];
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<SkadiKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || !kit->MyRevive || unit.MySkadiRevived || unit.MyOperatorHooksReleased) return;
		unit.MySkadiRevived = true;
		_event.MyPrevented = true;
		(void)AddBuff(unit.MyId, {.MyKey = "trait:skadi_tide", .MyModifiers = std::vector<AttributeChange>{
			{.MyAttribute = Attribute::HEALTH_MULTIPLIER, .MyValue = kit->MyReviveHealthMultiplier}, {.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = kit->MyReviveAttackSpeed}}});
		unit.MyHealth = std::max(1.0, unit.MyStats.MyMaxHealth * kit->MyReviveHealthRatio);
	}

	void BattleCore::OperatorAddition(UnitId _source, UnitId _target, double _scale, DamageTags _tags, bool _enemyOnly)
	{
		if (!_target || !Unit(_target).MyAlive || !std::isgreater(_scale, 0) || (_enemyOnly && Unit(_target).MySide != UnitSide::ENEMY)) return;
		(void)DealDamage(_source, _target, {.MyAmount = Unit(_source).MyStats.MyAttack * _scale, .MyType = DamageType::ARTS, .MyTags = _tags});
	}
}
