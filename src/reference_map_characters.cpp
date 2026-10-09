#include <stronghold/adapters/reference_choices.hpp>
#include <stronghold/adapters/reference_map_characters.hpp>

namespace Stronghold
{
	CombatDefinition MakeMapMedicDefinition(const AllyRecord& _body)
	{
		const bool touch = _body.MyId == "char_613_acmedc";
		if (!touch && _body.MyId != "char_605_cmedic") throw std::invalid_argument("unknown map medic");
		auto definition = _body.MakeDefinition(_body.MyBaseAttack);
		if (_body.MySkill)
		{
			const auto& record = *_body.MySkill;
			auto& skill = definition.MySkill;
			skill.MyKind = record.MyDuration > 0 ? SkillKind::DURATION : SkillKind::INSTANT;
			skill.MySpType = record.MySpType; skill.MySpCost = record.MySpCost; skill.MyInitialSp = record.MyInitialSp;
			skill.MyDuration = record.MyDuration; skill.MyMaxCharges = record.MyMaxCharges;
			skill.MyHealSkill = true;
			skill.MyModifiers.push_back({Attribute::ATTACK_PERCENT, record.MyBlackboard.Number("atk")});
			if (touch)
			{
				skill.MyRange.assign(record.MyRange.begin(), record.MyRange.end());
				skill.MyTrigger = SkillTrigger::ACTIVE_RANGE;
				skill.MyTriggerRange = skill.MyRange;
				skill.MyAttack = definition.MyAttack;
				skill.MyAttack->MyMaxTargets = static_cast<std::size_t>(std::max(1.0, record.MyBlackboard.Number("attack@max_target", 1)));
				definition.MyMedic = MedicKitDefinition{.MySkillHpRatio = record.MyBlackboard.Number("hp_ratio"),
					.MySkillHealMultiplier = record.MyBlackboard.Number("heal_scale", 1),
					.MySkillExtraHeal = record.MyBlackboard.Number("attack@addition_heal_scale")};
			}
		}
		definition.MyInitialBuffs.reserve(_body.MyTalents.size());
		for (const auto& talent : _body.MyTalents)
		{
			if (touch)
			{
				if (!definition.MyMedic) definition.MyMedic.emplace();
				if (talent.MyName == "攫升") definition.MyMedic->MyHealSp = talent.MyBlackboard.Number("sp");
				else if (talent.MyName == "超脱") definition.MyMedic->MyDeathSp = talent.MyBlackboard.Number("sp");
				else throw std::invalid_argument("unknown Touch talent");
			}
			else
			{
				std::vector<AttributeChange> mods; mods.reserve(4);
				for (const auto& [name, attribute] : std::array<std::pair<std::string_view, Attribute>, 4>{{
					{"atk", Attribute::ATTACK_PERCENT}, {"def", Attribute::DEFENSE_PERCENT},
					{"max_hp", Attribute::HEALTH_PERCENT}, {"attack_speed", Attribute::ATTACK_SPEED}}})
					if (const auto n = talent.MyBlackboard.Number(name); n != 0) mods.push_back({attribute, n});
				if (!mods.empty()) definition.MyInitialBuffs.push_back(BuffDefinition{.MyKey = "talent:" + std::string(talent.MyName),
					.MyModifiers = std::move(mods), .MyPersistent = true, .MyAllowDead = true});
			}
		}
		return definition;
	}

	std::vector<MapCharacterVariant> MakeMapCharacterVariants(const StageRecord& _stage)
	{
		std::vector<MapCharacterVariant> out;
		const auto rules = ReferenceMapCharacterBands(); out.reserve(rules.size());
		for (const auto& rule : rules)
		{
			auto& variant = out.emplace_back(MapCharacterVariant{.MyMinimumElites = rule.MyMinimumElites, .MyMaximumElites = rule.MyMaximumElites});
			variant.MyCharacters.reserve(rule.MyCharacters.size());
			for (const auto id : rule.MyCharacters)
			{
				auto& character = variant.MyCharacters.emplace_back(MapCharacterDefinition{.MyDefinition = MakeMapMedicDefinition(ReferenceToken(id).Variant().MyBody)});
				character.MyPositions.reserve(_stage.MyMapCharacters.size());
				for (const auto& slot : _stage.MyMapCharacters) if (slot.MyId == id) character.MyPositions.push_back(slot.MyTile);
			}
		}
		return out;
	}
}
