#include <stronghold/domain/wave_generation.hpp>

namespace Stronghold
{
	namespace
	{
		constexpr std::string_view ExcludedEnemy = "enemy_9012_acloon";

		template <class _ValueType, class _Eligible, class _Weight>
		const _ValueType* Pick(Random& _random, std::span<const _ValueType> _values, _Eligible _eligible, _Weight _weight)
		{
			const _ValueType* first = nullptr;
			const _ValueType* last = nullptr;
			double total = 0;
			for (const auto& value : _values)
				if (_eligible(value))
				{
					if (!first) first = &value;
					last = &value;
					total += std::max(0.0, _weight(value));
				}
			if (!first || !std::isgreater(total, 0)) return first;
			auto draw = _random.Next() * total;
			for (const auto& value : _values)
				if (_eligible(value))
				{
					draw -= std::max(0.0, _weight(value));
					if (std::isless(draw, 0)) return &value;
				}
			return last;
		}

		std::string_view PickId(Random& _random, std::span<const WeightedId> _values)
		{
			const auto* result = Pick(_random, _values, [](const auto&) { return true; }, [](const auto& _value) { return _value.MyWeight; });
			return result ? result->MyId : std::string_view{};
		}

		constexpr bool FlyingSlot(WaveSlot _slot) noexcept
		{ return _slot == WaveSlot::NF || _slot == WaveSlot::EF || _slot == WaveSlot::SF || _slot == WaveSlot::TF; }

		WaveSlot Slot(std::string_view _slot) noexcept
		{
			constexpr std::array<std::string_view, 9> Names{"", "N", "E", "S", "NF", "EF", "SF", "T", "TF"};
			const auto it = std::ranges::find(Names, _slot);
			return it == Names.end() ? WaveSlot::NONE : static_cast<WaveSlot>(it - Names.begin());
		}

		WaveSlot ClassOf(const WaveEnemyParameters& _enemy) noexcept
		{ return _enemy.MyFlying ? (_enemy.MyRank == EnemyRank::ELITE ? WaveSlot::EF : WaveSlot::NF) : (_enemy.MyRank == EnemyRank::ELITE ? WaveSlot::E : WaveSlot::N); }
	}

	WaveGenerator::WaveGenerator(const WaveGenerationRules& _rules, const WaveModeRules& _mode, std::span<const WaveRecord> _waves)
		: _MyRules(_rules), _MyMode(_mode), _MyWaves(_waves)
	{
		if (!_rules.MyRoundCount || !_rules.MyMinReplacement || _rules.MyMinReplacement > _rules.MyMaxReplacement ||
			!std::ranges::is_sorted(_rules.MyEntries, {}, &FactionEntry::MyEnemyId) ||
			!std::ranges::is_sorted(_rules.MyEnemies, {}, &WaveEnemyParameters::MyId) ||
			!std::ranges::is_sorted(_waves, {}, &WaveRecord::MyId)) throw std::invalid_argument("invalid wave generation tables");
		for (const auto& enemy : _rules.MyEnemies)
			if (!std::isfinite(enemy.MyPower) || std::isless(enemy.MyPower, 0) || !std::isfinite(enemy.MyFactor) || !std::isgreater(enemy.MyFactor, 0))
				throw std::invalid_argument("invalid enemy battle effectiveness");
		for (const auto& entry : _rules.MyEntries)
			if (!std::isfinite(entry.MyWeight)) throw std::invalid_argument("invalid faction weight");
		for (const auto list : {_mode.MyStages, _mode.MyBosses, _mode.MyHiddenBosses})
			for (const auto& entry : list)
				if (!std::isfinite(entry.MyWeight)) throw std::invalid_argument("invalid setup weight");
	}

	const WaveEnemyParameters* WaveGenerator::Enemy(std::string_view _id) const noexcept
	{
		const auto it = std::ranges::lower_bound(_MyRules.MyEnemies, _id, {}, &WaveEnemyParameters::MyId);
		return it != _MyRules.MyEnemies.end() && it->MyId == _id ? &*it : nullptr;
	}

