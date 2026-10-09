#include <algorithm>
#include <numeric>
#include <set>
#include <stronghold/simulation/battle.hpp>
#include <stronghold/domain/final_assault.hpp>

namespace Stronghold
{
	namespace
	{
		bool Bounded(double _value, double _minimum = 0, double _maximum = 1e9)
		{
			return std::isfinite(_value) && _value >= _minimum && _value <= _maximum;
		}

		void ValidateSkill(const SkillDefinition& _skill)
		{
			if (static_cast<unsigned>(_skill.MyKind) > static_cast<unsigned>(SkillKind::TOGGLE) || static_cast<unsigned>
				(_skill.MySpType) > static_cast<unsigned>(SpType::NONE) || static_cast<unsigned>(_skill.MyTrigger) >
				static_cast<unsigned>(SkillTrigger::NEVER) || !Bounded(_skill.MySpCost) || !Bounded(_skill.MyInitialSp)
				|| !Bounded(_skill.MyDuration, 0, 3600) || !Bounded(_skill.MyAmmo) || _skill.MyMaxCharges == 0 || !
				Bounded(_skill.MyTriggerHpAtMost, 0, 1) || _skill.MyRangeExtend < -100 || _skill.MyRangeExtend > 100)
				throw std::invalid_argument("invalid skill definition");
			for (const auto* grid : {&_skill.MyRange, &_skill.MyTriggerRange})
				for (const auto offset : *grid)
					if (offset.MyRow < -100 || offset.MyRow > 100 || offset.MyColumn < -100 || offset.MyColumn > 100)
						throw std::invalid_argument("invalid skill range");
			AttributeModifiers modifiers;
			modifiers.Add(_skill.MyModifiers);
		}

		void ValidateAttack(const AttackProfile& _attack)
		{
			if (static_cast<unsigned>(_attack.MyScaling) > static_cast<unsigned>(AttackScaling::REINFORCEMENT) || !Bounded(_attack.MyConditionalScale) ||
				!Bounded(_attack.MyHealFarMultiplier) || !Bounded(_attack.MyHealNearDistance) || !_attack.MyHits || !_attack.MyMaxTargets || !Bounded(_attack.MyAttackScale) || !
				Bounded(_attack.MyDamageMultiplier) || (_attack.MyHitMultiplier && !Bounded(*_attack.MyHitMultiplier))
				|| !Bounded(_attack.MyHealScale) || !Bounded(_attack.MySplashRadius) || !Bounded(_attack.MySplashScale)
				|| !Bounded(_attack.MyChainRadius) || !Bounded(_attack.MyChainFalloff, 0, 1) || !
				Bounded(_attack.MyChainSluggish) || !Bounded(_attack.MyHealChainFalloff, 0, 1) || !
				Bounded(_attack.MyElementHealRatio) || !Bounded(_attack.MyHealHpAtMost, 0, 1) || (_attack.MyOnHitStatus
					&& *_attack.MyOnHitStatus >= CombatStatus::COUNT) || !Bounded(_attack.MyProjectileSpeed, 0, 1000) ||
				static_cast<unsigned>(_attack.MyPriority) > static_cast<unsigned>(TargetPriority::HEAVIEST) ||
				static_cast<unsigned>(_attack.MyDamageType) > static_cast<unsigned>(DamageType::ELEMENTAL))
				throw std::invalid_argument("invalid attack profile");
		}
	}

	void Battle::ValidatePoint(WorldPoint _point, bool _spawn)
	{
		const auto margin = _spawn ? 0.5 : 0.0;
		if (!Bounded(_point.MyX, -margin, 20 + margin) || !Bounded(_point.MyY, -margin, 18 + margin))
			throw std::invalid_argument("combat point outside 19 by 21 field");
	}

