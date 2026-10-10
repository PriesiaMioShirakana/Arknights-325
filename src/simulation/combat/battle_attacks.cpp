#include <algorithm>
#include <ranges>
#include <utility>
#include "battle_core.hpp"

namespace Stronghold
{
	namespace
	{
		// 异常也必须归还工作区层级；容量保留给下一次攻击，不逐次分配和销毁。
		struct AttackGuard
		{
			std::size_t& MyDepth;
			~AttackGuard() { --MyDepth; }
		};
	}

	BattleCore::AttackScratch& BattleCore::AcquireAttackScratch()
	{
		if (_MyAttackDepth >= 32) throw std::runtime_error("recursive attack limit");
		if (_MyAttackDepth == _MyAttackScratch.size()) _MyAttackScratch.emplace_back();
		auto& scratch = _MyAttackScratch[_MyAttackDepth];
		scratch.MyTargets.clear();
		scratch.MySeen.clear();
		scratch.MyBuffIds.clear();
		scratch.MyTargets.reserve(_MyUnits.size());
		scratch.MySeen.reserve(_MyUnits.size());
		++_MyAttackDepth;
		return scratch;
	}

	bool BattleCore::TargetableEnemy(const CombatUnit& _target, const AttackProfile& _profile) const noexcept
	{
		return _target.MyAlive && !_target.MyHidden && !_target.MyStatuses.Has(CombatStatus::UNTARGETABLE) &&
			(!_target.MyStatuses.Has(CombatStatus::SLEEP) || _profile.MyHitSleep) &&
			!EnemyStealthed(_target) &&
			(!_target.Flying() || (_profile.MyCanHitFlying && !_profile.MyGroundOnly));
	}

	double BattleCore::AbsorbShields(CombatUnit& _unit, double _amount, DamageType _type)
	{
		if (!std::isgreater(_amount, 0)) return 0;
		const auto mask = 1U << static_cast<unsigned>(_type);
		for (auto& shield : _unit.MyShields)
			if ((shield.MyTypeMask & mask) && shield.MyHits > 0)
			{
				--shield.MyHits;
				return 0;
			}
		auto& scratch = AcquireAttackScratch();
		const AttackGuard guard{.MyDepth = _MyAttackDepth};
		scratch.MyBuffIds.reserve(_unit.MyBuffs.size());
		for (const auto& buff : _unit.MyBuffs)
			if (buff.MyDefinition.MyShield.MyTypeMask & mask) scratch.MyBuffIds.emplace_back(buff.MyId);
		// 移除耗尽的纯护盾可能触发 CUSTOM 回调，编号快照保证外层遍历不引用失效对象。
		const auto empty = [](const BuffDefinition& _definition)
		{
			return !std::isgreater(_definition.MyShield.MyHealth, 1e-9) && _definition.MyShield.MyHits == 0 &&
				!_definition.MyModifiers && !_definition.MyFlags;
		};
		for (const auto id : scratch.MyBuffIds)
		{
			const auto buff = std::ranges::find(_unit.MyBuffs, id, &CombatBuff::MyId);
			if (buff == _unit.MyBuffs.end() || buff->MyDefinition.MyShield.MyHits <= 0) continue;
			--buff->MyDefinition.MyShield.MyHits;
			if (empty(buff->MyDefinition)) (void)RemoveBuff(_unit.MyId, id);
			return 0;
		}
		for (auto& shield : _unit.MyShields)
			if (shield.MyTypeMask & mask)
			{
				const auto take = std::min(_amount, shield.MyHealth);
				shield.MyHealth -= take;
				_amount -= take;
			}
		for (const auto id : scratch.MyBuffIds)
		{
			if (!std::isgreater(_amount, 0) || !_unit.MyAlive || Finished()) break;
			const auto buff = std::ranges::find(_unit.MyBuffs, id, &CombatBuff::MyId);
			if (buff == _unit.MyBuffs.end() || !std::isgreater(buff->MyDefinition.MyShield.MyHealth, 0)) continue;
			const auto take = std::min(_amount, buff->MyDefinition.MyShield.MyHealth);
			buff->MyDefinition.MyShield.MyHealth -= take;
			_amount -= take;
			if (empty(buff->MyDefinition)) (void)RemoveBuff(_unit.MyId, id);
		}
		return _amount;
	}

