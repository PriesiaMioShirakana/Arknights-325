#include <stronghold/domain/choice_generation.hpp>

namespace Stronghold
{
	namespace
	{
		template <class _Range, class _Value>
		bool Has(const _Range& _range, const _Value& _value) { return std::ranges::find(_range, _value) != _range.end(); }

		std::optional<std::size_t> Weighted(Random& _random, std::span<const ChoiceWeight> _pairs)
		{
			if (_pairs.empty()) return {};
			double total = 0;
			for (const auto& pair : _pairs)
			{
				if (!std::isfinite(pair.MyWeight)) throw std::invalid_argument("non-finite choice weight");
				total += std::max(0.0, pair.MyWeight);
			}
			if (!std::isfinite(total)) throw std::overflow_error("choice weight sum overflow");
			if (!std::isgreater(total, 0)) return _pairs.front().MyValue;
			double remaining = _random.Next() * total;
			for (const auto& pair : _pairs) { remaining -= std::max(0.0, pair.MyWeight); if (std::isless(remaining, 0)) return pair.MyValue; }
			return _pairs.back().MyValue;
		}

		std::optional<std::size_t> Uniform(Random& _random, std::span<const std::size_t> _values)
		{ return _values.empty() ? std::nullopt : std::optional(_values[_random.Index(static_cast<std::uint32_t>(_values.size()))]); }

		// 表已在适配器中验证；局部构造器只保存引用，不创建运行时继承或共享所有权。
		class ChoiceBuilder final
		{
		public:
			ChoiceBuilder(const ChoiceGenerationRules& _rules, const ChoiceGenerationInput& _input, Random& _random)
				: _MyRules(_rules), _MyInput(_input), _MyRandom(_random) { }

			std::vector<std::size_t> Build(ChoiceFamily _family, unsigned _count, const ChoiceSchedule& _schedule)
			{
				if (_family == ChoiceFamily::BOUNTY) return Bounties(_count, _schedule.MyBountyKind);
				if (_family == ChoiceFamily::TACTIC) return Tactics(_count);
				if (_family == ChoiceFamily::SHOP && !_MyRules.MyShopSlots.empty() &&
					(_MyRules.MyShopRounds.empty() || Has(_MyRules.MyShopRounds, _MyInput.MyRound))) return Shop(_count);
				const auto bounds = _family == ChoiceFamily::SUPPLY ? _schedule.MySupplyTiers : std::array<int, 2>{1, 6};
				auto items = Items(bounds[0], bounds[1]); if (items.empty()) items = Items(1, 6);
				std::vector<std::size_t> output; output.reserve(_count);
				for (unsigned i = 0; i < _count && !items.empty(); ++i) output.emplace_back(*Uniform(_MyRandom, items));
				return output;
			}

		private:
			const ChoiceCardRecord& Card(std::size_t _index) const
			{
				if (_index >= _MyRules.MyCards.size()) throw std::out_of_range("choice card index out of range");
				return _MyRules.MyCards[_index];
			}

			int Series(std::size_t _index) const
			{
				const auto found = std::ranges::find(_MyRules.MyBounties, _index, &BountyDraftEntry::MyCard);
				return found == _MyRules.MyBounties.end() ? 0 : found->MySeries;
			}

			std::vector<std::size_t> Items(int _low, int _high) const
			{
				std::vector<std::size_t> items;
				std::size_t size = 0;
				for (int tier = std::max(1, _low); tier <= std::min(6, _high); ++tier) size += _MyRules.MyItemsByTier[static_cast<std::size_t>(tier - 1)].size();
				items.reserve(size);
				for (int tier = std::max(1, _low); tier <= std::min(6, _high); ++tier)
					for (const auto& entry : _MyRules.MyItemsByTier[static_cast<std::size_t>(tier - 1)]) items.emplace_back(entry.MyValue);
				return items;
			}