	void Battle::ValidateDefinition(const CombatDefinition& _definition, bool _enemy)
	{
		if (_definition.MyHitArea)
		{
			const auto& area = *_definition.MyHitArea;
			if (!std::isfinite(area.MyWidth) || !std::isfinite(area.MyHeight) || !std::isgreater(area.MyWidth, 0) || !
				std::isgreater(area.MyHeight, 0) || !std::isfinite(area.MyOffsetX) || !std::isfinite(area.MyOffsetY))
				throw std::invalid_argument("invalid hit area");
		}
		if (_definition.MyTraitFrontRange)
			for (const auto offset : *_definition.MyTraitFrontRange)
				if (offset.MyRow < -100 || offset.MyRow > 100 || offset.MyColumn < -100 || offset.MyColumn > 100)
					throw std::invalid_argument("invalid trait range offset");
		const auto& profession = _definition.MyProfession;
		if (static_cast<unsigned>(profession.MyKind) > static_cast<unsigned>(ProfessionTrait::DOLLKEEPER) ||
			(_enemy && profession.MyKind != ProfessionTrait::NONE) || !Bounded(profession.MyAmmoMax) ||
			!Bounded(profession.MyFunnelInitial) || !Bounded(profession.MyFunnelDelta) || !Bounded(profession.MyFunnelMax) ||
			profession.MyStoreMax == std::numeric_limits<unsigned>::max() || !Bounded(profession.MyGuardDefense) ||
			!Bounded(profession.MyGuardResistance) || !Bounded(profession.MyDodge, 0, 1))
			throw std::invalid_argument("invalid profession definition");
		for (const auto value : {profession.MySelfHeal, profession.MyHealRatio, profession.MyDpOnKill, profession.MyHpDrain,
			profession.MyMerchantInterval, profession.MyMerchantCost, profession.MyRampMax, profession.MyRampInitial, profession.MyAuraRatio, profession.MyShockScale})
			if (!Bounded(value)) throw std::invalid_argument("invalid profession parameter");
		if (!Bounded(profession.MyDollDuration, 0, 3600) || !Bounded(profession.MyDollHealthMultiplier, 0.05))
			throw std::invalid_argument("invalid doll parameters");
		if (!Bounded(profession.MyRampTime, 1e-9)) throw std::invalid_argument("invalid profession ramp time");
		ValidateSkill(_definition.MySkill);
		ValidateAttack(_definition.MyAttack);
		if (_definition.MySkill.MyAttack)
			ValidateAttack(*_definition.MySkill.MyAttack);
		const auto& stats = _definition.MyStats;
		// 所有基值必须有限，避免 NaN 穿过排序或浮点转整数进入未定义行为。
		for (const auto value : {
				 stats.MyMaxHealth,
				 stats.MyAttack,
				 stats.MyDefense,
				 stats.MyResistance,
				 stats.MyAttackSpeed,
				 stats.MyBaseAttackTime,
				 stats.MyMoveSpeed,
				 stats.MyTaunt,
				 stats.MyRedeploySeconds,
				 stats.MyDeploymentCost,
				 stats.MySpRecovery,
				 stats.MyHealthRegen,
				 stats.MyMass,
				 stats.MyElementalResistance,
				 stats.MyElementResistance,
				 stats.MyDefenseIgnorePercent,
				 stats.MyDefenseIgnoreFlat,
				 stats.MyResistanceIgnorePercent,
				 stats.MyResistanceIgnoreFlat,
				 stats.MyPhysicalDodge,
				 stats.MyArtsDodge,
				 stats.MyDamageDealtMultiplier,
				 stats.MyPhysicalDealtMultiplier,
				 stats.MyArtsDealtMultiplier,
				 stats.MyDamageTakenMultiplier,
				 stats.MyPhysicalTakenMultiplier,
				 stats.MyArtsTakenMultiplier,
				 stats.MyTrueTakenMultiplier,
				 stats.MyElementTakenMultiplier,
				 stats.MyElementalTakenMultiplier,
				 stats.MyHealingDealtMultiplier,
				 stats.MyHealingTakenMultiplier,
				 stats.MyAttackScaleMultiplier,
				 stats.MyRedeployMultiplier,
				 stats.MySpCostFlat,
				 stats.MyExtraTargets,
				 stats.MyBlockRadiusScale
			 })
			if (!std::isfinite(value))
				throw std::invalid_argument("non-finite combat attribute");
		const auto& attack = _definition.MyAttack;
		if (_definition.MyId.empty() || _definition.MyId.size() > 128 || !Bounded(stats.MyMaxHealth, 1) || !
			Bounded(stats.MyAttack) || !Bounded(stats.MyDefense) || !Bounded(stats.MyResistance, 0, 100) || !
			Bounded(stats.MyAttackSpeed, -1e6, 1e6) || !Bounded(stats.MyBaseAttackTime, 0.1, 3600) || !
			Bounded(stats.MyMoveSpeed, 0, 100) || stats.MyBlockCount < 0 || stats.MyBlockCount > 100 || !
			Bounded(stats.MyTaunt, -1e6, 1e6) || !Bounded(stats.MyRedeploySeconds, 0, 3600) || !
			Bounded(stats.MyDeploymentCost) || attack.MyMaxTargets == 0 || attack.MyMaxTargets > 600 || static_cast<
				unsigned>(attack.MyPriority) > static_cast<unsigned>(TargetPriority::HEAVIEST) || static_cast<unsigned>(
				attack.MyDamageType) > static_cast<unsigned>(DamageType::ELEMENTAL) || !
			Bounded(attack.MyProjectileSpeed, 0, 1000) || !Bounded(attack.MyEnemyRange, 0, 100) || !
			Bounded(attack.MyAnimationDuration, 0, 3600) || (attack.MyAnimationHit && !Bounded(
				*attack.MyAnimationHit,
				0,
				attack.MyAnimationDuration
			)) || _definition.MyBlockWeight < 1 || _definition.MyBlockWeight > 100 || _definition.MyRange.size() > 399)
			throw std::invalid_argument("invalid combat definition: " + _definition.MyId);

		for (const auto offset : _definition.MyRange)
			if (offset.MyRow < -100 || offset.MyRow > 100 || offset.MyColumn < -100 || offset.MyColumn > 100)
				throw std::invalid_argument("invalid combat range offset");
	}

