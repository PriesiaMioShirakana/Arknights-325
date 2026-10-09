#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	namespace
	{
		constexpr auto Slot(AddonBondKind _kind) { return static_cast<unsigned>(_kind); }
	}

	void Battle::NotifyIndomitable(ContentEvent& _event)
	{
		if (_MyAddonBonds.empty() || !_event.MyUnit) return;
		auto& unit = _MyUnits[Index(_event.MyUnit)];
		if (unit.MySide != UnitSide::ALLY || unit.MyOwner >= _MyAddonBonds.size() || unit.MyKind != UnitKind::OPERATOR) return;
		auto& state = _MyAddonBonds[unit.MyOwner];
		const auto p = unit.MyDefinition.MyIdentity.MyMeleePosition ? state.MyParameters[Slot(AddonBondKind::INDOMITABLE)] : nullptr;
		if (_event.MyKind == ContentEventKind::DEPLOY) { unit.MyIndomitableFree = false; return; }
		const auto chance = [&] { return std::clamp(p->MyProbability + p->MyProbabilityPerLayer * AddonLayers(state, AddonBondKind::INDOMITABLE), 0.0, 1.0); };
		if (_event.MyKind == ContentEventKind::DOLL_SWAP && p && unit.MyAlive && !unit.MyIndomitableFree)
			unit.MyIndomitableFree = _MyRandom.Next() < chance();
		if (_event.MyKind != ContentEventKind::DEATH || (!p && !unit.MyIndomitableFree) ||
			(_event.MyRemovalReason != RemovalReason::KILLED && _event.MyRemovalReason != RemovalReason::RETREAT && _event.MyRemovalReason != RemovalReason::MERCHANT)) return;
		if (p && _event.MyRemovalReason == RemovalReason::KILLED && state.MyTiers[Slot(AddonBondKind::INDOMITABLE)] >= 2 && p->MySp > 0)
			for (const auto id : state.MyOperators) if (Unit(id).MyAlive && !Unit(id).MyRemoved) (void)GainSp(id, p->MySp);
		if (unit.MyAlive || unit.MyRemoved || unit.MyIndomitableAt == Time()) return;
		if (unit.MyIndomitableFree || (p && _MyRandom.Next() < chance()))
		{
			unit.MyIndomitableAt = Time();
			(void)Redeploy(unit.MyId);
		}
	}

	void Battle::NotifyAddonBonds(ContentEvent& _event, bool _late)
	{
		if (_MyAddonBonds.empty()) return;
		if (_late)
		{
			if (_event.MyKind != ContentEventKind::BEFORE_DAMAGE || !_event.MyTarget || _event.MyCancel || _event.MyElement ||
				_event.MyDamage.MySteadCut != 0 || HasTag(_event.MyDamage.MyTags, DamageTag::STEAD_SHARE)) return;
			const auto& target = Unit(_event.MyTarget);
			if (target.MySide != UnitSide::ALLY || target.MyKind != UnitKind::OPERATOR || target.MyOwner >= _MyAddonBonds.size()) return;
			auto& state = _MyAddonBonds[target.MyOwner];
			const auto p = state.MyParameters[Slot(AddonBondKind::STEADFAST)];
			const auto& members = state.MyMembers[Slot(AddonBondKind::STEADFAST)];
			if (!p || state.MyTiers[Slot(AddonBondKind::STEADFAST)] < 2 || std::ranges::contains(members, target.MyId) ||
				std::ranges::none_of(members, [&](UnitId _id) { return Unit(_id).MyAlive && !Unit(_id).MyRemoved; })) return;
			const auto ratio = std::clamp(p->MyResistance, 0.0, 1.0);
			if (ratio > 0 && ratio < 1) { _event.MyDamage.MyMultiplier *= 1 - ratio; _event.MyDamage.MySteadCut = ratio; }
			return;
		}
		if (_event.MyKind == ContentEventKind::LAYER_GAIN && _event.MyPlayer < _MyAddonBonds.size())
		{
			auto& state = _MyAddonBonds[_event.MyPlayer];
			const auto found = std::ranges::find(AddonBondIds, _event.MyBondId);
			if (found != AddonBondIds.end())
			{
				const auto slot = static_cast<std::size_t>(found - AddonBondIds.begin());
				if (slot <= Slot(AddonBondKind::INDOMITABLE) && state.MyParameters[slot] && !state.MyPending)
				{
					state.MyPending = true;
					Schedule({.MyAt = Time(), .MyKind = ScheduledKind::BOND_REFRESH, .MyHandle = _event.MyPlayer});
				}
			}
		}
		if (_event.MyKind == ContentEventKind::BATTLE_START)
			for (std::size_t i = 0; i < _MyAddonBonds.size(); ++i) UpdateBondAura(i);
		if ((_event.MyKind == ContentEventKind::DEPLOY || _event.MyKind == ContentEventKind::DEATH || _event.MyKind == ContentEventKind::SKILL_END) && _event.MyUnit)
		{
			auto& unit = _MyUnits[Index(_event.MyUnit)];
			if (unit.MySide != UnitSide::ALLY || unit.MyOwner >= _MyAddonBonds.size()) return;
			auto& state = _MyAddonBonds[unit.MyOwner];
			if (unit.MyKind == UnitKind::OPERATOR && _event.MyKind != ContentEventKind::SKILL_END) UpdateBondAura(unit.MyOwner);
			if (_event.MyKind == ContentEventKind::DEPLOY)
			{
				const auto p = state.MyParameters[Slot(AddonBondKind::SOLO)];
				if (p && unit.MyKind == UnitKind::OPERATOR && std::ranges::contains(state.MyMembers[Slot(AddonBondKind::SOLO)], unit.MyId) && p->MySp > 0)
					(void)GainSp(unit.MyId, p->MySp, SpReason::INITIAL);
				const auto elite = state.MyParameters[Slot(AddonBondKind::ELITE)];
				if (elite && state.MyTiers[Slot(AddonBondKind::ELITE)] >= 2 && unit.MyKind == UnitKind::TOKEN && unit.MyOwnerUnit && Unit(unit.MyOwnerUnit).MyDefinition.MyIdentity.MyGolden)
					ApplyEliteSpCost(unit, *elite);
			}
			else if (_event.MyKind == ContentEventKind::SKILL_END && unit.MyKind == UnitKind::OPERATOR && unit.MyAlive &&
				_event.MySkillReason != SkillReason::DEATH && unit.MyDefinition.MySkill.MyKind != SkillKind::NONE)
			{
				if (const auto p = state.MyParameters[Slot(AddonBondKind::SWIFT)])
				{
					const auto layers = AddonLayers(state, AddonBondKind::SWIFT);
					const auto chance = std::clamp(p->MyProbability + p->MyProbabilityPerLayer * layers, 0.0, 1.0);
					if (std::ranges::contains(state.MyMembers[Slot(AddonBondKind::SWIFT)], unit.MyId) && _MyRandom.Next() < chance) (void)GainSp(unit.MyId, p->MySp);
					if (layers >= p->MyMilestone && _MyRandom.Next() < chance) (void)GainSp(unit.MyId, p->MyExtraSp);
				}
			}
		}
		if (_event.MyKind != ContentEventKind::DAMAGED || !_event.MyTarget) return;
		auto& target = _MyUnits[Index(_event.MyTarget)];
		if (target.MySide == UnitSide::ALLY)
		{
			if (target.MyKind != UnitKind::OPERATOR || target.MyOwner >= _MyAddonBonds.size()) return;
			auto& state = _MyAddonBonds[target.MyOwner];
			const auto p = state.MyParameters[Slot(AddonBondKind::STEADFAST)];
			if (!p || state.MyTiers[Slot(AddonBondKind::STEADFAST)] < 2) return;
			const auto& members = state.MyMembers[Slot(AddonBondKind::STEADFAST)];
			if (_event.MyDamage.MySteadCut != 0)
			{
				const auto ratio = _event.MyDamage.MySteadCut; _event.MyDamage.MySteadCut = 0;
				const auto share = _event.MyAmount / (1 - ratio) * ratio;
				if (!(share > 0)) return;
				// 回调可能击倒／复活其他成员；本次分摊的收件人和分母在首个命中前固定。
				struct DepthGuard
				{
					std::size_t& MyDepth;
					explicit DepthGuard(std::size_t& _depth) : MyDepth(_depth) { ++MyDepth; }
					~DepthGuard() { --MyDepth; }
				};

				if (state.MyShareDepth >= 32) throw std::runtime_error("recursive bond damage limit");
				if (state.MyShareDepth == state.MyShareFrames.size()) state.MyShareFrames.emplace_back();
				auto& alive = state.MyShareFrames[state.MyShareDepth]; alive.clear(); alive.reserve(members.size());
				DepthGuard guard(state.MyShareDepth);
				for (const auto id : members) if (Unit(id).MyAlive && !Unit(id).MyRemoved) alive.push_back(id);
				for (const auto id : alive) (void)DealDamage(0, id, DamageInfo{.MyAmount = share / static_cast<double>(alive.size()),
					.MyType = DamageType::TRUE_DAMAGE, .MyCanDodge = false, .MyTags = static_cast<DamageTags>(DamageTag::STEAD_SHARE)});
				return;
			}
			if (_event.MyElement || HasTag(_event.MyDamage.MyTags, DamageTag::STEAD_SHARE) || HasTag(_event.MyDamage.MyTags, DamageTag::STEAD_THORN) ||
				HasTag(_event.MyDamage.MyTags, DamageTag::HP_LOSS) || !std::ranges::contains(members, target.MyId) || Time() - target.MySteadThornAt < p->MyCooldown - 1e-9) return;
			target.MySteadThornAt = Time();
			if (!_event.MySource || Unit(_event.MySource).MySide != UnitSide::ENEMY || !Unit(_event.MySource).MyAlive) return;
			(void)DealDamage(target.MyId, _event.MySource, DamageInfo{.MyAmount = std::max(0.0, p->MyThornDamage + p->MyThornPerLayer * AddonLayers(state, AddonBondKind::STEADFAST)),
				.MyType = DamageType::ARTS, .MyCanDodge = false, .MySourceless = true, .MyTags = static_cast<DamageTags>(DamageTag::STEAD_THORN)});
			if (Unit(_event.MySource).MyAlive) (void)ApplyStatus(_event.MySource, CombatStatus::FRAGILE,
				StatusApplication{.MyDuration = p->MyDuration, .MySource = target.MyId, .MyValue = p->MyFragile - 1});
			return;
		}
		if (_event.MyElement || _event.MyDamage.MyType != DamageType::ARTS || !target.MyAlive || !_event.MySource) return;
		const auto& source = Unit(_event.MySource);
		if (source.MyKind != UnitKind::OPERATOR || source.MySide != UnitSide::ALLY || source.MyOwner >= _MyAddonBonds.size()) return;
		const auto& state = _MyAddonBonds[source.MyOwner];
		const auto p = state.MyParameters[Slot(AddonBondKind::ARCANE)];
		if (!p || !std::ranges::contains(state.MyMembers[Slot(AddonBondKind::ARCANE)], source.MyId)) return;
		const bool low = state.MyTiers[Slot(AddonBondKind::ARCANE)] >= 2 && target.MyHealth / target.MyStats.MyMaxHealth < p->MyHealthThreshold;
		(void)ApplyStrongest(target.MyId, "bond:arcaneShip", p->MyDuration,
			BuffStrength{.MyValue = low ? state.MyArcaneLowMultiplier : state.MyArcaneMultiplier, .MyAttribute = Attribute::ARTS_TAKEN_MULTIPLIER}, source.MyId);
	}
}