			std::vector<std::size_t> Initial(const BountyDraftRule& _rule, std::span<const std::size_t> _eligible)
			{
				std::vector<int> tiers(_rule.MyTiers.begin(), _rule.MyTiers.end()); _MyRandom.Shuffle(tiers.begin(), tiers.end());
				const auto preferredTier = [&](int _tier)
				{ return std::ranges::any_of(_eligible, [&](std::size_t _card) { return Has(_rule.MyPreferred, _card) && Card(_card).MyTier == _tier; }); };
				std::stable_partition(tiers.begin(), tiers.end(), preferredTier);
				std::vector<std::size_t> output; output.reserve(tiers.size());
				std::vector<std::size_t> candidates, preferred; candidates.reserve(_eligible.size()); preferred.reserve(_eligible.size());
				for (const int tier : tiers)
				{
					candidates.clear(); preferred.clear();
					for (const auto card : _eligible)
					{
						const int series = Series(card);
						const auto used = std::ranges::count_if(output, [&](std::size_t _card) { return Series(_card) == series; });
						if (Has(output, card) || Card(card).MyTier != tier || !Has(_rule.MySeries, series) || static_cast<std::size_t>(used) >= _rule.MyPerSeries) continue;
						candidates.emplace_back(card); if (Has(_rule.MyPreferred, card)) preferred.emplace_back(card);
					}
					const auto selected = Uniform(_MyRandom, preferred.empty() ? candidates : preferred);
					if (selected) output.emplace_back(*selected);
				}
				return output;
			}

			void Hunter(const BountyDraftRule& _rule, unsigned _size, std::vector<std::size_t>& _output, std::span<const std::size_t> _eligible)
			{
				std::vector<std::size_t> giants; giants.reserve(_rule.MyGiants.size());
				for (const auto card : _rule.MyGiants) if (Has(_eligible, card)) giants.emplace_back(card);
				if (_output.size() < _size && !std::ranges::any_of(_output, [&](std::size_t _card) { return Has(giants, _card); }))
				{
					const auto giant = Uniform(_MyRandom, giants); if (giant) _output.emplace_back(*giant);
				}
				std::vector<std::size_t> fill; fill.reserve(_rule.MyCards.size());
				for (const auto card : _rule.MyCards) if (Has(_eligible, card)) fill.emplace_back(card);
				_MyRandom.Shuffle(fill.begin(), fill.end());
				for (const auto card : fill)
				{
					if (_output.size() >= _size) break;
					const int series = Series(card);
					if (Has(_output, card)) continue;
					if (Has(_rule.MyOnePerSeries, series))
					{
						if (std::ranges::any_of(_output, [&](std::size_t _card) { return Series(_card) == series; })) continue;
					}
					else if (series == 16 && static_cast<std::size_t>(std::ranges::count_if(_output, [&](std::size_t _card)
					{ return Series(_card) == 16 && !Has(giants, _card); })) >= _rule.MyMaxSeries16) continue;
					_output.emplace_back(card);
				}
			}

			std::vector<std::size_t> Bounties(unsigned _count, BountyDraftKind _kind)
			{
				if (_kind == BountyDraftKind::NONE) _kind = _MyInput.MyRound < 8 ? BountyDraftKind::INITIAL : _MyInput.MyRound < 11 ? BountyDraftKind::BOSS : BountyDraftKind::HUNTER;
				std::vector<std::size_t> eligible; eligible.reserve(_MyRules.MyBounties.size());
				for (const auto& entry : _MyRules.MyBounties) if (entry.MyPool == BountyDraftKind::NONE || entry.MyPool == _kind) eligible.emplace_back(entry.MyCard);
				std::vector<std::size_t> output; output.reserve(_count);
				const auto spec = std::ranges::find(_MyRules.MyBountySpecs, _kind, &BountyDraftSpec::MyKind);
				if (spec != _MyRules.MyBountySpecs.end())
				{
					std::optional<std::size_t> group;
					if (spec->MyPickSeen)
					{
						std::vector<ChoiceWeight> weights; weights.reserve(spec->MyGroups.size());
						for (std::size_t i = 0; i < spec->MyGroups.size(); ++i) weights.emplace_back(ChoiceWeight{.MyValue = i, .MyWeight = static_cast<double>(spec->MyGroups[i].MySeen ? spec->MyGroups[i].MySeen : 1)});
						group = Weighted(_MyRandom, weights);
					}
					else
					{
						const auto slots = std::max(spec->MyGroups.size(), static_cast<std::size_t>(spec->MySlots));
						const auto selected = slots ? _MyRandom.Index(static_cast<std::uint32_t>(slots)) : 0;
						if (selected < spec->MyGroups.size()) group = selected;
					}
					std::vector<std::size_t> list; std::vector<ChoiceWeight> weights;
					if (group)
					{
						for (const auto& card : spec->MyGroups[*group].MyCards) if (Has(eligible, card.MyValue)) { list.emplace_back(card.MyValue); weights.emplace_back(card); }
						if (_kind == BountyDraftKind::HUNTER && spec->MyGroups[*group].MyOpen) Hunter(spec->MyRule, static_cast<unsigned>(list.size()) + spec->MyGroups[*group].MyOpen, list, eligible);
					}
					else if (_kind == BountyDraftKind::INITIAL) list = Initial(spec->MyRule, eligible);
					else if (_kind == BountyDraftKind::HUNTER) Hunter(spec->MyRule, spec->MyRule.MySize, list, eligible);
					while (weights.size() < list.size()) weights.emplace_back(ChoiceWeight{.MyValue = list[weights.size()]});
					while (output.size() < spec->MyCount && !weights.empty())
					{
						const auto selected = *Weighted(_MyRandom, weights); output.emplace_back(selected);
						weights.erase(std::ranges::find(weights, selected, &ChoiceWeight::MyValue));
					}
				}
				if (output.size() < _count)
				{
					_MyRandom.Shuffle(eligible.begin(), eligible.end());
					for (const auto card : eligible) { if (output.size() >= _count) break; if (!Has(output, card)) output.emplace_back(card); }
				}
				_MyRandom.Shuffle(output.begin(), output.end()); if (output.size() > _count) output.resize(_count);
				return output;
			}