	Battle::Battle(BattleInput _input)
		: _MyInput(std::move(_input)), _MyRandom(_MyInput.MySeed)
	{
		if (_MyInput.MyFinalAssault)
		{
			auto& assault = _MyInput.MyFinalAssault->get();
			if (!_MyInput.MyBossBattle || (_MyInput.MySharedBoss && &_MyInput.MySharedBoss->get() != &assault.Pool()))
				throw std::invalid_argument("final assault requires its own boss pool");
			for (const auto& player : _MyInput.MyPlayers)
				if (std::ranges::find(assault.Players(), player.MyPlayerId, &BossPlayerProgress::MyPlayerId) == assault.Players().end())
					throw std::invalid_argument("battle player does not participate in final assault");
			_MyInput.MySharedBoss = std::ref(assault.Pool());
		}
		const bool unlimitedBoss = _MyInput.MyBossBattle && std::isinf(_MyInput.MyTimeLimit) && std::isgreater(_MyInput.MyTimeLimit, 0);
		if (_MyInput.MyPlayers.empty() || _MyInput.MyPlayers.size() > 20 || _MyInput.MySpawns.size() > 10000 ||
			(!unlimitedBoss && !Bounded(_MyInput.MyTimeLimit, BattleClock::StepSeconds, 3600)) || !Bounded(_MyInput.MyInitialDp) || !Bounded(
				_MyInput.MyDpPerSecond
			) || !Bounded(_MyInput.MyMaxDp))
			throw std::invalid_argument("invalid battle input");
		if (_MyInput.MyField)
		{
			_MyGrid.emplace(*_MyInput.MyField);
			_MyHasTerrain = std::ranges::any_of(
				_MyInput.MyField->MyTiles,
				[](const FieldTile& _tile) { return _tile.MyTerrain != FieldTerrain::NONE; }
			) || std::ranges::any_of(_MyInput.MyField->MyAirflow, [](const auto& _flow) { return _flow.has_value(); });
		}
		// 道路仅用于查询，不执行 WAIT 等动作；位置仍在构造期验证，避免取整 NaN 或溢出。
		for (const auto& route : _MyInput.MyGroundRoutes)
		{
			ValidatePoint(route.MyStart, true);
			ValidatePoint(route.MyEnd, true);
			for (const auto& step : route.MySteps)
			{
				if (static_cast<unsigned>(step.MyKind) > static_cast<unsigned>(RouteStepKind::APPEAR))
					throw std::invalid_argument("invalid ground route step");
				if (step.MyKind == RouteStepKind::MOVE || step.MyKind == RouteStepKind::APPEAR)
					ValidatePoint(step.MyPosition, true);
			}
		}
		std::set<std::string, std::less<>> playerIds;
		std::set<std::pair<int, int>> occupied;
		for (const auto& player : _MyInput.MyPlayers)
		{
			if (player.MyPlayerId.empty() || !playerIds.insert(player.MyPlayerId).second || player.MyUnits.size() > 36)
				throw std::invalid_argument("invalid battle player");
			const auto owner = _MyPlayers.size();
			_MyPlayers.emplace_back(BattlePlayerState{.MyPlayerId = player.MyPlayerId, .MyDp = _MyInput.MyInitialDp});
			const auto firstUnit = _MyUnits.size();
			std::set<std::uint64_t> pieceIds;
			for (const auto& deployment : player.MyUnits)
			{
				ValidateDefinition(deployment.MyDefinition, false);
				ValidateContent(deployment.MyDefinition.MyContent, ContentTag::CUSTOM_OPERATOR);
				ValidatePoint(deployment.MyPosition);
				if (deployment.MyKind != UnitKind::OPERATOR && deployment.MyKind != UnitKind::TOKEN)
					throw std::invalid_argument("initial ally must be an operator or token");

				const auto position = deployment.MyPosition;
				if (std::floor(position.MyX) != position.MyX || std::floor(position.MyY) != position.MyY || !occupied.
					emplace(static_cast<int>(position.MyY), static_cast<int>(position.MyX)).second || deployment.
					MyPieceUid == 0 || !pieceIds.insert(deployment.MyPieceUid).second || static_cast<unsigned>(
						deployment.MyFacing) > static_cast<unsigned>(Facing::LEFT))
					throw std::invalid_argument("invalid ally deployment");
				auto& unit = _MyUnits.emplace_back();
				unit.MyId = static_cast<UnitId>(_MyUnits.size());
				_MyAllyIds.emplace_back(unit.MyId);
				unit.MyOwner = owner;
				unit.MyKind = deployment.MyKind;
				unit.MyCarry = deployment.MyCarry;
				unit.MyDeferred = deployment.MyDeferred;
				unit.MyDefinition = deployment.MyDefinition;
				unit.MyStats = ResolveStats(unit.MyDefinition.MyStats, {});
				unit.MyPieceUid = deployment.MyPieceUid;
				unit.MyPosition = unit.MyHome = deployment.MyPosition;
				unit.MyFacing = deployment.MyFacing;
				unit.MyGround = deployment.MyGround;
				unit.MyGroundPassable = deployment.MyGroundPassable;
				if (_MyGrid)
				{
					const auto row = static_cast<int>(unit.MyPosition.MyY), column = static_cast<int>(unit.MyPosition.
								   MyX);
					if (!_MyGrid->InRect(row, column))
						throw std::invalid_argument("deployment outside field rect");
					unit.MyGround = _MyGrid->Tile(row, column).MyLow;
					unit.MyGroundPassable = _MyGrid->GroundPassable(row, column, true);
				}
				unit.MyHealth = unit.MyStats.MyMaxHealth;
			}
			// 棋盘输入可先列出召唤物；全部创建后用同一玩家的棋子 UID 解析拥有者。
			for (std::size_t i = 0; i < player.MyUnits.size(); ++i)
			{
				const auto ownerUid = player.MyUnits[i].MyOwnerPieceUid;
				if (!ownerUid) continue;
				auto& unit = _MyUnits[firstUnit + i];
				for (std::size_t j = firstUnit; j < _MyUnits.size(); ++j)
					if (_MyUnits[j].MyPieceUid == ownerUid && _MyUnits[j].MyKind == UnitKind::OPERATOR)
						unit.MyOwnerUnit = _MyUnits[j].MyId;
				if (unit.MyKind != UnitKind::TOKEN || !unit.MyOwnerUnit)
					throw std::invalid_argument("token owner must be an operator in the same player input");
			}
		}
		if (_MyInput.MySharedBoss)
			for (const auto& player : _MyPlayers) _MyInput.MySharedBoss->get().PreparePlayer(player.MyPlayerId);
		_MyAllyCount = _MyUnits.size();
		// 阵营 ID 表预留初始规模；单位本体分段存放，后续召唤不使已有引用失效。
		_MyAllyIds.reserve(_MyAllyCount + 16);
		_MyEnemyIds.reserve(_MyInput.MySpawns.size());
		for (const auto& spawn : _MyInput.MySpawns)
		{
			ValidateSpawnMetadata(spawn);
			ValidateDefinition(spawn.MyDefinition, true);
			ValidateContent(spawn.MyDefinition.MyContent, ContentTag::CUSTOM_ENEMY);
			ValidatePoint(spawn.MyRoute.MyStart, true);
			ValidatePoint(spawn.MyRoute.MyEnd);
			if (!Bounded(spawn.MyTime, 0, 3600) || spawn.MyRoute.MySteps.size() > 4096 || spawn.MyLifeCost < 0 || spawn.
				MyLifeCost > 1000000)
				throw std::invalid_argument("invalid enemy spawn");
			const auto owner = Owner(spawn.MyOwnerId);
			if (spawn.MyCounted)
				++_MyPlayers[owner].MyTotal;
			for (const auto& step : spawn.MyRoute.MySteps)
			{
				if (static_cast<unsigned>(step.MyKind) > static_cast<unsigned>(RouteStepKind::APPEAR) || !Bounded(
					step.MyWaitSeconds,
					0,
					3600
				))
					throw std::invalid_argument("invalid route step");
				ValidatePoint(step.MyPosition);
			}
		}
		for (const auto& player : _MyInput.MyPlayers)
		{
			auto& state = _MyPlayers[Owner(player.MyPlayerId)];
			state.MyBonds.reserve(player.MyBonds.size()); state.MyLayerGains.reserve(player.MyBonds.size());
			for (const auto& bond : player.MyBonds)
			{
				if (bond.MyCount < 0) throw std::invalid_argument("negative bond count");
				SetBondLayers(player.MyPlayerId, bond.MyId, bond.MyLayers);
				auto& live = *std::ranges::find(state.MyBonds, bond.MyId, &BondLayer::MyId);
				live.MyCount = bond.MyCount; live.MyActive = bond.MyActive; live.MyTier = bond.MyTier;
			}
		}
		std::ranges::stable_sort(_MyInput.MySpawns, {}, &EnemySpawn::MyTime);
		_MyTotal = static_cast<std::size_t>(std::ranges::count(_MyInput.MySpawns, true, &EnemySpawn::MyCounted));
		const auto capacity = _MyAllyCount + _MyInput.MySpawns.size();
		_MyTargets.reserve(capacity);
		_MyTargetCandidates.reserve(capacity);
		_MyProjectiles.reserve(capacity);
		_MyScheduled.reserve(capacity + _MyInput.MyColdWinds.size());
		_MyDueActions.reserve(capacity + _MyInput.MyColdWinds.size());
		for (const auto& wind : _MyInput.MyColdWinds) (void)StartColdWind(wind);
		_MyArrivedProjectiles.reserve(capacity);
		_MyRouteTails.reserve(_MyTotal);
		for (const auto& spawn : _MyInput.MySpawns)
		{
			const auto& route = spawn.MyRoute;
			auto& tail = _MyRouteTails.emplace_back(route.MySteps.size() + 2, 0.0);
			auto position = route.MyStart;
			for (std::size_t i = 0; i <= route.MySteps.size(); ++i)
			{
				const auto step = i == route.MySteps.size()
									  ? RouteStep{.MyKind = RouteStepKind::MOVE, .MyPosition = route.MyEnd}
									  : route.MySteps[i];
				if (step.MyKind == RouteStepKind::MOVE)
					tail[i] = Distance(position, step.MyPosition);
				if (step.MyKind == RouteStepKind::MOVE || step.MyKind == RouteStepKind::APPEAR)
					position = step.MyPosition;
			}
			for (std::size_t i = tail.size() - 1; i > 0; --i)
				tail[i - 1] += tail[i];
		}
		_MyRouteTailVersions.assign(_MyInput.MySpawns.size(), std::numeric_limits<std::uint64_t>::max());
		std::size_t customCount = _MyInput.MyContentBindings.size();
		for (const auto& unit : _MyUnits)
			customCount += IsCustom(unit.MyDefinition.MyContent.MyTag) ? 1U : 0U;
		for (const auto& spawn : _MyInput.MySpawns)
			customCount += IsCustom(spawn.MyDefinition.MyContent.MyTag) ? 1U : 0U;
		_MyContentInstances.reserve(customCount);
		for (const auto& unit : _MyUnits)
			AttachContent(unit.MyDefinition.MyContent, unit.MyId, 0, unit.MyOwner);
		for (const auto id : _MyAllyIds) InstallProfession(_MyUnits[Index(id)]);
		InstallChoiceEffects();
		for (const auto& binding : _MyInput.MyContentBindings)
		{
			const auto tag = binding.MyContent.MyTag;
			if (tag != ContentTag::CUSTOM_BOND && tag != ContentTag::CUSTOM_BOND_EFFECT)
				throw std::invalid_argument("battle content binding must be a custom bond or bond effect");
			ValidateContent(binding.MyContent, tag);
			AttachContent(binding.MyContent, 0, 0, Owner(binding.MyPlayerId));
		}
	}

