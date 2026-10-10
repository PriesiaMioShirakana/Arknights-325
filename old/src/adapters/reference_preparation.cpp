#include <stronghold/adapters/reference_preparation.hpp>

namespace Stronghold
{
	PreparationBondCalculator::PreparationBondCalculator()
		: _MyCalculator(ReferenceBondRules())
	{
		_MyBoard.reserve(36); _MyHand.reserve(10); _MyEquipment.reserve(92);
	}

	BondComputation PreparationBondCalculator::Compute(
		const PlayerView& _player,
		std::span<const BondNumber> _layers,
		std::span<const BondCountBonus> _bonus,
		std::span<const std::string_view> _inactive,
		std::span<const BondRosterRecord> _rosterOverrides
	)
	{
		_MyBoard.clear(); _MyHand.clear(); _MyEquipment.clear();
		_MyHand.reserve(_player.MyHand.size());
		std::size_t total = 0;
		for (const auto& piece : _player.MyBoard) if (piece) total += piece->MyItems.size();
		for (const auto& piece : _player.MyHand) if (piece) total += piece->MyItems.size();
		// 先统计真实装备数量再生成 span，避免后续增长使已经生成的成员输入悬空。
		_MyEquipment.reserve(total);
		const auto roster = ReferenceBondRoster(); const auto items = ReferenceBondItems();
		const auto append = [&](const Piece& _piece, std::vector<BondMember>& _destination)
		{
			const auto custom = std::ranges::find(_rosterOverrides, _piece.MyId, &BondRosterRecord::MyId);
			const auto builtin = std::ranges::lower_bound(roster, _piece.MyId, {}, &BondRosterRecord::MyId);
			if (custom == _rosterOverrides.end() && (builtin == roster.end() || builtin->MyId != _piece.MyId)) return;
			const auto& record = custom != _rosterOverrides.end() ? *custom : *builtin;
			const auto first = _MyEquipment.size();
			for (const auto& item : _piece.MyItems)
			{
				const auto found = std::ranges::lower_bound(items, item.MyId, {}, &BondItemRecord::MyId);
				_MyEquipment.emplace_back(found != items.end() && found->MyId == item.MyId ? found->MyItem : BondItem{});
			}
			_destination.emplace_back(BondMember{.MyBaseId = record.MyBaseId, .MyGolden = record.MyGolden, .MyBonds = record.MyBonds,
				.MyItems = std::span<const BondItem>(_MyEquipment).subspan(first, _piece.MyItems.size())});
		};
		for (const auto& piece : _player.MyBoard) if (piece) append(*piece, _MyBoard);
		for (const auto& piece : _player.MyHand) if (piece) append(*piece, _MyHand);
		return _MyCalculator.Compute(_MyBoard, _MyHand, _layers, _bonus, _inactive);
	}
}
