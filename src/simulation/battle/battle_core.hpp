#ifndef STRONGHOLD_SIMULATION_BATTLE_CORE_HPP
#define STRONGHOLD_SIMULATION_BATTLE_CORE_HPP
#include <deque>
#include "builtin_skill.hpp"
#include <stronghold/simulation/component_registry.hpp>
#include <stronghold/simulation/battle.hpp>
#include <stronghold/simulation/operator_kits.hpp>
#include <stronghold/simulation/body.hpp>
#include <stronghold/core/random.hpp>
#include <stronghold/simulation/content_registry.hpp>
#include <span>
#include <string_view>
#include <stronghold/core/scheduler.hpp>
#include <stronghold/simulation/combat_types.hpp>

namespace Stronghold
{
	// 单线程所有者，显式推进时间；内置规则直接调用，只有 CUSTOM 标签进入扩展分派。
	// 注册表须覆盖本局生命周期。单位 ID 稳定，读取到的引用不能跨 Battle 移动保留。
	class BattleCore final
	{
	public:
		explicit BattleCore(BattleInput _input);
		BattleCore(const BattleCore&) = delete;
		BattleCore& operator=(const BattleCore&) = delete;
		BattleCore(BattleCore&&) = delete;
		BattleCore& operator=(BattleCore&&) = delete;

		[[nodiscard]] SkillBase& Skill(UnitId _unit);
		[[nodiscard]] OperatorBase& Operator(UnitId _unit);
		std::uint64_t AttachMechanism(UnitId _unit, MechanismDefinition _definition, std::string_view _playerId = {});
		std::uint64_t AttachMechanism(UnitId _unit, ComponentReference _reference, std::string_view _playerId = {});
		bool RemoveMechanism(std::uint64_t _handle);

		void Start();

		void Step();

		void Advance(std::uint64_t _ticks);

		void ForceEnd(BattleEndReason _reason = BattleEndReason::FORCED);
		// 首领技能的显式团队 LP 效果；返回实际扣除值。普通战场、未开始和已结束时无效果。
		double LoseTeamLife(double _amount);
		void SetObstacle(int _row, int _column, bool _enabled, ObstacleKind _kind = ObstacleKind::BLOCK);
		// 位移同步完成，返回实际移动格数；沿地形逐段检查，移动后解除阻挡并重算路线。
		// ID 必须存在；非战斗状态、死亡、静态刚体或失衡免疫时返回零。
		double Displace(UnitId _enemy, WorldPoint _direction, double _distance);
		double Push(UnitId _enemy, double _force, const PushOptions& _options = {});
		double Pull(UnitId _enemy, double _force, const PullOptions& _options);
		double PullToFront(UnitId _enemy, UnitId _source, double _force);
		[[nodiscard]] double PushDistance(UnitId _enemy, double _force, bool _effect = false) const;

		[[nodiscard]] bool Started() const noexcept { return _MyStarted; }

		[[nodiscard]] bool Finished() const noexcept { return _MyReason != BattleEndReason::RUNNING; }

		[[nodiscard]] double Time() const noexcept { return _MyClock.Seconds(); }

		[[nodiscard]] std::uint64_t Tick() const noexcept { return _MyClock.Tick(); }
		[[nodiscard]] std::uint32_t RandomState() const noexcept { return _MyRandom.State(); }

		// 单位引用跨新增单位保持有效；容器迭代器不跨会创建单位的回调保留，使用 ID 或下标。
		[[nodiscard]] const std::deque<CombatUnit>& Units() const noexcept { return _MyUnits; }

		[[nodiscard]] std::span<const BattlePlayerState> Players() const noexcept { return std::span(_MyPlayers).first(_MyParticipantCount); }
		[[nodiscard]] std::span<const BattlePlayerState> Owners() const noexcept { return _MyPlayers; }
		[[nodiscard]] const BattlePlayerState& UnitOwner(UnitId _unit) const;
		[[nodiscard]] bool SameOwner(UnitId _left, UnitId _right) const;
		[[nodiscard]] bool Teammates(std::size_t _left, std::size_t _right) const;


		[[nodiscard]] const CombatUnit& Unit(UnitId _id) const { return _MyUnits.at(Index(_id)); }

		// 内置与 CUSTOM 内容共用的位置／范围查询；绝对格范围仍保持调用者给定的坐标。
		[[nodiscard]] RulePositionMode PositionMode() const noexcept { return _MyInput.MyRulePositionMode; }
		[[nodiscard]] bool GarrisonEffectsAfterExit() const noexcept { return _MyInput.MyGarrisonEffectsAfterExit; }

		[[nodiscard]] bool RetainGrantedGarrisonsAfterExit() const noexcept { return _MyInput.MyRetainGrantedGarrisonsAfterExit; }

		[[nodiscard]] WorldPoint RulePosition(UnitId _unit) const { return RulePosition(Unit(_unit)); }
		[[nodiscard]] const std::bitset<FieldTiles>& RuleRange(UnitId _unit) const { return RuleRange(Unit(_unit)); }
		[[nodiscard]] std::span<const int> RuleRangeKeys(UnitId _unit) const { return RuleRangeKeys(Unit(_unit)); }
		[[nodiscard]] bool InRuleRange(UnitId _source, UnitId _target) const;

		[[nodiscard]] BattleResult Result() const;

		[[nodiscard]] std::span<const ContentError> ContentErrors() const noexcept { return _MyContentErrors; }

		[[nodiscard]] std::vector<BattleEvent> DrainEvents();

		// Explicit effects: source 0 means unattributed; otherwise IDs must exist, amounts finite and nonnegative.
		// Valid effects before Start or after finish are no-ops. Returned amounts are actual HP changes.
		double DealDamage(UnitId _source, UnitId _target, double _amount, DamageType _type);

		double DealDamage(UnitId _source, UnitId _target, const DamageInfo& _damage);

		double Heal(UnitId _source, UnitId _target, double _amount, HealOptions _options = {});

		void AddShield(UnitId _target, Shield _shield);

		// 元素损伤进入独立量表，不经过护盾或物理／法术防御；元素伤害仍走 DealDamage。
		double DealElement(UnitId _source, UnitId _target, ElementHit _hit);
		double ReduceElement(UnitId _target, double _amount, std::optional<Element> _element = std::nullopt);
		void BurstElement(UnitId _source, UnitId _target, Element _element);
		[[nodiscard]] std::optional<ElementDisplay> ElementView(UnitId _unit) const;

		// Duration is already resolved by the caller (resistance and content modifiers are separate).
		// Reapplication extends to the longer remaining duration; zero duration does nothing.
		bool ApplyStatus(
			UnitId _target,
			CombatStatus _status,
			double _seconds,
			UnitId _source = 0,
			bool _force = false
		);

		bool ApplyStatus(UnitId _target, CombatStatus _status, const StatusApplication& _application);

		bool RemoveStatus(UnitId _target, CombatStatus _status);

		std::uint64_t AddBuff(UnitId _target, BuffDefinition _definition);

		// 普通同名增益取绝对值最强者；较弱而更晚到期的值在强者结束后恢复。
		bool ApplyStrongest(UnitId _target, std::string _key, double _duration, BuffStrength _strength, UnitId _source = 0);

		std::size_t RemoveBuff(UnitId _target, std::string_view _key);

		bool RemoveBuff(UnitId _target, std::uint64_t _buff);

		double LoseHealth(UnitId _source, UnitId _target, double _amount, bool _silent = false);
		double AddDp(std::string_view _playerId, double _amount);
		// 结算钩子也可记账；仅存在的玩家接受有限正数，返回实际新增资金。
		double AddCoins(std::string_view _playerId, double _amount);
		// 层数钩子先修改增量，再按当前层数限制；满层时不触发钩子。结算钩子也可调用。
		// 非零来源必须是有效单位 ID；未知玩家、非有限/非正增量和关闭增长的战场返回零。
		double AddBondLayers(std::string_view _playerId, std::string_view _bondId, double _amount, LayerGainOptions _options = {});


		// 技力接口分别表达“回复”和“修改”；INITIAL 绕过阻回，用于部署／联防继承。
		// 借出已装备道具的战斗效果；目录来自 BattleInput，重复借出同 ID 只刷新到期时间。
		std::size_t LendEquipment(UnitId _from, UnitId _to, unsigned _maximumTier = 5, double _duration = 60);

		using LentEquipmentView = Battle::LentEquipmentView;

		[[nodiscard]] std::vector<LentEquipmentView> LentEquipment(UnitId _unit) const;
		double GainSp(UnitId _unit, double _amount, SpReason _reason = SpReason::GRANTED, bool _silent = false);
		void SetSpTotal(UnitId _unit, double _total);
		void SetSpCostMultiplier(UnitId _unit, double _multiplier);
		[[nodiscard]] double SpCost(UnitId _unit) const;
		[[nodiscard]] double SpTotal(UnitId _unit) const;
		bool ActivateSkill(UnitId _unit, bool _free = false, SkillReason _reason = SkillReason::MANUAL);
		void EndSkill(UnitId _unit, SkillReason _reason = SkillReason::STOPPED);
		bool ForceAttack(UnitId _unit, std::span<const UnitId> _targets = {}, bool _noAmmo = false);
		void AddSkillAmmo(UnitId _unit, double _amount);
		void ExtendSkill(UnitId _unit, double _seconds);
		void AddSkillCharge(UnitId _unit, int _amount = 1);

		// 动态规则保留至内容再次设置（包括再部署）；更新时立即重算朝向／永久攻击距离。
		void SetSkillTrigger(UnitId _unit, SkillTrigger _rule, std::vector<RangeOffset> _grid = {});
		std::uint64_t AddSkillTriggerRange(UnitId _unit, SkillTriggerArea _area);
		bool RemoveSkillTriggerRange(UnitId _unit, std::uint64_t _range);
		bool UpdateSkillTriggerRange(UnitId _unit, std::uint64_t _range, SkillTriggerArea _area);