	std::size_t Battle::Owner(std::string_view _id) const
	{
		for (std::size_t i = 0; i < _MyPlayers.size(); ++i)
			if (_MyPlayers[i].MyPlayerId == _id)
				return i;
		throw std::invalid_argument("unknown battle player");
	}

	void Battle::Start()
	{
		if (_MyStarted || Finished())
			return;
		_MyStarted = true;
		for (const auto& device : _MyInput.MyDevices)
			if (!device.MyAfterDeployment) (void)SpawnDevice(device);
		const auto firstSequence = _MyDeploySequence;
		// 两阶段部署，每阶段按各玩家的第 i 个单位交替执行；初始 ID／行动顺序不改变。
		std::vector<std::vector<std::size_t>> orders(_MyPlayers.size());
		for (std::size_t i = 0; i < orders.size(); ++i) orders[i].reserve(_MyInput.MyPlayers[i].MyUnits.size());
		_MyStartDeploying = true;
		for (const auto kind : {UnitKind::OPERATOR, UnitKind::TOKEN})
		{
			for (auto& order : orders) order.clear();
			for (std::size_t i = 0; i < _MyAllyCount; ++i)
				if (_MyUnits[i].MyKind == kind && !_MyUnits[i].MyDeferred)
					orders[_MyUnits[i].MyOwner].emplace_back(i);
			std::size_t longest = 0;
			for (std::size_t i = 0; i < orders.size(); ++i)
			{
				std::ranges::sort(orders[i], [&](std::size_t _a, std::size_t _b)
				{
					const auto a = _MyUnits[_a].MyHome;
					const auto b = _MyUnits[_b].MyHome;
					if (std::islessgreater(a.MyX, b.MyX))
						return _MyInput.MyPlayers[i].MyMirrorDeployment ? std::isgreater(a.MyX, b.MyX) : std::isless(a.MyX, b.MyX);
					return std::isgreater(a.MyY, b.MyY);
				});
				longest = std::max(longest, orders[i].size());
			}
			for (std::size_t i = 0; i < longest; ++i)
				for (const auto& order : orders)
					if (i < order.size() && !Finished()) Deploy(_MyUnits[order[i]], true);
		}
		_MyStartDeploying = false;
		// 拥有者部署钩子带入的召唤物也参与仇恨重排，但其弹道生命标识保持原值。
		std::vector<UnitId> summons;
		summons.reserve(_MyAllyIds.size());
		for (const auto id : _MyAllyIds)
		{
			const auto& unit = Unit(id);
			if (unit.MyKind == UnitKind::TOKEN && unit.MyAlive && unit.MyDeploySequence > firstSequence)
				summons.emplace_back(id);
		}
		std::ranges::sort(summons, {}, [&](UnitId _id) { return Unit(_id).MyDeploySequence; });
		for (const auto id : summons) _MyUnits[Index(id)].MyAggroSequence = ++_MyDeploySequence;
		for (std::size_t i = 0; i < _MyAllyIds.size() && !Finished(); ++i)
		{
			auto& unit = _MyUnits[Index(_MyAllyIds[i])];
			if (unit.MyKind == UnitKind::OPERATOR && unit.MyAlive && unit.MyCarry && unit.MyCarry->MyDown)
			{
				unit.MyHealth = 0;
				RemoveUnit(unit, RemovalReason::FORCED_EXIT, 0, false);
			}
		}
		for (const auto& device : _MyInput.MyDevices)
			if (device.MyAfterDeployment) (void)SpawnDevice(device);
		for (const auto& turret : _MyInput.MyTurrets) (void)SpawnTurret(turret);
		ContentEvent event{.MyKind = ContentEventKind::BATTLE_START};
		if (!Finished())
			NotifyContent(event);
		// battleStart 的再部署倍率也覆盖刚刚强制退场的干员；按真实退场时间起算。
		for (const auto id : _MyAllyIds)
		{
			auto& unit = _MyUnits[Index(id)];
			if (unit.MyKind == UnitKind::OPERATOR && !unit.MyAlive && !unit.MyRemoved && unit.MyRemovalReason == RemovalReason::FORCED_EXIT)
				unit.MyRespawnAt = unit.MyRemovedAt + std::max(0.0, unit.MyStats.MyRedeploySeconds * unit.MyStats.MyRedeployMultiplier);
		}
	}

