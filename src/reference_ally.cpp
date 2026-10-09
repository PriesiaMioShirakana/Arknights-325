#include <stronghold/adapters/reference_ally.hpp>

namespace Stronghold
{
	const OperatorRecord& ReferenceOperator(std::string_view _id)
	{
		const auto records = ReferenceOperators();
		const auto found = std::ranges::lower_bound(records, _id, {}, &OperatorRecord::MyId);
		if (found == records.end() || found->MyId != _id) throw std::out_of_range("unknown operator");
		return *found;
	}

	const TokenRecord& ReferenceToken(std::string_view _id)
	{
		const auto records = ReferenceTokens();
		const auto found = std::ranges::lower_bound(records, _id, {}, &TokenRecord::MyId);
		if (found == records.end() || found->MyId != _id) throw std::out_of_range("unknown token");
		return *found;
	}

	const AllyLoadoutRecord& OperatorRecord::Loadout(
		std::optional<int> _skill,
		std::optional<std::string_view> _module,
		bool _standIn
	) const
	{
		if (_standIn && MyStandIn) return *MyStandIn;
		const auto skill = _skill && std::ranges::any_of(MyLoadouts, [&](const auto& _entry) { return _entry.MySkillIndex == *_skill; })
			? *_skill : MyDefaultSkill;
		const auto module = _module && std::ranges::any_of(MyLoadouts, [&](const auto& _entry) { return _entry.MyModuleId == *_module; })
			? *_module : MyDefaultModule;
		for (const auto& entry : MyLoadouts)
			if (entry.MySkillIndex == skill && entry.MyModuleId == module) return entry;
		throw std::logic_error("operator table has no resolved loadout");
	}

	const DiyChoiceRecord& OperatorRecord::Diy(
		std::string_view _character,
		std::optional<int> _skill,
		std::optional<std::string_view> _module
	) const
	{
		if (!MyDiy) throw std::invalid_argument("operator is not a DIY slot");
		const auto module = _module && !_module->empty() ? *_module : std::string_view("none");
		for (const auto& choice : MyDiyChoices)
		{
			if (choice.MyCharacterId != _character) continue;
			if (choice.MyPrototype)
			{
				if ((!_skill || *_skill == choice.MySkillIndex) && (!_module || module == choice.MyModuleId)) return choice;
			}
			else if (_skill && *_skill == choice.MySkillIndex && module == choice.MyModuleId) return choice;
		}
		throw std::invalid_argument("invalid DIY character, skill or module choice");
	}

	const TokenVariantRecord& TokenRecord::DiyVariant(const DiyChoiceRecord& _choice) const
	{
		for (const auto& variant : MyDiyVariants)
			if (variant.MyOwnerId == _choice.MyTokenOwner && variant.MySkillIndex == _choice.MySkillIndex && variant.MyModuleId == _choice.MyModuleId)
				return variant;
		return MyUnowned;
	}

	const TokenVariantRecord& TokenRecord::Variant(
		std::string_view _owner,
		std::optional<int> _skill,
		std::optional<std::string_view> _module
	) const
	{
		const auto hasOwner = [&](std::string_view _id)
		{
			return std::ranges::any_of(MyVariants, [&](const auto& _entry) { return _entry.MyOwnerId == _id; });
		};
		auto owner = hasOwner(_owner) ? _owner : MyFallbackOwner;
		if (!hasOwner(_owner) && _owner.ends_with("_b"))
			for (const auto& entry : MyVariants)
				if (entry.MyOwnerId.ends_with("_a") && entry.MyOwnerId.size() == _owner.size() &&
					entry.MyOwnerId.substr(0, entry.MyOwnerId.size() - 2) == _owner.substr(0, _owner.size() - 2))
				{ owner = entry.MyOwnerId; break; }
		const auto defaultEntry = std::ranges::find_if(MyVariants, [&](const auto& _entry) { return _entry.MyOwnerId == owner && _entry.MyDefault; });
		if (defaultEntry == MyVariants.end()) throw std::logic_error("token table has no default variant");
		if (!_skill && !_module) return *defaultEntry;
		// 是否非默认取决于实际拥有者，随后才在回退变体上应用相应覆盖；二者不能混为一谈。
		const auto operators = ReferenceOperators();
		const auto actual = std::ranges::lower_bound(operators, _owner, {}, &OperatorRecord::MyId);
		if (actual == operators.end() || actual->MyId != _owner) return *defaultEntry;
		const auto& selected = actual->Loadout(_skill, _module);
		auto skill = selected.MySkillDefault ? defaultEntry->MySkillIndex : selected.MySkillIndex;
		auto module = selected.MyModuleDefault ? defaultEntry->MyModuleId : selected.MyModuleId;
		if (!std::ranges::any_of(MyVariants, [&](const auto& _entry) { return _entry.MyOwnerId == owner && _entry.MySkillIndex == skill; })) skill = defaultEntry->MySkillIndex;
		if (!std::ranges::any_of(MyVariants, [&](const auto& _entry) { return _entry.MyOwnerId == owner && _entry.MyModuleId == module; })) module = defaultEntry->MyModuleId;
		for (const auto& entry : MyVariants)
			if (entry.MyOwnerId == owner && entry.MySkillIndex == skill && entry.MyModuleId == module) return entry;
		throw std::logic_error("token table has no resolved variant");
	}
}