		// 创建返回稳定 ID；非法定义抛异常，合法但被占位／倒地体拒绝的召唤返回 0。
		UnitId SpawnToken(TokenSpawn _spawn);
		// 技能库存只会激活已布置且来源为 SKILL 的棋盘召唤物；没有棋子时不生成库存。
		UnitId ReleaseSkillSummon(UnitId _owner, std::string_view _token, unsigned _cap = 1);
		[[nodiscard]] unsigned SkillSummonStock(UnitId _owner, std::string_view _token) const;
		UnitId SpawnDevice(DeviceSpawn _spawn);
		// 炮台别名局内唯一，重复激活返回原 ID；找不到有效射程/空位时返回 0。
		UnitId SpawnTurret(TurretSpawn _spawn);
		void SetBondLayers(std::string_view _playerId, std::string_view _bondId, double _layers);
		[[nodiscard]] double BondLayers(std::string_view _playerId, std::string_view _bondId) const;
		// 可在 Start 前注册，返回局内稳定句柄；取消后保留阵数以供战报读取。
		// 合法定义在战斗结束后返回 0；未知非零句柄抛出 out_of_range。
		std::uint64_t StartColdWind(ColdWindDefinition _definition);
		bool CancelColdWind(std::uint64_t _handle);
		[[nodiscard]] std::uint64_t ColdWindGusts(std::uint64_t _handle) const;
		UnitId SpawnEnemy(EnemySpawn _spawn);
		void Retreat(UnitId _unit, bool _permanent = false, RemovalReason _reason = RemovalReason::RETREAT, bool _dying = false);
		bool Redeploy(UnitId _unit, bool _free = true, std::optional<WorldPoint> _tile = std::nullopt, bool _keepSp = false);
		[[nodiscard]] bool CanDeploy(WorldPoint _position, UnitId _excludedUnit = 0, bool _includeDown = true) const;
		[[nodiscard]] bool IsDown(UnitId _unit) const;
		[[nodiscard]] WorldPoint RestPosition(UnitId _unit) const;
		[[nodiscard]] bool ReservedTile(WorldPoint _position) const;
		// 基础射程内依次按道路、距离、朝向局部坐标选择；地形版本变化才重建道路缓存。
		[[nodiscard]] std::optional<WorldPoint> FindTacticalPoint(UnitId _unit);
		[[nodiscard]] const std::bitset<FieldTiles>& GroundPathTiles();
		bool Relocate(UnitId _unit, WorldPoint _position);
		bool MoveRedeploy(UnitId _unit, WorldPoint _position, bool _clearSp = false);
		void SetDownAtHome(UnitId _unit, bool _enabled);
		// 仅存活、尚处于本体的傀儡师能主动切换；致命伤使用相同的转换入口。
		bool EnterDoll(UnitId _unit);

		[[nodiscard]] double RemainingDistance(UnitId _enemy) const;

	private:
		friend class SkillBase;
		friend class BuiltinSkill;
		friend class OperatorBase;
		friend class EffectExecutor;
		Battle _MyView;
		std::size_t _MyParticipantCount{};
		std::string _MyEnvironmentPlayer;
		struct UnitComponents
		{
			BuiltinSkill MySkill;
			OperatorBase MyOperator;
			std::unique_ptr<SkillBase> MyCustomSkill;
			std::unique_ptr<OperatorBase> MyCustomOperator;

			UnitComponents(Battle& _battle, UnitId _unit) : MySkill(_battle, _unit), MyOperator(_battle, _unit) {}
		};

		struct MechanismState
		{
			MechanismDefinition MyDefinition;
			UnitId MyUnit{};
			std::size_t MyOwner{NoPlayer};
			double MyReadyAt{};
			bool MyRemoved{};
		};

		struct EffectTask
		{
			double MyAt{};
			std::uint64_t MySequence{};
			std::uint64_t MyMechanism{};
			std::uint64_t MyDeployment{};
			std::uint64_t MyActivation{};
			EffectContext MyContext{};
			std::vector<UnitId> MyTargets;
		};

		void EnsureUnitComponents(UnitId _unit);
		void NotifyComponents(ContentEvent& _event);
		void TickEffectTasks();
		void PrepareComponents();
		[[nodiscard]] const ComponentRegistry& Registry() const;
		[[nodiscard]] const SelectorBase& CustomSelector(ComponentReference _reference) const;
		[[nodiscard]] const OperationBase& CustomOperationHandler(ComponentReference _reference) const;
		std::deque<UnitComponents> _MyComponents;
		std::vector<UnitId> _MyCustomOperators;
		std::vector<std::unique_ptr<SelectorBase>> _MyCustomSelectors;
		std::vector<std::unique_ptr<OperationBase>> _MyCustomOperations;
		std::deque<std::vector<EffectTarget>> _MyEffectScratch;
		std::vector<UnitId> _MyEffectSortIds;
		std::vector<EffectTarget> _MyGenericSelection;
		std::size_t _MyEffectDepth{};
		unsigned _MyMechanismDepth{};
		std::deque<MechanismState> _MyMechanisms;
		std::vector<EffectTask> _MyEffectTasks;
		std::uint64_t _MyEffectTaskSequence{};

		void PrepareOwners();
		void ResolveOwnerPrincipals();

		struct ContentInstance
		{
			ContentReference MyReference;
			std::unique_ptr<CustomContent> MyHandler;
			UnitId MyUnit{};
			std::uint64_t MyBuff{};
			std::size_t MyOwner{};
			bool MyRetired{};
		};

		void ValidateContent(ContentReference _reference, ContentTag _expected) const;
		void AttachContent(ContentReference _reference, UnitId _unit, std::uint64_t _buff, std::size_t _owner);
		void RetireContent(UnitId _unit, std::uint64_t _buff);
		void NotifyContent(ContentEvent& _event);
		void InstallUnitEffects(CombatUnit& _unit);
		void NotifyMedics(ContentEvent& _event, bool _late = false);
		void NotifyGenericSkill(ContentEvent& _event);
		void InstallOperatorKit(CombatUnit& _unit);
		void ReleaseOperatorHooks();
		void NotifyOperatorEarly(ContentEvent& _event);
		void NotifyOperatorKits(ContentEvent& _event);
		bool OperatorBeforeAttack(CombatUnit& _unit, std::vector<UnitId>& _targets);
		void OperatorAfterHit(UnitId _source, UnitId _target, const AttackProfile& _profile, WorldPoint _point, double _dealt);
		[[nodiscard]] bool OperatorCanAttack(const CombatUnit& _unit);
		void SortOperatorTargets(UnitId _source, std::vector<UnitId>& _targets, unsigned _limit = 0, const AttackProfile* _profile = nullptr);
		void OperatorEnemiesInGrid(UnitId _source, std::span<const RangeOffset> _grid, std::vector<UnitId>& _targets, bool _sort = true);
		[[nodiscard]] UnitId HighestHealthAllyInRange(UnitId _source) const;
		void NotifyVendlas(ContentEvent& _event);
		void TexasSkill(UnitId _unit, const TexasKit& _kit);
		void SunbrSkill(UnitId _unit, const SunbrKit& _kit, ContentEventKind _event);
		void GrantInsiderAmmo(UnitId _unit, std::uint64_t _deployment);
		void OperatorReveal(UnitId _unit);
		void NotifyOperatorObservers(ContentEvent& _event);
		[[nodiscard]] bool OperatorInGrid(UnitId _source, UnitId _target, std::span<const RangeOffset> _grid) const;

		enum class OperatorAuraStacking { STRONGEST, PER_SOURCE, REPLACE, STRONGEST_LIVING, HIGHEST_PRESENT };

		enum class OperatorAuraValue { FIXED, SOURCE_ATTACK };

		struct OperatorAuraDefinition
		{
			std::string_view MyKey{};
			Attribute MyAttribute{Attribute::ATTACK_PERCENT};
			double MyValue{};
			OperatorAuraValue MyValueFrom{OperatorAuraValue::FIXED};
			OperatorAuraStacking MyStacking{OperatorAuraStacking::STRONGEST};
			std::optional<OperatorProfession> MyProfession{};
			std::optional<double> MyCostLimit{};
			double MyInterval{0.5};
			double MyInitialDelay{};
			double MyDuration{0.6};
			std::span<const RangeOffset> MyRange{};
			std::span<const RangeOffset> MySkillRange{};
			double MySkillScale{1};
			bool MyOperatorsOnly{};
			bool MyMeleeOnly{};
			bool MyDropOutside{};
			bool MySkillInactive{};
			bool MySkillActive{};
			bool MyEnemies{};
			bool MyNormalEnemies{};
			bool MyAttackRange{};
			bool MySkipHidden{};
			std::optional<double> MyHealthRatioBelow{};
			std::span<const AttributeChange> MyModifiers{};
			BuffRefresh MyRefresh{BuffRefresh::REPLACE};
			bool MyOwnerOnly{};
			std::optional<double> MyMinimum{};
			std::optional<CombatStatus> MyStatus{};
			std::string_view MyRequiredBuff{};
			bool MySkipSelf{};
			bool MyIgnoreIsolation{};
			std::span<const std::string_view> MyCharacters{};
			std::string_view MyGroup{};
			std::string_view MyNation{};
			bool MyNoSource{};
			std::optional<double> MyGroundExtra{};
			std::optional<double> MyStrengthValue{};
			bool MyStrengthRequiresLivingSource{};
			unsigned MyMinimumPartners{};
			bool MyCarriedSource{};
		};

		struct OperatorAuraRuntime
		{
			UnitId MySource{};
			OperatorAuraDefinition MyDefinition{};
			std::string MyBuffKey{};
			std::vector<UnitId> MyCurrent{};
			double MyScale{1};
			double MyOffset{};
		};

		std::size_t InstallOperatorAura(UnitId _unit, const OperatorAuraDefinition& _definition);
		void RefreshOperatorAura(std::size_t _handle);
		void OperatorEnemyTimeSp(ContentEvent& _event, double _perSecond);
		// 回调可安装新光环；deque 保持当前光环及缓存 key 的引用有效。
		std::deque<OperatorAuraRuntime> _MyOperatorAuras{};

		struct GroundAttackSpeedRuntime
		{
			UnitId MySource{};
			std::string_view MyKey{};
			double MyAmount{};
			unsigned MyCount{1};
			bool MyApplied{};
		};

		void InstallGroundAttackSpeed(UnitId _source, std::string_view _key, double _amount, unsigned _count);
		void RefreshGroundAttackSpeed(std::size_t _handle);
		std::deque<GroundAttackSpeedRuntime> _MyGroundAttackSpeeds{};