	bool Battle::Deploy(
		CombatUnit& _unit,
		bool _initial,
		std::optional<WorldPoint> _tile,
		std::optional<double> _keepSp
	)
	{
		const auto position = _tile.value_or(_unit.MyHome);
		if (_unit.MyAlive || _unit.MyRemoving || !CanDeploy(position, _unit.MyId))
			return false;
		const auto carry = (_initial || (!_unit.MyDeploySequence && _MyStartDeploying)) ? _unit.MyCarry : std::nullopt;
		const auto carrySp = carry && carry->MySp && std::isfinite(*carry->MySp) ? carry->MySp : std::nullopt;
		_unit.MyRemoved = false;
		_unit.MyBody.reset();
		_unit.MyDownAtHome = false;
		_unit.MyAlive = true;
		_unit.MyHidden = false;
		_unit.MyPosition = position;
		if (_MyGrid)
		{
			const auto row = static_cast<int>(position.MyY), column = static_cast<int>(position.MyX);
			const auto tile = _MyGrid->Tile(row, column);
			_unit.MyGround = tile.MyLow && !tile.MyDeploymentElevation.value_or(tile.MyElevated);
			_unit.MyGroundPassable = _MyGrid->GroundPassable(row, column, true);
		}
		_unit.MyHealth = _unit.MyStats.MyMaxHealth;
		_unit.MyAttackCooldown = 0;
		_unit.MyLastAttackAt = -std::numeric_limits<double>::infinity();
		_unit.MyStatuses = {};
		_unit.MyElements = {};
		_unit.MyDeploySequence = ++_MyDeploySequence;
		_unit.MyAggroSequence = _unit.MyDeploySequence;
		Recalculate(_unit);
		_unit.MyHealth = _unit.MyStats.MyMaxHealth;
		if (carry && _unit.MyKind == UnitKind::OPERATOR && carry->MyHealthRatio && std::isfinite(*carry->MyHealthRatio))
			_unit.MyHealth = std::max(1.0, _unit.MyStats.MyMaxHealth * std::clamp(*carry->MyHealthRatio, 0.01, 1.0));
		RefreshRange(_unit);
		ResetSkill(_unit, _initial, carrySp);
		if (_keepSp && !_unit.MySkill.MyActive && _unit.MyDefinition.MySkill.MyKind != SkillKind::PASSIVE)
			SetSpTotal(_unit.MyId, *_keepSp);
		Emit(BattleEventKind::DEPLOYED, _unit.MyId);
		RefreshTerrain(_unit, true);
		ContentEvent event{.MyKind = ContentEventKind::DEPLOY, .MyUnit = _unit.MyId, .MyInitial = _initial};
		NotifyContent(event);
		// 修改而非获得技力：部署时赠送的 SP 不与继承值叠加；已启动的持续技能不会被回填。
		if (carrySp && _unit.MyAlive) SetSpTotal(_unit.MyId, *carrySp);
		return true;
	}