	const WaveRecord* WaveGenerator::Template(std::string_view _id) const noexcept
	{
		const auto it = std::ranges::lower_bound(_MyWaves, _id, {}, &WaveRecord::MyId);
		return it != _MyWaves.end() && it->MyId == _id ? &*it : nullptr;
	}

	const WaveRoundRules* WaveGenerator::Round(unsigned _round) const noexcept
	{
		const auto it = std::ranges::find(_MyMode.MyRounds, _round, &WaveRoundRules::MyRound);
		return it != _MyMode.MyRounds.end() ? &*it : nullptr;
	}

	std::optional<WavePick> WaveGenerator::PickRound(Random& _random, FactionType _type, unsigned _round) const
	{
		const bool firstHalf = _round <= _MyRules.MyFirstHalfLastRound;
		const auto* entry = Pick(_random, _MyRules.MyEntries, [&](const FactionEntry& _entry)
		{
			return _entry.MyType == _type && _entry.MyFirstHalf == firstHalf && _entry.MyEnemyId != ExcludedEnemy && Enemy(_entry.MyEnemyId) &&
				std::ranges::find(_MyMode.MyInactiveEnemies, _entry.MyEnemyId) == _MyMode.MyInactiveEnemies.end();
		}, [](const FactionEntry& _entry) { return _entry.MyWeight; });
		if (!entry) return _type == FactionType::SPECIAL ? std::nullopt : PickRound(_random, FactionType::SPECIAL, _round);
		const auto attached = [&](std::span<const std::string_view> _keys)
		{
			// 附带普通／精英不受模式禁用名单过滤，保持原列表顺序和一次随机取样。
			const auto* selected = Pick(_random, _keys, [&](std::string_view _key) { return _key != ExcludedEnemy && Enemy(_key); },
				[](std::string_view) { return 1.0; });
			return selected ? *selected : std::string_view{};
		};
		return WavePick{.MyRound = _round, .MyType = entry->MyType, .MySpecial = entry->MyEnemyId,
			.MyNormal = attached(entry->MyNormal), .MyElite = attached(entry->MyElite), .MyFlying = entry->MyFlying, .MyFirstHalf = firstHalf};
	}

	WaveSetup WaveGenerator::Setup(Random& _random) const
	{
		WaveSetup setup{.MyStageId = PickId(_random, _MyMode.MyStages)};
		if (setup.MyStageId.empty()) setup.MyStageId = _MyMode.MyFallbackStage;
		std::vector<FactionType> chosen;
		chosen.reserve(_MyRules.MyFactions.size());
		for (const auto& type : _MyRules.MyFactions) if (type.MyRandom) chosen.emplace_back(type.MyType);
		_random.Shuffle(chosen.begin(), chosen.end());
		chosen.resize(std::min(chosen.size(), static_cast<std::size_t>(_MyRules.MyFactionCount)));
		setup.MyFactions = chosen;
		const auto definition = [&](FactionType _type) -> const FactionDefinition&
		{
			const auto found = std::ranges::find(_MyRules.MyFactions, _type, &FactionDefinition::MyType);
			if (found == _MyRules.MyFactions.end()) throw std::logic_error("missing faction definition");
			return *found;
		};
		std::ranges::stable_sort(setup.MyFactions, [&](FactionType _left, FactionType _right) { return definition(_left).MySort < definition(_right).MySort; });
		setup.MyBossId = PickId(_random, _MyMode.MyBosses);
		if (_MyMode.MyHiddenRound) setup.MyHiddenBossId = PickId(_random, _MyMode.MyHiddenBosses);
		Random schedule(DeriveSeed(_random.State(), u"waves:schedule"));
		setup.MyTypeSlots.reserve(_MyRules.MyRoundCount);
		for (const auto type : chosen)
			for (unsigned n = 0; n < definition(type).MyCount && setup.MyTypeSlots.size() < _MyRules.MyRoundCount; ++n)
				setup.MyTypeSlots.emplace_back(type);
		setup.MyTypeSlots.resize(_MyRules.MyRoundCount, _MyRules.MyFillType);
		schedule.Shuffle(setup.MyTypeSlots.begin(), setup.MyTypeSlots.end());
		setup.MyPicks.reserve(static_cast<std::size_t>(_MyRules.MyRoundCount) + 1);
		setup.MyPicks.emplace_back();
		for (std::size_t i = 0; i < setup.MyTypeSlots.size(); ++i)
			setup.MyPicks.emplace_back(PickRound(schedule, setup.MyTypeSlots[i], static_cast<unsigned>(i + 1)));
		return setup;
	}

