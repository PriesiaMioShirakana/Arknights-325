#include <limits>
#include <stdexcept>
#include <stronghold/domain/bonds.hpp>

namespace Stronghold
{
	namespace
	{
		double LayerValue(std::span<const BondNumber> _values, std::string_view _id)
		{
			const auto found = std::ranges::find(_values, _id, &BondNumber::MyId);
			return found != _values.end() && std::isfinite(found->MyValue) && std::isgreater(found->MyValue, 0) ? found->MyValue : 0;
		}

		std::int64_t SumCount(std::size_t _count, std::int64_t _bonus)
		{
			if (_count > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) throw std::overflow_error("bond count overflow");
			const auto count = static_cast<std::int64_t>(_count);
			if (_bonus > 0 && count > std::numeric_limits<std::int64_t>::max() - _bonus) throw std::overflow_error("bond count bonus overflow");
			return count + _bonus;
		}
	}

	unsigned BondTier(const BondRule& _rule, std::int64_t _count) noexcept
	{
		if (_rule.MyThresholds.empty()) return 0;
		const double count = static_cast<double>(_count);
		if (_rule.MyDownward) return std::isgreaterequal(count, _rule.MyThresholds.front()) && std::islessequal(count, _rule.MyMaximum.value_or(_rule.MyThresholds.front())) ? 1U : 0U;
		return static_cast<unsigned>(std::ranges::count_if(_rule.MyThresholds, [&](double _threshold) { return std::isgreaterequal(count, _threshold); }));
	}

	double ActivatedLayers(std::span<const BondState> _states) noexcept
	{
		double layers = 0;
		for (const auto& state : _states) if (state.MyActive) layers += state.MyLayers;
		return layers;
	}

	void AddBondGains(std::span<BondState> _states, std::span<const BondNumber> _gains)
	{
		for (const auto& gain : _gains)
			if (const auto found = std::ranges::find(_states, gain.MyId, &BondState::MyId); found != _states.end())
				found->MyLayers += LayerGainRoom(found->MyLayers, std::floor(gain.MyValue));
	}

	BondCalculator::BondCalculator(std::span<const BondRule> _rules)
		: _MyRules(_rules), _MyMembers(_rules.size())
	{
		_MyEnabled.reserve(_rules.size()); _MyDisabled.reserve(_rules.size());
		for (std::size_t i = 0; i < _rules.size(); ++i)
		{
			const auto& rule = _rules[i];
			if (rule.MyId.empty() || std::ranges::find(_rules.first(i), rule.MyId, &BondRule::MyId) != _rules.first(i).end() ||
				rule.MyThresholds.empty() || !std::ranges::is_sorted(rule.MyThresholds, [](double _left, double _right) { return std::isless(_left, _right); }) ||
				std::ranges::any_of(rule.MyThresholds, [](double _value) { return !std::isfinite(_value); }) ||
				(rule.MyMaximum && !std::isfinite(*rule.MyMaximum)) ||
				(rule.MyCountMode != BondCountMode::BOARD && rule.MyCountMode != BondCountMode::BOARD_AND_HAND && rule.MyCountMode != BondCountMode::BOARD_ALL_ELITES))
				throw std::invalid_argument("invalid bond rules");
			_MyMembers[i].MyBoard.reserve(36); _MyMembers[i].MyAll.reserve(46); _MyMembers[i].MyVariants.reserve(36);
		}
	}