	void Battle::SpawnDue()
	{
		while (_MyNextSpawn < _MyInput.MySpawns.size() && std::islessequal(
			_MyInput.MySpawns[_MyNextSpawn].MyTime,
			Time() + 1e-9
		))
		{
			const auto index = _MyNextSpawn++;
			(void)CreateEnemy(_MyInput.MySpawns[index], index);
			if (Finished())
				return;
		}
	}

	void Battle::Step()
	{
		// 内容回调可以请求结束，但不能重入时间推进；否则同一帧会被嵌套推进两次。
		if (_MyStepping)
			throw std::logic_error("battle step is not reentrant");
		struct StepGuard
		{
			bool& MyFlag;
			explicit StepGuard(bool& _flag) : MyFlag(_flag) { MyFlag = true; }
			~StepGuard() { MyFlag = false; }
		} guard(_MyStepping);
		if (Finished())
			return;
		Start();
		if (Finished())
			return;
		TickScheduled();
		if (Finished())
			return;
		SpawnDue();
		if (Finished())
			return;
		for (auto& player : _MyPlayers)
			player.MyDp = std::min(_MyInput.MyMaxDp, player.MyDp + _MyInput.MyDpPerSecond * BattleClock::StepSeconds);
		TickStatuses();
		TickBuffs();
		if (Finished())
			return;
		// 敌军阶段固定本帧长度；阶段中产生的敌军下一帧行动，友军召唤则沿原规则当帧行动。
		for (std::size_t i = 0, count = _MyEnemyIds.size(); i < count && !Finished(); ++i)
			if (auto& unit = _MyUnits[Index(_MyEnemyIds[i])]; unit.MyAlive)
				UpdateEnemy(unit);
		for (std::size_t i = 0; i < _MyAllyIds.size() && !Finished(); ++i)
			if (auto& unit = _MyUnits[Index(_MyAllyIds[i])]; unit.MyAlive && unit.MyKind != UnitKind::DEVICE)
				UpdateAlly(unit);
		if (Finished())
			return;
		UpdateProjectiles();
		if (Finished())
			return;
		CheckRedeploys();
		SyncBossUnits();
		TickTerrain();
		TickTurrets();
		ContentEvent event{.MyKind = ContentEventKind::TICK, .MyDelta = BattleClock::StepSeconds};
		NotifyContent(event);
		if (_MyContentFault && !Finished())
			Finish(BattleEndReason::FORCED);
		if (Finished())
			return;
		_MyClock.Step();
		const bool livingEnemy = std::ranges::any_of(
			_MyUnits,
			[](const CombatUnit& _unit) { return _unit.MySide == UnitSide::ENEMY && _unit.MyAlive; }
		);
		const bool poolEmpty = _MyInput.MySharedBoss && !std::isgreater(_MyInput.MySharedBoss->get().Health(), 0);
		const bool poolHolds = _MyInput.MySharedBoss && _MyInput.MyBossBattle;
		if (poolEmpty || (_MyInput.MyAutoFinish && !poolHolds && _MyNextSpawn == _MyInput.MySpawns.size() && !livingEnemy))
			Finish(BattleEndReason::CLEARED);
		else if (std::isgreaterequal(Time(), _MyInput.MyTimeLimit - 1e-9))
			Finish(BattleEndReason::TIMEOUT);
	}

