#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <stronghold/simulation/field.hpp>

namespace Stronghold
{
	namespace
	{
		constexpr std::size_t CacheCapacity = 64;
		constexpr std::array Directions{FieldPoint{.MyRow = 1}, FieldPoint{.MyColumn = 1}, FieldPoint{.MyRow = -1}, FieldPoint{.MyColumn = -1}};

		template <class _Clear>
		bool Ray(int _from, int _to, bool _diagonal, _Clear _clear)
		{
			int row = _from / FieldColumns, column = _from % FieldColumns;
			const int endRow = _to / FieldColumns, endColumn = _to % FieldColumns;
			if (!_diagonal && row != endRow && column != endColumn) return false;
			const int dr = std::abs(endRow - row), dc = std::abs(endColumn - column);
			const int sr = endRow > row ? 1 : -1, sc = endColumn > column ? 1 : -1;
			int error = dc - dr;
			if (!_clear(row, column)) return false;
			while (row != endRow || column != endColumn)
			{
				const int twice = 2 * error;
				int nextRow = row, nextColumn = column;
				if (twice > -dr) { error -= dr; nextColumn += sc; }
				if (twice < dc) { error += dc; nextRow += sr; }
				if (nextRow != row && nextColumn != column && (!_clear(nextRow, column) || !_clear(row, nextColumn))) return false;
				row = nextRow; column = nextColumn;
				if (!_clear(row, column)) return false;
			}
			return true;
		}

		// 整数交点遍历只统计穿过内部的格子，恰好擦过角点的邻格不算路径惩罚。
		int CrossPenalty(int _from, int _to, const std::array<int, FieldTiles>& _penalty)
		{
			int row = _from / FieldColumns, column = _from % FieldColumns;
			const int endRow = _to / FieldColumns, endColumn = _to % FieldColumns;
			const int dy = std::abs(endRow - row), dx = std::abs(endColumn - column);
			const int sr = endRow > row ? 1 : -1, sc = endColumn > column ? 1 : -1;
			int count = 0, kx = 1, ky = 1;
			while (row != endRow || column != endColumn)
			{
				const int tx = kx <= dx ? (2 * kx - 1) * dy : std::numeric_limits<int>::max();
				const int ty = ky <= dy ? (2 * ky - 1) * dx : std::numeric_limits<int>::max();
				if (tx == ty) { column += sc; row += sr; ++kx; ++ky; }
				else if (tx < ty) { column += sc; ++kx; }
				else { row += sr; ++ky; }
				count += _penalty[static_cast<std::size_t>(FieldGrid::Key(row, column))];
			}
			return count;
		}

		bool OnSegment(int _from, int _middle, int _to)
		{
			const int ar = _from / FieldColumns, ac = _from % FieldColumns;
			const int br = _middle / FieldColumns, bc = _middle % FieldColumns;
			const int cr = _to / FieldColumns, cc = _to % FieldColumns;
			return (br - ar) * (cc - ac) == (bc - ac) * (cr - ar) && (br - ar) * (cr - br) + (bc - ac) * (cc - bc) > 0;
		}
	}