	BondComputation BondCalculator::Compute(
		std::span<const BondMember> _board,
		std::span<const BondMember> _hand,
		std::span<const BondNumber> _layers,
		std::span<const BondCountBonus> _bonus,
		std::span<const std::string_view> _inactive
	)
	{
		_MyEnabled.clear(); _MyDisabled.clear();
		for (auto& members : _MyMembers) { members.MyBoard.clear(); members.MyAll.clear(); members.MyVariants.clear(); }
		const auto add = [&](const BondMember& _piece, std::string_view _id, bool _onBoard)
		{
			const auto found = std::ranges::find(_MyRules, _id, &BondRule::MyId);
			if (found == _MyRules.end()) return;
			auto& members = _MyMembers[static_cast<std::size_t>(found - _MyRules.begin())];
			if (std::ranges::find(members.MyAll, _piece.MyBaseId) == members.MyAll.end()) members.MyAll.emplace_back(_piece.MyBaseId);
			if (!_onBoard) return;
			if (std::ranges::find(members.MyBoard, _piece.MyBaseId) == members.MyBoard.end()) members.MyBoard.emplace_back(_piece.MyBaseId);
			const Variant variant{.MyBase = _piece.MyBaseId, .MyGolden = _piece.MyGolden};
			if (std::ranges::find(members.MyVariants, variant) == members.MyVariants.end()) members.MyVariants.emplace_back(variant);
		};
		const auto scan = [&](std::span<const BondMember> _pieces, bool _onBoard)
		{
			for (const auto& piece : _pieces)
			{
				for (const auto id : piece.MyBonds) add(piece, id, _onBoard);
				if (piece.MyItems.size() >= 2 && std::ranges::any_of(piece.MyItems, &BondItem::MyCanGiveBond))
					for (const auto& item : piece.MyItems) if (!item.MyCanGiveBond) add(piece, item.MyGrantedBond, _onBoard);
			}
		};
		scan(_board, true); scan(_hand, false); // 临时区从不参与计数，接口有意不接受它。
		const auto elites = static_cast<std::size_t>(std::ranges::count_if(_board, &BondMember::MyGolden));
		bool harmony = false;
		for (std::size_t i = 0; i < _MyRules.size(); ++i)
		{
			const auto& rule = _MyRules[i]; auto& members = _MyMembers[i];
			const auto bonus = std::ranges::find(_bonus, rule.MyId, &BondCountBonus::MyId);
			members.MyBonus = bonus == _bonus.end() ? 0 : bonus->MyValue;
			const auto count = rule.MyCountMode == BondCountMode::BOARD_ALL_ELITES ? elites : rule.MyCountMode == BondCountMode::BOARD_AND_HAND ? members.MyAll.size() : members.MyBoard.size();
			members.MyRaw = std::max<std::int64_t>(0, SumCount(count, members.MyBonus));
			members.MyOff = std::ranges::find(_inactive, rule.MyId) != _inactive.end();
			if (rule.MyId == "maniShip") harmony = !members.MyOff && BondTier(rule, members.MyRaw) >= 1;
		}
		for (std::size_t i = 0; i < _MyRules.size(); ++i)
		{
			const auto& rule = _MyRules[i]; const auto& members = _MyMembers[i];
			const auto layers = LayerValue(_layers, rule.MyId);
			if (members.MyOff)
			{
				if (members.MyRaw > 0) _MyDisabled.emplace_back(BondState{.MyId = rule.MyId, .MyCount = members.MyRaw, .MyLayers = layers, .MyOff = true});
				continue;
			}
			const bool plus = harmony && rule.MyCore && !members.MyBoard.empty();
			if (plus && members.MyRaw == std::numeric_limits<std::int64_t>::max()) throw std::overflow_error("harmony count overflow");
			const auto count = members.MyRaw + (plus ? 1 : 0);
			auto tier = BondTier(rule, count);
			if (rule.MyId == "deputShip" && tier >= 1)
			{
				const auto variants = static_cast<double>(SumCount(members.MyVariants.size(), members.MyBonus));
				tier = 1;
				for (std::size_t j = 1; j < rule.MyThresholds.size(); ++j)
					if (std::isgreaterequal(variants, rule.MyThresholds[j])) tier = static_cast<unsigned>(j + 1);
			}
			_MyEnabled.emplace_back(BondState{.MyId = rule.MyId, .MyCount = count, .MyActive = tier >= 1, .MyTier = tier, .MyLayers = layers, .MyHarmony = plus});
		}
		return BondComputation{.MyEnabled = _MyEnabled, .MyDisabled = _MyDisabled};
	}

	std::vector<BondState> BondCalculator::View(std::span<const BondNumber> _gains) const
	{
		std::vector<BondState> result; result.reserve(_MyRules.size());
		result.assign(_MyEnabled.begin(), _MyEnabled.end());
		AddBondGains(result, _gains);
		std::erase_if(result, [](const BondState& _state) { return _state.MyCount <= 0 && !std::isgreater(_state.MyLayers, 0) && !_state.MyActive; });
		std::stable_sort(result.begin(), result.end(), [](const BondState& _left, const BondState& _right)
		{
			if (_left.MyActive != _right.MyActive) return _left.MyActive;
			return std::isgreater(_left.MyLayers, _right.MyLayers);
		});
		result.insert(result.end(), _MyDisabled.begin(), _MyDisabled.end());
		return result;
	}
}