	unsigned WaveGenerator::ReplacedCount(std::string_view _templateEnemy, unsigned _count, std::string_view _enemy) const
	{
		const auto* original = Enemy(_templateEnemy);
		const auto* replacement = Enemy(_enemy);
		// 每一步显式二进制 32 位舍入；不能让编译器合并成一次双精度运算。
		const auto origin = static_cast<float>(static_cast<double>(static_cast<float>(_count * (original ? original->MyPower : 0))) /
			(original ? original->MyFactor : 1));
		const auto unit = static_cast<float>((replacement ? replacement->MyPower : 0) / (replacement ? replacement->MyFactor : 1));
		if (!std::isgreater(unit, 0)) return _MyRules.MyMinReplacement;
		const auto ratio = static_cast<double>(static_cast<float>(static_cast<double>(origin) / unit));
		if (!std::isless(ratio, _MyRules.MyMaxReplacement)) return _MyRules.MyMaxReplacement;
		if (!std::isgreater(ratio, _MyRules.MyMinReplacement)) return _MyRules.MyMinReplacement;
		const auto floor = std::floor(ratio);
		const auto rounded = !std::islessgreater(ratio - floor, 0.5) ? floor + (static_cast<unsigned>(floor) % 2) : std::floor(ratio + 0.5);
		return std::clamp(static_cast<unsigned>(rounded), _MyRules.MyMinReplacement, _MyRules.MyMaxReplacement);
	}

	WavePlan WaveGenerator::BuildNormal(const WaveSetup& _setup, unsigned _round) const
	{
		const auto* round = Round(_round);
		return Build(round ? round->MyTemplateId : std::string_view{}, _setup, _round, round ? round->MyTimeLimit : 120);
	}

	WavePlan WaveGenerator::BuildBoss(const WaveSetup& _setup, unsigned _round, std::string_view _boss, bool _solo) const
	{
		const auto* round = Round(_round);
		std::string_view id;
		if (round && !round->MyBossTemplates.empty())
		{
			const auto found = std::ranges::find(round->MyBossTemplates, _boss, &BossTemplate::MyBossId);
			id = (found == round->MyBossTemplates.end() ? round->MyBossTemplates.front() : *found).MyTemplateId;
		}
		// 返回静态表中的 ID，避免让计划借用此处临时构造的字符串。
		if (_solo && !id.empty() && !id.ends_with("_s"))
		{
			if (const auto* solo = Template(std::string(id) + "_s")) id = solo->MyId;
		}
		else if (!_solo && id.ends_with("_s"))
			if (const auto* pair = Template(id.substr(0, id.size() - 2))) id = pair->MyId;
		return Build(id, _setup, _round, std::numeric_limits<double>::infinity());
	}

