#include <algorithm>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <stronghold/adapters/reference_catalog.hpp>
#include <stronghold/domain/combat_math.hpp>
#include <stronghold/domain/preparation.hpp>

namespace
{
	using namespace Stronghold;

	void String(std::string_view _value)
	{
		std::cout << '"';
		for (const unsigned char c : _value)
		{
			if (c == '"' || c == '\\')
				std::cout << '\\' << c;
			else if (c < 32)
				std::cout << "\\u00" << "0123456789abcdef"[c >> 4] << "0123456789abcdef"[c & 15];
			else
				std::cout << c;
		}
		std::cout << '"';
	}

	void Id(const std::optional<std::string>& _value)
	{
		if (_value)
			String(*_value);
		else
			std::cout << "null";
	}

	std::uint32_t Seed(std::string_view _text)
	{
		std::uint32_t value{};
		const auto [end, error] = std::from_chars(_text.data(), _text.data() + _text.size(), value);
		if (error != std::errc{} || end != _text.data() + _text.size())
			throw std::invalid_argument("invalid uint32 seed");
		return value;
	}

	void Probe(const Catalog& _catalog, std::uint32_t _seed)
	{
		Random random(_seed);
		std::cout << "{\"rng\":[";
		for (int i = 0; i < 256; ++i)
		{
			if (i)
				std::cout << ',';
			std::cout << random.NextU32();
		}
		std::cout << "],\"derived\":[" << DeriveSeed(_seed, u"shop") << ',' << DeriveSeed(_seed, u"waves") << ','
				  << DeriveSeed(_seed, u"\u536b\U0001f600") << "],\"shuffle\":[";
		std::array<int, 16> values{};
		for (int i = 0; i < 16; ++i)
			values[static_cast<std::size_t>(i)] = i;
		random.Shuffle(values.begin(), values.end());
		for (std::size_t i = 0; i < values.size(); ++i)
		{
			if (i)
				std::cout << ',';
			std::cout << values[i];
		}
		std::cout << "],\"rolls\":[";
		SharedPool pool(_catalog);
		for (int i = 0; i < 120; ++i)
		{
			if (i)
				std::cout << ',';
			RollOptions options;
			options.MyMaxTier = 1 + i % 6;
			auto chess = pool.Roll(random, options);
			auto item = pool.RollItem(random, options.MyMaxTier, _catalog);
			std::cout << '[';
			Id(chess);
			std::cout << ',';
			Id(item);
			std::cout << ']';
			if (chess)
				(void)pool.Take(*chess);
		}
		std::cout << "],\"damage\":[";
		for (int i = 0; i < 160; ++i)
		{
			if (i)
				std::cout << ',';
			const double amount = i * 19.25;
			const Mitigation target{
				double(i * 31 - 100),
				double(i * 3 - 20),
				(i % 14 - 2) / 10.0,
				double(i % 9 * 10),
				(i % 14 - 2) / 10.0,
				double(i % 5 * 7),
				double(i * 2 - 10)};
			std::cout << Mitigate(amount, static_cast<DamageType>(i % 4), target);
		}
		std::cout << "]}\n";
	}

