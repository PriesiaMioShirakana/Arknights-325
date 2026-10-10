#include "battle_core.hpp"

namespace Stronghold
{
	std::optional<WorldPoint> BattleCore::FindYanyouTile(std::size_t _player, const std::bitset<FieldTiles>& _taken) const
	{
		const auto rect = _MyGrid ? _MyGrid->Rect() : FieldRect{};
		int first = rect.MyFirstColumn, last = rect.MyLastColumn;
		const auto& player = _MyInput.MyPlayers[_player];
		if (_MyParticipantCount > 1)
		{
			if (player.MyRightHalf.value_or(player.MyMirrorDeployment)) first = std::max(first, 11);
			else last = std::min(last, 10);
		}
		const double rowCenter = (rect.MyFirstRow + rect.MyLastRow) / 2.0, colCenter = (first + last) / 2.0;
		std::bitset<FieldTiles> held;
		for (const auto point : _MyInput.MyMapCharacterTiles)
			if (point.MyRow >= 0 && point.MyRow < FieldRows && point.MyColumn >= 0 && point.MyColumn < FieldColumns)
				held.set(static_cast<std::size_t>(FieldGrid::Key(point.MyRow, point.MyColumn)));
		std::optional<WorldPoint> best;
		int bestClass = 0, bestKey = 0; double bestDistance = 0;
		for (int row = rect.MyFirstRow; row <= rect.MyLastRow; ++row)
			for (int column = first; column <= last; ++column)
			{
				const auto key = FieldGrid::Key(row, column);
				const WorldPoint point{static_cast<double>(column), static_cast<double>(row)};
				if (_taken.test(static_cast<std::size_t>(key)) || held.test(static_cast<std::size_t>(key)) || ReservedTile(point)) continue;
				const auto tile = _MyGrid ? _MyGrid->Tile(row, column) : FieldTile{};
				const int priority = tile.MyBuild == FieldBuild::NONE ? (tile.MyWalkable && tile.MyFlyable ? 1 : 0) : 2;
				const auto distance = std::hypot(row - rowCenter, column - colCenter);
				if (!best || priority < bestClass || (priority == bestClass && (distance < bestDistance - 1e-9 ||
					(std::abs(distance - bestDistance) <= 1e-9 && key < bestKey))))
				{ best = point; bestClass = priority; bestDistance = distance; bestKey = key; }
			}
		return best;
	}

	void BattleCore::SpawnBondYanyou(std::size_t _index)
	{
		if (!_MyInput.MyYanyouDefinition) return;
		const auto& state = _MyCoreBonds[_index];
		const auto& player = _MyInput.MyPlayers[state.MyPlayer];
		const auto& p = player.MyCoreBonds[state.MyEffect].MyParameters;
		double attack = 0, health = 0;
		for (const auto id : state.MyMembers)
		{
			const auto& unit = Unit(id);
			if (!unit.MyAlive || unit.MyRemoved || !std::ranges::contains(unit.MyDefinition.MyIdentity.MyBonds, std::string_view("yanShip"))) continue;
			attack += unit.MyStats.MyAttack; health += unit.MyStats.MyMaxHealth;
		}
		if (!(health > 0)) return;
		std::bitset<FieldTiles> taken;
		const auto count = std::min(state.MyExPower ? 2U : 1U, _MyInput.MyYanyouDefinition->MyYanyou->MyDeployLimit);
		unsigned created = 0;
		for (unsigned i = 0; i < count; ++i)
		{
			const auto tile = FindYanyouTile(state.MyPlayer, taken);
			if (!tile) break;
			taken.set(static_cast<std::size_t>(FieldGrid::Key(static_cast<int>(tile->MyY), static_cast<int>(tile->MyX))));
			auto definition = *_MyInput.MyYanyouDefinition;
			definition.MyStats.MyAttack += attack * p.MySummonShare;
			definition.MyStats.MyMaxHealth += health * p.MySummonShare;
			const auto id = SpawnToken({.MyDefinition = std::move(definition), .MyPosition = *tile, .MyPlayerId = player.MyPlayerId,
				.MyFacing = player.MyMirrorDeployment ? Facing::LEFT : Facing::RIGHT});
			if (!id) continue;
			auto& unit = _MyUnits[Index(id)];
			if (state.MyExPower)
			{
				std::vector<AttributeChange> modifiers; modifiers.reserve(2);
				if (p.MySummonAttackMultiplier >= 0 && p.MySummonAttackMultiplier != 1) modifiers.push_back({Attribute::ATTACK_MULTIPLIER, p.MySummonAttackMultiplier});
				const auto multiplier = std::max(0.0, 1 - p.MySummonDamageResistance);
				if (multiplier != 1) modifiers.push_back({Attribute::DAMAGE_TAKEN_MULTIPLIER, multiplier});
				if (!modifiers.empty()) (void)AddBuff(id, {.MyKey = "bond:yanyou", .MyModifiers = std::move(modifiers), .MyPersistent = true, .MyAllowDead = true});
			}
			_MyYanyous[unit.MyYanyouIndex].MyHoverOffset = created == 0 ? 0 : created % 2 ? 0.8 : -0.8;
			unit.MyHealth = unit.MyStats.MyMaxHealth; ++created;
		}
	}
}