	FieldGrid::FieldGrid(FieldDefinition _definition) : _MyDefinition(std::move(_definition)), _MyObstacles(_MyDefinition.MyInitialObstacles)
	{
		for (const auto obstacle : _MyObstacles)
			if (obstacle > 3) throw std::invalid_argument("invalid initial obstacle");
		const auto& rules = _MyDefinition.MyTerrainRules;
		for (const auto value : {rules.MyMireInterval, rules.MyMireAttackSpeed, rules.MyMireMovePerStack, rules.MyMireHeavyMass,
			rules.MyDeepseaDamage, rules.MyDeepseaAttackSpeed, rules.MyDeepseaMoveMultiplier, rules.MyInfectionDamage,
			rules.MyInfectionAttackPercent, rules.MyInfectionAttackSpeed, rules.MyInfectionDuration})
			if (!std::isfinite(value)) throw std::invalid_argument("non-finite terrain rule");
		if (!std::isgreater(rules.MyMireInterval, 0) || !rules.MyMireMaxStacks || !std::isgreater(rules.MyInfectionDuration, 0) ||
			std::isless(rules.MyDeepseaDamage, 0) || std::isless(rules.MyInfectionDamage, 0)) throw std::invalid_argument("invalid terrain rule");
		for (const auto& tile : _MyDefinition.MyTiles)
			if (static_cast<unsigned>(tile.MyTerrain) > static_cast<unsigned>(FieldTerrain::INFECTION)) throw std::invalid_argument("invalid terrain kind");
		for (const auto& flow : _MyDefinition.MyAirflow)
			if (flow)
			{
				if (flow->MyX < -1 || flow->MyX > 1 || flow->MyY < -1 || flow->MyY > 1 ||
					std::abs(flow->MyX) + std::abs(flow->MyY) != 1) throw std::invalid_argument("invalid airflow direction");
				for (const auto value : {flow->MyAllyEqualAttack, flow->MyAllyOppositeAttack, flow->MyAllyVerticalAttack, flow->MyEnemyEqualSpeed, flow->MyEnemyOppositeSpeed})
					if (!std::isfinite(value)) throw std::invalid_argument("invalid airflow parameter");
			}
		const auto& rect = _MyDefinition.MyRect;
		if (!InBounds(rect.MyFirstRow, rect.MyFirstColumn) || !InBounds(rect.MyLastRow, rect.MyLastColumn) ||
			rect.MyFirstRow > rect.MyLastRow || rect.MyFirstColumn > rect.MyLastColumn) throw std::invalid_argument("invalid field bounds");
		for (int key = 0; key < FieldTiles; ++key)
			_MyUnblockable[static_cast<std::size_t>(key)] = Tile(key / FieldColumns, key % FieldColumns).MyWalkable && !Blockable(key / FieldColumns, key % FieldColumns) ? 1 : 0;
		_MyFields.reserve(CacheCapacity);
	}

	bool FieldGrid::InRect(int _row, int _column) const noexcept
	{
		const auto& rect = _MyDefinition.MyRect;
		return _row >= rect.MyFirstRow && _row <= rect.MyLastRow && _column >= rect.MyFirstColumn && _column <= rect.MyLastColumn;
	}

	FieldTile FieldGrid::Tile(int _row, int _column) const noexcept
	{
		return InBounds(_row, _column) ? _MyDefinition.MyTiles[static_cast<std::size_t>(Key(_row, _column))]
			: FieldTile{.MyWalkable = false, .MyFlyable = false, .MyLow = false, .MyBuild = FieldBuild::NONE};
	}

	bool FieldGrid::Obstacle(int _row, int _column, ObstacleKind _kind) const noexcept
	{ return InBounds(_row, _column) && (_MyObstacles[static_cast<std::size_t>(Key(_row, _column))] & static_cast<unsigned>(_kind)) != 0; }

	bool FieldGrid::GroundPassable(int _row, int _column, bool _ignoreObstacles) const noexcept
	{ return InRect(_row, _column) && Tile(_row, _column).MyWalkable && (_ignoreObstacles || _MyObstacles[static_cast<std::size_t>(Key(_row, _column))] == 0); }

	bool FieldGrid::Walkable(int _row, int _column, bool _ignoreObstacles) const noexcept
	{ return InRect(_row, _column) && Tile(_row, _column).MyWalkable && (_ignoreObstacles || !Obstacle(_row, _column, ObstacleKind::BLOCK)); }

	bool FieldGrid::FlyPassable(int _row, int _column) const noexcept
	{ return InRect(_row, _column) && Tile(_row, _column).MyFlyable; }

	bool FieldGrid::Blockable(int _row, int _column) const noexcept
	{
		const auto tile = Tile(_row, _column);
		return tile.MyLow && (tile.MyBuild == FieldBuild::ALL || tile.MyBuild == FieldBuild::MELEE);
	}