		void PodegoStartZone(UnitId _unit, const PodegoKit& _kit);
		void PodegoZonePulse(UnitId _unit, WorldPoint _point, double _damage);
		void TinmanStartZone(UnitId _unit, const TinmanKit& _kit);
		void TinmanZonePulse(UnitId _unit, WorldPoint _point, double _attack, std::uint64_t _sequence, bool _damage);
		void IndigoTick(UnitId _unit, const IndigoKit& _kit, double _delta);
		void UtageTick(UnitId _unit);
		void UtageProtect(UnitId _unit, const UtageKit& _kit);
		void WildmnDeploy(UnitId _unit);
		void LiskamDamaged(UnitId _unit, const LiskamKit& _kit);
		void SlchanTick(UnitId _unit);
		void SlchanSkill(UnitId _unit, const SlchanKit& _kit);
		void GrabdsSkill(UnitId _unit, const GrabdsKit& _kit);
		void HaroldBeforeAttack(CombatUnit& _unit, std::vector<UnitId>& _targets);
		void HaroldElementHit(ContentEvent& _event);
		void OperatorHealHit(UnitId _source, UnitId _target);
		[[nodiscard]] UnitId PapyrsTarget(UnitId _source) const;
		void PapyrsSkill(UnitId _unit, const PapyrsKit& _kit, ContentEventKind _event);
		void NotifyOperatorHealing(ContentEvent& _event, bool _last = false);
		void BubbleTick(UnitId _unit);
		void BubbleDamaged(ContentEvent& _event, const BubbleKit& _kit);
		void HumusPeakBuff(UnitId _unit, const HumusKit& _kit);
		void RockrTick(UnitId _unit);
		void RockrSkill(UnitId _unit, const RockrKit& _kit, const ContentEvent& _event);
		void KazemaTick(UnitId _unit);
		void KazemaSkill(UnitId _unit, const KazemaKit& _kit, ContentEventKind _event);
		void OrigamiBurst(UnitId _source, double _scale, bool _tokenBurst = false);
		void GravelSkill(UnitId _unit, const GravelKit& _kit, const ContentEvent& _event);
		void TippiHit(ContentEvent& _event);
		void AkkordTick(UnitId _unit);
		void AkkordSonic(UnitId _unit, const AkkordKit& _kit);
		void WhitewBlock(ContentEvent& _event);
		void BranchSkill(UnitId _unit, const BranchKit& _kit, const ContentEvent& _event);
		void AshlokDeploy(UnitId _unit, const AshlokKit& _kit);
		void ScaleAttackByBlock(ContentEvent& _event, double _scale, bool _blocked);
		void AngelBless(UnitId _unit);
		void AyerAttack(UnitId _unit, const AyerKit& _kit);
		void OperatorAddition(UnitId _source, UnitId _target, double _scale, DamageTags _tags, bool _enemyOnly = true);
		void InstallDiyOperator(UnitId _unit, const DiyOperatorKit& _kit);
		void DiyOperatorTick(UnitId _unit, bool _periodic);
		void DiyOperatorObserve(ContentEvent& _event, bool _late = false);
		void DiyOperatorHealHit(UnitId _unit, UnitId _target, const AttackProfile& _profile);
		void DiyOperatorSkillHit(UnitId _unit, UnitId _target, const AttackProfile& _profile);
		void DiyOperatorBeforeStart(UnitId _unit);
		void DiyOperatorDroneTick(UnitId _unit, double _delta);
		[[nodiscard]] bool DiyInFrontLine(UnitId _unit, UnitId _target) const;
		void InstallDiyTeam(UnitId _unit, const DiyOperatorKit& _kit);
		void DiyTeamObserve(ContentEvent& _event);
		void DiyStudentUpdate(UnitId _unit);
		void DiyPeakUpdate();
		void DiyBlessUpdate(UnitId _unit);
		void DiyLinkStrike(UnitId _unit, std::uint64_t _activation);
		void DiyLinkClear(UnitId _unit, UnitId _target);
		void CgbirdPhantomDeploy(UnitId _unit);
		void CgbirdPhantomObserve(ContentEvent& _event);
		void CgbirdPhantomRespawn(UnitId _owner, UnitId _token, std::uint64_t _deployment);
		void GrantSharedBarrier(UnitId _source, UnitId _target, std::string_view _key, std::string_view _statKey,
			double _shield, double _duration, std::span<const AttributeChange> _modifiers, unsigned _typeMask = 15);
		void SyncSharedBarrier(UnitId _target, std::string_view _statKey);
		void InstallStandin(UnitId _unit, const StandinKit& _kit);
		void StandinConditions(UnitId _unit, const StandinKit& _kit, bool _periodic);
		void StandinPulse(UnitId _unit, double _interval);
		void StandinFeedback(UnitId _unit, const StandinKit& _kit);
		void StandinAura(UnitId _unit, const StandinKit& _kit, bool _talent);
		void StandinBurst(UnitId _unit, const StandinKit& _kit, bool _pull = false, bool _slash = false);
		void StandinObserve(ContentEvent& _event, bool _late = false);
		void StandinEarly(ContentEvent& _event);
		void StandinBeforeAttack(CombatUnit& _unit, std::span<const UnitId> _targets);
		void StandinEachHit(UnitId _unit, UnitId _target, const AttackProfile& _profile, bool _main);
		void StandinHealAttack(UnitId _unit, UnitId _target, double _dealt);
		void StandinSlash(UnitId _unit, std::uint64_t _deployment);
		void InstallSkadi(UnitId _unit, const SkadiKit& _kit);
		void NotifyOperatorLate(ContentEvent& _event);
		void PhilaeElementTalent(ContentEvent& _event);
		void PhilaeObserve(ContentEvent& _event);
		void PhilaeSkill(UnitId _unit, const PhilaeKit& _kit, const ContentEvent& _event);
		void ForcerSkill(UnitId _unit, const ForcerKit& _kit);
		void MintSkill(UnitId _unit, const MintKit& _kit, const ContentEvent& _event);
		void HainiSkill(UnitId _unit, const HainiKit& _kit, const ContentEvent& _event);
		void HainiKill(UnitId _victim);
		void PinecnSkill(UnitId _unit, const PinecnKit& _kit, ContentEvent& _event);
		void SnhuntSkill(UnitId _unit, const SnhuntKit& _kit);
		void SnhuntTick(UnitId _unit);
		std::vector<UnitId> _MyHainis{};
		std::vector<UnitId> _MySnhunts{};

		void BlemshSkill(UnitId _unit, const BlemshKit& _kit, ContentEvent& _event);
		void BlemshAttackSp(UnitId _unit);
		void BlemshHeal(UnitId _unit, const BlemshKit& _kit, bool _othersOnly);
		[[nodiscard]] UnitId InjuredAllyInGrid(UnitId _unit, std::span<const RangeOffset> _range, bool _othersOnly) const;
		double PerTargetFunnel(CombatUnit& _unit, UnitId _target, bool _perHit = false);
		std::vector<UnitId> _MyBlemshs{};

		void UpdateBlockingDefense(UnitId _unit, std::string_view _key, double _defense);
		void ShotstBurst(UnitId _unit, const ShotstKit& _kit);
		void ShotstShred(UnitId _unit, UnitId _target, const ShotstKit& _kit);
		void CurseDollTick(UnitId _unit, double _delta);
		std::vector<UnitId> _MyBlockingDefenders{};
		std::vector<UnitId> _MyCurseDolls{};