	void Battle::Advance(std::uint64_t _ticks)
	{
		while (_ticks-- > 0 && !Finished())
			Step();
	}

	void Battle::ForceEnd(BattleEndReason _reason)
	{
		if (_reason != BattleEndReason::FORCED && _reason != BattleEndReason::TIMEOUT) throw std::invalid_argument("invalid forced battle reason");
		if (Finished())
			return;
		Start();
		if (!Finished()) Finish(_reason);
	}

	void Battle::CheckRedeploys()
	{
		for (std::size_t i = 0; i < _MyAllyIds.size() && !Finished(); ++i)
		{
			const auto& unit = Unit(_MyAllyIds[i]);
			if (unit.MyAlive || unit.MyRemoved || unit.MyKind != UnitKind::OPERATOR || std::isless(
				Time() + 1e-9,
				unit.MyRespawnAt
			))
				continue;
			(void)Redeploy(unit.MyId, false);
		}
	}

	void Battle::Finish(BattleEndReason _reason)
	{
		if (_reason == BattleEndReason::TIMEOUT)
		{
			for (const auto id : _MyEnemyIds)
				if (auto& unit = _MyUnits[Index(id)]; unit.MyAlive && unit.MySpawnTag != EnemySpawnTag::BOSS)
					Leak(unit, true);
			_MyUnspawned = 0;
			for (std::size_t i = _MyNextSpawn; i < _MyInput.MySpawns.size(); ++i)
				if (_MyInput.MySpawns[i].MyCounted)
					++_MyUnspawned;
			_MyTotal -= _MyUnspawned;
			for (std::size_t i = _MyNextSpawn; i < _MyInput.MySpawns.size(); ++i)
				if (_MyInput.MySpawns[i].MyCounted)
					--_MyPlayers[Owner(_MyInput.MySpawns[i].MyOwnerId)].MyTotal;
		}
		_MyReason = _reason;
		if (_MyInput.MySharedBoss) _MyBossHealthAtEnd = _MyInput.MySharedBoss->get().Health();
		_MyProjectiles.clear();
		Emit(BattleEventKind::FINISHED);
		ContentEvent event{.MyKind = ContentEventKind::BATTLE_END};
		NotifyContent(event);
	}

