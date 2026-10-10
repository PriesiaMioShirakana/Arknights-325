#include "battle_core.hpp"

namespace Stronghold
{
	void BattleCore::GuardDamage(ContentEvent& _event)
	{
		if (!_event.MyTarget || (_event.MyKind != ContentEventKind::BEFORE_DAMAGE && _event.MyKind != ContentEventKind::DAMAGED)) return;
		auto& unit = _MyUnits[Index(_event.MyTarget)];
		const auto* rules = unit.MyDefinition.MyOperatorKit;
		if (!rules || unit.MyOperatorHooksReleased) return;
		if (const auto* hsguma = std::get_if<HsgumaKit>(rules))
		{
			const auto credit = _event.MySource ? _event.MySource : _event.MyCredit;
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && !_event.MyCancel && credit && Unit(credit).MySide == UnitSide::ENEMY &&
				(_event.MyDamage.MyType == DamageType::PHYSICAL || _event.MyDamage.MyType == DamageType::ARTS) && std::isless(_MyRandom.Next(), hsguma->MyBlockProbability)) _event.MyCancel = true;
			if (_event.MyKind == ContentEventKind::DAMAGED && hsguma->MyCounter && unit.MyAlive && _event.MySource && !_event.MyElement &&
				!_event.MyDamage.MySourceless && !HasTag(_event.MyDamage.MyTags, DamageTag::HP_LOSS) && !HasTag(_event.MyDamage.MyTags, DamageTag::COUNTER) &&
				!HasTag(_event.MyDamage.MyTags, DamageTag::REFLECT) && Unit(_event.MySource).MySide == UnitSide::ENEMY && Unit(_event.MySource).MyAlive)
				(void)DealDamage(unit.MyId, _event.MySource, {.MyAmount = unit.MyStats.MyAttack * hsguma->MyCounterScale, .MyCanDodge = false,
					.MyTags = static_cast<DamageTags>(DamageTag::COUNTER), .MyIsSkill = true});
		}
		else if (const auto* mudrok = std::get_if<MudrokKit>(rules))
		{
			if (_event.MyKind == ContentEventKind::BEFORE_DAMAGE && _event.MySource)
			{
				const auto& source = Unit(_event.MySource);
				if (source.MySide == UnitSide::ENEMY && std::ranges::contains(source.MyDefinition.MyEnemyTags, "sarkaz")) _event.MyDamage.MyMultiplier *= 1 - mudrok->MySarkazReduction;
				if (std::isgreater(mudrok->MyBlockedScale, 0) && source.MyBlockedBy == unit.MyId) _event.MyDamage.MyMultiplier *= mudrok->MyBlockedScale;
			}
			else if (_event.MyKind == ContentEventKind::DAMAGED && unit.MyAlive)
			{
				const auto found = std::ranges::find(unit.MyBuffs, "mudrok:layers", [](const CombatBuff& _buff) -> const auto& { return _buff.MyDefinition.MyKey; });
				const auto left = found == unit.MyBuffs.end() ? 0 : found->MyDefinition.MyShield.MyHits;
				const auto broken = unit.MyMudrokLayers - left;
				if (broken <= 0) return;
				unit.MyMudrokLayers = left;
				(void)Heal(unit.MyId, unit.MyId, unit.MyStats.MyMaxHealth * mudrok->MyLayerHeal * broken, {.MySelf = true});
			}
		}
	}

	void BattleCore::MudrokLayers(UnitId _unit, int _layers)
	{
		_MyUnits[Index(_unit)].MyMudrokLayers = _layers;
		if (_layers > 0) (void)AddBuff(_unit, {.MyKey = "mudrok:layers", .MyShield = {.MyHits = _layers}});
		else (void)RemoveBuff(_unit, "mudrok:layers");
	}

	void BattleCore::MudrokSkill(UnitId _unit, const MudrokKit& _kit, const ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY)
		{
			MudrokLayers(_unit, std::min(_kit.MyMaxLayers, _kit.MyLayerGain));
			Schedule({.MyAt = Time() + _kit.MyLayerInterval, .MyKind = ScheduledKind::MUDROK_LAYERS, .MySource = _unit,
				.MyInterval = _kit.MyLayerInterval, .MyVersion = unit.MyDeploySequence});
		}
		if (_kit.MySkill == MudrokSkillKind::HAMMER && _event.MyKind == ContentEventKind::SKILL_START)
			(void)Heal(_unit, _unit, unit.MyStats.MyMaxHealth * _kit.MySkillHeal, {.MySelf = true});
		if (_kit.MySkill != MudrokSkillKind::DORMANT) return;
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			unit.MyMudrokAwake = false; unit.MyMudrokDormantTime = 0;
			StatusFlags flags;
			for (const auto status : {CombatStatus::INVULNERABLE, CombatStatus::DISARM, CombatStatus::NO_BLOCK}) flags.set(static_cast<std::size_t>(status));
			(void)AddBuff(_unit, {.MyKey = "mudrok:dormant", .MyDuration = _kit.MySleep + 1, .MyFlags = flags});
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK && !unit.MyMudrokAwake)
		{
			unit.MyMudrokDormantTime += _event.MyDelta;
			auto& scratch = AcquireAttackScratch();
			struct Guard
			{
				std::size_t& MyDepth;

				~Guard() { --MyDepth; }
			};
			const Guard guard{.MyDepth = _MyAttackDepth};
			OperatorEnemiesInGrid(_unit, _kit.MyRange, scratch.MyTargets, false);
			for (const auto id : scratch.MyTargets) (void)AddBuff(id, {.MyKey = unit.MyMudrokSlowKey, .MyDuration = 0.25,
				.MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::MOVE_MULTIPLIER, .MyValue = _kit.MyMoveMultiplier}}});
			if (std::isless(unit.MyMudrokDormantTime + 1e-9, _kit.MySleep)) return;
			unit.MyMudrokAwake = true;
			(void)RemoveBuff(_unit, "mudrok:dormant");
			for (const auto id : scratch.MyTargets) if (Unit(id).MyAlive && !Unit(id).Flying()) (void)ApplyStatus(id, CombatStatus::STUN, _kit.MyStun, _unit);
			(void)AddBuff(_unit, {.MyKey = "mudrok:awake", .MyModifiers = std::vector<AttributeChange>(_kit.MyAwakeModifiers.begin(), _kit.MyAwakeModifiers.end())});
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			unit.MyMudrokAwake = false; unit.MyMudrokDormantTime = 0;
			(void)RemoveBuff(_unit, "mudrok:dormant"); (void)RemoveBuff(_unit, "mudrok:awake");
		}
	}
}