		void VulpisSkill(UnitId _unit, const VulpisKit& _kit, ContentEvent& _event);
		void VulpisTick(UnitId _unit);
		void VulpisPunish(UnitId _unit, UnitId _target, const VulpisKit& _kit);
		void KjeraDeploy(UnitId _unit, const KjeraKit& _kit);
		void KjeraBeforeAttack(CombatUnit& _unit, const KjeraKit& _kit, std::vector<UnitId>& _targets);
		[[nodiscard]] double LockedFunnelMultiplier(CombatUnit& _unit, UnitId _target);
		void MostmaSkill(UnitId _unit, const MostmaKit& _kit, const ContentEvent& _event);
		void RmixerSkill(UnitId _unit, const RmixerKit& _kit, const ContentEvent& _event);
		void PrecisionSkill(UnitId _unit, const PrecisionKit& _kit, ContentEvent& _event);
		void PrecisionBeforeAttack(CombatUnit& _unit, const PrecisionKit& _kit, std::vector<UnitId>& _targets);
		void PrecisionDeploySp(UnitId _unit);
		void BeewaxSkill(UnitId _unit, const BeewaxKit& _kit, const ContentEvent& _event);
		void InesEarly(const ContentEvent& _event);
		void InesObserve(const ContentEvent& _event);
		void InesSkill(UnitId _unit, const InesKit& _kit, const ContentEvent& _event);
		void InesHit(UnitId _unit, UnitId _target, const InesKit& _kit);
		void InesSentry(UnitId _unit);
		void InesRecall(UnitId _unit, const InesKit& _kit);
		void InesClearSpeed(UnitId _unit);
		void InesRefreshAttack(UnitId _unit, const InesKit& _kit);
		void RosesaObserve(ContentEvent& _event);
		void MizukiAttack(UnitId _unit, const MizukiKit& _kit, const ContentEvent& _event);
		void MizukiPresence(UnitId _unit);
		void AromaHit(ContentEvent& _event, const AromaKit& _kit);
		void AromaObserve(const ContentEvent& _event);
		void AromaLanding(UnitId _unit);
		void OperatorEachHit(UnitId _source, UnitId _target, const AttackProfile& _profile, bool _main = true);
		void InstallTokenKit(CombatUnit& _unit);
		void CatShieldConnect(UnitId _unit);
		void CatShieldGive(UnitId _unit, UnitId _target, double _ratio);
		void CatShieldTick(UnitId _unit);
		void GladySkill(UnitId _unit, const GladyKit& _kit, ContentEvent& _event);
		void GladyPull(UnitId _unit, UnitId _target, const GladyKit& _kit);
		void GladyDragDamage(UnitId _unit, UnitId _target, const GladyKit& _kit, double _moved);
		void GladyTide(ContentEvent& _event);
		void SetOperatorAttribute(UnitId _unit, std::string_view _key, bool _on, Attribute _attribute, double _value);
		void SetOperatorModifiers(UnitId _unit, std::string_view _key, bool _on, std::span<const AttributeChange> _modifiers);
		void ApplyResistanceCut(UnitId _unit, UnitId _target, std::string_view _key, double _duration, double _value);
		[[nodiscard]] bool LonelyOperator(UnitId _unit, bool _diagonal) const;
		void OperatorModuleTick(UnitId _unit);
		void Texas2Start(UnitId _unit, const Texas2Kit& _kit, SkillReason _reason);
		void Texas2FinishStart(UnitId _unit);
		void Texas2Cast(UnitId _unit, const Texas2Kit& _kit);
		void Texas2Rain(UnitId _unit);
		void Texas2Skill(UnitId _unit, const Texas2Kit& _kit, const ContentEvent& _event);
		void Texas2Death(const ContentEvent& _event);
		void GuardDamage(ContentEvent& _event);
		void MudrokLayers(UnitId _unit, int _layers);
		void MudrokSkill(UnitId _unit, const MudrokKit& _kit, const ContentEvent& _event);
		void FlamtlSkill(UnitId _unit, const FlamtlKit& _kit, const ContentEvent& _event);
		void FlamtlBeforeAttack(CombatUnit& _unit, std::vector<UnitId>& _targets);
		void FlamtlDodge(ContentEvent& _event);
		void FartthSkill(UnitId _unit, const FartthKit& _kit, ContentEvent& _event);
		void FartthBeforeAttack(CombatUnit& _unit, std::vector<UnitId>& _targets);
		void SetExtraRange(UnitId _unit, std::span<const int> _keys);
		void TrackDeferredDamage(CombatUnit& _unit, std::uint64_t _sequence);
		bool ConsumeDeferredDamage(CombatUnit& _unit, std::uint64_t _sequence);
		void GvialObserve(ContentEvent& _event, bool _late = false);
		void GvialSkill(UnitId _unit, const GvialKit& _kit, ContentEvent& _event);
		void BillroSkill(UnitId _unit, const BillroKit& _kit, ContentEvent& _event);
		void BldskSkill(UnitId _unit, const BldskKit& _kit, ContentEvent& _event);
		void BldskBeforeAttack(CombatUnit& _unit, const BldskKit& _kit, std::span<const UnitId> _targets);
		void BldskDeath(const ContentEvent& _event);
		void BldskEarly(ContentEvent& _event);
		void BldskPlasma(UnitId _unit, UnitId _target, const BldskKit& _kit);
		void OperatorAlliesInRange(UnitId _unit, std::vector<UnitId>& _targets) const;
		void LowHealthHealBonus(ContentEvent& _event, double _threshold, double _scale, bool _inclusive, bool _self = false, bool _skipRegen = false);
		void CetsyrSkill(UnitId _unit, const CetsyrKit& _kit, ContentEvent& _event);
		void LisaSkill(UnitId _unit, const LisaKit& _kit, ContentEvent& _event);
		void LisaAura(UnitId _unit);
		void PasngrSkill(UnitId _unit, const PasngrKit& _kit, ContentEvent& _event);
		void PasngrStorm(UnitId _unit, WorldPoint _point);
		void PepeSkill(UnitId _unit, const PepeKit& _kit, ContentEvent& _event);
		void PepeBeforeAttack(CombatUnit& _unit, const PepeKit& _kit, std::vector<UnitId>& _targets);
		void PepeCleanse(UnitId _unit);
		[[nodiscard]] bool HasAbnormal(UnitId _unit) const;
		unsigned CleanseAbnormal(UnitId _unit);
		void InstallWhitw2(CombatUnit& _unit, const Whitw2Kit& _kit);
		void Whitw2Targets(CombatUnit& _unit, const Whitw2Kit& _kit, std::vector<UnitId>& _targets);
		void Whitw2Stage(UnitId _unit);
		void Whitw2Honor(ContentEvent& _event);
		void Whitw2Skill(UnitId _unit, const Whitw2Kit& _kit, ContentEvent& _event);
		void ReleaseFlyingDrones(UnitId _unit, unsigned _count);
		void FlyDrone(UnitId _unit, std::size_t _index, double _delta);
		void FlyingDronesTick(UnitId _unit, double _delta);

		struct SiracusaHonor
		{
			std::size_t MyOwner{};
			double MySp{};
			double MyAttackSpeed{};
		};

		std::vector<SiracusaHonor> _MySiracusaHonor{};
		void InstallMlyss(CombatUnit& _unit);
		void MlyssDeploy(UnitId _unit);
		void MlyssSkill(UnitId _unit, const MlyssKit& _kit, ContentEvent& _event);
		void MlyssPulse(UnitId _unit, bool _adaptation = false);
		void MlyssObserve(ContentEvent& _event);
		void BuffManifold(UnitId _unit, UnitId _owner);
		UnitId StandingManifold(UnitId _owner) const;
		void InstallManifold(CombatUnit& _unit);
		void ManifoldDeploy(UnitId _unit);
		UnitId ManifoldCopyTarget(UnitId _unit) const;
		void CopyManifold(UnitId _unit, UnitId _target, double _scale, bool _managed, bool _clone = false);
		void ManifoldTargets(CombatUnit& _unit, std::vector<UnitId>& _targets);
		void SplitManifold(UnitId _unit, bool _managed);
		void StealManifold(UnitId _unit, std::span<const UnitId> _targets, bool _managed);
		void NotifyManifolds(ContentEvent& _event, bool _late);
		void ManifoldRespawn(UnitId _owner, UnitId _token, unsigned _attempt, std::uint64_t _version);
		std::vector<UnitId> _MyMlysses{};
		std::vector<UnitId> _MyManifolds{};
		std::vector<std::size_t> _MyRhineOwners{};
		void Skadi2Pulse(UnitId _unit, bool _predator = false);
		bool Skadi2Covers(UnitId _source, UnitId _target) const;
		void Skadi2Covered(UnitId _unit, std::vector<UnitId>& _allies, std::vector<UnitId>& _tokens) const;
		void Skadi2Observe(ContentEvent& _event, bool _late = false);
		void SeabornDeploy(UnitId _unit);
		void SeabornExpired(UnitId _unit, std::uint64_t _version);
		void SeabornRespawn(UnitId _owner, UnitId _token, WorldPoint _point, unsigned _attempt);
		void SeabornTick(UnitId _unit, double _delta);
		std::vector<UnitId> _MySkadi2s{};
		std::vector<UnitId> _MySeaborns{};

		void Angel2Coordinate(UnitId _unit);
		void Angel2Airstrike(UnitId _unit, WorldPoint _point, double _scale, DamageTags _tags, bool _center);
		void Angel2Observe(ContentEvent& _event);
		void Angel2Pulse(UnitId _unit, bool _calm);
		void Angel2Skill(UnitId _unit, const Angel2Kit& _kit, ContentEvent& _event);
		std::uint64_t DecayingShield(UnitId _unit, std::string _key, double _total, double _duration);
		void DecayingShieldTick(ContentEvent& _event);
		std::vector<UnitId> _MyAngel2s{};

		void RosmonSkill(UnitId _unit, const RosmonKit& _kit, ContentEvent& _event);
		void RosmonTargets(CombatUnit& _unit, std::vector<UnitId>& _targets);
		void RosmonStable(UnitId _unit);
		std::optional<WorldPoint> TacticalSummonTile(UnitId _unit) const;
		void RosmonGearTick(UnitId _unit);
		std::vector<UnitId> _MyRosmonGears{};
		void CelloPick(UnitId _unit);
		void CelloSkill(UnitId _unit, const CelloKit& _kit, ContentEvent& _event);
		void CelloElement(ContentEvent& _event);
		void CelloHit(ContentEvent& _event);
		void CelloPulse(UnitId _unit, unsigned _kind);
		void Reed2Scorch(UnitId _unit, UnitId _target);
		void Reed2Burst(UnitId _unit, WorldPoint _point);
		void Reed2Module(UnitId _unit);
		void Reed2Skill(UnitId _unit, const Reed2Kit& _kit, ContentEvent& _event);
		void Reed2Kill(ContentEvent& _event);
		std::vector<UnitId> _MyCellos{};
		std::vector<UnitId> _MyReed2s{};
		void Halo2BeforeAttack(CombatUnit& _unit, const Halo2Kit& _kit, std::vector<UnitId>& _targets);
		void Halo2Hit(UnitId _unit, UnitId _target, const Halo2Kit& _kit);
		void Halo2Observe(ContentEvent& _event);
		void Halo2Pulse(UnitId _unit);
		void Agoat2BeforeAttack(CombatUnit& _unit, const Agoat2Kit& _kit, std::vector<UnitId>& _targets);
		void Agoat2Skill(UnitId _unit, const Agoat2Kit& _kit, ContentEvent& _event);
		void Agoat2Observe(ContentEvent& _event);
		void Agoat2Ash(ContentEvent& _event);
		void Agoat2Pulse(UnitId _unit, bool _ash);
		void Agoat2Veil(ContentEvent& _event);
		static void AbsorbElement(ContentEvent& _event, double& _pool, bool _multiplier = false);
		std::vector<UnitId> _MyHalo2s{};

		struct ElementVeil
		{
			std::bitset<FieldTiles> MyKeys{};
			double MyPool{};
			double MyUntil{};
			UnitId MySource{};
		};

