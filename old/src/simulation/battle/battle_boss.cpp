#include "battle_core.hpp"
#include <stronghold/domain/final_assault.hpp>

namespace Stronghold
{
	// 对应 MatchBoss 的 lpLoss / enemyLeak 监听；直接调用保证同一帧多个效果的到达顺序。
	double BattleCore::LoseTeamLife(double _amount)
	{
		if (!_MyStarted || Finished() || !_MyInput.MyFinalAssault) return 0;
		auto& assault = _MyInput.MyFinalAssault->get();
		const double before = assault.TeamLife();
		assault.LoseLife(_amount);
		return before - assault.TeamLife();
	}

	void BattleCore::SyncBossHealth(CombatUnit& _unit)
	{
		const auto& pool = _MyInput.MySharedBoss->get();
		if (std::islessgreater(_unit.MyDefinition.MyStats.MyMaxHealth, pool.MaxHealth()))
		{
			_unit.MyDefinition.MyStats.MyMaxHealth = pool.MaxHealth();
			Recalculate(_unit);
		}
		// 本地显示生命可受 Buff 改变；实际扣血／贡献始终以服务器血池为单位。
		_unit.MyHealth = _unit.MyStats.MyMaxHealth * std::clamp(pool.Health() / pool.MaxHealth(), 0.0, 1.0);
	}

	void BattleCore::SyncBossUnits()
	{
		if (!_MyInput.MySharedBoss) return;
		// 与 JS 顺序一致：普通攻击／弹道和再部署之后、内容 tick 之前同步其它战场造成的伤害。
		for (std::size_t i = 0; i < _MyEnemyIds.size() && !Finished(); ++i)
		{
			auto& unit = _MyUnits[Index(_MyEnemyIds[i])];
			if (!unit.MyAlive || !unit.MyDefinition.MySharedBoss) continue;
			SyncBossHealth(unit);
			if (!std::isgreater(_MyInput.MySharedBoss->get().Health(), 0)) Kill(unit, 0);
		}
	}
}