	bool BattleCore::ForceAttack(UnitId _unit, std::span<const UnitId> _targets, bool _noAmmo)
	{
		auto& unit = _MyUnits[Index(_unit)];
		for (const auto id : _targets) (void)Index(id);
		if (!_MyStarted || Finished() || !unit.MyAlive) return false;
		if (_targets.empty()) _targets = unit.MySide == UnitSide::ALLY ? AllyTargets(unit) : EnemyTargets(unit);
		if (_targets.empty()) return false;
		return Attack(unit, _targets, _noAmmo);
	}

	bool BattleCore::Attack(CombatUnit& _source, std::span<const UnitId> _targets, bool _noAmmo)
	{
		auto& scratch = AcquireAttackScratch();
		const AttackGuard guard{.MyDepth = _MyAttackDepth};
		// 显式强制攻击仍消费攻击次数和弹药；目标刚被连锁效果击倒时，命中阶段自行跳过伤害。
		scratch.MyTargets.assign(_targets.begin(), _targets.end());
		if (scratch.MyTargets.empty() || !OperatorBeforeAttack(_source, scratch.MyTargets)) return false;
		++_source.MyTotals.MyAttacks;
		_source.MyLastAttackAt = Time();
		auto profile = EffectiveAttack(_source);
		if (const auto* kit = _source.MyDefinition.MyOperatorKit ? std::get_if<PapyrsKit>(_source.MyDefinition.MyOperatorKit) : nullptr;
			kit && kit->MyLockSkill && _source.MySkill.MyActive && _source.MyPapyrsLock)
			profile.MyHealChainCount += kit->MyExtraChains;
		const auto& skill = _source.MyDefinition.MySkill;
		profile.MySkillDamage = _source.MySkill.MyActive && (skill.MyAttack || !skill.MyRange.empty() || skill.MyRangeExtend != 0 || skill.MyNoRangeExtend);
		if (profile.MyFortress && _source.MyProfession.MyFortressMelee) profile.MyRanged = false;
		if (_source.MyDefinition.MyOperatorKit && !_source.MyOperatorHooksReleased) OperatorAttackProfile(_source, profile);
		const auto attackId = ++_MyAttackSequence;
		profile.MyAttackId = attackId;
		const bool initialPosition = _source.MySkill.MyActive && UsesInitialPosition(_source);
		const auto sourceLife = _source.MyDeploySequence;
		const bool mystic = _source.MyDefinition.MyProfession.MyKind == ProfessionTrait::MYSTIC && !profile.MyHealing;
		const auto energy = mystic ? std::exchange(_source.MyProfession.MyStored, 0U) : 0U;
		const bool usedOverride = _source.MySkill.MyActive && _source.MyDefinition.MySkill.MyAttack.has_value();
		const bool rangedEnemy = _source.MySide == UnitSide::ENEMY && profile.MyRanged && std::isgreater(profile.MyEnemyRange, 0) &&
			!(_source.MyBlockedBy == scratch.MyTargets.front() && std::isless(profile.MyEnemyRange, 1));
		unsigned targetIndex = 0;
		for (const auto id : scratch.MyTargets)
		{
			if (Finished() || !_source.MyAlive) break;
			const auto& target = Unit(id);
			Emit(BattleEventKind::ATTACKED, _source.MyId, id);
			if (profile.MyHealing)
			{
				HealAttack(_source, id, profile);
				continue;
			}
			auto hitProfile = profile;
			if (mystic) hitProfile.MyHits = 1 + (targetIndex++ == 0 ? energy : 0U);
			const auto speed = _source.MySide == UnitSide::ALLY ? (profile.MyRanged ? profile.MyProjectileSpeed : 0)
				: (rangedEnemy && std::isgreater(Distance(_source.MyPosition, target.MyPosition), 0.75) ? 10.0 : 0.0);
			if (std::isgreater(speed, 0))
			{
				if (profile.MyBoomerang) ++_source.MyProfession.MyBoomerangsOut;
				_MyProjectiles.emplace_back(Projectile{.MySource = _source.MyId, .MyTarget = id, .MyTargetLife = target.MyDeploySequence,
					.MyPosition = _source.MyPosition, .MySpeed = profile.MyBoomerang ? 15.0 : speed, .MyProfile = hitProfile,
					.MyDestination = target.MyPosition, .MySourceLife = sourceLife, .MyAttackId = attackId,
					.MySkillAttack = _source.MySkill.MyActive && _source.MyDefinition.MySkill.MyKind != SkillKind::PASSIVE,
					.MyInitialPositionAttack = initialPosition});
			}
			else Hit(_source.MyId, id, hitProfile, {}, initialPosition);
		}
		if (!Finished()) SkillAttackPerformed(_source, usedOverride, _noAmmo, scratch.MyTargets);
		if (_source.MyDefinition.MyProfession.MyKind == ProfessionTrait::HUNTER)
			_source.MyProfession.MyAmmo = std::max(0.0, _source.MyProfession.MyAmmo - 1);
		return true;
	}

