#include <iomanip>
#include <stronghold/domain/preparation_content.hpp>
#include <iostream>
#include <sstream>
#include <stronghold/adapters/reference_bonds.hpp>
#include <stronghold/adapters/reference_catalog.hpp>
#include <stronghold/adapters/reference_choices.hpp>
#include <stronghold/adapters/reference_roster.hpp>

namespace
{
	using namespace Stronghold;
	void PrintPiece(const Piece& _piece)
	{
		std::cout << '[' << _piece.MyUid << ',' << std::quoted(_piece.MyId) << ',' << _piece.MyPoolCopies << ',';
		if (_piece.MyTemporaryDue) std::cout << *_piece.MyTemporaryDue; else std::cout << "null";
		std::cout << ']';
	}

	void Snapshot(const EconomySession& _economy, const RoundLedger& _ledger, const PreparationContent& _rewards, const Random& _random, bool _ok)
	{
		if (!_economy.PoolConservationHolds()) throw std::logic_error("choice reward pool conservation");
		std::cout << '[' << _ok << ',' << _random.State();
		for (const auto& progress : _ledger.Players())
		{
			const auto view = *_economy.View(progress.MyPlayerId);
			const auto reward = *_rewards.View(progress.MyPlayerId);
			std::cout << ",[" << view.MyFunds << ',' << view.MyFreeRefreshes << ',' << reward.MySequence;
			for (const auto& slots : {view.MyHand, view.MyTemporary})
			{
				std::cout << ",[";
				for (std::size_t i = 0; i < slots.size(); ++i) { if (i) std::cout << ','; if (slots[i]) PrintPiece(*slots[i]); else std::cout << "null"; }
				std::cout << ']';
			}
			std::cout << ",{"; bool first = true;
			for (const auto& layer : reward.MyLayers) if (std::isgreater(layer.MyLayers, 0.0)) { if (!first) std::cout << ','; first = false; std::cout << std::quoted(layer.MyId) << ':' << layer.MyLayers; }
			std::cout << "},[";
			for (std::size_t i = 0; i < view.MyPurchaseUpgrades.size(); ++i)
			{
				if (i) std::cout << ',';
				const auto& bonus = view.MyPurchaseUpgrades[i];
				std::cout << '[' << std::quoted(bonus.MySource) << ',' << (bonus.MyKind == PieceKind::ITEM) << ',' << bonus.MyRemaining << ']';
			}
			std::cout << "],[";
			for (std::size_t i = 0; i < reward.MyBattleEffects.size(); ++i)
			{
				if (i) std::cout << ',';
				const auto& effect = reward.MyBattleEffects[i];
				std::cout << '[' << std::quoted(effect.MyId) << ',' << std::quoted(effect.MyRule) << ',' << effect.MyRound << ',';
				if (effect.MyPreparationPassed) std::cout << *effect.MyPreparationPassed; else std::cout << "null";
				std::cout << ','; if (effect.MyBenchCount) std::cout << *effect.MyBenchCount; else std::cout << "null";
				std::cout << ']';
			}
			std::cout << "],{"; first = true;
			for (const auto& setting : reward.MyDevices) { if (!first) std::cout << ','; first = false; std::cout << std::quoted(setting.MyAlias) << ':' << setting.MyActive; }
			std::cout << "},[";
			for (std::size_t i = 0; i < progress.MyBounties.size(); ++i)
			{
				if (i) std::cout << ',';
				const auto& bounty = progress.MyBounties[i];
				std::cout << '[' << std::quoted(bounty.MyCard.MyId) << ',' << std::quoted(bounty.MyCard.MyEnemyId) << ',' << bounty.MyCard.MyCount << ','
					<< bounty.MyCard.MyCoins << ',' << bounty.MyCard.MyPerfect << ',' << bounty.MyRoundsLeft << ']';
			}
			std::cout << "]]";
		}
		std::cout << "]\n";
	}
}

