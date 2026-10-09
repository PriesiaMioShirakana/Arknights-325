#include <iomanip>
#include <iostream>
#include <stronghold/adapters/reference_choices.hpp>
#include <stronghold/adapters/reference_match.hpp>

namespace
{
	void String(std::string_view _value)
	{
		constexpr char Hex[] = "0123456789abcdef";
		std::cout << '"';
		for (const unsigned char c : _value)
		{
			if (c == '"' || c == '\\') std::cout << '\\' << static_cast<char>(c);
			else if (c < 32) std::cout << "\\u00" << Hex[c >> 4U] << Hex[c & 15U];
			else std::cout << static_cast<char>(c);
		}
		std::cout << '"';
	}
}

int main()
{
	using namespace Stronghold;
	// 外部适配器的无效索引/权重不得越界读取，也不得提前消耗对局随机流。
	for (unsigned scene = 0; scene < 2; ++scene)
	{
		Random random(17); const auto before = random.State();
		const std::array item{ChoiceWeight{.MyValue = 999}};
		const std::array family{ChoiceWeight{.MyValue = 1, .MyWeight = std::numeric_limits<double>::quiet_NaN()}};
		const std::array schedule{ChoiceSchedule{.MyMode = "invalid", .MyRound = 2, .MyFamilies = family}};
		ChoiceGenerationRules invalid;
		if (scene == 0) invalid.MyItemsByTier[0] = item; else invalid.MySchedules = schedule;
		bool rejected = false;
		try { (void)GenerateChoices(invalid, ChoiceGenerationInput{.MyMode = "invalid", .MyRound = 2}, random); }
		catch (const std::logic_error&) { rejected = true; }
		if (!rejected || random.State() != before) return 2;
	}
	bool first = true; std::cout << std::setprecision(17) << '[';
	constexpr std::array<unsigned, 7> rounds{2, 3, 6, 9, 11, 14, 15};
	constexpr std::array<std::string_view, 2> live{"yanShip", "swiftShip"};
	for (const auto& mode : ReferenceMatchModes())
		for (unsigned seed = 1; seed <= 60; ++seed)
			for (const auto round : rounds)
			{
				Random random(seed);
				const auto bonds = seed % 4 == 0 ? std::optional<std::span<const std::string_view>>{} : seed % 4 == 1 ? std::optional(std::span<const std::string_view>{}) : std::optional(std::span<const std::string_view>(live));
				const auto draft = GenerateChoices(ReferenceChoices(), ChoiceGenerationInput{.MyMode = mode.MyId, .MyRound = round, .MySolo = mode.MySolo,
					.MyStage = seed % 3 == 0 ? "" : seed % 3 == 1 ? "act2autochess_m01" : "act2autochess_m02", .MyLiveBonds = bonds,
					.MyPlayerCount = seed % 7 == 0 ? 20U : 8U, .MyCapacityExperiment = seed % 3 != 0}, random);
				if (!first) std::cout << ',';
				first = false; std::cout << '['; String(mode.MyId); std::cout << ',' << seed << ',' << round << ',' << random.State() << ',';
				if (!draft) { std::cout << "null]"; continue; }
				std::cout << '[' << static_cast<int>(draft->MyFamily) << ','; String(draft->MyName); std::cout << ','; String(draft->MyDescription); std::cout << ','; String(draft->MyEventId); std::cout << ",[";
				for (std::size_t i = 0; i < draft->MyCards.size(); ++i)
				{
					if (i) std::cout << ',';
					const auto& c = draft->MyCards[i];
					std::cout << '[' << static_cast<int>(c.MyKind) << ','; String(c.MyId); std::cout << ','; String(c.MyName); std::cout << ',';
					String(c.MyDescription); std::cout << ','; String(c.MyRichDescription); std::cout << ',';
					if (c.MyTier) std::cout << *c.MyTier; else std::cout << "null";
					std::cout << ','; String(c.MyBounty.MyEnemyId);
					std::cout << ',' << c.MyBounty.MyCount << ',' << c.MyBounty.MyCoins << ',' << c.MyBattles << ',' << c.MyBounty.MyPerfect << ',' << c.MyTeam << ','; String(c.MyTacticKind); std::cout << ']';
				}
				std::cout << "]]]";
			}
	std::cout << ']';
}