	// 正前方判定使用身体占格；单点敌人的纵向位置仍用连续坐标，避免半格处提前切换倍率。
	// ReSharper disable once CppMemberFunctionMayBeStatic
	double BattleCore::MainAttackMultiplier(
		CombatUnit& _source,
		const CombatUnit& _target,
		const AttackProfile& _profile,
		bool _initialPosition
	) noexcept
	{
		if (const auto* mlynar = _source.MyDefinition.MyOperatorKit ? std::get_if<MlynarKit>(_source.MyDefinition.MyOperatorKit) : nullptr)
			return std::isgreaterequal(static_cast<double>(_source.MyMlynarNear), mlynar->MyNearCount) ? mlynar->MyNearScale : mlynar->MyAttackScale;
		if (const auto* f12yin = _source.MyDefinition.MyOperatorKit ? std::get_if<F12yinKit>(_source.MyDefinition.MyOperatorKit) : nullptr) return _source.MyF12yinCritical ? f12yin->MyCriticalScale : 1;
		const auto forward = RotateOffset(RangeOffset{.MyColumn = 1}, _source.MyFacing);
		const auto origin = _initialPosition ? _source.MyHome : _source.MyPosition;
		const auto row = static_cast<int>(std::floor(origin.MyY + 0.5));
		const auto column = static_cast<int>(std::floor(origin.MyX + 0.5));
		switch (_profile.MyScaling)
		{
		case AttackScaling::NONE: return 1;
		case AttackScaling::HUNTER: return _profile.MyConditionalScale;
		case AttackScaling::FUNNEL:
		{
			auto& state = _source.MyProfession;
			const auto& definition = _source.MyDefinition.MyProfession;
			const auto* rockr = _source.MyDefinition.MyOperatorKit ? std::get_if<RockrKit>(_source.MyDefinition.MyOperatorKit) : nullptr;
			const auto maximum = definition.MyFunnelMax * (rockr && _source.MyRockrOver ? rockr->MyOverloadScale : 1);
			state.MyFunnelScale = state.MyFunnelTarget == _target.MyId
				? std::min(maximum, state.MyFunnelScale + definition.MyFunnelDelta) : definition.MyFunnelInitial;
			state.MyFunnelTarget = _target.MyId;
			return state.MyFunnelScale;
		}
		case AttackScaling::FLYING: return _target.Flying() ? _profile.MyConditionalScale : 1;
		case AttackScaling::BLOCKED: return _target.MyBlockedBy ? _profile.MyConditionalScale : 1;
		case AttackScaling::UNBLOCKED: return _target.MyBlockedBy == _source.MyId ? 1 : _profile.MyConditionalScale;
		case AttackScaling::DISTANT:
			return _target.MyBlockedBy == _source.MyId || BodyOnTile(_target, row, column) ||
				BodyOnTile(_target, row + forward.MyRow, column + forward.MyColumn) ? 1 : _profile.MyConditionalScale;
		case AttackScaling::REINFORCEMENT:
			return _source.MyProfession.MyReinforcement && _target.MyBlockedBy == _source.MyProfession.MyReinforcement ? _profile.MyConditionalScale : 1;
		case AttackScaling::FRONT:
			if (_source.MyDefinition.MyTraitFrontRange)
				return BodyInRange(_target, _initialPosition ? _source.MyInitialTraitFrontMask : _source.MyTraitFrontMask) ? _profile.MyConditionalScale : 1;
			if (!_target.MyDefinition.MyHitArea)
			{
				const auto dy = std::floor(_target.MyPosition.MyY + 0.5) - row;
				const auto dx = std::floor(_target.MyPosition.MyX + 0.5) - column;
				return !std::islessgreater(dy * forward.MyColumn - dx * forward.MyRow, 0) &&
					std::isgreaterequal((_target.MyPosition.MyX - origin.MyX) * forward.MyColumn +
						(_target.MyPosition.MyY - origin.MyY) * forward.MyRow, 0) ? _profile.MyConditionalScale : 1;
			}
			const auto tiles = BodyTiles(_target);
			for (int r = tiles.MyFirstRow; r <= tiles.MyLastRow; ++r)
				for (int c = tiles.MyFirstColumn; c <= tiles.MyLastColumn; ++c)
					if ((r - row) * forward.MyColumn == (c - column) * forward.MyRow &&
						(c - column) * forward.MyColumn + (r - row) * forward.MyRow >= 0) return _profile.MyConditionalScale;
			return 1;
		}
		return 1;
	}