		std::vector<ElementVeil> _MyElementVeils{};
		void FreeSummonTiles(UnitId _unit, std::span<const RangeOffset> _grid, std::vector<std::uint64_t>& _tiles, bool _ground = true) const;
		std::optional<WorldPoint> BestSummonTile(std::span<const std::uint64_t> _tiles) const;
		UnitId LastDeployedOperator(std::size_t _owner) const;
		void Nearl2Dawn(UnitId _unit, const Nearl2Kit& _kit);
		void Nearl2Skill(UnitId _unit, const Nearl2Kit& _kit, ContentEvent& _event);
		void Nearl2Fatal(ContentEvent& _event);
		void Siege2Skill(UnitId _unit, const Siege2Kit& _kit, ContentEvent& _event);
		void Siege2Burst(UnitId _unit, const Siege2Kit& _kit);
		void Siege2Pulse(UnitId _unit);
		void Siege2Range(UnitId _unit);
		void Siege2Observe(ContentEvent& _event);
		std::vector<UnitId> _MySiege2s{};
		std::vector<UnitId> _MyLastDeployedOperators{};
		std::vector<UnitId> _MySkillBoundTokens{};
		bool Sbell2AddSnow(UnitId _unit, int _key);
		bool Sbell2FreezeTile(UnitId _unit, int _key);
		void Sbell2Lay(UnitId _unit);
		void Sbell2Tick(UnitId _unit, double _delta);
		void Sbell2Skill(UnitId _unit, const Sbell2Kit& _kit, ContentEvent& _event);
		void Sbell2Observe(ContentEvent& _event, bool _fatal = false);
		void Sbell2Module(UnitId _unit);
		std::vector<UnitId> _MySbell2s{};
		void BlkkgtSlash(UnitId _unit, const BlkkgtKit& _kit, double _scale);
		void BlkkgtPull(UnitId _unit, const BlkkgtKit& _kit, bool _final);
		void BlkkgtSkill(UnitId _unit, const BlkkgtKit& _kit, ContentEvent& _event);
		void BlkkgtTick(UnitId _unit);
		void BlkkgtStatus(ContentEvent& _event);
		std::vector<UnitId> _MyBlkkgts{};
		bool TeleportEnemy(UnitId _enemy, WorldPoint _point);
		bool YuCrosses(const CombatUnit& _unit, UnitId _source, UnitId _target) const;
		void YuSkill(UnitId _unit, const YuKit& _kit, ContentEvent& _event);
		void YuPulse(UnitId _unit, unsigned _kind);
		void YuObserve(ContentEvent& _event);
		std::vector<UnitId> _MyYus{};
		static bool AbnormalStatus(CombatStatus _status);
		void HealingTargets(UnitId _unit, std::vector<UnitId>& _targets, bool _abnormal) const;
		void LumenBeforeAttack(CombatUnit& _unit, const LumenKit& _kit, std::vector<UnitId>& _targets);
		void LumenHealHit(UnitId _unit, UnitId _target, const LumenKit& _kit);
		void LumenSkill(UnitId _unit, const LumenKit& _kit, ContentEvent& _event);
		void LumenObserve(ContentEvent& _event);
		void LumenHealing(ContentEvent& _event);
		void LumenTick(UnitId _unit);
		std::vector<UnitId> _MyLumens{};
		void QiubaiSkill(UnitId _unit, const QiubaiKit& _kit, ContentEvent& _event);
		void QiubaiHit(UnitId _unit, UnitId _target, const QiubaiKit& _kit);
		void QiubaiBurst(UnitId _unit, UnitId _target);
		std::vector<UnitId> _MyPepes{};

		void LemuenSkill(UnitId _unit, const LemuenKit& _kit, ContentEvent& _event);
		void LemuenObserve(ContentEvent& _event);
		void LemuenWanted();
		void LemuenRange(UnitId _unit);
		void UpdateBombardmentLock(BombardmentLock& _lock);
		void LemuenFire(std::size_t _handle, unsigned _shot);
		void LemuenBlast(UnitId _unit, WorldPoint _point, double _attack);

		struct LemuenBombardment
		{
			UnitId MySource{};
			std::uint64_t MyDeployment{};
			double MyAttack{};
			std::vector<BombardmentLock> MyLocks{};
		};

		std::deque<LemuenBombardment> _MyLemuenBombardments{};
		std::vector<UnitId> _MyLemuens{};

		void Thorn2Start(UnitId _unit, const Thorn2Kit& _kit);
		void Thorn2Zones(UnitId _unit);
		void Thorn2Vision(UnitId _unit);
		unsigned StraightRoad(int _row, int _column);
		std::optional<std::array<unsigned, FieldTiles>> _MyStraightRoads{};

		void MlynarSkill(UnitId _unit, const MlynarKit& _kit, ContentEvent& _event);
		void MlynarObserve(ContentEvent& _event, bool _early = false);
		void MlynarTrait(UnitId _unit, const MlynarKit& _kit);
		void AglinaSkill(UnitId _unit, const AglinaKit& _kit, ContentEvent& _event);
		void SntllaSkill(UnitId _unit, const SntllaKit& _kit, ContentEvent& _event);
		void SntllaTalent(UnitId _unit);
		void SntllaImpact(UnitId _unit, WorldPoint _point);
		void NymphSkill(UnitId _unit, const NymphKit& _kit, ContentEvent& _event);
		void NymphObserve(ContentEvent& _event);
		void NymphSoul(UnitId _unit, UnitId _target, double _scale);
		void Svash2Skill(UnitId _unit, const Svash2Kit& _kit, ContentEvent& _event);
		void Svash2Observe(ContentEvent& _event);
		void Svash2Snow(UnitId _unit);
		void Svash2Slash(UnitId _unit, const Svash2Kit& _kit);
		void SkillEnemiesIgnoringStealth(UnitId _unit, std::span<const RangeOffset> _range, std::vector<UnitId>& _targets);
		void TargetsOnLine(CombatUnit& _unit, std::vector<UnitId>& _targets);
		void Ghost2Skill(UnitId _unit, const Ghost2Kit& _kit, ContentEvent& _event);
		void Ghost2Pulse(UnitId _unit, bool _damage);
		void Ghost2Team(UnitId _unit);
		void Ghost2EachHit(UnitId _source, UnitId _target);
		void DuskSkill(UnitId _unit, const DuskKit& _kit, ContentEvent& _event);
		void DuskKill(ContentEvent& _event);
		void DuskSummon(UnitId _unit, const DuskKit& _kit, UnitId _target);
		void PreferUnblocked(CombatUnit& _unit, std::vector<UnitId>& _targets);
		void DemkniSkill(UnitId _unit, const DemkniKit& _kit, ContentEvent& _event);
		void DemkniSuit(UnitId _unit);
		void DemkniAuto(UnitId _unit);
		void DemkniTargets(UnitId _unit, const DemkniKit& _kit, std::vector<UnitId>& _targets) const;
		void HornSkill(UnitId _unit, const HornKit& _kit, ContentEvent& _event);
		void HornFatal(ContentEvent& _event);
		void HornFlares(UnitId _unit);
		void OperatorAttackProfile(CombatUnit& _unit, AttackProfile& _profile);
		void PeriodicHealthLoss(UnitId _unit, double& _accumulator, double _delta, double _interval, double _ratio, bool _silent);
		void SurtrSkill(UnitId _unit, const SurtrKit& _kit, ContentEvent& _event);
		void SurtrFatal(ContentEvent& _event);
		void EtlchiSkill(UnitId _unit, const EtlchiKit& _kit, ContentEvent& _event);
		void EtlchiObserve(ContentEvent& _event, bool _late = false);
		void EtlchiCandles(UnitId _unit, const EtlchiKit& _kit);
		void RedirectCandles(UnitId _unit, std::vector<UnitId>& _targets);
		void CrowdAttackSpeed(UnitId _unit, std::string_view _key, double _speed, double _count);
		void UlpiaSkill(UnitId _unit, const UlpiaKit& _kit, ContentEvent& _event);
		void UlpiaContact(UnitId _unit, const UlpiaKit& _kit);
		void UlpiaAnchor(UnitId _unit, const UlpiaKit& _kit);
		void UlpiaHurt(ContentEvent& _event);
		bool IsAbyssal(UnitId _unit) const;
		bool OnOwnBoard(std::size_t _player, int _row, int _column) const;
		void Blaze2Skill(UnitId _unit, const Blaze2Kit& _kit, ContentEvent& _event);
		void Blaze2Observe(ContentEvent& _event);
		void Blaze2Fatal(ContentEvent& _event);
		void Blaze2Ground(UnitId _unit);
		void ArtsAndElement(UnitId _unit, UnitId _target, double _scale, double _elementScale, Element _element);
		void BurstSpAura(UnitId _unit, std::string_view _key, double _sp);
		void TitiSkill(UnitId _unit, const TitiKit& _kit, ContentEvent& _event);
		void TitiObserve(ContentEvent& _event);
		void TitiPulse(UnitId _unit, bool _dream);
		void TitiSleepOthers(UnitId _unit, UnitId _from, const TitiKit& _kit);
		void TitiWard(UnitId _unit, UnitId _target, bool _fatal);
		void FoesInRadius(WorldPoint _origin, double _radius, std::vector<UnitId>& _targets, bool _center = false) const;
		void Excu2Skill(UnitId _unit, const Excu2Kit& _kit, ContentEvent& _event);
		void Excu2Dodge(ContentEvent& _event);
		void Excu2ExtraAttack(UnitId _unit, std::uint64_t _deployment);
		void CetsyrMotes(UnitId _unit);
		void CetsyrInspire(UnitId _unit, const CetsyrKit& _kit);
		bool IsOperatorLeader(UnitId _unit) const;
		void CetsyrProtect(ContentEvent& _event);
		void Inspire(UnitId _source, UnitId _target, double _value, bool _health = false);
		void InspireAttribute(UnitId _source, UnitId _target, double _value, Attribute _attribute, double _duration, double _handover);
		void BardRegen(UnitId _source, UnitId _target, double _value, double _duration);
		void GnosisSkill(UnitId _unit, const GnosisKit& _kit, const ContentEvent& _event);
		void GnosisBeforeAttack(CombatUnit& _unit, std::vector<UnitId>& _targets);
		void GnosisAura(UnitId _unit, bool _resist);
		void LionhdSkill(UnitId _unit, const LionhdKit& _kit);
		void LionhdPresence(UnitId _unit);
		void ReckprObserve(const ContentEvent& _event);
		void BeewaxSummon(UnitId _unit, const BeewaxKit& _kit);
		void TokenBurst(UnitId _unit, UnitId _credit, double _scale, double _stun, std::span<const RangeOffset> _range, DamageType _type = DamageType::ARTS, unsigned _hits = 1);
		void RmixerObserve(ContentEvent& _event);
		void RmixerShield(UnitId _unit);
		void ReloadNation(UnitId _unit, std::string_view _nation, double _ammo, std::optional<double> _radius = {});
		[[nodiscard]] bool HoldsUndying(UnitId _unit) const;
		void ArchetTactics(UnitId _unit);
		void ArchetScatter(UnitId _unit, UnitId _target, WorldPoint _point, const ArchetKit& _kit);
		void ArchetPursuit(UnitId _unit, const ArchetKit& _kit);
		[[nodiscard]] UnitId NearestProjectileTarget(UnitId _source, WorldPoint _point, double _radius, std::span<const UnitId> _skip) const;
		std::vector<UnitId> _MyVulpises{};
		std::vector<UnitId> _MyArchets{};

