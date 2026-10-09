#ifndef STRONGHOLD_DOMAIN_BOND_LEDGER_HPP
#define STRONGHOLD_DOMAIN_BOND_LEDGER_HPP
#include <functional>
#include <stdexcept>
#include <stronghold/domain/bonds.hpp>

namespace Stronghold
{
	struct BondLayerProgress
	{
		std::string_view MyId{};
		double MyLayers{};
		double MyCredited{}; // 当前普通战斗已经处理过的累计收益，不随层数封顶而停止推进。
	};

	struct BondLayerChange { std::string_view MyId{}; double MyBefore{}; double MyAfter{}; };

	// 每位玩家一个持久层数账本；规则字符串须覆盖其生命周期。宿主只将有权发放层数的普通战斗送入这里。
	class BondLayerLedger final
	{
	public:
		explicit BondLayerLedger(std::span<const BondRule> _rules)
		{
			_MyEntries.reserve(_rules.size());
			for (const auto& rule : _rules)
			{
				if (rule.MyId.empty() || std::ranges::find(_MyEntries, rule.MyId, &BondLayerProgress::MyId) != _MyEntries.end()) throw std::invalid_argument("invalid bond ledger rules");
				_MyEntries.emplace_back(BondLayerProgress{.MyId = rule.MyId});
			}
		}

		void BeginRound(unsigned _round)
		{
			if (_round == 0 || _round <= _MyRound) throw std::invalid_argument("stale bond ledger round");
			_MyRound = _round;
			for (auto& entry : _MyEntries) entry.MyCredited = 0;
		}

		// 导入持久快照/调试值；不会产生收益或清除本回合的去重水位。
		bool SetLayers(std::string_view _id, double _layers)
		{
			if (!std::isfinite(_layers) || std::isless(_layers, 0)) throw std::invalid_argument("invalid persistent layers");
			const auto found = std::ranges::find(_MyEntries, _id, &BondLayerProgress::MyId);
			if (found == _MyEntries.end()) return false;
			found->MyLayers = _layers; return true;
		}

		[[nodiscard]] std::optional<BondLayerChange> Add(std::string_view _id, double _amount)
		{
			if (!std::isfinite(_amount) || !std::isgreater(_amount, 0)) return {};
			const auto found = std::ranges::find(_MyEntries, _id, &BondLayerProgress::MyId);
			if (found == _MyEntries.end()) return {};
			const auto add = LayerGainRoom(found->MyLayers, std::floor(_amount));
			if (!std::isgreater(add, 0)) return {};
			const auto before = found->MyLayers; found->MyLayers += add;
			return BondLayerChange{.MyId = found->MyId, .MyBefore = before, .MyAfter = found->MyLayers};
		}

		// 累计收益可以逐帧同步，也可以在结算时补最后一段；旧包和重复包不再增加层数。
		// 水位先写入，再同步调用效果。效果可重入 Synchronize；条目数量固定，不持有可失效的迭代器。
		template <class _OnChange>
		bool Synchronize(unsigned _round, std::span<const BondNumber> _gains, _OnChange&& _onChange)
		{
			if (!_round || _round != _MyRound) throw std::invalid_argument("stale layer gain report");
			bool changed = false;
			for (const auto& gain : _gains)
			{
				if (!std::isfinite(gain.MyValue) || !std::isgreater(gain.MyValue, 0)) continue;
				const auto found = std::ranges::find(_MyEntries, gain.MyId, &BondLayerProgress::MyId);
				if (found == _MyEntries.end()) continue;
				const auto total = std::floor(gain.MyValue);
				if (!std::isgreater(total, found->MyCredited)) continue;
				const auto delta = total - found->MyCredited; found->MyCredited = total;
				if (const auto change = Add(gain.MyId, delta)) { changed = true; std::invoke(_onChange, *change); }
			}
			return changed;
		}

		bool Synchronize(unsigned _round, std::span<const BondNumber> _gains)
		{ return Synchronize(_round, _gains, [](const BondLayerChange&) { }); }
		[[nodiscard]] std::span<const BondLayerProgress> Entries() const noexcept { return _MyEntries; }

	private:
		std::vector<BondLayerProgress> _MyEntries;
		unsigned _MyRound{};
	};
}
#endif