	bool FieldGrid::CanStand(int _row, int _column, bool _ranged) const noexcept
	{
		const auto tile = Tile(_row, _column);
		return _ranged ? tile.MyBuild == FieldBuild::ALL || tile.MyBuild == FieldBuild::RANGED || (tile.MyLow && tile.MyBuild == FieldBuild::MELEE)
			: !Obstacle(_row, _column, ObstacleKind::BLOCK) && Blockable(_row, _column);
	}

	void FieldGrid::SetObstacle(int _row, int _column, bool _enabled, ObstacleKind _kind)
	{
		if (_kind != ObstacleKind::BLOCK && _kind != ObstacleKind::CRATE) throw std::invalid_argument("invalid obstacle kind");
		if (!InBounds(_row, _column)) return;
		auto& bits = _MyObstacles[static_cast<std::size_t>(Key(_row, _column))];
		const auto value = static_cast<unsigned char>(_enabled ? bits | static_cast<unsigned>(_kind) : bits & ~static_cast<unsigned>(_kind));
		if (bits == value) return;
		bits = value;
		++_MyVersion;
		_MyFields.clear();
		_MyReplacement = 0;
	}

	const FlowField& FieldGrid::Flow(int _row, int _column, bool _allowDiagonal, bool _ignoreObstacles)
	{
		const auto key = InBounds(_row, _column) ? Key(_row, _column) : -1;
		for (const auto& field : _MyFields)
			if (field.MyDestination == key && field.MyAllowDiagonal == _allowDiagonal && field.MyIgnoreObstacles == _ignoreObstacles) return field;
		auto field = Build(_row, _column, _allowDiagonal, _ignoreObstacles);
		if (_MyFields.size() < CacheCapacity) return _MyFields.emplace_back(std::move(field));
		const auto slot = _MyReplacement;
		_MyReplacement = (_MyReplacement + 1) % CacheCapacity;
		return _MyFields[slot] = std::move(field);
	}

	void FieldGrid::Spfa(FlowField& _field, bool _preferBlockable) const
	{
		_field.MyDistance.fill(-1);
		_field.MyParent.fill(-1);
		_field.MyPenalty.fill(0);
		std::array<int, FieldTiles> queue{};
		std::array<bool, FieldTiles> queued{};
		std::size_t head = 0, tail = 1, count = 1;
		queue[0] = _field.MyDestination;
		queued[static_cast<std::size_t>(_field.MyDestination)] = true;
		_field.MyDistance[static_cast<std::size_t>(_field.MyDestination)] = 0;
		while (count)
		{
			const auto current = queue[head];
			head = (head + 1) % queue.size();
			--count;
			queued[static_cast<std::size_t>(current)] = false;
			for (const auto direction : Directions)
			{
				const auto row = current / FieldColumns + direction.MyRow, column = current % FieldColumns + direction.MyColumn;
				if (!Walkable(row, column, _field.MyIgnoreObstacles)) continue;
				const auto next = static_cast<std::size_t>(Key(row, column));
				const auto distance = _field.MyDistance[static_cast<std::size_t>(current)] + (!_field.MyIgnoreObstacles && Obstacle(row, column, ObstacleKind::CRATE) ? 1000 : 1);
				const auto penalty = _preferBlockable ? _field.MyPenalty[static_cast<std::size_t>(current)] + _MyUnblockable[next] : 0;
				if (_field.MyDistance[next] < 0 || distance < _field.MyDistance[next] || (distance == _field.MyDistance[next] && penalty < _field.MyPenalty[next]))
				{
					_field.MyDistance[next] = distance;
					_field.MyPenalty[next] = penalty;
					_field.MyParent[next] = current;
					if (!queued[next]) { queue[tail] = static_cast<int>(next); tail = (tail + 1) % queue.size(); ++count; queued[next] = true; }
				}
			}
		}
	}