		void VigilDeploy(UnitId _unit, const VigilKit& _kit);
		void VigilSkill(UnitId _unit, const VigilKit& _kit, const ContentEvent& _event);
		void VigilTick(UnitId _unit);
		void VigilMark(UnitId _unit);
		void WolfBlocked(UnitId _wolf, UnitId _enemy);
		void WolfDeploy(UnitId _unit);
		void SetWolfShadows(UnitId _unit, unsigned _count);
		bool AddWolfShadow(UnitId _unit, unsigned _maximum);
		bool ReturnWolf(UnitId _unit);
		void WolfFatal(ContentEvent& _event, bool _late);
		void WolfDeath(const ContentEvent& _event, bool _late);
		void NotifyWolfCombat(ContentEvent& _event);
		void WolfBeforeAttack(CombatUnit& _unit);
		[[nodiscard]] bool WolfOnField(UnitId _unit) const;
		std::vector<UnitId> _MyVigils{};
		std::vector<UnitId> _MyLockedFunnelUsers{};
		std::vector<UnitId> _MyInitialSpCarriers{};
		std::vector<UnitId> _MyIneses{};
		std::vector<UnitId> _MyRosesas{};
		std::vector<UnitId> _MyAromas{};
		std::vector<UnitId> _MyGladys{};
		std::vector<UnitId> _MyReckprs{};
		std::vector<UnitId> _MyBldsks{};
		std::vector<UnitId> _MyCetsyrs{};
		std::vector<UnitId> _MyTitis{};
		std::vector<UnitId> _MyBlazes{};
		std::vector<UnitId> _MyEtlchis{};
		std::vector<UnitId> _MyDemknis{};
		std::vector<UnitId> _MyGhost2s{};
		std::vector<UnitId> _MySvash2s{};
		std::vector<UnitId> _MyNymphs{};
		std::vector<UnitId> _MyMlynars{};
		std::vector<UnitId> _MyWolves{};

		void Swire2Pay(UnitId _unit, const Swire2Kit& _kit);
		void Swire2Fatal(ContentEvent& _event);
		void Swire2Skill(UnitId _unit, const Swire2Kit& _kit, const ContentEvent& _event);
		void Swire2Heal(UnitId _unit, const Swire2Kit& _kit);
		void Swire2Tick(UnitId _unit);
		void Swire2Cash(UnitId _unit, const Swire2Kit& _kit);
		std::size_t Swire2BombTiles(UnitId _unit, std::array<WorldPoint, 9>& _tiles) const;
		bool Swire2ThrowDue(UnitId _unit, const Swire2Kit& _kit);
		void ChampagneTick(UnitId _unit);
		std::vector<UnitId> _MySwire2s{};
		std::vector<UnitId> _MyChampagnes{};

		struct InsiderAmmoGrant
		{
			UnitId MySource{};
			UnitId MyTarget{};
			std::uint64_t MyDeployment{};
			double MyAmount{};
		};

		std::vector<InsiderAmmoGrant> _MyInsiderGrants{};
		std::vector<UnitId> _MyPendingOperatorReleases{};
		std::size_t _MyOperatorBeforeAttackHandlers{};
		bool _MyTinmanWither{};
		std::vector<UnitId> _MyStandins{};
		std::vector<UnitId> _MyRaidians{};
		std::vector<UnitId> _MyDiyOperators{};
		std::vector<UnitId> _MyDiyPallases{};

		struct VendlaRuntime
		{
			UnitId MyUnit{};
			UnitId MyProtege{};
			UnitId MyHealingTarget{};
			double MyCachedAt{-1};
		};

		std::deque<VendlaRuntime> _MyVendlas{};
		std::vector<UnitId> _MyTexasUnits{};
		std::vector<UnitId> _MyEstells{};
		std::vector<UnitId> _MyPodegos{};
		std::vector<UnitId> _MyUtages{};
		std::vector<UnitId> _MyWildmns{};
		std::vector<UnitId> _MySilents{};
		std::vector<UnitId> _MySlchans{};
		std::vector<UnitId> _MyHarolds{};
		std::vector<UnitId> _MyBubbles{};
		std::vector<UnitId> _MyRockrs{};
		std::vector<UnitId> _MyKazemas{};
		std::vector<UnitId> _MyAkkords{};

		struct SkillSummonRuntime
		{
			UnitId MyOwner{};
			std::string MyToken{};
			unsigned MyStock{};
			std::vector<UnitId> MyPieces{};
		};

		void DockSkillSummons();
		bool DeployDockedSummon(UnitId _unit);
		void NotifyTokenKits(ContentEvent& _event, bool _late = false);
		void TokenDeploy(UnitId _unit);
		[[nodiscard]] const CombatDefinition* FindTokenTemplate(UnitId _owner, std::string_view _token) const;
		std::vector<SkillSummonRuntime> _MySkillSummons{};

		void GenericHit(UnitId _source, UnitId _target, double _dealt);
		void GenericElement(UnitId _source, UnitId _target, double _dealt);
		void GenericHeal(UnitId _source);
		void GenericEnemies(UnitId _source, std::vector<UnitId>& _targets, unsigned _limit = 0, bool _sort = true);
		void GenericBurst(UnitId _source, double _scale);
		void GenericDebuff(UnitId _source, UnitId _target, double _duration);
		void MedicHealAttack(UnitId _source, UnitId _target, double _amount);
		void SpawnMapCharacters();
		std::vector<UnitId> _MyMedics{};

		struct EquipmentRuntime
		{
			UnitId MyUnit{};
			std::size_t MyEffect{};
			unsigned MyUsed{};
			std::uint64_t MyDeployment{};
			std::string MyBuffKey{};
			std::uint64_t MyTickBuff{};
			std::optional<std::size_t> MyHammer{};
			std::uint64_t MyHammerGeneration{};
			std::optional<std::size_t> MyLend{};
			EquipmentEffect MyLentEffect{};
			std::vector<UnitId> MyTargets{};
			double MyBonus{};
			bool MyArmed{};
			bool MyWasStealth{};
			bool MyCombo{};
			bool MyDoomed{};
			double MyReadyAt{-std::numeric_limits<double>::infinity()};
		};

		enum class HammerKind : std::size_t { BURN, UNDYING, SPEED, TREMBLE };

		struct HammerRuntime
		{
			UnitId MyUnit{};
			unsigned MyReferences{};
			std::uint64_t MyGeneration{};
			std::array<unsigned, 4> MyOwned{};
			std::array<std::optional<EquipmentParameters>, 4> MyParameters{};
			std::bitset<4> MyFieldKinds{};
			unsigned MySteam{};
			std::optional<EquipmentParameters> MySteamParameters{};
			std::string MySpeedKey{};
			double MySpeed{};
			std::uint64_t MyLockDeployment{};
			std::uint64_t MyHeldDeployment{};
			double MyUntil{-std::numeric_limits<double>::infinity()};
		};

		struct HammerField
		{
			double MyAt{-std::numeric_limits<double>::infinity()};
			std::bitset<4> MyKinds{};
		};

		struct EquipmentBuffOwner
		{
			UnitId MyUnit{};
			std::string MyKey{};
		};

		struct EquipmentLend
		{
			UnitId MyUnit{};
			std::string MyItem{};
			double MyUntil{};
			std::uint64_t MyVersion{};
			bool MyActive{true};
			std::vector<EquipmentBuffOwner> MyBuffs{};
			std::vector<std::size_t> MyEffects{};
		};

		void InitializeEquipment(std::size_t _index);
		[[nodiscard]] const EquipmentEffect& EquipmentDefinition(const EquipmentRuntime& _runtime) const;
		[[nodiscard]] bool EquipmentActive(const EquipmentRuntime& _runtime) const;
		std::uint64_t ApplyEquipmentBuff(EquipmentRuntime& _runtime, UnitId _target, BuffDefinition _buff);
		void TrackEquipmentBuff(std::size_t _lend, UnitId _target, std::string _key);
		void ExpireEquipmentLend(std::size_t _index);
		void InstallEquipmentLend(std::size_t _index, const EquipmentTemplate& _definition);
		void CheckEquipmentFront(EquipmentRuntime& _runtime, bool _initial);
		std::deque<EquipmentLend> _MyEquipmentLends{};

		void InstallHammer(EquipmentRuntime& _runtime);
		void ReleaseHammer(EquipmentRuntime& _runtime);
		void RefreshHammer(HammerRuntime& _runtime);
		void NotifyHammer(HammerRuntime& _runtime, ContentEvent& _event);
		void HammerFatal(ContentEvent& _event, bool _held);
		[[nodiscard]] unsigned HammerMultiplier(const HammerRuntime& _runtime, HammerKind _kind);
		[[nodiscard]] const EquipmentParameters* HammerParameters(const HammerRuntime& _runtime, HammerKind _kind) const;
		std::deque<HammerRuntime> _MyHammers{};
		std::vector<HammerField> _MyHammerFields{};

		void InstallEquipmentEffects(CombatUnit& _unit);
		void NotifyEquipment(ContentEvent& _event, bool _early);
		void NotifyEquipmentLate(ContentEvent& _event);
		void EquipmentPeriodic(std::size_t _index);
		void EquipmentSolvent(EquipmentRuntime& _runtime);
		void NotifyEquipmentSignature(EquipmentRuntime& _runtime, ContentEvent& _event);
		void BuildEnemyIndex();
		void EquipmentEnemies(EquipmentRuntime& _runtime, const AttackProfile& _profile);
		[[nodiscard]] bool EquipmentMember(const CombatUnit& _unit, std::string_view _bond) const;
		[[nodiscard]] bool CarriesEquipment(const CombatUnit& _unit, std::string_view _base) const;
		void RetypeWeakness(DamageInfo& _damage, UnitId _source, UnitId _target, bool _schoolMultipliers = false) const;
		std::deque<EquipmentRuntime> _MyEquipment{};
		std::array<std::vector<UnitId>, FieldTiles> _MyEnemyBuckets{};
		bool _MyEquipmentEnemyQueries{};
		bool _MyEquipmentHitEffects{};
		bool _MyEquipmentStatusEffects{};
		void RecordContentError(ContentReference _reference, std::string _message);

		struct YanyouRuntime
		{
			UnitId MyUnit{};
			UnitId MyLock{};
			double MyFlameAccumulator{};
			double MyAuraAccumulator{0.2};
			double MyHoverOffset{};
			std::optional<WorldPoint> MyKeysAt{};
			std::uint64_t MyRangeRevision{};
			std::vector<UnitId> MyTargets{};
		};

		void InstallYanyou(CombatUnit& _unit);
		void RefreshYanyouRange(CombatUnit& _unit, YanyouRuntime& _state);
		void NotifyYanyou(ContentEvent& _event);
		void YanyouRadius(YanyouRuntime& _state, WorldPoint _point, double _radius);
		[[nodiscard]] UnitId YanyouLock(const YanyouRuntime& _state) const;
		[[nodiscard]] std::optional<WorldPoint> FindYanyouTile(std::size_t _player, const std::bitset<FieldTiles>& _taken) const;
		void SpawnBondYanyou(std::size_t _index);
		std::deque<YanyouRuntime> _MyYanyous{};