int main(int _argc, char** _argv)
{
	using namespace Stronghold;
	if (_argc != 2) return 2;
	const auto seed = static_cast<std::uint32_t>(std::stoul(_argv[1]));
	const auto catalog = ReferenceCatalog();
	const std::array seats{Seat{.MySeat = 0, .MyPlayerId = "p"}, Seat{.MySeat = 1, .MyPlayerId = "q"}, Seat{.MySeat = 2, .MyPlayerId = "r"}};
	EconomySession economy(catalog, "mode_multi_normal", seats, seed);
	PlayerRoster roster;
	const std::array picks{DiyPick{.MySlot = "chess_char_5_diy1_a", .MyCharacter = "char_2013_cerber", .MySkill = 0, .MyModule = "none"}};
	const std::array<std::string_view, 1> kits{"char_2013_cerber"};
	if (!roster.SetDiy(picks, kits) || !ConfigurePreparationRoster(economy, "p", roster)) return 3;
	const auto members = roster.MakeContentPoolRoster();
	const auto defaults = PlayerRoster{}.MakeContentPoolRoster();
	const std::array players{ChoiceRewardPlayerConfig{.MyPlayerId = "p", .MyRoster = members},
		ChoiceRewardPlayerConfig{.MyPlayerId = "q", .MyRoster = defaults}, ChoiceRewardPlayerConfig{.MyPlayerId = "r", .MyRoster = defaults}};
	RoundLedger ledger({}, {MatchPlayerProgress{.MyPlayerId = "p"}, MatchPlayerProgress{.MyPlayerId = "q"}, MatchPlayerProgress{.MyPlayerId = "r", .MyAlive = false}});
	PreparationContent rewards(economy, ledger, ReferenceChoiceRewards(), ReferenceContentPools(), ReferenceBondRules(), players);
	economy.BeginRound(1); economy.EndPreparation();
	const std::array settlement{EconomySettlement{.MyPlayerId = "p"}, EconomySettlement{.MyPlayerId = "q"}, EconomySettlement{.MyPlayerId = "r", .MyEliminated = true}};
	economy.ApplySettlement(1, settlement); economy.BeginRound(2);
	Random random(seed);
	for (std::string line; std::getline(std::cin, line);)
	{
		std::istringstream input(line); std::string operation, player; input >> operation >> player; bool ok = true;
		if (operation == "pick")
		{
			int kind = 0; bool team = false; std::string id; input >> kind >> id >> team;
			ok = rewards.Apply(player, ChoiceCard{.MyKind = static_cast<ChoiceCardKind>(kind), .MyId = id, .MyTeam = team}, random).has_value();
		}
		else if (operation == "clear")
		{
			const auto view = *economy.View(player);
			for (const auto& slots : {view.MyHand, view.MyTemporary}) for (const auto& piece : slots)
				if (piece) (void)economy.ApplyInventoryEffect(player, RemoveOwnedPiece{.MyUid = piece->MyUid});
		}
		else if (operation == "grant") { std::string id; input >> id; ok = economy.GrantPiece(player, id).has_value(); }
		else if (operation == "layers") { std::string id; double count = 0; input >> id >> count; (void)rewards.AddLayers(player, id, count); }
		else if (operation == "prep_end") { economy.ApplyPreparationDeadline(); rewards.OnPreparationEnd(); }
		else if (operation == "unready") ok = economy.Execute(CommandEnvelope{.MyPlayerId = player, .MyRound = economy.Round(), .MyCommand = SetReady{.MyReady = false}}).has_value();
		else if (operation == "buy") { std::size_t slot = 0; input >> slot; ok = economy.Execute(CommandEnvelope{.MyPlayerId = player, .MyRound = economy.Round(), .MyCommand = Buy{.MySlot = slot}}).has_value(); }
		else if (operation == "refresh") ok = economy.Execute(CommandEnvelope{.MyPlayerId = player, .MyRound = economy.Round(), .MyCommand = Refresh{}}).has_value();
		else return 4;
		Snapshot(economy, ledger, rewards, random, ok);
	}
}
