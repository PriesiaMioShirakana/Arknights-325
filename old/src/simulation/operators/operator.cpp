#include "battle_core.hpp"
#include <stronghold/simulation/operator.hpp>

namespace Stronghold
{
	bool OperatorBase::Deploy(bool _free, std::optional<WorldPoint> _tile, bool _keepSp)
	{
		return Core().Redeploy(Id(), _free, _tile, _keepSp);
	}

	void OperatorBase::Retreat(bool _permanent, RemovalReason _reason)
	{
		Core().Retreat(Id(), _permanent, _reason);
	}
}