	WavePlan WaveGenerator::Build(std::string_view _id, const WaveSetup& _setup, unsigned _round, double _timeLimit) const
	{
		WavePlan plan{.MyTemplateId = _id, .MyTimeLimit = _timeLimit};
		const auto* wave = Template(_id);
		if (!wave) return plan;
		if (_round < _setup.MyPicks.size()) plan.MyPick = _setup.MyPicks[_round];
		const auto* round = Round(_round);
		const auto scale = round ? round->MyScale : WaveScale{};
		const bool leader = wave->MyKind == WaveKind::BOSS || wave->MyKind == WaveKind::HIDDEN;
		plan.MySpawns.reserve(wave->MySpawns.size()); plan.MyActions.reserve(wave->MySpawns.size());
		for (std::size_t i = 0; i < wave->MySpawns.size(); ++i)
		{
			const auto& group = wave->MySpawns[i];
			if (group.MyAction != WaveAction::SPAWN || group.MyEnemyId == ExcludedEnemy) continue;
			const bool boss = group.MyTag == EnemySpawnTag::BOSS, part = group.MyTag == EnemySpawnTag::PART;
			const auto n = std::clamp(group.MyCount, 1U, 50U);
			const auto window = n * std::max(0.0, group.MyInterval), time = std::max(0.0, group.MyTime);
			auto key = group.MyEnemyId;
			auto count = n;
			auto step = std::max(0.0, group.MyInterval);
			const auto placeholder = std::ranges::find(_MyRules.MyPlaceholders, key, &WavePlaceholder::MyEnemyId);
			if (!boss && !part && placeholder != _MyRules.MyPlaceholders.end() && plan.MyPick)
			{
				const auto slot = placeholder->MySlot;
				const auto& pick = *plan.MyPick;
				const auto replacement = slot == WaveSlot::S || slot == WaveSlot::SF ? pick.MySpecial :
					slot == WaveSlot::E || slot == WaveSlot::EF ? pick.MyElite : pick.MyNormal;
				if (const auto* enemy = Enemy(replacement))
				{
					if (enemy->MyFlying != FlyingSlot(slot))
					{
						plan.MyActions.emplace_back(WavePlannedAction{.MyIndex = i, .MyTemplateEnemy = key, .MySlot = slot,
							.MyTime = time, .MyTemplateCount = n, .MyWindow = window, .MyRoute = group.MyRoute});
						continue;
					}
					count = ReplacedCount(key, n, replacement);
					step = std::max(window / count, 0.05 * window);
					key = replacement;
				}
			}
			const auto* enemy = Enemy(key);
			if (!enemy) continue;
			const auto slot = group.MySlot.empty() ? ClassOf(*enemy) : Slot(group.MySlot);
			const auto start = group.MyRoute < wave->MyRoutes.size() ? std::optional(wave->MyRoutes[group.MyRoute].MyStart) : std::nullopt;
			plan.MySpawns.emplace_back(WavePlannedSpawn{.MyTime = time, .MyEnemyId = key, .MyRoute = group.MyRoute, .MyCount = count,
				.MyInterval = count > 1 ? step : 0, .MyScale = boss ? WaveScale{.MyAttack = scale.MyAttack, .MySpeed = scale.MySpeed} : scale,
				.MySlot = slot, .MyTag = group.MyTag, .MyActionIndex = i, .MyCounted = !group.MyUnharmful && !part,
				.MyPreview = WavePreview{.MyUpper = start && std::isgreaterequal(start->MyY + (leader ? 13 : 6), 18),
					.MyStart = start, .MyFlying = enemy->MyFlying, .MyElite = enemy->MyRank != EnemyRank::NORMAL, .MyBoss = boss}});
			plan.MyActions.emplace_back(WavePlannedAction{.MyIndex = i, .MyEnemyId = key, .MyTemplateEnemy = group.MyEnemyId,
				.MySlot = Slot(group.MySlot), .MyTime = time, .MyCount = count, .MyTemplateCount = n, .MyWindow = window,
				.MyRoute = group.MyRoute, .MyValid = true, .MyServer = !boss && group.MyGroup.empty()});
		}
		return plan;
	}