		struct BattleGarrisonRuntime
		{
			UnitId MyUnit{};
			std::size_t MyRule{};
			std::string MyKey{};
			std::vector<BondLayer> MyUsed{};
			double MyWholeUsed{};
			double MyCount{};
			double MySteps{-1};
			bool MyDoll{};
			bool MyGranted{};
			bool MyRetired{};
		};

		void InstallGarrisons();
		void AddGarrison(UnitId _unit, std::size_t _rule, bool _granted = false);
		void GrantGarrison(std::size_t _index);
		bool RevokeGrantedGarrisons(UnitId _unit);
		void RefreshGarrisons();
		void RefreshGarrison(std::size_t _index);
		void ApplyGarrisonAttributes(std::size_t _index, double _duration = 0);
		void NotifyGarrisons(ContentEvent& _event, bool _late = false);
		void GainGarrisonLayers(std::size_t _index);
		[[nodiscard]] double GarrisonSteps(const BattleGarrisonRuntime& _state) const;
		[[nodiscard]] bool GarrisonOnField(UnitId _unit) const;
		[[nodiscard]] bool GarrisonSourceActive(UnitId _unit) const;
		[[nodiscard]] bool GarrisonAmmoTarget(const BattleGarrisonRuntime& _state, UnitId _unit) const;
		std::deque<BattleGarrisonRuntime> _MyGarrisons{};
		std::array<std::vector<std::size_t>, static_cast<unsigned>(BattleGarrisonKind::COUNT)> _MyGarrisonGroups{};
		std::vector<BattleGarrisonKind> _MyGarrisonOrder{};
		bool _MyGarrisonReaders{};
		bool _MyGarrisonPending{};

		struct CoreBondRuntime
		{
			std::size_t MyPlayer{};
			std::size_t MyEffect{};
			std::vector<UnitId> MyMembers{};
			std::vector<UnitId> MyKnocked{};
			std::vector<UnitId> MyTargets{};
			bool MyPower{};
			bool MyExPower{};
			bool MyPending{};
			std::uint64_t MyCount{};
			double MyBonus{};
		};

		void InstallCoreBonds();
		void RefreshCoreBond(std::size_t _index);
		void NotifyCoreBonds(ContentEvent& _event, bool _late = false);
		void CoreBondPulse(std::size_t _index);
		void SyncSargon(UnitId _unit);
		void NotifyEgirDeath(ContentEvent& _event);
		void DevourEgir(std::size_t _index);
		void FlushEgirRevives();
		std::size_t _MyEgirPasses{};
		bool _MyEgirDevouring{};
		std::vector<CoreBondRuntime> _MyCoreBonds{};

		struct AddonBondRuntime
		{
			std::size_t MyPlayer{};
			std::array<const AddonBondParameters*, static_cast<unsigned>(AddonBondKind::COUNT)> MyParameters{};
			std::array<unsigned, static_cast<unsigned>(AddonBondKind::COUNT)> MyTiers{};
			std::array<std::vector<UnitId>, static_cast<unsigned>(AddonBondKind::COUNT)> MyMembers{};
			std::vector<UnitId> MyOperators{};
			std::vector<UnitId> MyAura{};
			std::vector<UnitId> MyNextAura{};
			std::vector<std::pair<UnitId, double>> MyRaidTargets{};
			std::vector<RangeOffset> MyRaidReach{};
			std::deque<std::vector<UnitId>> MyShareFrames{};
			std::size_t MyShareDepth{};
			bool MyPending{};
			bool MyAuraWide{};
			double MyAuraSpeed{};
			double MyArcaneMultiplier{1};
			double MyArcaneLowMultiplier{1};
		};

		void InstallAddonBonds();
		void RefreshAddonBonds(std::size_t _player, bool _initial = false);
		void UpdateBondAura(std::size_t _player);
		void PollRaidBond(std::size_t _player);
		void NotifyAddonBonds(ContentEvent& _event, bool _late = false);
		void NotifyIndomitable(ContentEvent& _event);
		void ApplyEliteSpCost(CombatUnit& _unit, const AddonBondParameters& _parameters);
		[[nodiscard]] double AddonLayers(const AddonBondRuntime& _state, AddonBondKind _kind) const;
		std::vector<AddonBondRuntime> _MyAddonBonds{};
		bool _MyBondHitEffects{};

		enum class ScheduledKind { CRATE_BREAK, TOKEN_EXPIRE, COLD_WIND, PROFESSION, AFTERSHOCK, EQUIPMENT, EQUIPMENT_RETREAT, HAMMER, EQUIPMENT_EXPIRE, BOND_REFRESH, BOND_AURA, BOND_RAID, CORE_BOND_REFRESH, CORE_BOND_PULSE, GARRISON_REFRESH, INSIDER_AMMO, OPERATOR_REVEAL, OPERATOR_AURA, PODEGO_ZONE, TINMAN_ZONE, DOCKED_SUMMON_RETRY, TOKEN_KIT_EXPIRE, OPERATOR_GROUND_ASPD, ANGEL_BLESS, ARCHET_TACTICS, WOLF_GROW, WOLF_RETURN, VIGIL_MARK, RMIXER_SHIELD, INES_SENTRY, INES_FIRST_RETREAT, MIZUKI_PRESENCE, AROMA_LANDING, CATHY_FORGE, CAT_SHIELD, GNOSIS_AURA, GNOSIS_RESIST, LIONHD_PRESENCE, OPERATOR_MODULE, TEXAS2_RAIN, MUDROK_LAYERS, CETSYR_MOTES, EXCU2_ATTACK, TITI_DREAM, TITI_VIGOR, BLAZE_GROUND, SURTR_RETREAT, HORN_FLARES, LISA_AURA, DEMKNI_SUIT, DUSK_EXPIRE, GHOST2_SLOW, GHOST2_DAMAGE, SVASH2_SNOW, SNTLLA_TALENT, SNTLLA_IMPACT, THORN2_ZONES, THORN2_VISION, LEMUEN_WANTED, LEMUEN_EXTRADITION, LEMUEN_FIRE, LEMUEN_BLAST, PASNGR_STORM, QIUBAI_BURST, YU_PULSE, SBELL2_MODULE, SIEGE2_PULSE, HALO2_PULSE, AGOAT2_PULSE, CELLO_PULSE, REED2_BURST, REED2_MODULE, ROSMON_STABLE, SKADI2_PULSE, SEABORN_RESPAWN, ANGEL2_PULSE, WHITW2_STAGE, MLYSS_PULSE, MLYSS_ADAPTATION, MANIFOLD_RESPAWN, STANDIN_PULSE, TULIP_SLASH, DIY_OPERATOR_PULSE, CGBIRD_RESPAWN, DIY_STUDENT_PULSE, DIY_LINK_STRIKE, DIY_SP_PULSE };

		// 所有内置延迟动作共用稳定的时间／序号排序，不捕获 this，移动 Battle 安全。
		struct ScheduledAction
		{
			double MyAt{};
			std::uint64_t MySequence{};
			ScheduledKind MyKind{};
			UnitId MySource{};
			UnitId MyTarget{};
			std::uint64_t MyHandle{};
			double MyInterval{};
			std::uint64_t MyVersion{};
			WorldPoint MyPoint{};
			double MyAmount{};
			unsigned MyRemaining{};

			static bool Later(const ScheduledAction& _left, const ScheduledAction& _right) noexcept
			{
				return std::isgreater(_left.MyAt, _right.MyAt) ||
					(!std::islessgreater(_left.MyAt, _right.MyAt) && _left.MySequence > _right.MySequence);
			}
		};

		struct ColdWindState
		{
			ColdWindDefinition MyDefinition{};
			std::size_t MyOwner{NoPlayer};
			std::uint64_t MyGusts{};
			bool MyCancelled{};
		};

		struct TurretState
		{
			TurretSpawn MyDefinition{};
			UnitId MyUnit{};
			double MyCooldown{};
		};

		struct DirectProjectileHit
		{
			DamageType MyType{DamageType::PHYSICAL};
			double MyScale{1};
			std::optional<double> MyStillScale{};
			unsigned MyHits{1};
			DamageTags MyTags{static_cast<DamageTags>(DamageTag::SKILL)};
			double MyBounceRadius{};
			bool MyProfileScale{};
		};

		void LaunchSkillProjectile(UnitId _source, UnitId _target, double _speed, const DirectProjectileHit& _hit, std::optional<WorldPoint> _from = {}, std::size_t _chain = NoPlayer);

		std::deque<std::vector<UnitId>> _MyProjectileChains{};
		std::vector<std::size_t> _MyFreeProjectileChains{};

		struct Projectile
		{
			UnitId MySource{};
			UnitId MyTarget{};
			std::uint64_t MyTargetLife{};
			WorldPoint MyPosition;
			double MySpeed{};
			double MyAge{};
			AttackProfile MyProfile{};
			WorldPoint MyDestination{};
			std::uint64_t MySourceLife{};
			bool MyReturning{};
			std::uint64_t MyAttackId{};
			bool MySkillAttack{};
			std::optional<DirectProjectileHit> MyDirectHit{};
			std::size_t MyChain{NoPlayer};
			bool MyInitialPositionAttack{}; // 发射时捕获，瞬发技能结束后命中仍使用同一位置模式。
		};

		// 每层同步攻击独占工作区，回调中的强制攻击不会覆盖外层目标／连锁去重列表。
		// deque 保证新增嵌套帧不使已有工作区引用失效；向量容量在后续攻击中复用。
		struct AttackScratch
		{
			std::vector<UnitId> MyTargets{};
			std::vector<UnitId> MySeen{};
			std::vector<std::uint64_t> MyBuffIds{};
		};

		struct TargetCandidate
		{
			UnitId MyId{};
			int MyBlockedPriority{};
			double MyPriority{};
			double MyTaunt{};
			double MyDistance{};
			std::uint64_t MySequence{};
		};

		[[nodiscard]] std::size_t Index(UnitId _id) const
		{
			if (_id == 0 || _id > _MyUnits.size()) throw std::out_of_range("unknown battle unit");
			return static_cast<std::size_t>(_id - 1);
		}

		[[nodiscard]] std::size_t Owner(std::string_view _id) const;