	void BattleCore::Hit(UnitId _source, UnitId _target, const AttackProfile& _profile, WorldPoint _point, bool _initialPosition)
	{
		auto& scratch = AcquireAttackScratch();
		const AttackGuard guard{.MyDepth = _MyAttackDepth};
		auto& source = _MyUnits[Index(_source)];
		// 配置取发射时快照，ATK 取命中时值；同一次命中的多段／溅射／连锁共用该次 ATK。
		const auto amount = source.MyStats.MyAttack * source.MyStats.MyAttackScaleMultiplier * _profile.MyAttackScale * _profile.MyDamageMultiplier;
		double totalDealt = 0;
		if (_target && Unit(_target).MyAlive)
		{
			double dealt = 0;
			auto multiplier = _profile.MyLockedFunnel ? LockedFunnelMultiplier(source, _target) : _profile.MyPerTargetFunnel ? PerTargetFunnel(source, _target, _profile.MyFunnelPerHit) : MainAttackMultiplier(source, Unit(_target), _profile, _initialPosition);
			if (_profile.MyCriticalProbability && std::isless(_MyRandom.Next(), *_profile.MyCriticalProbability)) multiplier *= _profile.MyCriticalScale;
			const auto mainAmount = amount * multiplier;
			auto hits = source.MyDefinition.MyTokenKit && source.MyDefinition.MyTokenKit->MyKind == TokenKitKind::WOLF_PACK ? std::max(1U, source.MyWolfShadows) : _profile.MyHits;
			if (_profile.MyProgressiveHits)
				if (const auto* kit = source.MyDefinition.MyOperatorKit ? std::get_if<PrecisionKit>(source.MyDefinition.MyOperatorKit) : nullptr)
					hits = std::isgreaterequal(static_cast<double>(source.MyPrecisionHits), kit->MyUpgradeAfter) ? kit->MyUpgradedHits : kit->MyInitialHits;
			for (unsigned hit = 0; hit < hits && Unit(_target).MyAlive && !Finished(); ++hit)
				dealt += DealDamage(_source, _target, DamageInfo{.MyAmount = mainAmount, .MyType = _profile.MyDamageType,
					.MyMultiplier = _profile.MyHitMultiplier.value_or(1), .MyHitSleep = _profile.MyHitSleep,
					.MyNoSp = _profile.MyHitMultiplier.has_value() && hit != 0, .MyTags = _profile.MyTags, .MyIsAttack = true, .MyIsSkill = _profile.MySkillDamage, .MyAttackId = _profile.MyAttackId});
			if (_profile.MyOnHitStatus && Unit(_target).MyAlive)
			{
				auto application = _profile.MyOnHitApplication;
				application.MySource = _source;
				(void)ApplyStatus(_target, *_profile.MyOnHitStatus, application);
			}
			if (_profile.MyOperatorEachHit) OperatorEachHit(_source, _target, _profile);
			ContentEvent event{.MyKind = ContentEventKind::ATTACK_HIT, .MySource = _source, .MyTarget = _target, .MyAmount = dealt};
			NotifyContent(event);
			totalDealt += dealt;
			_point = Unit(_target).MyPosition;
		}
		if (std::isgreater(_profile.MySplashRadius, 0))
		{
			// 溅射以落点和单位中心判定，不扩大为巨型受击矩形；仍然跳过有效隐匿的敌人。
			for (const auto& target : _MyUnits)
			{
				// const auto& target = _MyUnits[targetIndex];
				if (Finished()) break;
				if (target.MyId == _target || target.MySide == source.MySide || !target.MyAlive || target.MyHidden ||
					target.MyStatuses.Has(CombatStatus::UNTARGETABLE) || (target.MySide == UnitSide::ENEMY && EnemyStealthed(target)) || (target.Flying() && (_profile.MyGroundOnly ||
					(!_profile.MyCanHitFlying && !_profile.MySplashHitsFlying))) ||
					std::isgreater(Distance(_point, target.MyPosition), _profile.MySplashRadius + 1e-9)) continue;
				const auto dealt = DealDamage(_source, target.MyId, DamageInfo{.MyAmount = amount * _profile.MySplashScale, .MyType = _profile.MyDamageType, .MyTags = _profile.MyTags, .MyIsAttack = true, .MyIsSplash = true, .MyIsSkill = _profile.MySkillDamage, .MyAttackId = _profile.MyAttackId});
				totalDealt += dealt;
				if (_profile.MyOperatorEachHit) OperatorEachHit(_source, target.MyId, _profile, false);
				ContentEvent event{.MyKind = ContentEventKind::ATTACK_HIT, .MySource = _source, .MyTarget = target.MyId, .MyAmount = dealt, .MySplash = true};
				NotifyContent(event);
			}
		}
		if (_target && _profile.MyChainCount > 1)
		{
			scratch.MySeen.emplace_back(_target);
			auto previous = _target;
			for (unsigned bounce = 1; bounce < _profile.MyChainCount && !Finished(); ++bounce)
			{
				UnitId best = 0;
				double nearest = std::numeric_limits<double>::infinity();
				for (const auto& target : _MyUnits)
				{
					if (target.MySide == source.MySide || !TargetableEnemy(target, _profile) ||
						std::ranges::find(scratch.MySeen, target.MyId) != scratch.MySeen.end()) continue;
					const auto distance = Distance(Unit(previous).MyPosition, target.MyPosition);
					// 连锁的候选半径按受击体积进入，候选间仍按中心距离排序；溅射仅按中心进入。
					if (std::isgreater(BodyDistance(target, Unit(previous).MyPosition), _profile.MyChainRadius + 1e-9)) continue;
					if (std::isless(distance, nearest - 1e-9) || (best && std::islessequal(std::abs(distance - nearest), 1e-9) && target.MySpawnSequence < Unit(best).MySpawnSequence))
					{ best = target.MyId; nearest = distance; }
				}
				if (!best) break;
				scratch.MySeen.emplace_back(best);
				Emit(BattleEventKind::ATTACKED, previous, best);
				const auto dealt = DealDamage(_source, best, DamageInfo{.MyAmount = amount * std::pow(1 - _profile.MyChainFalloff, bounce), .MyType = _profile.MyDamageType,
					.MyTags = _profile.MyTags | DamageTag::CHAIN, .MyIsAttack = true, .MyIsSkill = _profile.MySkillDamage, .MyAttackId = _profile.MyAttackId});
				totalDealt += dealt;
				if (std::isgreater(_profile.MyChainSluggish, 0) && Unit(best).MyAlive) (void)ApplyStatus(best, CombatStatus::SLUGGISH, _profile.MyChainSluggish, _source);
				if (_profile.MyOperatorEachHit) OperatorEachHit(_source, best, _profile, false);
				ContentEvent event{.MyKind = ContentEventKind::ATTACK_HIT, .MySource = _source, .MyTarget = best, .MyAmount = dealt, .MyChain = true};
				NotifyContent(event);
				previous = best;
			}
			if (std::isgreater(_profile.MyChainSluggish, 0) && Unit(_target).MyAlive) (void)ApplyStatus(_target, CombatStatus::SLUGGISH, _profile.MyChainSluggish, _source);
		}
		if (source.MyDefinition.MyProfession.MyKind == ProfessionTrait::BOMBARDER)
			for (unsigned i = 0; i < source.MyDefinition.MyProfession.MyShockCount; ++i)
				Schedule(ScheduledAction{.MyAt = Time() + 0.3 * (i + 1.0), .MyKind = ScheduledKind::AFTERSHOCK,
					.MySource = _source, .MyPoint = _point});
		if (source.MyDefinition.MyOperatorKit) OperatorAfterHit(_source, _target, _profile, _point, totalDealt);
		if (_profile.MyGenericHit) GenericHit(_source, _target, totalDealt);
	}