	FlowField FieldGrid::Build(int _row, int _column, bool _allowDiagonal, bool _ignoreObstacles) const
	{
		FlowField field{.MyDestination = InBounds(_row, _column) ? Key(_row, _column) : -1,
			.MyVersion = _MyVersion, .MyAllowDiagonal = _allowDiagonal, .MyIgnoreObstacles = _ignoreObstacles};
		field.MyDistance.fill(-1); field.MyParent.fill(-1); field.MyNext.fill(-1); field.MyOfficial.fill(-1); field.MyLength.fill(-1);
		if (field.MyDestination < 0) return field;
		Spfa(field, false);
		auto official = field.MyParent;
		Spfa(field, true);
		auto preference = field.MyParent;
		const auto clear = [&](int _r, int _c) { return Walkable(_r, _c, _ignoreObstacles) && (_ignoreObstacles || !Obstacle(_r, _c, ObstacleKind::CRATE)); };
		std::array<int, FieldTiles> onChain;
		onChain.fill(-1);
		// 必须按行优先原地更新，后续格能看到先前格已平滑的父节点。
		for (int key = 0; key < FieldTiles; ++key)
		{
			const auto n = static_cast<std::size_t>(key);
			if (field.MyDistance[n] < 0 || official[n] < 0) continue;
			auto cursor = official[n];
			while (official[static_cast<std::size_t>(cursor)] >= 0 && Ray(key, official[static_cast<std::size_t>(cursor)], _allowDiagonal, clear)) cursor = official[static_cast<std::size_t>(cursor)];
			official[n] = cursor;
			for (auto chain = key; chain >= 0; chain = field.MyParent[static_cast<std::size_t>(chain)]) onChain[static_cast<std::size_t>(chain)] = key;
			cursor = preference[n];
			while (preference[static_cast<std::size_t>(cursor)] >= 0)
			{
				const auto to = preference[static_cast<std::size_t>(cursor)];
				if (!Ray(key, to, _allowDiagonal, [&](int _r, int _c)
				{
					const auto k = static_cast<std::size_t>(Key(_r, _c));
					return clear(_r, _c) && (!_MyUnblockable[k] || (onChain[k] == key && field.MyDistance[k] >= field.MyDistance[static_cast<std::size_t>(to)]));
				})) break;
				cursor = to;
			}
			preference[n] = cursor;
		}
		field.MyOfficial = official;
		std::array<int, FieldTiles> order;
		std::iota(order.begin(), order.end(), 0);
		std::ranges::sort(order, [&](int _a, int _b)
		{
			const auto a = field.MyDistance[static_cast<std::size_t>(_a)], b = field.MyDistance[static_cast<std::size_t>(_b)];
			return a < b || (a == b && _a < _b);
		});
		for (const auto key : order)
		{
			const auto n = static_cast<std::size_t>(key);
			const auto o = official[n], p = preference[n];
			if (o < 0) continue;
			auto best = CrossPenalty(key, o, _MyUnblockable) + field.MyCost[static_cast<std::size_t>(o)];
			auto use = o;
			if (p >= 0 && p != o)
			{
				const auto penalty = CrossPenalty(key, p, _MyUnblockable) + field.MyCost[static_cast<std::size_t>(p)];
				if (penalty < best || (penalty == best && field.MyNext[static_cast<std::size_t>(o)] == p && OnSegment(key, o, p))) { use = p; best = penalty; }
			}
			field.MyNext[n] = use;
			field.MyCost[n] = best;
		}
		return field;
	}