			std::vector<std::size_t> Shop(unsigned _count)
			{
				std::vector<std::size_t> output; output.reserve(_MyRules.MyShopSlots.size());
				std::vector<ChoiceWeight> kinds; kinds.reserve(7);
				for (const auto& slot : _MyRules.MyShopSlots)
				{
					kinds.clear(); for (const auto& kind : slot.MyKinds) if (kind.MyValue != 0 || _MyRules.MyCoinCard) kinds.emplace_back(kind);
					const auto kind = Weighted(_MyRandom, kinds); if (!kind) continue;
					std::vector<ChoiceWeight> items;
					if (*kind == 0) items.emplace_back(ChoiceWeight{.MyValue = *_MyRules.MyCoinCard, .MyWeight = ItemWeight(*_MyRules.MyCoinCard)});
					else
					{
						for (auto tier = std::min(std::size_t{6}, *kind); tier > 0; --tier)
							if (!_MyRules.MyItemsByTier[tier - 1].empty()) { items.assign(_MyRules.MyItemsByTier[tier - 1].begin(), _MyRules.MyItemsByTier[tier - 1].end()); break; }
						if (items.empty()) for (const auto& tier : _MyRules.MyItemsByTier) items.insert(items.end(), tier.begin(), tier.end());
					}
					const auto item = Weighted(_MyRandom, items); if (item) output.emplace_back(*item);
				}
				_MyRandom.Shuffle(output.begin(), output.end()); if (output.size() > _count) output.resize(_count);
				return output;
			}

			double ItemWeight(std::size_t _card) const
			{
				for (const auto& tier : _MyRules.MyItemsByTier)
					if (const auto found = std::ranges::find(tier, _card, &ChoiceWeight::MyValue); found != tier.end()) return found->MyWeight;
				return 1;
			}

			std::vector<std::size_t> Tactics(unsigned _count)
			{
				const bool official = _MyRules.MyHasTacticSpec && (_MyRules.MyTacticRounds.empty() || Has(_MyRules.MyTacticRounds, _MyInput.MyRound));
				std::vector<ChoiceWeight> all, kinds; all.reserve(_MyRules.MyTactics.size()); kinds.reserve(_MyRules.MyTactics.size());
				for (const auto& entry : _MyRules.MyTactics)
				{
					const auto& card = Card(entry.MyCard);
					if (card.MyTacticKind == "terrain" && (_MyInput.MyStage.empty() || _MyInput.MyStage != entry.MyStage)) continue;
					if (_MyInput.MyLiveBonds && !entry.MyTargetBonds.empty() && !std::ranges::any_of(entry.MyTargetBonds, [&](std::string_view _bond) { return Has(*_MyInput.MyLiveBonds, _bond); })) continue;
					const ChoiceWeight pair{.MyValue = entry.MyCard, .MyWeight = official ? entry.MyWeight : 1};
					all.emplace_back(pair);
					if (!official || _MyRules.MyTacticKinds.empty() || Has(_MyRules.MyTacticKinds, card.MyTacticKind)) kinds.emplace_back(pair);
				}
				const auto& pairs = kinds.empty() ? all : kinds;
				std::vector<std::size_t> output; output.reserve(_count);
				for (unsigned i = 0; i < _count && !pairs.empty(); ++i) output.emplace_back(*Weighted(_MyRandom, pairs));
				return output;
			}