	void BattleCore::HealAttack(const CombatUnit& _source, UnitId _target, const AttackProfile& _profile)
	{
		auto& scratch = AcquireAttackScratch();
		const AttackGuard guard{.MyDepth = _MyAttackDepth};
		auto amount = _source.MyStats.MyAttack * _profile.MyAttackScale * _profile.MyHealScale * _source.MyStats.MyAttackScaleMultiplier;
		const bool initialPosition = _source.MySkill.MyActive && UsesInitialPosition(_source);
		const auto point = [&](const CombatUnit& _unit) { return initialPosition ? RulePosition(_unit) : _unit.MyPosition; };
		const auto position = point(Unit(_target)), origin = point(_source);
		const auto tileDistance = std::max(std::abs(std::floor(position.MyX + 0.5) - std::floor(origin.MyX + 0.5)),
			std::abs(std::floor(position.MyY + 0.5) - std::floor(origin.MyY + 0.5)));
		if (std::isgreater(_profile.MyHealFarMultiplier, 0) && std::isgreater(tileDistance, _profile.MyHealNearDistance)) amount *= _profile.MyHealFarMultiplier;
		if (std::isgreater(_profile.MyElementHealRatio, 0)) (void)ReduceElement(_target, _source.MyStats.MyAttack * _profile.MyElementHealRatio);
		const auto healed = Heal(_source.MyId, _target, amount);
		StandinHealAttack(_source.MyId, _target, healed);
		DiyOperatorHealHit(_source.MyId, _target, _profile);
		MedicHealAttack(_source.MyId, _target, amount);
		scratch.MySeen.emplace_back(_target);
		auto previous = _target;
		for (unsigned bounce = 1; bounce < _profile.MyHealChainCount && !Finished(); ++bounce)
		{
			UnitId best = 0;
			for (std::size_t i = 0; i < _MyAllyIds.size(); ++i)
			{
				const auto& ally = _MyUnits[Index(_MyAllyIds[i])];
				if (!ally.MyAlive || ally.MyKind == UnitKind::DEVICE || ally.MyHidden || std::ranges::find(scratch.MySeen, ally.MyId) != scratch.MySeen.end()) continue;
				const auto from = point(Unit(previous)), to = point(ally);
				if (std::isgreater(std::abs(to.MyX - from.MyX), 1) || std::isgreater(std::abs(to.MyY - from.MyY), 1)) continue;
				if (ally.MyId != _source.MyId && (ally.MyStatuses.Has(CombatStatus::ISOLATED) || ally.MyStatuses.Has(CombatStatus::NO_HEAL) || ally.MyDefinition.MyAttack.MyNoHeal)) continue;
				const auto ratio = ally.MyHealth / ally.MyStats.MyMaxHealth;
				const auto bestRatio = best ? Unit(best).MyHealth / Unit(best).MyStats.MyMaxHealth : 2;
				if (!best || std::isless(ratio, bestRatio - 1e-12) || (std::islessequal(std::abs(ratio - bestRatio), 1e-12) && ally.MyAggroSequence > Unit(best).MyAggroSequence)) best = ally.MyId;
			}
			if (!best) break;
			scratch.MySeen.emplace_back(best);
			Emit(BattleEventKind::ATTACKED, previous, best);
			(void)Heal(_source.MyId, best, amount * std::pow(1 - _profile.MyHealChainFalloff, bounce));
			previous = best;
		}
		OperatorHealHit(_source.MyId, _target);
	}

