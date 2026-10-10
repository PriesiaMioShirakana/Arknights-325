#include "battle_core.hpp"
#include <numbers>

namespace Stronghold
{
	namespace
	{
		struct BardScratchGuard
		{
			std::size_t& MyDepth;

			~BardScratchGuard() { --MyDepth; }
		};
	}

	void BattleCore::Inspire(UnitId _source, UnitId _target, double _value, bool _health)
	{
		InspireAttribute(_source, _target, _value, _health ? Attribute::HEALTH_FLAT : Attribute::ATTACK_FLAT, 0.25, 1e-6);
	}

	void BattleCore::InspireAttribute(UnitId _source, UnitId _target, double _value, Attribute _attribute, double _duration, double _handover)
	{
		const auto& target = Unit(_target);
		if (!target.MyAlive || target.MyNoInspire || !std::isgreater(_value, 0)) return;
		const std::string_view key = _attribute == Attribute::HEALTH_FLAT ? "inspire:hp" : _attribute == Attribute::DEFENSE_FLAT ? "inspire:def" : "inspire";
		const auto found = std::ranges::find(target.MyBuffs, key, [](const CombatBuff& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
		if (found != target.MyBuffs.end() && found->MyDefinition.MySource != _source && found->MyDefinition.MyStrength &&
			std::isgreater(found->MyDefinition.MyStrength->MyValue, _value) && std::isgreater(found->MyRemaining, _handover)) return;
		AttributeModifiers modifiers;
		for (const auto& buff : target.MyBuffs)
			if (buff.MyDefinition.MyKey != key && buff.MyDefinition.MyModifiers) modifiers.Add(*buff.MyDefinition.MyModifiers, buff.MyDefinition.MyStacks);
		const auto percent = _attribute == Attribute::HEALTH_FLAT ? Attribute::HEALTH_PERCENT : _attribute == Attribute::DEFENSE_FLAT ? Attribute::DEFENSE_PERCENT : Attribute::ATTACK_PERCENT;
		const auto multiplier = _attribute == Attribute::HEALTH_FLAT ? Attribute::HEALTH_MULTIPLIER : _attribute == Attribute::DEFENSE_FLAT ? Attribute::DEFENSE_MULTIPLIER : Attribute::ATTACK_MULTIPLIER;
		const auto factor = std::max(0.0, 1 + modifiers.At(percent)) * modifiers.At(multiplier);
		const auto flat = std::isgreater(factor, 1e-6) ? _value / factor : _value;
		(void)AddBuff(_target, {.MyKey = std::string(key), .MySource = _source, .MyDuration = _duration,
			.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = _attribute, .MyValue = flat}}, .MyStrength = BuffStrength{.MyValue = _value, .MyAttribute = _attribute}});
	}