	BattleResult Battle::Result() const
	{
		BattleResult result{
			.MyReason = _MyReason,
			.MyTime = Time(),
			.MyKilled = _MyKilled,
			.MyLeaked = _MyLeaked,
			.MyTotal = _MyTotal,
			.MyUnspawned = _MyUnspawned,
			.MyPlayers = _MyPlayers,
			.MyBossHealthLeft = Finished() ? _MyBossHealthAtEnd : _MyInput.MySharedBoss ? std::optional<double>(_MyInput.MySharedBoss->get().Health()) : std::nullopt
		};
		for (std::size_t i = 0; i < result.MyPlayers.size(); ++i)
			result.MyPlayers[i].MyUnitsEnd.reserve(_MyInput.MyPlayers[i].MyUnits.size());
		for (const auto id : _MyAllyIds)
		{
			const auto& unit = Unit(id);
			if (!unit.MyPieceUid || (unit.MyKind != UnitKind::OPERATOR && unit.MyKind != UnitKind::TOKEN)) continue;
			result.MyPlayers[unit.MyOwner].MyUnitsEnd.emplace_back(UnitEndState{
				.MyPieceUid = unit.MyPieceUid,
				.MyId = unit.MyId,
				.MyDefinitionId = unit.MyDefinition.MyId,
				.MyKind = unit.MyKind,
				.MyHealthRatio = unit.MyAlive ? std::clamp(unit.MyHealth / unit.MyStats.MyMaxHealth, 0.0, 1.0) : 0.0,
				.MySp = std::round(SpTotal(id) * 100) / 100,
				.MySkillActive = unit.MySkill.MyActive && unit.MyDefinition.MySkill.MyKind != SkillKind::PASSIVE,
				.MyAlive = unit.MyAlive});
		}
		if (_MyReason == BattleEndReason::TIMEOUT)
		{
			result.MyPendingEnemies.reserve(_MyInput.MySpawns.size() - _MyNextSpawn);
			for (std::size_t i = _MyNextSpawn; i < _MyInput.MySpawns.size(); ++i)
			{
				const auto& spawn = _MyInput.MySpawns[i];
				result.MyPendingEnemies.emplace_back(PendingEnemy{.MyEnemyId = spawn.MyDefinition.MyId, .MyTime = spawn.MyTime,
					.MyTag = spawn.MyTag, .MySourcePlayer = spawn.MySourcePlayer});
			}
		}
		return result;
	}

	void Battle::Emit(
		BattleEventKind _kind,
		UnitId _source,
		UnitId _target,
		double _amount,
		std::optional<CombatStatus> _status,
		std::size_t _player
	)
	{
		constexpr std::size_t eventLimit = 50000;
		if (_MyEvents.size() == eventLimit)
			_MyEvents.pop_front();
		_MyEvents.emplace_back(BattleEvent{.MyKind = _kind, .MyTick = Tick(), .MySource = _source, .MyTarget = _target,
			.MyAmount = _amount, .MyStatus = _status, .MyPlayer = _player});
	}

	std::vector<BattleEvent> Battle::DrainEvents()
	{
		std::vector<BattleEvent> events(_MyEvents.begin(), _MyEvents.end());
		_MyEvents.clear();
		return events;
	}
}
