#include <stronghold/adapters/reference_roster.hpp>

namespace Stronghold
{
	namespace
	{
		constexpr bool IsIdentifier(std::string_view _value) noexcept
		{
			if (_value.empty() || _value.size() > 64) return false;
			for (const char ch : _value)
				if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
					ch == '_' || ch == '-' || ch == '.' || ch == ':')) return false;
			return true;
		}

		const RosterRule* Rule(std::string_view _id)
		{
			const auto rules = ReferenceRosterRules();
			const auto found = std::ranges::lower_bound(rules, _id, {}, &RosterRule::MyId);
			return found != rules.end() && found->MyId == _id ? &*found : nullptr;
		}
	}

	std::expected<void, RosterError> PlayerRoster::SetLoadout(std::span<const LoadoutChoice> _choices)
	{
		if (_MyBot) return std::unexpected(RosterError::BOT);
		if (_choices.size() > 160) return std::unexpected(RosterError::BAD_MESSAGE);
		std::vector<SelectedLoadout> next;
		next.reserve(_choices.size());
		for (std::size_t i = 0; i < _choices.size(); ++i)
		{
			const auto& choice = _choices[i];
			if (!IsIdentifier(choice.MyId) || (!choice.MySkill && !choice.MyModule) ||
				(choice.MySkill && (*choice.MySkill < 0 || *choice.MySkill > 9)) ||
				(choice.MyModule && !IsIdentifier(*choice.MyModule))) return std::unexpected(RosterError::BAD_MESSAGE);
			for (std::size_t j = 0; j < i; ++j)
				if (_choices[j].MyId == choice.MyId) return std::unexpected(RosterError::BAD_MESSAGE);
			const auto* rule = Rule(choice.MyId);
			if (!rule || !rule->MySelectable) return std::unexpected(RosterError::BAD_TARGET);
			const auto skill = choice.MySkill.value_or(rule->MyDefaultSkill);
			if (!std::ranges::contains(rule->MySkills, skill)) return std::unexpected(RosterError::BAD_TARGET);
			auto module = rule->MyDefaultModule;
			if (choice.MyModule)
			{
				const auto found = std::ranges::find(rule->MyModules, *choice.MyModule);
				if (found == rule->MyModules.end()) return std::unexpected(RosterError::BAD_TARGET);
				module = *found;
			}
			if (skill != rule->MyDefaultSkill || module != rule->MyDefaultModule)
				next.emplace_back(SelectedLoadout{.MyId = rule->MyId, .MySkill = skill, .MyModule = module});
		}
		_MyLoadouts = std::move(next);
		return {};
	}

	std::expected<std::size_t, RosterError> PlayerRoster::SetNotOwned(std::span<const std::string_view> _ids)
	{
		if (_MyBot) return std::unexpected(RosterError::BOT);
		if (_ids.size() > 160 || !std::ranges::all_of(_ids, IsIdentifier))
			return std::unexpected(RosterError::BAD_MESSAGE);
		std::vector<std::string_view> next;
		next.reserve(_ids.size());
		for (const auto id : _ids)
			if (const auto* rule = Rule(id); rule && rule->MyOwnable && ReferenceOperator(id).MyStandIn)
				next.emplace_back(rule->MyId);
		std::ranges::sort(next);
		next.erase(std::unique(next.begin(), next.end()), next.end());
		const auto dropped = _ids.size() - next.size();
		_MyNotOwned = std::move(next);
		return dropped;
	}

	std::expected<std::size_t, RosterError> PlayerRoster::SetDiy(
		std::span<const DiyPick> _picks,
		std::span<const std::string_view> _implementedCharacters
	)
	{
		if (_MyBot) return std::unexpected(RosterError::BOT);
		if (_picks.size() > 8) return std::unexpected(RosterError::BAD_MESSAGE);
		for (std::size_t i = 0; i < _picks.size(); ++i)
		{
			const auto& pick = _picks[i];
			if (!IsIdentifier(pick.MySlot) || (!pick.MyCharacter.empty() &&
				(!IsIdentifier(pick.MyCharacter) || (pick.MySkill && (*pick.MySkill < 0 || *pick.MySkill > 9)) ||
					(pick.MyModule && !IsIdentifier(*pick.MyModule))))) return std::unexpected(RosterError::BAD_MESSAGE);
			for (std::size_t j = 0; j < i; ++j)
				if (_picks[j].MySlot == pick.MySlot) return std::unexpected(RosterError::BAD_MESSAGE);
		}
		std::vector<SelectedDiy> next;
		next.reserve(ReferenceDiySlots().size());
		std::size_t dropped = 0;
		for (const auto& pick : _picks)
			if (!pick.MyCharacter.empty() && !std::ranges::contains(ReferenceDiySlots(), pick.MySlot, &DiySlotRule::MyId)) ++dropped;
		// 原始 JSON 的槽位顺序决定冲突赢家；请求的排列顺序不能改变结果。
		for (const auto& slot : ReferenceDiySlots())
		{
			const auto pick = std::ranges::find(_picks, slot.MyId, &DiyPick::MySlot);
			if (pick == _picks.end() || pick->MyCharacter.empty()) continue;
			const auto& op = ReferenceOperator(slot.MyId);
			const auto found = std::ranges::find_if(op.MyDiyChoices, [&](const DiyChoiceRecord& _choice)
			{
				if (_choice.MyCharacterId != pick->MyCharacter) return false;
				if (_choice.MyPrototype)
					return (!pick->MySkill || *pick->MySkill == _choice.MySkillIndex) &&
						(!pick->MyModule || *pick->MyModule == _choice.MyModuleId);
				return pick->MySkill && *pick->MySkill == _choice.MySkillIndex &&
					pick->MyModule.value_or("none") == _choice.MyModuleId;
			});
			bool valid = found != op.MyDiyChoices.end() && std::ranges::contains(_implementedCharacters, pick->MyCharacter);
			if (valid && std::ranges::binary_search(ReferenceExcludedDiyModules(), found->MyModuleId)) valid = false;
			if (valid)
				for (const auto& old : next)
				{
					if (old.MyCharacter != found->MyCharacterId) continue;
					const auto& oldOp = ReferenceOperator(old.MySlot);
					const auto& oldChoice = oldOp.Diy(old.MyCharacter, old.MySkill, old.MyModule);
					if (oldOp.MyTier == slot.MyTier || (!oldChoice.MyPrototype && !found->MyPrototype)) valid = false;
				}
			if (!valid) { ++dropped; continue; }
			next.emplace_back(SelectedDiy{.MySlot = slot.MyId, .MyCharacter = found->MyCharacterId,
				.MySkill = found->MySkillIndex, .MyModule = found->MyModuleId});
		}
		_MyDiy = std::move(next);
		return dropped;
	}

	RosterOperator PlayerRoster::Resolve(std::string_view _id) const
	{
		const auto& op = ReferenceOperator(_id);
		if (op.MyDiy)
		{
			const auto pick = std::ranges::find(_MyDiy, op.MyBaseId, &SelectedDiy::MySlot);
			if (pick != _MyDiy.end())
			{
				const auto& choice = op.Diy(pick->MyCharacter, pick->MySkill, pick->MyModule);
				return RosterOperator{.MyIdentity = op, .MyLoadout = choice.MyLoadout, .MyBonds = choice.MyBonds,
					.MyTokenOwner = choice.MyTokenOwner, .MyDiySelected = true};
			}
		}
		if (op.MyStandIn && std::ranges::binary_search(_MyNotOwned, op.MyBaseId))
			return RosterOperator{.MyIdentity = op, .MyLoadout = *op.MyStandIn, .MyBonds = op.MyBonds, .MyStandIn = true};
		const auto selected = std::ranges::find(_MyLoadouts, op.MyBaseId, &SelectedLoadout::MyId);
		const auto& body = selected == _MyLoadouts.end() ? op.Loadout() :
			op.Loadout(selected->MySkill, op.MyGolden ? std::optional(selected->MyModule) : std::nullopt);
		return RosterOperator{.MyIdentity = op, .MyLoadout = body, .MyBonds = op.MyBonds, .MyTokenOwner = op.MyId};
	}

	void PlayerRoster::PlaceableTokens(std::string_view _id, std::vector<TokenAllowance>& _out) const
	{
		_out.clear();
		const auto selected = Resolve(_id);
		if (selected.MyStandIn || (selected.MyIdentity.MyDiy && !selected.MyDiySelected)) return;
		const auto tokens = selected.MyDiySelected ? selected.MyLoadout.MyBody.MyTokens :
			selected.MyIdentity.Loadout().MyBody.MyTokens;
		_out.reserve(tokens.size());
		const auto rules = ReferenceTokenSupplies();
		for (const auto id : tokens)
		{
			const auto rule = std::ranges::lower_bound(rules, id, {}, &TokenSupplyRule::MyId);
			if (rule == rules.end() || rule->MyId != id) continue;
			auto variant = std::ranges::find(rule->MyVariants, selected.MyTokenOwner, &TokenSupplyVariant::MyOwner);
			if (variant == rule->MyVariants.end() && !selected.MyDiySelected)
				variant = std::ranges::find(rule->MyVariants, selected.MyIdentity.MyBaseId, &TokenSupplyVariant::MyOwner);
			auto count = rule->MyCount;
			if (variant != rule->MyVariants.end())
			{
				const auto source = std::ranges::find(variant->MySkills, selected.MyLoadout.MySkillIndex, &TokenSkillSource::MySkill);
				if (!(source == variant->MySkills.end() ? variant->MyAvailable : source->MyAvailable)) continue;
				count = variant->MyCount;
				// 原版预设干员的准备发放仅读基础上限；自选则读取当前生效模组的上限覆盖。
				if (selected.MyDiySelected && selected.MyLoadout.MyModuleActive)
				{
					const auto module = std::ranges::find(variant->MyModules, selected.MyLoadout.MyEquippedModuleId, &TokenModuleCount::MyModule);
					if (module != variant->MyModules.end()) count = module->MyCount;
				}
			}
			_out.emplace_back(TokenAllowance{.MyId = rule->MyId, .MyCount = count,
				.MyOwnerRange = rule->MyOwnerRange, .MyOwnerRangeOutside = rule->MyOwnerRangeOutside});
		}
	}

	SummonCatalog PlayerRoster::MakeSummonCatalog() const
	{
		std::vector<SummonDefinition> tokens; tokens.reserve(ReferenceTokenSupplies().size());
		std::vector<SummonOwner> owners; owners.reserve(ReferenceOperators().size());
		std::vector<TokenAllowance> grants;
		for (const auto& rule : ReferenceTokenSupplies())
			tokens.emplace_back(SummonDefinition{.MyId = std::string(rule.MyId),
				.MyPlacement = ReferenceToken(rule.MyId).MyUnowned.MyBody.MyPreparationPlacement,
				.MyInsideOwnerRange = rule.MyOwnerRange, .MyOutsideOwnerRange = rule.MyOwnerRangeOutside});
		for (const auto& op : ReferenceOperators())
		{
			const auto selected = Resolve(op.MyId);
			if (op.MyDiy && !selected.MyDiySelected) continue;
			PlaceableTokens(op.MyId, grants);
			const auto range = selected.MyLoadout.MyBody.MyPreparationRange;
			SummonOwner owner{.MyId = std::string(op.MyId), .MyRange = std::vector<RangeOffset>(range.begin(), range.end())};
			owner.MyTokens.reserve(grants.size());
			for (const auto& grant : grants) owner.MyTokens.emplace_back(SummonCount{.MyId = std::string(grant.MyId), .MyCount = grant.MyCount});
			owners.emplace_back(std::move(owner));
		}
		return SummonCatalog(std::move(tokens), std::move(owners));
	}

	std::expected<void, CommandError> ConfigurePreparationRoster(
		EconomySession& _session,
		std::string_view _player,
		const PlayerRoster& _roster,
		std::span<const std::string_view> _inactiveBonds
	)
	{
		std::vector<PrivateStockSelection> stocks;
		std::vector<PlacementOverride> placements;
		stocks.reserve(_roster.Diy().size());
		placements.reserve((_roster.Diy().size() + _roster.NotOwned().size()) * 2);
		const auto addPlacement = [&](std::string_view _id)
		{
			const auto selected = _roster.Resolve(_id);
			const auto& body = selected.MyLoadout.MyBody;
			placements.emplace_back(PlacementOverride{.MyId = std::string(_id), .MyPlacement = body.MyPreparationPlacement});
		};
		for (const auto& pick : _roster.Diy())
		{
			const auto slot = std::ranges::find(ReferenceDiySlots(), pick.MySlot, &DiySlotRule::MyId);
			const auto selected = _roster.Resolve(pick.MySlot);
			const bool banned = !selected.MyBonds.empty() && std::ranges::all_of(selected.MyBonds,
				[&](std::string_view _bond) { return std::ranges::contains(_inactiveBonds, _bond); });
			stocks.emplace_back(PrivateStockSelection{.MyId = slot->MyId, .MyShopLevel = static_cast<int>(slot->MyShopLevel), .MyEnabled = !banned});
			addPlacement(slot->MyId); addPlacement(slot->MyGoldenId);
		}
		for (const auto id : _roster.NotOwned())
		{
			addPlacement(id);
			if (const auto* rule = Rule(id); rule && !rule->MyGoldenId.empty()) addPlacement(rule->MyGoldenId);
		}
		return _session.ConfigureRoster(_player, stocks, placements);
	}
	std::vector<ContentPoolRoster> PlayerRoster::MakeContentPoolRoster() const
	{
		std::vector<ContentPoolRoster> result; result.reserve(ReferenceOperators().size());
		for (const auto& record : ReferenceOperators())
		{
			const auto resolved = Resolve(record.MyId);
			result.emplace_back(ContentPoolRoster{.MyId = record.MyId, .MyBonds = resolved.MyBonds,
				.MyGarrisons = resolved.MyDiySelected ? std::span<const std::string_view>{} : record.MyGarrisons});
		}
		return result;
	}

}
