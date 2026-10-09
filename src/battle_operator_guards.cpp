#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::ScaleAttackByBlock(ContentEvent& _event, double _scale, bool _blocked)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MyDamage.MyIsAttack || !_event.MyTarget) return;
		const auto& target = Unit(_event.MyTarget);
		if (target.MySide == UnitSide::ENEMY && static_cast<bool>(target.MyBlockedBy) == _blocked) _event.MyDamage.MyAmount *= _scale;
	}

	void Battle::WhitewBlock(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MyTarget || _event.MyCancel || _event.MyDamage.MyType != DamageType::PHYSICAL) return;
		const auto& target = Unit(_event.MyTarget);
		const auto* kit = target.MyDefinition.MyOperatorKit ? std::get_if<WhitewKit>(target.MyDefinition.MyOperatorKit) : nullptr;
		const auto source = _event.MySource ? _event.MySource : _event.MyCredit;
		if (!kit || !kit->MySundial || !target.MySkill.MyActive || target.MyOperatorHooksReleased || !source || Unit(source).MySide != UnitSide::ENEMY || !std::isgreater(kit->MyBlockProbability, 0)) return;
		if (std::isless(_MyRandom.Next(), kit->MyBlockProbability)) _event.MyCancel = true;
	}

	void Battle::BranchSkill(UnitId _unit, const BranchKit& _kit, const ContentEvent& _event)
	{
		const auto& unit = Unit(_unit);
		if (_event.MyKind == ContentEventKind::SKILL_END && unit.MyAlive && _event.MySkillReason != SkillReason::DEATH && std::isgreater(_kit.MyHealRatio, 0))
			(void)Heal(_unit, _unit, unit.MyStats.MyMaxHealth * _kit.MyHealRatio, {.MySelf = true});
		if (_event.MyKind != ContentEventKind::SKILL_START) return;
		if (!_kit.MyResolve)
		{
			if (std::isgreater(_kit.MyResistance, 0)) (void)ApplyStatus(_unit, CombatStatus::RESIST,
				{.MyDuration = unit.MySkill.MyTimeLeft, .MySource = _unit, .MyValue = _kit.MyResistance});
			return;
		}
		auto& scratch = AcquireAttackScratch();
		struct Guard
		{
			std::size_t& MyDepth;

			~Guard() { --MyDepth; }
		};
		const Guard guard{.MyDepth = _MyAttackDepth};
		if (_kit.MyHasRange) OperatorEnemiesInGrid(_unit, _kit.MyRange, scratch.MyTargets);
		else
		{
			const AttackProfile filter{.MyCanHitFlying = true, .MyHitSleep = true};
			for (const auto id : _MyEnemyIds)
				if (TargetableEnemy(Unit(id), filter) && std::islessequal(BodyDistance(Unit(id), RulePosition(unit)), 1.5 + 1e-9)) scratch.MyTargets.push_back(id);
		}
		for (const auto id : scratch.MyTargets) if (!Unit(id).Flying()) (void)ApplyStatus(id, CombatStatus::TREMBLE, _kit.MyTremble, _unit);
	}

	void Battle::AshlokDeploy(UnitId _unit, const AshlokKit& _kit)
	{
		const auto point = RulePosition(Unit(_unit));
		const int row = static_cast<int>(std::floor(point.MyY + 0.5)), column = static_cast<int>(std::floor(point.MyX + 0.5));
		unsigned low = 0;
		constexpr std::array<RangeOffset, 4> Cross{{{.MyRow = 1}, {.MyRow = -1}, {.MyColumn = 1}, {.MyColumn = -1}}};
		for (const auto offset : Cross)
		{
			const auto r = row + offset.MyRow, c = column + offset.MyColumn;
			if (r < 0 || r >= FieldRows || c < 0 || c >= FieldColumns) continue;
			if (!_MyGrid || _MyGrid->Tile(r, c).MyLow) ++low;
		}
		const auto value = std::isgreaterequal(static_cast<double>(low), _kit.MyGroundCount) ? _kit.MyGroundAttack : _kit.MyAttack;
		if (std::islessgreater(value, 0)) (void)AddBuff(_unit, {.MyKey = "talent:ashlok",
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = value}}});
	}
}