	void Snapshot(const EconomySession& _session, bool _placement = false)
	{
		std::cout << '[';
		bool firstPlayer = true;
		for (const auto& publicPlayer : _session.PublicView())
		{
			if (!firstPlayer)
				std::cout << ',';
			firstPlayer = false;
			const auto view = *_session.View(publicPlayer.MyPlayerId);
			std::cout << "{\"id\":";
			String(view.MyPlayerId);
			std::cout << ",\"funds\":" << view.MyFunds << ",\"level\":" << view.MyLevel
					  << ",\"upgrade\":" << view.MyUpgradePrice << ",\"frozen\":" << view.MyFrozen
					  << ",\"ready\":" << view.MyReady << ",\"hand\":[";
			for (std::size_t i = 0; i < view.MyHand.size(); ++i)
			{
				if (i)
					std::cout << ',';
				if (!view.MyHand[i])
				{
					std::cout << "null";
					continue;
				}
				const auto& piece = *view.MyHand[i];
				std::cout << '[' << piece.MyUid << ',';
				String(piece.MyId);
				std::cout << ',' << piece.MyPoolCopies << ']';
			}
			const auto slot = [](const ShopSlot& _slot)
			{
				std::cout << '[';
				String(_slot.MyId);
				std::cout << ',' << _slot.MyPrice << ',' << _slot.MyFrozen << ',' << _slot.MySold << ']';
			};
			if (_placement)
			{
				std::cout << "],\"handFacing\":[";
				for (std::size_t i = 0; i < view.MyHand.size(); ++i)
				{
					if (i)
						std::cout << ',';
					if (view.MyHand[i])
						std::cout << static_cast<int>(view.MyHand[i]->MyFacing);
					else
						std::cout << "null";
				}
				std::cout << "],\"board\":[";
				bool first = true;
				for (std::size_t i = 0; i < view.MyBoard.size(); ++i)
				{
					if (!view.MyBoard[i])
						continue;
					if (!first)
						std::cout << ',';
					first = false;
					const auto position = BoardPosition::FromIndex(i);
					const auto& piece = *view.MyBoard[i];
					std::cout << '[' << position.MyRow << ',' << position.MyColumn << ',' << piece.MyUid << ',';
					String(piece.MyId);
					std::cout << ',' << piece.MyPoolCopies << ',' << static_cast<int>(piece.MyFacing) << ']';
				}
			}
			std::cout << "],\"shop\":[";
			for (std::size_t i = 0; i < view.MyShop.size(); ++i)
			{
				if (i)
					std::cout << ',';
				if (view.MyShop[i])
					slot(*view.MyShop[i]);
				else
					std::cout << "null";
			}
			std::cout << "],\"offers\":[";
			for (std::size_t i = 0; i < view.MyOffers.size(); ++i)
			{
				if (i)
					std::cout << ',';
				std::cout << '[';
				for (std::size_t j = 0; j < view.MyOffers[i].size(); ++j)
				{
					if (j)
						std::cout << ',';
					slot(view.MyOffers[i][j]);
				}
				std::cout << ']';
			}
			std::cout << "]}";
		}
		std::cout << ']';
	}

