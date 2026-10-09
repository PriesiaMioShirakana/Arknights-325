#include <iomanip>
#include <iostream>
#include <stronghold/domain/bond_ledger.hpp>
#include <stronghold/simulation/battle.hpp>

namespace
{
	using namespace Stronghold;
	struct LayerHook final : CustomBond<LayerHook>
	{
		void OnLayerGain(Battle& _battle, ContentEvent& _event)
		{
			if (_event.MyPlayer != _event.MyHandlerOwner) return;
			if (_event.MyReason == "double") _event.MyAmount *= 2;
			if (_event.MyReason == "cancel") _event.MyAmount = 0;
			if (_event.MyReason == "invalid") _event.MyAmount = std::numeric_limits<double>::quiet_NaN();
			if (_event.MyReason == "nested") { (void)_battle.AddBondLayers("p", "other", 1); _event.MyAmount += 1; }
			if (_event.MyReason == "tile") (void)_battle.AddCoins("p", _event.MySourceTile ? _event.MySourceTile->MyX + 10 * _event.MySourceTile->MyY : 1);
		}
		void OnBattleEnd(Battle& _battle, ContentEvent&) { (void)_battle.AddBondLayers("p", "known", 2); }
	};

	void Snapshot(Battle& _battle, double _value)
	{
		const auto& player = _battle.Players()[0];
		std::cout << '[' << _value << ',' << _battle.BondLayers("p", "known") << ',' << player.MyCoins << ",[";
		for (std::size_t i = 0; i < player.MyLayerGains.size(); ++i) { if (i) std::cout << ','; std::cout << '[' << std::quoted(player.MyLayerGains[i].MyId) << ',' << player.MyLayerGains[i].MyLayers << ']'; }
		std::cout << "],["; bool first = true;
		for (const auto& event : _battle.DrainEvents()) if (event.MyKind == BattleEventKind::LAYERS_GAINED)
		{
			if (!first) std::cout << ',';
			first = false;
			std::cout << '[' << std::quoted(_battle.Players()[event.MyPlayer].MyPlayerId) << ',' << std::quoted(_battle.Players()[event.MyPlayer].MyLayerGains[event.MyBondIndex].MyId) << ',' << event.MyAmount << ']';
		}
		std::cout << "]]";
	}
}

int main()
{
	using namespace Stronghold;
	ContentRegistry registry; const auto hook = registry.Register<LayerHook>("layers"); registry.Seal();
	std::cout << std::setprecision(17) << "{\"battles\":[";
	constexpr std::array Initial{0.0, 998.5, 999.0, 1200.0};
	for (unsigned scene = 0; scene < 32; ++scene)
	{
		const bool hooked = scene % 2 != 0; const auto kind = (scene / 2) % 4;
		const CombatDefinition ally{.MyId = "ally", .MyStats = CombatStats{.MyMaxHealth = 100, .MyBlockCount = 0}, .MyAttack = AttackProfile{.MyDisabled = true}};
		BattleInput input{.MyPlayers = {BattlePlayerInput{.MyPlayerId = "p", .MyUnits = {AllyDeployment{.MyPieceUid = 1, .MyDefinition = ally, .MyPosition = {.MyX = 5, .MyY = 3}}},
			.MyBonds = {BondLayer{.MyId = "known", .MyLayers = Initial[scene / 8], .MyCount = 3, .MyActive = true, .MyTier = 1}}}},
			.MyTimeLimit = 100, .MyAutoFinish = false, .MyBossBattle = kind == 1 || kind == 3, .MyContentRegistry = std::cref(registry)};
		if (kind == 2) input.MyLayerGainsEnabled = false;
		if (kind == 3) input.MyLayerGainsEnabled = true;
		if (hooked) input.MyContentBindings.emplace_back(ContentBinding{.MyContent = hook, .MyPlayerId = "p"});
		Battle battle(std::move(input)); battle.Start();
		if (scene) std::cout << ',';
		std::cout << '[';
		bool first = true;
		const auto add = [&](std::string_view _bond, double _amount, std::string_view _reason = "", UnitId _source = 0, std::optional<WorldPoint> _tile = {})
		{
			if (!first) std::cout << ',';
			first = false;
			const auto added = battle.AddBondLayers("p", _bond, _amount, LayerGainOptions{.MySource = _source, .MyTile = _tile, .MyReason = _reason}); Snapshot(battle, added);
		};
		add("known", 0.75); add("absent", 1.5); add("known", 10000, "double"); add("known", 1);
		battle.SetBondLayers("p", "known", 998.5); add("known", 1, "double"); add("absent", 0.5);
		add("known", std::numeric_limits<double>::infinity()); add("known", std::numeric_limits<double>::quiet_NaN()); add("known", -1);
		battle.SetBondLayers("p", "known", 0); add("known", 1, "cancel"); add("known", 1, "invalid"); add("known", 1, "nested");
		add("known", 1, "tile", 1); add("known", 1, "tile", 1, WorldPoint{.MyX = 15, .MyY = 7});
		(void)battle.LoseHealth(0, 1, 10000); add("known", 1, "tile", 1); battle.Step(); add("known", 1, "tile", 1);
		battle.ForceEnd(); add("known", 1); std::cout << ']';
		if (!battle.ContentErrors().empty()) throw std::logic_error("layer hook failed");
	}
	std::cout << "],\"ledger\":[";
	constexpr std::array Thresholds{1.0};
	const std::array rules{BondRule{.MyId = "known", .MyThresholds = Thresholds}, BondRule{.MyId = "other", .MyThresholds = Thresholds}};
	BondLayerLedger ledger(rules); ledger.BeginRound(1);
	constexpr std::array Reports{0.0, 1.5, 1.9, 1.0, 3.5, 999.0, 1002.0, 1002.0, 1005.0};
	for (unsigned round = 1; round <= 3; ++round)
	{
		if (round > 1) ledger.BeginRound(round);
		(void)ledger.SetLayers("known", round == 1 ? 0 : round == 2 ? 998.5 : 0);
		(void)ledger.SetLayers("other", 0);
		for (std::size_t i = 0; i < Reports.size(); ++i)
		{
			const std::array gains{BondNumber{.MyId = "known", .MyValue = Reports[i]}, BondNumber{.MyId = "other", .MyValue = Reports[(i + 3) % Reports.size()]}, BondNumber{.MyId = "unknown", .MyValue = 100}};
			std::vector<BondLayerChange> changes;
			const auto changed = ledger.Synchronize(round, gains, [&](const BondLayerChange& _change)
			{
				changes.emplace_back(_change);
				// Reentrant delivery of the same cumulative value must see the watermark already advanced.
				const std::array repeated{BondNumber{.MyId = _change.MyId, .MyValue = _change.MyId == "known" ? gains[0].MyValue : gains[1].MyValue}};
				if (ledger.Synchronize(round, repeated)) throw std::logic_error("duplicate reentrant layer credit");
			});
			if (round > 1 || i) std::cout << ',';
			std::cout << '[' << changed << ",[";
			for (std::size_t j = 0; j < ledger.Entries().size(); ++j) { if (j) std::cout << ','; const auto& e = ledger.Entries()[j]; std::cout << '[' << std::quoted(e.MyId) << ',' << e.MyLayers << ',' << e.MyCredited << ']'; }
			std::cout << "],[";
			for (std::size_t j = 0; j < changes.size(); ++j) { if (j) std::cout << ','; const auto& c = changes[j]; std::cout << '[' << std::quoted(c.MyId) << ',' << c.MyBefore << ',' << c.MyAfter << ']'; }
			std::cout << "]]";
		}
	}
	std::cout << "]}";
}
