#ifndef STRONGHOLD_ADAPTERS_REFERENCE_PREPARATION_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_PREPARATION_HPP
#include <stronghold/adapters/reference_bonds.hpp>
#include <stronghold/domain/preparation.hpp>

namespace Stronghold
{
	// 将经济库存投影为盟约输入。每个宿主可复用一个实例，查询结果在下一次 Compute 前有效。
	// 自选/自定义干员通过 _rosterOverrides 提供本局解析后的成员关系；覆盖表不需要排序。
	class PreparationBondCalculator final
	{
	public:
		PreparationBondCalculator();
		[[nodiscard]] BondComputation Compute(
			const PlayerView& _player,
			std::span<const BondNumber> _layers = {},
			std::span<const BondCountBonus> _bonus = {},
			std::span<const std::string_view> _inactive = {},
			std::span<const BondRosterRecord> _rosterOverrides = {}
		);
		[[nodiscard]] std::vector<BondState> View(std::span<const BondNumber> _gains = {}) const { return _MyCalculator.View(_gains); }

	private:
		BondCalculator _MyCalculator;
		std::vector<BondMember> _MyBoard;
		std::vector<BondMember> _MyHand;
		std::vector<BondItem> _MyEquipment;
	};
}
#endif