	double FieldGrid::Length(const FlowField& _field, int _key)
	{
		if (_key < 0 || _key >= FieldTiles || _field.MyDistance[static_cast<std::size_t>(_key)] < 0) return std::numeric_limits<double>::infinity();
		std::array<int, FieldTiles> stack;
		std::size_t size = 0;
		auto key = _key;
		while (key >= 0 && std::isless(_field.MyLength[static_cast<std::size_t>(key)], 0))
		{
			if (key == _field.MyDestination || _field.MyNext[static_cast<std::size_t>(key)] < 0) { _field.MyLength[static_cast<std::size_t>(key)] = 0; break; }
			if (size == stack.size()) return std::numeric_limits<double>::infinity();
			stack[size++] = key;
			key = _field.MyNext[static_cast<std::size_t>(key)];
		}
		while (size)
		{
			const auto a = stack[--size], b = _field.MyNext[static_cast<std::size_t>(a)];
			_field.MyLength[static_cast<std::size_t>(a)] = _field.MyLength[static_cast<std::size_t>(b)] + std::hypot(b / FieldColumns - a / FieldColumns, b % FieldColumns - a % FieldColumns);
		}
		return _field.MyLength[static_cast<std::size_t>(_key)];
	}

	bool FieldGrid::Waypoints(FieldPoint _start, FieldPoint _end, std::vector<FieldPoint>& _output, bool _allowDiagonal, bool _ignoreObstacles)
	{
		_output.clear();
		if (!InBounds(_start.MyRow, _start.MyColumn) || !InBounds(_end.MyRow, _end.MyColumn)) return false;
		const auto& field = Flow(_end.MyRow, _end.MyColumn, _allowDiagonal, _ignoreObstacles);
		auto key = Key(_start.MyRow, _start.MyColumn);
		if (field.MyDistance[static_cast<std::size_t>(key)] < 0) return false;
		_output.reserve(FieldTiles);
		_output.emplace_back(_start);
		while (key != field.MyDestination && _output.size() <= FieldTiles)
		{
			key = field.MyNext[static_cast<std::size_t>(key)];
			if (key < 0) { _output.clear(); return false; }
			_output.emplace_back(key / FieldColumns, key % FieldColumns);
		}
		if (key != field.MyDestination) { _output.clear(); return false; }
		return true;
	}

	bool FieldGrid::StraightClear(double _x, double _y, FieldPoint _end) const
	{
		if (!std::isfinite(_x) || !std::isfinite(_y) || !std::isgreaterequal(_x, 0) || !std::isless(_x, FieldColumns) ||
			!std::isgreaterequal(_y, 0) || !std::isless(_y, FieldRows) || !InBounds(_end.MyRow, _end.MyColumn)) return false;
		int column = static_cast<int>(std::floor(_x + 0.5)), row = static_cast<int>(std::floor(_y + 0.5));
		const auto dx = _end.MyColumn - _x, dy = _end.MyRow - _y;
		const int sc = std::isgreater(dx, 0) ? 1 : -1, sr = std::isgreater(dy, 0) ? 1 : -1;
		const auto infinity = std::numeric_limits<double>::infinity();
		const auto ddx = std::islessgreater(dx, 0) ? std::abs(1 / dx) : infinity, ddy = std::islessgreater(dy, 0) ? std::abs(1 / dy) : infinity;
		auto tx = std::islessgreater(dx, 0) ? (column + 0.5 * sc - _x) / dx : infinity;
		auto ty = std::islessgreater(dy, 0) ? (row + 0.5 * sr - _y) / dy : infinity;
		const auto clear = [&](int _r, int _c) { return (_r == _end.MyRow && _c == _end.MyColumn) || (Walkable(_r, _c) && !Obstacle(_r, _c, ObstacleKind::CRATE)); };
		for (int guard = 4 * FieldColumns; guard > 0 && (row != _end.MyRow || column != _end.MyColumn); --guard)
		{
			if (std::isgreaterequal(tx, 1) && std::isgreaterequal(ty, 1)) break;
			if (std::isless(std::abs(tx - ty), 1e-9))
			{
				if (!clear(row, column + sc) || !clear(row + sr, column)) return false;
				column += sc; row += sr; tx += ddx; ty += ddy;
			}
			else if (std::isless(tx, ty)) { column += sc; tx += ddx; }
			else { row += sr; ty += ddy; }
			if (!clear(row, column)) return false;
		}
		return true;
	}
}