		bool Deploy(CombatUnit& _unit, bool _initial = false, std::optional<WorldPoint> _tile = std::nullopt, std::optional<double> _keepSp = std::nullopt);
		void RemoveUnit(CombatUnit& _unit, RemovalReason _reason, UnitId _source = 0, bool _permanent = false, bool _dying = false);
		UnitId CreateEnemy(const EnemySpawn& _spawn, std::size_t _index);
		[[nodiscard]] const EnemySpawn& SpawnDefinition(std::size_t _index) const;
		void Schedule(ScheduledAction _action);
		void TickScheduled();
		void Gust(ColdWindState& _state);
		void TickTerrain();
		void TickTurrets();
		void SyncBossHealth(CombatUnit& _unit);
		void SyncBossUnits();
		void RefreshTerrain(CombatUnit& _unit, bool _reset = false);
		void LayBody(CombatUnit& _unit);
		static void ValidateDefinition(const CombatDefinition& _definition, bool _enemy);
		static void ValidateBuff(const BuffDefinition& _definition);
		static void ValidatePoint(WorldPoint _point, bool _spawn = false);

		void SpawnDue();

		void UpdateEnemy(CombatUnit& _unit);

		void MoveEnemy(CombatUnit& _unit, bool _standing);
		void PlanRoute(CombatUnit& _unit, WorldPoint _destination);
		void RebuildRouteTail(std::size_t _spawn) const;
		void MoveAttracted(CombatUnit& _unit);
		void MoveFeared(CombatUnit& _unit);
		void BuildFearTiles(CombatUnit& _unit);
		bool PlanFear(CombatUnit& _unit);
		void PickFearPoint(CombatUnit& _unit);
		[[nodiscard]] bool FearReachable(const CombatUnit& _unit, int _from, int _to) const;

		void UpdateAlly(CombatUnit& _unit);
		static void ValidateSpawnMetadata(const EnemySpawn& _spawn);
		void PayBounty(const CombatUnit& _unit, UnitId _killer);
		void InstallChoiceEffects();
		void InstallBandEffects();
		void NotifyBands(ContentEvent& _event, bool _early);
		void NotifyChoices(const ContentEvent& _event);
		void SyncChoiceFullHealth(UnitId _unit);
		void InstallProfession(CombatUnit& _unit);
		void NotifyProfession(const ContentEvent& _event);
		void NotifyProfessionLate(ContentEvent& _event);
		void StartDollSwitch(CombatUnit& _unit);
		void EndProfessionBuff(CombatUnit& _unit, BuiltinBuff _kind, bool _expired);
		void ProfessionPeriodic(CombatUnit& _unit);
		void Aftershock(UnitId _source, WorldPoint _point);
		void ApplyLibrator(CombatUnit& _unit, bool _ramp);
		[[nodiscard]] UnitId LowestHpAllyInRange(const CombatUnit& _unit) const;
		[[nodiscard]] bool ProfessionCanAttack(const CombatUnit& _unit) const noexcept;
		void StoreEnergy(CombatUnit& _unit);

		[[nodiscard]] bool EnemyAttack(CombatUnit& _unit, double _previousCooldown);

		[[nodiscard]] bool CheckBlock(CombatUnit& _enemy);

		void ReleaseBlock(CombatUnit& _enemy);
		void SwitchStealth(CombatUnit& _enemy);
		[[nodiscard]] bool EnemyStealthed(const CombatUnit& _enemy) const noexcept;
		[[nodiscard]] bool Displaceable(const CombatUnit& _unit) const noexcept;

		void ReleaseBlocked(CombatUnit& _ally);

		void TickStatuses();

		void TickBuffs();
		void TickElementBurst(CombatUnit& _unit, CombatBuff& _buff);

		void Recalculate(CombatUnit& _unit);

		void RefreshRange(CombatUnit& _unit);
		[[nodiscard]] std::bitset<399> RangeMask(WorldPoint _origin, Facing _facing, std::span<const RangeOffset> _grid, int _extend, std::vector<int>* _order = nullptr) const;
		[[nodiscard]] bool UsesInitialPosition(const CombatUnit& _unit) const noexcept;
		[[nodiscard]] WorldPoint RulePosition(const CombatUnit& _unit) const noexcept;
		[[nodiscard]] const std::bitset<FieldTiles>& RuleRange(const CombatUnit& _unit) const noexcept;
		[[nodiscard]] std::span<const int> RuleRangeKeys(const CombatUnit& _unit) const noexcept;
		[[nodiscard]] const AttackProfile& EffectiveAttack(const CombatUnit& _unit) const noexcept;
		void ResetSkill(CombatUnit& _unit, bool _initial, std::optional<double> _carrySp);
		void NormalizeSp(CombatUnit& _unit);
		void TickSkill(CombatUnit& _unit);
		void ApplySkillModifiers(CombatUnit& _unit);
		[[nodiscard]] bool SkillCondition(const CombatUnit& _unit, bool _allyOnly = false) const;
		[[nodiscard]] bool ExtraSkillTriggerSatisfied(const CombatUnit& _unit) const;
		[[nodiscard]] bool AttackDisabled(const CombatUnit& _unit) const noexcept;
		[[nodiscard]] bool CanAutoSkill(const CombatUnit& _unit) const noexcept;
		bool SkillAboutToAttack(CombatUnit& _unit);
		void SkillDamaged(CombatUnit& _unit);
		void SkillAttackPerformed(CombatUnit& _unit, bool _usedOverride, bool _noAmmo, std::span<const UnitId> _targets);
		double ApplyHealthLoss(UnitId _source, UnitId _target, double _amount, bool _recoverSp, const DamageInfo& _damage);

		void RunBuffEffects(UnitId _source, UnitId _target, std::span<const BuffEffect> _effects, double _delta);


		[[nodiscard]] bool InRange(const CombatUnit& _attacker, const CombatUnit& _target) const;

		[[nodiscard]] std::span<const UnitId> AllyTargets(CombatUnit& _unit);

		[[nodiscard]] std::span<const UnitId> EnemyTargets(const CombatUnit& _unit);

		[[nodiscard]] std::span<const UnitId> SelectTargets(std::size_t _limit);

		bool Attack(CombatUnit& _source, std::span<const UnitId> _targets, bool _noAmmo = false);

		void Hit(UnitId _source, UnitId _target, const AttackProfile& _profile, WorldPoint _point = {}, bool _initialPosition = false);
		[[nodiscard]] double MainAttackMultiplier(CombatUnit& _source, const CombatUnit& _target, const AttackProfile& _profile, bool _initialPosition) noexcept;
		void HealAttack(const CombatUnit& _source, UnitId _target, const AttackProfile& _profile);
		[[nodiscard]] AttackScratch& AcquireAttackScratch();
		[[nodiscard]] bool TargetableEnemy(const CombatUnit& _target, const AttackProfile& _profile) const noexcept;

		void UpdateProjectiles();
		double AbsorbShields(CombatUnit& _unit, double _amount, DamageType _type);

		void Kill(CombatUnit& _unit, UnitId _source);

		void Leak(CombatUnit& _unit, bool _timeout);

		void CheckRedeploys();

		void Finish(BattleEndReason _reason);

		void Emit(
			BattleEventKind _kind,
			UnitId _source = 0,
			UnitId _target = 0,
			double _amount = 0,
			std::optional<CombatStatus> _status = std::nullopt,
			std::size_t _player = NoPlayer
		);

		BattleInput _MyInput;
		BattleClock _MyClock;
		Random _MyRandom;
		std::vector<std::uint64_t> _MyBuffTickIds;
		std::vector<BuffEffect> _MyBuffTickEffects;
		struct ChoiceRuntime { std::size_t MyPlayer{}; std::size_t MyEffect{}; bool MyEnabled{}; };
		std::vector<ChoiceRuntime> _MyChoiceEffects;
		std::vector<double> _MyChoiceHealing;
		bool _MyChoiceFullHealth{};

		struct BandRuntime
		{
			std::size_t MyPlayer{};
			std::size_t MyEffect{};
			unsigned MyUsed{};
			std::vector<UnitId> MyChosen{};
		};

		std::vector<BandRuntime> _MyBandEffects;
		bool _MyBandHitEffects{};
		std::vector<ContentInstance> _MyContentInstances;
		std::vector<ContentError> _MyContentErrors;
		unsigned _MyContentDepth{};
		bool _MyContentFault{};
		// 分段值存储使召唤回调不会让攻击／Buff 当前持有的单位引用悬空，不为每个单位单独分配指针。
		std::deque<CombatUnit> _MyUnits;
		std::vector<UnitId> _MyAllyIds;
		std::vector<UnitId> _MyEnemyIds;
		std::deque<EnemySpawn> _MyExtraSpawns;
		std::vector<BattlePlayerState> _MyPlayers;
		std::deque<TurretState> _MyTurrets;
		std::deque<ColdWindState> _MyColdWinds;
		std::vector<ScheduledAction> _MyScheduled;
		std::vector<ScheduledAction> _MyDueActions;
		std::uint64_t _MyScheduleSequence{};
		std::uint64_t _MyDamageSequence{};
		std::uint64_t _MyAttackSequence{};
		std::vector<Projectile> _MyProjectiles;
		std::vector<Projectile> _MyArrivedProjectiles;
		std::vector<TargetCandidate> _MyTargetCandidates;
		std::deque<AttackScratch> _MyAttackScratch;
		std::size_t _MyAttackDepth{};
		std::vector<UnitId> _MyTargets;
		mutable std::vector<std::vector<double>> _MyRouteTails;
		mutable std::vector<std::uint64_t> _MyRouteTailVersions;
		mutable std::optional<FieldGrid> _MyGrid;
		std::deque<BattleEvent> _MyEvents;
		std::size_t _MyAllyCount{};
		std::size_t _MyNextSpawn{};
		std::size_t _MyKilled{};
		std::size_t _MyLeaked{};
		std::size_t _MyTotal{};
		std::size_t _MyUnspawned{};
		std::uint64_t _MyDeploySequence{};
		std::uint64_t _MySpawnSequence{};
		std::uint64_t _MyBuffSequence{};
		std::uint64_t _MyFearSequence{};
		std::uint64_t _MyTriggerRangeSequence{};
		std::vector<FieldPoint> _MyFieldWaypoints;
		std::bitset<FieldTiles> _MyGroundPathMask{};
		std::uint64_t _MyGroundPathVersion{std::numeric_limits<std::uint64_t>::max()};
		BattleEndReason _MyReason{BattleEndReason::RUNNING};
		std::optional<double> _MyBossHealthAtEnd{};
		bool _MyStarted{};
		bool _MyStartDeploying{};
		bool _MyHasTerrain{};
		bool _MyStepping{};
	};
}
#endif