			const ChoiceGenerationRules& _MyRules;
			const ChoiceGenerationInput& _MyInput;
			Random& _MyRandom;
		};
	}

	std::optional<ChoiceDraft> GenerateChoices(const ChoiceGenerationRules& _rules, const ChoiceGenerationInput& _input, Random& _random)
	{
		auto random = _random; // 生成失败时保留调用方随机流；成功但无卡可选仍提交已消费的随机数。
		constexpr std::array fallbackFamily{ChoiceWeight{.MyValue = static_cast<std::size_t>(ChoiceFamily::SUPPLY)}};
		ChoiceSchedule fallback{.MyFamilies = fallbackFamily, .MyCount = _input.MySolo ? 3U : 6U, .MySupplyTiers = {1, static_cast<int>(std::min(6U, 1 + _input.MyRound / 3))}};
		const auto found = std::ranges::find_if(_rules.MySchedules, [&](const ChoiceSchedule& _s) { return _s.MyMode == _input.MyMode && _s.MyRound == _input.MyRound; });
		const auto& schedule = found == _rules.MySchedules.end() ? fallback : *found;
		const auto families = schedule.MyFamilies.empty() ? std::span<const ChoiceWeight>(fallbackFamily) : schedule.MyFamilies;
		const auto selected = *Weighted(random, families);
		if (selected >= _rules.MyFamilies.size()) throw std::invalid_argument("invalid choice family");
		auto family = static_cast<ChoiceFamily>(selected);
		const unsigned count = schedule.MyCount ? std::min(6U, schedule.MyCount) : _input.MySolo ? _rules.MySoloCount : _rules.MyCoopCount;
		ChoiceBuilder builder(_rules, _input, random);
		auto cards = builder.Build(family, count, schedule);
		if (cards.empty() && family != ChoiceFamily::SUPPLY) { family = ChoiceFamily::SUPPLY; cards = builder.Build(family, count, schedule); }
		if (cards.empty()) { _random = random; return {}; }
		if (!_input.MySolo && _input.MyCapacityExperiment && _input.MyPlayerCount > 4 && _input.MyPlayerCount <= 20)
		{
			const auto target = std::min(std::size_t{22}, std::max(std::size_t{6}, _input.MyPlayerCount + 2));
			const auto originals = cards; cards.reserve(target);
			while (cards.size() < target)
			{
				auto batch = family == ChoiceFamily::BOUNTY ? originals : builder.Build(family, static_cast<unsigned>(std::min(std::size_t{6}, target - cards.size())), schedule);
				if (family == ChoiceFamily::BOUNTY) random.Shuffle(batch.begin(), batch.end());
				const auto& fill = batch.empty() ? originals : batch;
				cards.insert(cards.end(), fill.begin(), fill.begin() + static_cast<std::ptrdiff_t>(std::min(fill.size(), target - cards.size())));
			}
		}
		const auto index = static_cast<std::size_t>(family);
		ChoiceDraft output{.MyFamily = family, .MyName = _rules.MyFamilies[index].MyName, .MyDescription = _rules.MyFamilies[index].MyDescription};
		const auto events = schedule.MyEvents[index]; if (!events.empty()) output.MyEventId = events[random.Index(static_cast<std::uint32_t>(events.size()))];
		output.MyCards.reserve(cards.size());
		for (const auto id : cards)
		{
			if (id >= _rules.MyCards.size()) throw std::out_of_range("choice card index out of range");
			const auto& card = _rules.MyCards[id];
			output.MyCards.emplace_back(ChoiceCard{.MyKind = card.MyKind, .MyId = std::string(card.MyId), .MyName = std::string(card.MyName),
				.MyDescription = std::string(card.MyDescription), .MyRichDescription = std::string(card.MyRichDescription), .MyTier = card.MyTier,
				.MyBounty = WaveBounty{.MyEnemyId = card.MyEnemyId, .MyCount = card.MyCount, .MyCoins = card.MyCoins, .MyPerfect = card.MyPerfect},
				.MyBattles = card.MyBattles, .MyTeam = card.MyTeam, .MyTacticKind = std::string(card.MyTacticKind)});
		}
		_random = random;
		return output;
	}
}