	void BattleCore::BardRegen(UnitId _source, UnitId _target, double _value, double _duration)
	{
		if (!Unit(_target).MyAlive || !std::isgreater(_value, 0)) return;
		ContentEvent event{.MyKind = ContentEventKind::BARD_REGEN, .MyUnit = _source, .MySource = _source, .MyTarget = _target, .MyAmount = _value};
		NotifyContent(event);
		if (!Finished() && std::isfinite(event.MyAmount) && std::isgreater(event.MyAmount, 0))
		{
			auto& key = _MyUnits[Index(_source)].MyProfession.MyAuraKey;
			if (key.empty()) key = "trait:bard:" + std::to_string(_source);
			(void)AddBuff(_target, {.MyKey = key, .MySource = _source, .MyDuration = _duration,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALTH_REGEN, .MyValue = event.MyAmount}}});
		}
	}

	bool BattleCore::IsOperatorLeader(UnitId _unit) const
	{
		const auto& character = Unit(_unit).MyDefinition.MyIdentity.MyCharacterId;
		for (const auto id : _MyAllyIds)
			if (Unit(id).MyAlive && Unit(id).MyDefinition.MyIdentity.MyCharacterId == character) return id == _unit;
		return false;
	}

	void BattleCore::CetsyrProtect(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MyTarget || !_event.MySource || Unit(_event.MyTarget).MySide != UnitSide::ALLY ||
			Unit(_event.MySource).MySide != UnitSide::ENEMY || !std::ranges::contains(Unit(_event.MySource).MyDefinition.MyEnemyTags, "sarkaz")) return;
		for (const auto id : _MyCetsyrs)
		{
			const auto& source = Unit(id); const auto& kit = std::get<CetsyrKit>(*source.MyDefinition.MyOperatorKit);
			if (source.MyOperatorHooksReleased || (kit.MyOrbitMotes ? !IsOperatorLeader(id) : source.MyOwner != Unit(_event.MyTarget).MyOwner)) continue;
			_event.MyDamage.MyMultiplier *= 1 - kit.MySarkazReduction;
		}
	}

	void BattleCore::CetsyrInspire(UnitId _unit, const CetsyrKit& _kit)
	{
		if (!std::isgreater(_kit.MyInspire, 0)) return;
		auto& scratch = AcquireAttackScratch(); const BardScratchGuard guard{.MyDepth = _MyAttackDepth};
		OperatorAlliesInRange(_unit, scratch.MyTargets);
		const auto value = Unit(_unit).MyStats.MyMaxHealth * _kit.MyInspire;
		for (const auto id : scratch.MyTargets)
			if (id != _unit) (void)AddBuff(id, {.MyKey = "cetsyr:inspire", .MySource = _unit, .MyDuration = 0.5,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::HEALTH_FLAT, .MyValue = value}}});
	}

	void BattleCore::CetsyrMotes(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (!unit.MyAlive || unit.MyOperatorHooksReleased) return;
		const auto& kit = std::get<CetsyrKit>(*unit.MyDefinition.MyOperatorKit);
		const auto cooldown = unit.MySkill.MyActive && kit.MySkill == CetsyrSkillKind::PAST ? kit.MySkillCooldown : kit.MyCooldown;
		const auto take = [&]()
		{
			const auto found = std::ranges::find_if(unit.MyMoteReady, [&](double _time) { return std::islessequal(_time, Time() + 1e-9); });
			if (found == unit.MyMoteReady.end()) return false;
			*found = Time() + cooldown;
			return true;
		};
		const auto origin = RulePosition(unit);
		auto& scratch = AcquireAttackScratch(); const BardScratchGuard guard{.MyDepth = _MyAttackDepth};
		if (kit.MyOrbitMotes)
		{
			const auto reach = kit.MyAllyRadius + 0.41;
			for (const auto id : _MyAllyIds)
			{
				const auto& ally = Unit(id);
				if (id != _unit && ally.MyAlive && !ally.MyHidden && ally.MyKind == UnitKind::OPERATOR &&
					std::islessequal(Distance(origin, RulePosition(ally)), reach) && !std::ranges::any_of(ally.MyBuffs, [](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == "cetsyr:mote"; })) scratch.MyTargets.push_back(id);
			}
			const auto forward = RotateOffset({.MyColumn = 1}, unit.MyFacing), left = RotateOffset({.MyRow = 1}, unit.MyFacing);
			for (std::size_t k = 0; k < unit.MyMoteReady.size(); ++k)
			{
				if (std::isgreater(unit.MyMoteReady[k], Time() + 1e-9)) continue;
				const auto angle = kit.MyAngularSpeed * (Time() - unit.MyDeployedAt) + 2 * std::numbers::pi * static_cast<double>(k) / static_cast<double>(unit.MyMoteReady.size());
				const WorldPoint point{.MyX = origin.MyX + kit.MyAllyRadius * (forward.MyColumn * std::cos(angle) + left.MyColumn * std::sin(angle)),
					.MyY = origin.MyY + kit.MyAllyRadius * (forward.MyRow * std::cos(angle) + left.MyRow * std::sin(angle))};
				UnitId target = 0; double nearest = std::numeric_limits<double>::infinity();
				for (const auto id : scratch.MyTargets)
				{
					const auto& ally = Unit(id);
					if (std::ranges::any_of(ally.MyBuffs, [](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == "cetsyr:mote"; })) continue;
					const auto distance = Distance(point, RulePosition(ally));
					if (std::islessequal(distance, 0.4 + 1e-9) && std::isless(distance, nearest)) { target = id; nearest = distance; }
				}
				if (!target) continue;
				(void)AddBuff(target, {.MyKey = "cetsyr:mote", .MySource = _unit, .MyDuration = kit.MyMoteDuration, .MyStrength = BuffStrength{.MyValue = kit.MyTraitScale}});
				if (!unit.MySkill.MyActive) unit.MyMoteReady[k] = Time() + kit.MyCooldown;
			}
			return;
		}
		if (unit.MySkill.MyActive && kit.MySkill == CetsyrSkillKind::TOMORROW)
		{
			const AttackProfile filter{.MyCanHitFlying = true};
			for (const auto id : _MyEnemyIds)
			{
				const auto distance = BodyDistance(Unit(id), origin);
				if (TargetableEnemy(Unit(id), filter) && std::islessequal(distance * distance, kit.MyEnemyRadius * kit.MyEnemyRadius + 1e-9)) scratch.MyTargets.push_back(id);
			}
			std::ranges::sort(scratch.MyTargets);
			for (const auto id : scratch.MyTargets)
			{
				const auto mark = std::ranges::find(unit.MyMoteHits, id, &TimedTargetMark::MyTarget);
				if (mark != unit.MyMoteHits.end() && std::isgreater(mark->MyTime, Time() + 1e-9)) continue;
				if (!take()) break;
				if (mark == unit.MyMoteHits.end()) unit.MyMoteHits.push_back({.MyTarget = id, .MyTime = Time() + 0.5});
				else mark->MyTime = Time() + 0.5;
				(void)DealDamage(_unit, id, {.MyAmount = unit.MyStats.MyAttack * kit.MyMoteScale, .MyType = DamageType::TRUE_DAMAGE,
					.MyTags = static_cast<DamageTags>(DamageTag::SKILL), .MyIsSkill = true});
				if (Unit(id).MyAlive) (void)ApplyStatus(id, CombatStatus::BIND, kit.MyBind, _unit);
			}
			return;
		}
		const bool keep = unit.MySkill.MyActive && kit.MySkill == CetsyrSkillKind::REWEAVE;
		for (const auto id : _MyAllyIds)
		{
			const auto& ally = Unit(id); const auto distance = Distance(origin, RulePosition(ally));
			if (id != _unit && ally.MyAlive && !ally.MyHidden && ally.MyOwner == unit.MyOwner && ally.MyKind == UnitKind::OPERATOR &&
				std::islessequal(distance * distance, kit.MyAllyRadius * kit.MyAllyRadius + 1e-9)) scratch.MyTargets.push_back(id);
		}
		for (const auto id : scratch.MyTargets)
		{
			if (std::ranges::any_of(Unit(id).MyBuffs, [&](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == unit.MyMoteKey; })) continue;
			if (!keep && !take()) break;
			(void)AddBuff(id, {.MyKey = unit.MyMoteKey, .MySource = _unit, .MyDuration = kit.MyMoteDuration});
		}
	}

	void BattleCore::CetsyrSkill(UnitId _unit, const CetsyrKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY && _kit.MyOrbitMotes) std::ranges::fill(unit.MyMoteReady, 0);
		if (_event.MyKind == ContentEventKind::BARD_REGEN && _event.MyTarget)
		{
			const auto& buffs = Unit(_event.MyTarget).MyBuffs;
			const auto found = std::ranges::find(buffs, unit.MyMoteKey, [](const CombatBuff& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
			if (found != buffs.end()) _event.MyAmount *= _kit.MyOrbitMotes ? (found->MyDefinition.MyStrength ? found->MyDefinition.MyStrength->MyValue : 1) : _kit.MyTraitScale;
		}
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			if (_kit.MySkill == CetsyrSkillKind::TOMORROW)
			{
				unit.MyMoteReady.assign(_kit.MyMotes + 3, -std::numeric_limits<double>::infinity());
				unit.MyMoteHits.clear();
			}
			else unit.MyBardRatio = _kit.MyTraitRatio.value_or(unit.MyDefinition.MyProfession.MyAuraRatio);
			unit.MyRedistributeAccumulator = 0; unit.MyInspireAccumulator = 0;
			if (_kit.MyOrbitMotes) CetsyrInspire(_unit, _kit);
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			if (_kit.MySkill == CetsyrSkillKind::TOMORROW)
			{
				std::ranges::sort(unit.MyMoteReady);
				if (unit.MyMoteReady.size() > _kit.MyMotes) unit.MyMoteReady.resize(_kit.MyMotes);
				unit.MyMoteHits.clear();
			}
			else if (_kit.MyOrbitMotes)
			{
				unit.MyBardRatio = _kit.MyBaseRatio;
				for (const auto id : _MyAllyIds)
					if (std::ranges::any_of(Unit(id).MyBuffs, [&](const CombatBuff& _buff) { return _buff.MyDefinition.MyKey == "cetsyr:inspire" && _buff.MyDefinition.MySource == _unit; })) (void)RemoveBuff(id, "cetsyr:inspire");
			}
			else unit.MyBardRatio.reset();
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK && _kit.MySkill != CetsyrSkillKind::PAST)
		{
			auto& scratch = AcquireAttackScratch(); const BardScratchGuard guard{.MyDepth = _MyAttackDepth};
			OperatorAlliesInRange(_unit, scratch.MyTargets);
			if (_kit.MyOrbitMotes)
			{
				unit.MyInspireAccumulator += _event.MyDelta;
				if (std::isgreaterequal(unit.MyInspireAccumulator, 0.25)) { unit.MyInspireAccumulator = 0; CetsyrInspire(_unit, _kit); }
			}
			else for (const auto id : scratch.MyTargets)
				if (id != _unit) Inspire(_unit, id, (_kit.MySkill == CetsyrSkillKind::REWEAVE ? unit.MyStats.MyMaxHealth : unit.MyStats.MyAttack) * _kit.MyInspire, _kit.MySkill == CetsyrSkillKind::REWEAVE);
			if (_kit.MySkill != CetsyrSkillKind::REWEAVE) return;
			unit.MyRedistributeAccumulator += _event.MyDelta;
			if (std::isless(unit.MyRedistributeAccumulator + 1e-9, _kit.MyRedistributeInterval)) return;
			unit.MyRedistributeAccumulator -= _kit.MyRedistributeInterval;
			OperatorAlliesInRange(_unit, scratch.MyTargets);
			if (!_kit.MyOrbitMotes) std::erase_if(scratch.MyTargets, [&](UnitId _id) { return _id != _unit && (Unit(_id).MyStatuses.Has(CombatStatus::NO_HEAL) || Unit(_id).MyDefinition.MyAttack.MyNoHeal); });
			double health = 0, maximum = 0;
			for (const auto id : scratch.MyTargets) { health += Unit(id).MyHealth; maximum += Unit(id).MyStats.MyMaxHealth; }
			if (scratch.MyTargets.size() < 2 || !std::isgreater(maximum, 0)) return;
			const auto ratio = std::min(1.0, health / maximum);
			for (const auto id : scratch.MyTargets)
			{
				auto& ally = _MyUnits[Index(id)];
				ally.MyHealth = std::max(1.0, std::min(ally.MyStats.MyMaxHealth, ally.MyStats.MyMaxHealth * ratio));
			}
		}
	}
}
