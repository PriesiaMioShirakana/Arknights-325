#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::OrigamiBurst(UnitId _source, double _scale, bool _tokenBurst)
	{
		if (!std::isgreater(_scale, 0)) return;
		const auto& source = Unit(_source);
		const auto point = RulePosition(source);
		const auto amount = source.MyStats.MyAttack * _scale;
		const int row = static_cast<int>(std::floor(point.MyY + 0.5)), column = static_cast<int>(std::floor(point.MyX + 0.5));
		for (std::size_t i = 0, count = _MyEnemyIds.size(); i < count; ++i)
		{
			const auto id = _MyEnemyIds[i];
			const auto& target = Unit(id);
			if (target.MyAlive && !target.MyHidden && (_tokenBurst || !target.MyStatuses.Has(CombatStatus::UNTARGETABLE)) && BodyTileReach(target, row, column) <= 1)
				(void)DealDamage(_source, id, {.MyAmount = amount, .MyType = DamageType::ARTS, .MyCanDodge = false,
					.MyTags = _tokenBurst ? DamageTag::SUMMON | DamageTag::BURST : static_cast<DamageTags>(DamageTag::TALENT), .MyIsSkill = true});
		}
	}

	void Battle::KazemaTick(UnitId _unit)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (unit.MyOperatorHooksReleased) return;
		const bool sub = unit.MyAlive && std::ranges::any_of(unit.MyBuffs, [](const auto& _buff) { return _buff.MyDefinition.MyKey == "trait:substitute"; });
		if (sub == unit.MyKazemaSub) return;
		unit.MyKazemaSub = sub;
		if (!sub) { (void)RemoveBuff(_unit, "kazema:doll"); return; }
		const auto& kit = std::get<KazemaKit>(*unit.MyDefinition.MyOperatorKit);
		const auto* token = FindTokenTemplate(_unit, kit.MyToken);
		std::vector<AttributeChange> modifiers; modifiers.reserve(3);
		if (token)
		{
			if (std::isgreater(token->MyStats.MyAttack, 0)) modifiers.push_back({.MyAttribute = Attribute::ATTACK_FLAT, .MyValue = token->MyStats.MyAttack - unit.MyDefinition.MyStats.MyAttack});
			modifiers.push_back({.MyAttribute = Attribute::DEFENSE_FLAT, .MyValue = token->MyStats.MyDefense - unit.MyDefinition.MyStats.MyDefense});
		}
		if (std::isgreater(kit.MyDollAttack, 0)) modifiers.push_back({.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = kit.MyDollAttack});
		(void)AddBuff(_unit, {.MyKey = "kazema:doll", .MyModifiers = std::move(modifiers)});
		OrigamiBurst(_unit, kit.MyBurstScale);
	}

	void Battle::KazemaSkill(UnitId _unit, const KazemaKit& _kit, ContentEventKind _event)
	{
		auto& unit = _MyUnits[Index(_unit)];
		if (_kit.MyCut && _event == ContentEventKind::ATTACK && unit.MySkill.MyActive && unit.MySkill.MyPending && unit.MyAlive)
		{
			const auto loss = unit.MyStats.MyMaxHealth * _kit.MyHealthLoss;
			if (std::isgreater(loss, 0)) (void)LoseHealth(_unit, _unit, loss);
		}
		if (!_kit.MySummon) return;
		if (_event == ContentEventKind::SKILL_ENDING)
		{
			if (unit.MyKazemaDoll && Unit(unit.MyKazemaDoll).MyAlive) Retreat(unit.MyKazemaDoll, true, RemovalReason::EXPIRED);
			unit.MyKazemaDoll = 0;
		}
		if (_event != ContentEventKind::SKILL_START) return;
		const auto loss = unit.MyHealth * _kit.MyHealthLoss;
		if (std::isgreater(loss, 0) && std::isgreaterequal(unit.MyHealth - loss, 1)) (void)LoseHealth(_unit, _unit, loss);
		const auto* token = FindTokenTemplate(_unit, _kit.MyToken);
		if (!token) return;
		constexpr std::array<RangeOffset, 8> Order{{{.MyColumn = 1}, {.MyRow = 1, .MyColumn = 1}, {.MyRow = -1, .MyColumn = 1},
			{.MyRow = 1}, {.MyRow = -1}, {.MyColumn = -1}, {.MyRow = 1, .MyColumn = -1}, {.MyRow = -1, .MyColumn = -1}}};
		const auto origin = RulePosition(unit);
		for (const auto offset : Order)
		{
			const auto local = RotateOffset(offset, unit.MyFacing);
			const int row = static_cast<int>(std::floor(origin.MyY + 0.5)) + local.MyRow, column = static_cast<int>(std::floor(origin.MyX + 0.5)) + local.MyColumn;
			const WorldPoint point{.MyX = static_cast<double>(column), .MyY = static_cast<double>(row)};
			if (row < 0 || row >= FieldRows || column < 0 || column >= FieldColumns || ReservedTile(point) || (_MyGrid && !_MyGrid->CanStand(row, column))) continue;
			const auto doll = SpawnToken({.MyDefinition = *token, .MyPosition = point, .MyOwnerUnit = _unit});
			if (doll)
			{
				unit.MyKazemaDoll = doll;
				OrigamiBurst(doll, _kit.MyBurstScale);
			}
			return;
		}
	}
}
