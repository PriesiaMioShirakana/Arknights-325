#include <stronghold/simulation/boss_battle_group.hpp>

namespace Stronghold
{
	BossBattleGroup::BossBattleGroup(FinalAssault& _assault, std::vector<BattleInput> _fields)
		: _MyAssault(_assault)
	{
		if (_fields.size() != _assault.Fields().size()) throw std::invalid_argument("boss field count does not match pairings");
		_MyFields.reserve(_fields.size());
		for (std::size_t i = 0; i < _fields.size(); ++i)
		{
			auto& input = _fields[i]; const auto& pairing = _assault.Fields()[i];
			if (input.MyPlayers.size() != pairing.MyCount) throw std::invalid_argument("boss field player count mismatch");
			for (std::size_t j = 0; j < input.MyPlayers.size(); ++j)
				if (input.MyPlayers[j].MyPlayerId != pairing.MyPlayers[j]) throw std::invalid_argument("boss field seating mismatch");
			input.MyBossBattle = true; input.MyTimeLimit = std::numeric_limits<double>::infinity();
			input.MyLayerGainsEnabled = false;
			input.MySharedBoss = std::ref(_assault.Pool()); input.MyFinalAssault = std::ref(_assault);
			_MyFields.emplace_back(std::move(input));
		}
	}

	void BossBattleGroup::Step()
	{
		if (_MyFinished) return;
		_MyAssault.Observe();
		if (_MyAssault.Outcome() != BossOutcome::ACTIVE) { Finish(BattleEndReason::FORCED); return; }
		for (auto& field : _MyFields)
		{
			field.Step();
			if (_MyAssault.Outcome() != BossOutcome::ACTIVE) break;
		}
		_MyClock.Step();
		_MyAssault.Advance(Time());
		if (_MyAssault.Outcome() != BossOutcome::ACTIVE) Finish(BattleEndReason::FORCED);
		else if (std::ranges::all_of(_MyFields, &Battle::Finished)) Finish(BattleEndReason::FORCED);
		else if (std::isgreaterequal(Time(), 3700)) Finish(BattleEndReason::TIMEOUT); // 原 FieldRunner 的确定性硬上限。
	}

	void BossBattleGroup::Advance(std::uint64_t _ticks)
	{ while (_ticks-- > 0 && !_MyFinished) Step(); }

	void BossBattleGroup::Finish(BattleEndReason _reason)
	{
		if (_MyFinished) return;
		_MyFinished = true;
		_MyAssault.Conclude(); // 在 BATTLE_END 内容钩子前固定结果，迟到伤害不能撤销失败。
		for (auto& field : _MyFields) field.ForceEnd(_reason);
	}

	void BossBattleGroup::ForceEnd() { Finish(BattleEndReason::FORCED); }

	std::vector<BattleResult> BossBattleGroup::Results() const
	{
		if (!_MyFinished) throw std::logic_error("boss fields are still running");
		std::vector<BattleResult> results; results.reserve(_MyFields.size());
		for (const auto& field : _MyFields) results.emplace_back(field.Result());
		return results;
	}
}