	std::vector<EnemySpawn> WaveGenerator::MakeSpawns(const WavePlan& _plan, std::span<const BattlePlayerInput> _players, FieldRect _rect) const
	{
		std::vector<EnemySpawn> output;
		const auto* wave = Template(_plan.MyTemplateId);
		if (!wave) return output;
		if (_players.empty() || wave->MyRoutes.empty()) throw std::invalid_argument("wave requires players and routes");
		std::size_t capacity = 0;
		for (const auto& spec : _plan.MySpawns) capacity += spec.MyCount;
		output.reserve(capacity);
		for (const auto& spec : _plan.MySpawns)
		{
			const auto& route = wave->MyRoutes[spec.MyRoute < wave->MyRoutes.size() ? spec.MyRoute : 0];
			const auto found = std::ranges::find_if(_players, [&](const BattlePlayerInput& _player)
			{ return spec.MyOwnerPlayer.empty() ? _player.MyRightHalf.value_or(_player.MyMirrorDeployment) == std::isgreaterequal(route.MyStart.MyX, 11) : _player.MyPlayerId == spec.MyOwnerPlayer; });
			if (!spec.MyOwnerPlayer.empty() && found == _players.end()) throw std::invalid_argument("bounty owner is not a battle player");
			const auto& owner = found == _players.end() ? _players.front() : *found;
			const auto& record = wave->Enemy(spec.MyEnemyId);
			auto definition = record.MakeDefinition();
			definition.MyStats.MyMaxHealth *= spec.MyScale.MyHealth;
			definition.MyStats.MyAttack *= spec.MyScale.MyAttack;
			definition.MyStats.MyMoveSpeed *= spec.MyScale.MySpeed;
			definition.MyStats.MyDefense *= spec.MyScale.MyDefense;
			definition.MyStats.MyResistance *= spec.MyScale.MyResistance;
			constexpr std::array<std::string_view, 9> Slots{"", "N", "E", "S", "NF", "EF", "SF", "T", "TF"};
			if (static_cast<std::size_t>(spec.MySlot) >= Slots.size()) throw std::invalid_argument("invalid wave slot");
			const auto mods = spec.MyOriginalModifiers.value_or(EnemySpawnModifiers{
				.MyHealth = spec.MyTag == EnemySpawnTag::BOSS ? std::nullopt : std::optional(spec.MyScale.MyHealth),
				.MyAttack = spec.MyScale.MyAttack, .MyDefense = spec.MyScale.MyDefense, .MyResistance = spec.MyScale.MyResistance,
				.MySpeed = spec.MyScale.MySpeed, .MySupplyHealth = spec.MyScale.MySupplyHealth,
				.MySlot = std::string(Slots[static_cast<std::size_t>(spec.MySlot)]), .MyBountyId = spec.MyBountyId});
			const auto reward = spec.MyCoins > 0 ? std::optional(BountyReward{.MyCoins = static_cast<double>(spec.MyCoins), .MyOwnerId = spec.MyRewardOwner}) : std::nullopt;
			const auto compiled = route.Compile(_rect);
			for (unsigned n = 0; n < spec.MyCount; ++n)
				output.emplace_back(EnemySpawn{.MyTime = spec.MyTime + n * spec.MyInterval, .MyOwnerId = owner.MyPlayerId,
					.MyDefinition = definition, .MyRoute = compiled, .MyLifeCost = record.MyLifeCost,
					.MyCounted = spec.MyCounted && record.MyCounted && spec.MyTag != EnemySpawnTag::BOSS && spec.MyTag != EnemySpawnTag::PART,
					.MyTag = spec.MyTag, .MyModifiers = mods, .MySourcePlayer = spec.MySourcePlayer, .MyBounty = reward});
		}
		std::ranges::stable_sort(output, {}, &EnemySpawn::MyTime);
		return output;
	}

	std::vector<CombatRoute> WaveGenerator::MakeGroundRoutes(const WavePlan& _plan, FieldRect _rect) const
	{
		const auto* wave = Template(_plan.MyTemplateId);
		if (!wave) return {};
		std::vector<std::size_t> used;
		used.reserve(_plan.MySpawns.size());
		for (const auto& spec : _plan.MySpawns)
			if (std::ranges::find(used, spec.MyRoute) == used.end()) used.emplace_back(spec.MyRoute);
		return wave->MakeGroundRoutes(used, _rect);
	}
}
