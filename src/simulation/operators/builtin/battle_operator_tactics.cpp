#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		struct TacticsScratchGuard
		{
			std::size_t& MyDepth;

			~TacticsScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::SlchanTick(UnitId _unit)
	{
		const auto& unit = Unit(_unit);
		if (unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<SlchanKit>(*unit.MyDefinition.MyOperatorKit);
		if (!std::islessgreater(kit.MyAttack, 0) && !std::islessgreater(kit.MyDefense, 0)) return;
		const bool wanted = unit.MyAlive && unit.MyBlocking.empty();
		const bool present = std::ranges::any_of(unit.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "slchan:hunter"; });
		if (wanted && !present) (void)AddBuff(_unit, {.MyKey = "slchan:hunter", .MyModifiers = std::vector<AttributeChange>{
			{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyAttack}, {.MyAttribute = Attribute::DEFENSE_PERCENT, .MyValue = kit.MyDefense}}});
		else if (!wanted && present) (void)RemoveBuff(_unit, "slchan:hunter");
	}

	void BattleCore::SlchanSkill(UnitId _unit, const SlchanKit& _kit)
	{
		auto& scratch = AcquireAttackScratch(); const TacticsScratchGuard guard{.MyDepth = _MyAttackDepth};
		if (_kit.MyHasRange) OperatorEnemiesInGrid(_unit, _kit.MyRange, scratch.MyTargets);
		else GenericEnemies(_unit, scratch.MyTargets);
		scratch.MyTargets.resize(std::min(scratch.MyTargets.size(), static_cast<std::size_t>(_kit.MyTargets)));
		for (const auto id : scratch.MyTargets)
		{
			const auto moved = PullToFront(id, _unit, _kit.MyForce);
			if (std::isgreater(moved, 0) && std::isgreater(_kit.MyDragDamage, 0))
				(void)DealDamage(_unit, id, {.MyAmount = _kit.MyDragDamage * moved / std::max(0.01, _kit.MyDragDistance), .MyType = DamageType::ARTS,
					.MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::DRAG), .MyIsSkill = true});
			if (!Unit(id).MyAlive) continue;
			(void)DealDamage(_unit, id, {.MyAmount = Unit(_unit).MyStats.MyAttack * _kit.MyScale, .MyType = DamageType::TRUE_DAMAGE,
				.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
			if (Unit(id).MyAlive) (void)ApplyStatus(id, CombatStatus::STUN, _kit.MyStun, _unit);
		}
	}

	void BattleCore::GrabdsSkill(UnitId _unit, const GrabdsKit& _kit)
	{
		_MyUnits[Index(_unit)].MyGrabdsQuietUntil = Time() + _kit.MySleep;
		auto& scratch = AcquireAttackScratch(); const TacticsScratchGuard guard{.MyDepth = _MyAttackDepth};
		if (_kit.MyHasRange) OperatorEnemiesInGrid(_unit, _kit.MyRange, scratch.MyTargets);
		else GenericEnemies(_unit, scratch.MyTargets);
		scratch.MyTargets.resize(std::min(scratch.MyTargets.size(), static_cast<std::size_t>(_kit.MyTargets)));
		for (const auto id : scratch.MyTargets) (void)ApplyStatus(id, CombatStatus::SLEEP, _kit.MySleep, _unit);
	}

	void BattleCore::WildmnDeploy(UnitId _unit)
	{
		for (const auto id : _MyWildmns)
		{
			auto& source = _MyUnits[Index(id)];
			if (source.MyOperatorHooksReleased) continue;
			if (id == _unit)
			{
				const auto& kit = std::get<WildmnKit>(*source.MyDefinition.MyOperatorKit);
				const bool first = !source.MyWildmnDeployed; source.MyWildmnDeployed = true;
				if (!first && !kit.MyEveryDeploy) continue;
				for (const auto target : _MyAllyIds)
				{
					auto& ally = _MyUnits[Index(target)];
					if (target == id || ally.MyKind != UnitKind::OPERATOR || ally.MyOwner != source.MyOwner || ally.MyAlive || ally.MyRemoved ||
						ally.MyDefinition.MyOperatorProfession != OperatorProfession::WARRIOR) continue;
					auto& cost = ally.MyDefinition.MyStats.MyDeploymentCost;
					const auto cut = std::min({kit.MyCostCut, kit.MyCostCap - ally.MyWildmnCostReduction, cost});
					if (std::isgreater(cut, 0)) { cost -= cut; ally.MyWildmnCostReduction += cut; }
				}
			}
			else
			{
				auto& target = _MyUnits[Index(_unit)];
				// 折扣已经用于本次支付；部署后恢复基础费用。普通撤退的野鬃仍持有此监听。
				if (std::isgreater(target.MyWildmnCostReduction, 0))
				{
					target.MyDefinition.MyStats.MyDeploymentCost += target.MyWildmnCostReduction;
					target.MyWildmnCostReduction = 0;
				}
			}
		}
	}

	void BattleCore::NotifyOperatorEarly(ContentEvent& _event)
	{
		MlynarObserve(_event, true);
		Agoat2Ash(_event);
		if (_event.MyKind == ContentEventKind::FATAL && _event.MyUnit)
		{
			const auto& unit = Unit(_event.MyUnit);
			const auto* ghost = unit.MyDefinition.MyOperatorKit ? std::get_if<Ghost2Kit>(unit.MyDefinition.MyOperatorKit) : nullptr;
			if (ghost && ghost->MySkill == Ghost2SkillKind::DESIRE && unit.MySkill.MyActive && !unit.MyOperatorHooksReleased) _event.MyPrevented = true;
		}
		if (_event.MyKind == ContentEventKind::DEPLOY && _event.MyUnit) WolfDeploy(_event.MyUnit);
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MyTarget)
		{
			auto& target = _MyUnits[Index(_event.MyTarget)];
			const auto* rmixer = target.MyDefinition.MyOperatorKit ? std::get_if<RmixerKit>(target.MyDefinition.MyOperatorKit) : nullptr;
			if (rmixer && rmixer->MySkill == RmixerSkillKind::GUARD && !target.MyOperatorHooksReleased)
			{ target.MyRmixerPreSequence = _event.MyDamage.MySequence; target.MyRmixerPreHealth = target.MyHealth; }
		}
		if (_event.MyKind == ContentEventKind::SKILL_START && _event.MyUnit)
		{
			const auto& unit = Unit(_event.MyUnit);
			if (const auto* cathy = unit.MyDefinition.MyOperatorKit ? std::get_if<CathyKit>(unit.MyDefinition.MyOperatorKit) : nullptr; cathy && cathy->MyForge)
				Schedule({.MyAt = Time() + 0.2, .MyKind = ScheduledKind::CATHY_FORGE, .MySource = unit.MyId, .MyHandle = unit.MySkillAura,
					.MyInterval = 0.2, .MyVersion = unit.MyDeploySequence});
		}
		if (_event.MyKind == ContentEventKind::SKILL_START) ReckprObserve(_event);
		if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MySource && _event.MyDamage.MyIsAttack && _event.MyDamage.MyType == DamageType::PHYSICAL)
		{
			const auto& source = Unit(_event.MySource);
			const auto* texas = source.MyDefinition.MyOperatorKit ? std::get_if<Texas2Kit>(source.MyDefinition.MyOperatorKit) : nullptr;
			if (texas && texas->MySkill == Texas2SkillKind::STORM && source.MySkill.MyActive && !source.MyOperatorHooksReleased) _event.MyDamage.MyType = DamageType::ARTS;
		}
		BldskEarly(_event);
		if (_event.MyKind == ContentEventKind::FATAL) TitiObserve(_event);
		InesEarly(_event);
		PhilaeElementTalent(_event);
		HaroldElementHit(_event);
		TippiHit(_event);
		WhitewBlock(_event);
		FlamtlDodge(_event);
		if (_event.MyKind == ContentEventKind::FATAL && _event.MyUnit)
		{
			const auto& unit = Unit(_event.MyUnit);
			const auto* ghost = unit.MyDefinition.MyOperatorKit ? std::get_if<GhostKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
			if (ghost && ghost->MyUndying && unit.MySkill.MyActive && !unit.MyOperatorHooksReleased) _event.MyPrevented = true;
		}
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MyTarget || _event.MyCancel || !std::isgreater(_event.MyDamage.MyAmount, 0)) return;
		const auto& target = Unit(_event.MyTarget); const auto* rules = target.MyDefinition.MyOperatorKit;
		const auto* kit = rules ? std::get_if<LiskamKit>(rules) : nullptr;
		if (!kit || !kit->MyDefense || target.MyOperatorHooksReleased) return;
		if (std::ranges::any_of(target.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "liskam:block"; }))
		{
			// 原 hit 监听优先级为 50，先于普通伤害修正；取消后仍广播其余监听。
			_event.MyCancel = true;
			(void)RemoveBuff(_event.MyTarget, "liskam:block");
		}
	}

	void BattleCore::LiskamDamaged(UnitId _unit, const LiskamKit& _kit)
	{
		const auto& unit = Unit(_unit);
		if (!unit.MyAlive || unit.MyOperatorHooksReleased || !std::isgreater(_kit.MySp, 0)) return;
		(void)GainSp(_unit, _kit.MySp);
		constexpr std::array<RangeOffset, 4> Cross{{{.MyRow = 1}, {.MyColumn = -1}, {.MyColumn = 1}, {.MyRow = -1}}};
		const auto grid = _kit.MyHasRange ? _kit.MyRange : std::span<const RangeOffset>(Cross);
		const auto origin = RulePosition(unit);
		const int row = static_cast<int>(std::floor(origin.MyY + 0.5)), column = static_cast<int>(std::floor(origin.MyX + 0.5));
		auto& scratch = AcquireAttackScratch();
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id);
			if (id == _unit || !ally.MyAlive || ally.MyHidden || ally.MyKind == UnitKind::DEVICE ||
				ally.MyStatuses.Has(CombatStatus::ISOLATED) || ally.MyDefinition.MySkill.MyKind == SkillKind::NONE) continue;
			const auto point = RulePosition(ally);
			const int r = static_cast<int>(std::floor(point.MyY + 0.5)), c = static_cast<int>(std::floor(point.MyX + 0.5));
			if (std::ranges::any_of(grid, [&](RangeOffset _offset)
			{
				const auto local = RotateOffset(_offset, unit.MyFacing);
				return r == row + local.MyRow && c == column + local.MyColumn;
			})) scratch.MyTargets.push_back(id);
		}
		// 技能活动期的友方仍参与抽签；是否获得 SP 由 GainSp 统一判断。
		const auto target = scratch.MyTargets.empty() ? UnitId{} : scratch.MyTargets[_MyRandom.Index(static_cast<std::uint32_t>(scratch.MyTargets.size()))];
		--_MyAttackDepth;
		if (target) (void)GainSp(target, _kit.MySp);
	}
}
