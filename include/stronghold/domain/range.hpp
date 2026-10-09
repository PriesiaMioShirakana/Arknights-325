#ifndef STRONGHOLD_DOMAIN_RANGE_HPP
#define STRONGHOLD_DOMAIN_RANGE_HPP

namespace Stronghold
{
	// 相对格坐标，以朝右为基准；准备部署与战斗范围共用，避免两套旋转规则产生偏差。
	struct RangeOffset
	{
		int MyRow{};
		int MyColumn{};
	};
}
#endif
