#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::PeriodicHealthLoss(UnitId _unit, double& _accumulator, double _delta, double _interval, double _ratio, bool _silent)
	{
		_accumulator += _delta;
		while (std::isgreaterequal(_accumulator, _interval) && Unit(_unit).MyAlive)
		{
			_accumulator -= _interval;
			if (std::isgreater(_ratio, 0)) (void)LoseHealth(_unit, _unit, Unit(_unit).MyStats.MyMaxHealth * _ratio * _interval, _silent);
		}
	}

	void Battle::OperatorAttackProfile(CombatUnit& _unit, AttackProfile& _profile)
	{
		const auto* horn = std::get_if<HornKit>(_unit.MyDefinition.MyOperatorKit);
		if (!horn) return;
		if (horn->MySkill == HornSkillKind::FLARE && _unit.MySkill.MyPending)
		{
			_unit.MyHornFlare = !_unit.MyProfession.MyFortressMelee;
			if (_unit.MyHornFlare) _profile.MySplashRadius = std::max(_profile.MySplashRadius, horn->MyFlareRadius);
		}
		else if (horn->MySkill == HornSkillKind::STORM && _unit.MySkill.MyActive)
		{
			const auto ammo = _unit.MyDefinition.MySkill.MyAmmo;
			_unit.MyHornOverload = std::isgreater(ammo - _unit.MySkill.MyAmmoLeft + 1, ammo * 0.5 + 1e-9);
			if (_unit.MyHornOverload && std::isgreater(horn->MyMagicScale, 0))
			{ _profile.MyOperatorEachHit = true; _profile.MyOperatorBonusScale = horn->MyMagicScale; }
		}
	}

	void Battle::HornFatal(ContentEvent& _event)
	{
		if (_event.MyKind != ContentEventKind::FATAL || !_event.MyUnit || _event.MyPrevented) return;
		auto& unit = _MyUnits[Index(_event.MyUnit)];
		const auto* kit = unit.MyDefinition.MyOperatorKit ? std::get_if<HornKit>(unit.MyDefinition.MyOperatorKit) : nullptr;
		if (!kit || unit.MyOperatorHooksReleased || unit.MyHornRevived) return;
		_event.MyPrevented = true; unit.MyHornRevived = true;
		(void)AddBuff(unit.MyId, {.MyKey = "horn:bloodBattle", .MyModifiers = std::vector<AttributeChange>(kit->MyReviveModifiers.begin(), kit->MyReviveModifiers.end())});
		unit.MyHealth = std::max(1.0, unit.MyHealth);
		(void)Heal(unit.MyId, unit.MyId, unit.MyStats.MyMaxHealth * kit->MyReviveHeal, {.MySelf = true});
	}

	void Battle::HornFlares(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (unit.MyOperatorHooksReleased) return;
		std::erase_if(unit.MyFlares, [&](const TimedPosition& _flare) { return !std::isgreater(_flare.MyUntil, Time() + 1e-9); });
		const auto& kit = std::get<HornKit>(*unit.MyDefinition.MyOperatorKit);
		auto& scratch = AcquireAttackScratch();
		struct Guard
		{
			std::size_t& MyDepth;

			~Guard() { --MyDepth; }
		};
		const Guard guard{.MyDepth = _MyAttackDepth};
		for (std::size_t n = 0, count = unit.MyFlares.size(); n < count; ++n)
		{
			const auto point = unit.MyFlares[n].MyPoint; scratch.MyTargets.clear();
			for (const auto id : _MyEnemyIds)
			{
				const auto& enemy = Unit(id); const auto distance = BodyDistance(enemy, point);
				if (enemy.MyAlive && !enemy.MyHidden && std::islessequal(distance * distance, kit.MyFlareRadius * kit.MyFlareRadius + 1e-9)) scratch.MyTargets.push_back(id);
			}
			for (const auto id : scratch.MyTargets) (void)ApplyStatus(id, CombatStatus::REVEAL, 0.5, _unit);
		}
	}

	void Battle::HornSkill(UnitId _unit, const HornKit& _kit, ContentEvent& _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_event.MyKind == ContentEventKind::DEPLOY) unit.MyHornRevived = false;
		if (_event.MyKind == ContentEventKind::SKILL_START)
		{
			unit.MyHornTime = 0; unit.MyHornAccumulator = 0; unit.MyHornOverload = false;
		}
		else if (_event.MyKind == ContentEventKind::SKILL_ENDING)
		{
			unit.MyHornOverload = false;
			if (_kit.MySkill == HornSkillKind::DEFENSE) (void)RemoveBuff(_unit, "horn:overload");
		}
		else if (_event.MyKind == ContentEventKind::SKILL_TICK && _kit.MySkill == HornSkillKind::DEFENSE)
		{
			unit.MyHornTime += _event.MyDelta;
			if (!unit.MyHornOverload && std::isgreaterequal(unit.MyHornTime, _kit.MyMainDuration - 1e-9))
			{
				unit.MyHornOverload = true;
				(void)AddBuff(_unit, {.MyKey = "horn:overload", .MyModifiers = std::vector<AttributeChange>{{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = _kit.MyOverloadAttack}}});
			}
			if (unit.MyHornOverload && std::isgreater(_kit.MyPeakLoss, 0)) PeriodicHealthLoss(_unit, unit.MyHornAccumulator, _event.MyDelta, _kit.MyLossInterval,
				_kit.MyPeakLoss * std::min(1.0, (unit.MyHornTime - _kit.MyMainDuration) / _kit.MyOverloadDuration), true);
		}
	}
}