	void BattleCore::LaunchSkillProjectile(UnitId _source, UnitId _target, double _speed, const DirectProjectileHit& _hit, std::optional<WorldPoint> _from, std::size_t _chain)
	{
		const auto& source = Unit(_source); const auto& target = Unit(_target);
		if (std::isgreater(_hit.MyBounceRadius, 0))
		{
			if (_chain == NoPlayer)
			{
				if (_MyFreeProjectileChains.empty()) { _chain = _MyProjectileChains.size(); _MyProjectileChains.emplace_back(); }
				else { _chain = _MyFreeProjectileChains.back(); _MyFreeProjectileChains.pop_back(); }
				_MyProjectileChains[_chain].clear();
				_MyProjectileChains[_chain].reserve(_hit.MyHits);
			}
			_MyProjectileChains[_chain].push_back(_target);
		}
		_MyProjectiles.emplace_back(Projectile{.MySource = _source, .MyTarget = _target, .MyTargetLife = target.MyDeploySequence,
			.MyPosition = _from.value_or(source.MyPosition), .MySpeed = _speed, .MyDestination = target.MyPosition, .MyDirectHit = _hit, .MyChain = _chain});
	}

	void BattleCore::UpdateProjectiles()
	{
		std::size_t kept = 0;
		_MyArrivedProjectiles.clear();
		for (auto projectile : _MyProjectiles)
		{
			projectile.MyAge += BattleClock::StepSeconds;
			if (projectile.MyTarget)
			{
				const auto& target = Unit(projectile.MyTarget);
				if (target.MyAlive && !target.MyHidden && target.MyDeploySequence == projectile.MyTargetLife) projectile.MyDestination = target.MyPosition;
				else if (!std::isgreater(projectile.MyProfile.MySplashRadius, 0) && !projectile.MyProfile.MyBoomerang && !projectile.MyReturning && !(projectile.MyDirectHit && std::isgreater(projectile.MyDirectHit->MyBounceRadius, 0))) continue;
				else projectile.MyTarget = 0;
			}
			const auto distance = Distance(projectile.MyDestination, projectile.MyPosition);
			const auto step = projectile.MySpeed * BattleClock::StepSeconds;
			if (std::islessequal(distance, step) || std::isgreaterequal(projectile.MyAge, 10)) _MyArrivedProjectiles.emplace_back(projectile);
			else
			{
				projectile.MyPosition.MyX += (projectile.MyDestination.MyX - projectile.MyPosition.MyX) / distance * step;
				projectile.MyPosition.MyY += (projectile.MyDestination.MyY - projectile.MyPosition.MyY) / distance * step;
				_MyProjectiles[kept++] = projectile;
			}
		}
		_MyProjectiles.resize(kept);
		// 先统一推进所有弹道，再依发射顺序结算；早到的伤害可能击杀后面弹道的目标。
		for (const auto& projectile : _MyArrivedProjectiles)
		{
			if (Finished()) break;
			const auto target = projectile.MyTarget && Unit(projectile.MyTarget).MyAlive && Unit(projectile.MyTarget).MyDeploySequence == projectile.MyTargetLife ? projectile.MyTarget : 0;
			auto& source = _MyUnits[Index(projectile.MySource)];
			if (projectile.MyDirectHit)
			{
				const auto& hit = *projectile.MyDirectHit;
				if (target)
				{
					const auto& enemy = Unit(target);
					const bool still = enemy.MyBlockedBy || enemy.MyStatuses.Has(CombatStatus::STUN) || enemy.MyStatuses.Has(CombatStatus::NO_MOVE) || !enemy.MyMoving;
					const auto scale = still ? hit.MyStillScale.value_or(hit.MyScale) : hit.MyScale;
					for (unsigned i = 0; i < hit.MyHits && Unit(target).MyAlive && !Finished(); ++i)
					{
						const auto multiplier = hit.MyProfileScale ? MainAttackMultiplier(source, Unit(target), source.MyDefinition.MyAttack, false) : 1;
						(void)DealDamage(source.MyId, target, {.MyAmount = source.MyStats.MyAttack * scale * multiplier, .MyType = hit.MyType, .MyTags = hit.MyTags, .MyIsSkill = true});
					}
				}
				if (projectile.MyChain != NoPlayer)
				{
					const auto next = hit.MyHits > 1 ? NearestProjectileTarget(source.MyId, projectile.MyDestination, hit.MyBounceRadius, _MyProjectileChains[projectile.MyChain]) : UnitId{};
					if (next)
					{
						auto bounce = hit; --bounce.MyHits;
						LaunchSkillProjectile(source.MyId, next, projectile.MySpeed, bounce, projectile.MyDestination, projectile.MyChain);
					}
					else { _MyProjectileChains[projectile.MyChain].clear(); _MyFreeProjectileChains.push_back(projectile.MyChain); }
				}
				continue;
			}
			if (projectile.MyReturning)
			{
				if (source.MyAlive && source.MyDeploySequence == projectile.MySourceLife && source.MyProfession.MyBoomerangsOut)
				{
					--source.MyProfession.MyBoomerangsOut;
					ContentEvent caught{.MyKind = ContentEventKind::BOOMERANG_CAUGHT, .MyUnit = source.MyId,
						.MyAttackId = projectile.MyAttackId, .MySkillAttack = projectile.MySkillAttack, .MyPoint = projectile.MyDestination};
					NotifyContent(caught);
				}
				continue;
			}
			if (!projectile.MyProfile.MyBoomerang || target || std::isgreater(projectile.MyProfile.MySplashRadius, 0))
				Hit(projectile.MySource, target, projectile.MyProfile, projectile.MyDestination, projectile.MyInitialPositionAttack);
			// 返程下一帧才开始推进；发射者死亡或免费移动改变部署身份时不产生返程。
			if (projectile.MyProfile.MyBoomerang && source.MyAlive && source.MyDeploySequence == projectile.MySourceLife)
				_MyProjectiles.emplace_back(Projectile{.MySource = source.MyId, .MyTarget = source.MyId,
					.MyTargetLife = source.MyDeploySequence, .MyPosition = projectile.MyDestination, .MySpeed = 3.75,
					.MyDestination = source.MyPosition, .MySourceLife = projectile.MySourceLife, .MyReturning = true,
					.MyAttackId = projectile.MyAttackId, .MySkillAttack = projectile.MySkillAttack});
		}
	}
}