	void Trace(
		const Catalog& _catalog,
		std::uint32_t _seed,
		std::string_view _mode,
		std::string_view _stage = {},
		DeployField _field = DeployField::NORMAL)
	{
		const std::array<Seat, 2> Seats{{{0, "p0"}, {1, "p1"}}};
		const bool placement = !_stage.empty();
		EconomySession session(
			_catalog,
			_mode,
			Seats,
			_seed,
			false,
			false,
			{},
			placement ? ReferenceStages().at(std::string(_stage)).At(_field) : BoardLayout::Fallback());
		std::cout << '[';
		for (int round = 1; round <= 12; ++round)
		{
			session.BeginRound(round);
			if (round > 1)
				std::cout << ',';
			std::cout << "{\"ok\":true,\"players\":";
			Snapshot(session, placement);
			std::cout << '}';
			for (int step = 0; step < 24; ++step)
			{
				const auto id = step % 2 == 0 ? "p0" : "p1";
				const auto view = *session.View(id);
				PreparationCommand command = Buy{static_cast<std::size_t>((step / 2) % int(view.MyShop.size()))};
				switch (step % 12)
				{
				case 4:
					command = LevelUp{};
					break;
				case 5:
					command = Refresh{};
					break;
				case 6:
					command = Freeze{};
					break;
				case 7:
					command = PickReward{0};
					break;
				case 8:
					{
						const auto it =
							std::ranges::find_if(view.MyHand, [](const auto& _piece) { return bool(_piece); });
						command = Sell{it == view.MyHand.end() ? 0 : (*it)->MyUid};
						break;
					}
				case 9:
					command = SetReady{true};
					break;
				case 11:
					command = SetReady{false};
					break;
				default:
					break;
				}
				const auto result = session.Execute({id, round, command});
				if (!session.PoolConservationHolds())
					throw std::logic_error("pool invariant failed");
				std::cout << ",{\"ok\":" << bool(result) << ",\"players\":";
				Snapshot(session, placement);
				std::cout << '}';
				if (placement)
				{
					for (int move = 0; move < 3; ++move)
					{
						const auto current = *session.View(id);
						std::vector<PieceUid> owned;
						for (const auto& piece : current.MyHand)
							if (piece)
								owned.push_back(piece->MyUid);
						for (const auto& piece : current.MyBoard)
							if (piece)
								owned.push_back(piece->MyUid);
						const auto selector = static_cast<std::size_t>(round * 7 + step * 3 + move);
						const auto uid = owned.empty() ? 0 : owned[selector % owned.size()];
						PreparationCommand action = MoveToBoard{
							uid, BoardPosition::FromIndex(selector * 7 % 36), static_cast<Facing>(selector % 4)};
						if (move == 2)
							action = MoveToHand{uid, selector % current.MyHand.size()};
						const auto moved = session.Execute({id, round, action});
						if (!session.PoolConservationHolds())
							throw std::logic_error("placement pool invariant failed");
						std::cout << ",{\"ok\":" << bool(moved) << ",\"players\":";
						Snapshot(session, true);
						std::cout << '}';
					}
				}
			}
			session.EndPreparation();
			std::cout << ",{\"ok\":true,\"players\":";
			Snapshot(session, placement);
			std::cout << '}';
		}
		std::cout << "]\n";
	}
	void BoardProbe(const Catalog& _catalog)
	{
		std::cout << "{\"stages\":{";
		bool first = true;
		for (const auto& [id, boards] : ReferenceStages())
		{
			if (!first)
				std::cout << ',';
			first = false;
			String(id);
			std::cout << ":[";
			for (int field = 0; field < 3; ++field)
			{
				if (field)
					std::cout << ',';
				std::cout << '[';
				const auto& layout = boards.At(static_cast<DeployField>(field));
				for (std::size_t i = 0; i < layout.MyTiles.size(); ++i)
				{
					if (i)
						std::cout << ',';
					std::cout << static_cast<int>(layout.MyTiles[i]);
				}
				std::cout << ']';
			}
			std::cout << ']';
		}
		std::cout << "},\"placement\":{";
		first = true;
		for (const auto& id : _catalog.VisibleChess())
		{
			for (const auto& key : {id, _catalog.At(id).MyGoldenId})
			{
				if (key.empty())
					continue;
				if (!first)
					std::cout << ',';
				first = false;
				String(key);
				std::cout << ':' << static_cast<int>(_catalog.At(key).MyPlacement);
			}
		}
		std::cout << "}}\n";
	}

} // namespace

int main(int _argc, char** _argv)
{
	try
	{
		std::cout << std::boolalpha << std::setprecision(17);
		const std::string_view command = _argc > 1 ? _argv[1] : "catalog";
		const auto catalog = ReferenceCatalog();
		const auto seed = _argc > 2 ? Seed(_argv[2]) : 1U;
		if (command == "catalog")
		{
			std::cout << "{\"definitions\":" << catalog.Size() << ",\"visibleChess\":" << catalog.VisibleChess().size()
					  << ",\"modes\":" << catalog.Modes().size() << ",\"sha256\":";
			String(ReferenceDataFingerprint());
			std::cout << "}\n";
		}
		else if (command == "probe")
			Probe(catalog, seed);
		else if (command == "boards")
			BoardProbe(catalog);
		else if (command == "placement")
			Trace(
				catalog,
				seed,
				_argc > 3 ? _argv[3] : "mode_multi_normal",
				_argc > 4 ? _argv[4] : "act2autochess_m01",
				_argc > 5 ? static_cast<DeployField>(Seed(_argv[5])) : DeployField::NORMAL);
		else if (command == "trace")
			Trace(catalog, seed, _argc > 3 ? _argv[3] : "mode_multi_normal");
		else
			throw std::invalid_argument(
				"usage: stronghold_cli [catalog | probe [seed] | trace [seed] [mode] | boards | placement [seed] "
				"[mode] [stage] [field:0..2]]");
	}

	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
