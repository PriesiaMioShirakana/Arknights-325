#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		constexpr std::array<std::string_view, 5> TerrainKeys{"", "terrain:mire", "terrain:smog", "terrain:deepsea", "terrain:infection"};
		constexpr std::string_view AirKey = "terrain:airflow";
	}

	void BattleCore::RefreshTerrain(CombatUnit& _unit, bool _reset)
	{
		if (!_MyHasTerrain || !_unit.MyAlive || _unit.MyKind == UnitKind::DEVICE || Finished()) return;
		auto& state = _unit.MyTerrainState;
		if (_reset) state = {};
		const auto& field = *_MyInput.MyField;
		const auto& rules = field.MyTerrainRules;
		const auto row = static_cast<int>(std::floor(_unit.MyPosition.MyY + 0.5)), column = static_cast<int>(std::floor(_unit.MyPosition.MyX + 0.5));
		const bool inside = _MyGrid->InRect(row, column);
		const auto key = inside ? static_cast<std::size_t>(FieldGrid::Key(row, column)) : 0;
		auto terrain = inside && !_unit.MyHidden && !_unit.Flying() ? field.MyTiles[key].MyTerrain : FieldTerrain::NONE;
		if ((terrain == FieldTerrain::SMOG && _unit.MySide != UnitSide::ALLY) ||
			(terrain == FieldTerrain::DEEPSEA && _unit.MySide == UnitSide::ALLY)) terrain = FieldTerrain::NONE;
		if (terrain != state.MyTerrain)
		{
			// 离开地形移除即时效果；活性源石的持续效果继续到期，不因离开而提前结束。
			if (state.MyTerrain != FieldTerrain::NONE && state.MyTerrain != FieldTerrain::INFECTION)
				(void)RemoveBuff(_unit.MyId, TerrainKeys[static_cast<std::size_t>(state.MyTerrain)]);
			state.MyTerrain = terrain;
			state.MySince = Time();
			state.MyMireStacks = 0;
			state.MyMireTriggers = 0;
			if (terrain == FieldTerrain::SMOG)
			{
				StatusFlags flags;
				flags.set(static_cast<std::size_t>(CombatStatus::STEALTH));
				(void)AddBuff(_unit.MyId, BuffDefinition{.MyKey = "terrain:smog", .MyFlags = flags});
			}
			else if (terrain == FieldTerrain::DEEPSEA)
			{
				(void)AddBuff(_unit.MyId, BuffDefinition{.MyKey = "terrain:deepsea", .MyModifiers = std::vector<AttributeChange>{
					AttributeChange{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = rules.MyDeepseaAttackSpeed},
					AttributeChange{.MyAttribute = Attribute::MOVE_MULTIPLIER, .MyValue = rules.MyDeepseaMoveMultiplier}}, .MyInterval = 1,
					.MyTickEffects = {BuffEffect{.MyKind = BuffEffectKind::DAMAGE, .MyAmount = rules.MyDeepseaDamage,
						.MyCanDodge = false, .MySourceless = true, .MyNoSp = true, .MyTags = DamageTag::DOT | DamageTag::PERIODIC | DamageTag::DEEPSEA}}});
			}
		}
		if (!_unit.MyAlive || Finished()) return;
		if (terrain == FieldTerrain::INFECTION)
		{
			const auto found = std::ranges::find(_unit.MyBuffs, TerrainKeys[4], [](const CombatBuff& _buff) { return std::string_view(_buff.MyDefinition.MyKey); });
			if (found != _unit.MyBuffs.end()) found->MyRemaining = std::max(found->MyRemaining, rules.MyInfectionDuration);
			else (void)AddBuff(_unit.MyId, BuffDefinition{.MyKey = "terrain:infection", .MyDuration = rules.MyInfectionDuration,
				.MyModifiers = std::vector<AttributeChange>{AttributeChange{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = rules.MyInfectionAttackPercent},
					AttributeChange{.MyAttribute = Attribute::ATTACK_SPEED, .MyValue = rules.MyInfectionAttackSpeed}}, .MyInterval = 1,
				.MyTickEffects = {BuffEffect{.MyKind = BuffEffectKind::DAMAGE, .MyAmount = rules.MyInfectionDamage,
					.MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::TERRAIN)}}});
		}
		else if (terrain == FieldTerrain::MIRE)
		{
			const auto due = 1 + std::floor((Time() - state.MySince + 1e-9) / rules.MyMireInterval);
			if (std::isgreater(due, state.MyMireTriggers))
			{
				const bool enemy = _unit.MySide == UnitSide::ENEMY;
				const auto per = enemy && std::isgreaterequal(_unit.MyStats.MyMass, rules.MyMireHeavyMass) ? 2 : 1;
				const auto stacks = static_cast<unsigned>(std::min(static_cast<double>(rules.MyMireMaxStacks), state.MyMireStacks + per * (due - state.MyMireTriggers)));
				state.MyMireTriggers = due;
				if (stacks != state.MyMireStacks)
				{
					state.MyMireStacks = stacks;
					std::vector<AttributeChange> modifiers;
					modifiers.reserve(enemy ? 2U : 1U);
					modifiers.emplace_back(Attribute::ATTACK_SPEED, rules.MyMireAttackSpeed * stacks);
					if (enemy) modifiers.emplace_back(Attribute::MOVE_MULTIPLIER, std::max(0.0, 1 + rules.MyMireMovePerStack * stacks));
					(void)AddBuff(_unit.MyId, BuffDefinition{.MyKey = "terrain:mire", .MyModifiers = std::move(modifiers)});
				}
			}
		}
		if (!_unit.MyAlive || Finished()) return;
		const auto flow = inside ? field.MyAirflow[key] : std::nullopt;
		if (_unit.MySide == UnitSide::ALLY)
		{
			double attack = 0;
			if (flow && !_unit.MyDefinition.MyFlying)
			{
				const auto forward = RotateOffset(RangeOffset{.MyColumn = 1}, _unit.MyFacing);
				const auto dot = forward.MyColumn * flow->MyX + forward.MyRow * flow->MyY;
				attack = dot == 1 ? flow->MyAllyEqualAttack : dot == -1 ? flow->MyAllyOppositeAttack : flow->MyAllyVerticalAttack;
			}
			if (!std::islessgreater(attack, state.MyAirAttack)) return;
			state.MyAirAttack = attack;
			if (std::islessgreater(attack, 0)) (void)AddBuff(_unit.MyId, BuffDefinition{.MyKey = std::string(AirKey),
				.MyModifiers = std::vector<AttributeChange>{AttributeChange{.MyAttribute = Attribute::ATTACK_PERCENT, .MyValue = attack}}});
			else (void)RemoveBuff(_unit.MyId, AirKey);
		}
		else
		{
			const auto previous = state.MyAirPosition;
			state.MyAirPosition = _unit.MyPosition;
			double multiplier = 1;
			if (!_unit.MyHidden && previous && flow)
			{
				const auto dx = _unit.MyPosition.MyX - previous->MyX, dy = _unit.MyPosition.MyY - previous->MyY, length = std::hypot(dx, dy);
				if (std::isgreater(length, 1e-6))
				{
					const auto dot = (dx * flow->MyX + dy * flow->MyY) / length;
					multiplier = std::isgreater(dot, 0.7) ? 1 + flow->MyEnemyEqualSpeed : std::isless(dot, -0.7) ? 1 + flow->MyEnemyOppositeSpeed : 1;
				}
				else multiplier = state.MyAirMultiplier;
			}
			multiplier = std::max(0.0, multiplier);
			if (!std::islessgreater(multiplier, state.MyAirMultiplier)) return;
			state.MyAirMultiplier = multiplier;
			if (std::islessgreater(multiplier, 1)) (void)AddBuff(_unit.MyId, BuffDefinition{.MyKey = std::string(AirKey),
				.MyModifiers = std::vector<AttributeChange>{AttributeChange{.MyAttribute = Attribute::MOVE_MULTIPLIER, .MyValue = multiplier}}});
			else (void)RemoveBuff(_unit.MyId, AirKey);
		}
	}

	void BattleCore::TickTerrain()
	{
		if (!_MyHasTerrain) return;
		for (std::size_t i = 0; i < _MyAllyIds.size() && !Finished(); ++i) RefreshTerrain(_MyUnits[Index(_MyAllyIds[i])]);
		for (std::size_t i = 0; i < _MyEnemyIds.size() && !Finished(); ++i) RefreshTerrain(_MyUnits[Index(_MyEnemyIds[i])]);
	}
}
