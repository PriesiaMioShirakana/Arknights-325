#ifndef STRONGHOLD_SIMULATION_BATTLE_HPP
#define STRONGHOLD_SIMULATION_BATTLE_HPP
#include <deque>
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
	class Battle final
	{
	public:
		explicit Battle(BattleInput _input);
		Battle(const Battle&) = delete;
		Battle& operator=(const Battle&) = delete;
		Battle(Battle&&) = default;
		Battle& operator=(Battle&&) = default;

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

		[[nodiscard]] std::span<const BattlePlayerState> Players() const noexcept { return _MyPlayers; }

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

		struct LentEquipmentView
		{
			UnitId MyUnit{};
			std::string_view MyItem{};
			double MyUntil{};
		};

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
		void NotifyOperatorKits(ContentEvent& _event);
		bool OperatorBeforeAttack(CombatUnit& _unit, std::vector<UnitId>& _targets);
		void OperatorAfterHit(UnitId _source, UnitId _target);
		[[nodiscard]] bool OperatorCanAttack(const CombatUnit& _unit) const;
		void SortOperatorTargets(UnitId _source, std::vector<UnitId>& _targets, unsigned _limit = 0, const AttackProfile* _profile = nullptr);
		void OperatorEnemiesInGrid(UnitId _source, std::span<const RangeOffset> _grid, std::vector<UnitId>& _targets);
		[[nodiscard]] UnitId HighestHealthAllyInRange(UnitId _source) const;
		void NotifyVendlas(ContentEvent& _event);
		void TexasSkill(UnitId _unit, const TexasKit& _kit);
		void SunbrSkill(UnitId _unit, const SunbrKit& _kit, ContentEventKind _event);
		void GrantInsiderAmmo(UnitId _unit, std::uint64_t _deployment);
		void OperatorReveal(UnitId _unit);
		void NotifyOperatorObservers(ContentEvent& _event);
		[[nodiscard]] bool OperatorInGrid(UnitId _source, UnitId _target, std::span<const RangeOffset> _grid) const;
		void PodegoAura(UnitId _unit);
		void PodegoStartZone(UnitId _unit, const PodegoKit& _kit);
		void PodegoZonePulse(UnitId _unit, WorldPoint _point, double _damage);
		void TinmanStartZone(UnitId _unit, const TinmanKit& _kit);
		void TinmanZonePulse(UnitId _unit, WorldPoint _point, double _attack, std::uint64_t _sequence, bool _damage);
		void IndigoTick(UnitId _unit, const IndigoKit& _kit, double _delta);
		void UtageTick(UnitId _unit);
		void UtageProtect(UnitId _unit, const UtageKit& _kit);

		struct InsiderAmmoGrant
		{
			UnitId MySource{};
			UnitId MyTarget{};
			std::uint64_t MyDeployment{};
			double MyAmount{};
		};

		std::vector<InsiderAmmoGrant> _MyInsiderGrants{};
		bool _MyOperatorBeforeAttack{};
		bool _MyTinmanWither{};

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

		enum class ScheduledKind { CRATE_BREAK, TOKEN_EXPIRE, COLD_WIND, PROFESSION, AFTERSHOCK, EQUIPMENT, EQUIPMENT_RETREAT, HAMMER, EQUIPMENT_EXPIRE, BOND_REFRESH, BOND_AURA, BOND_RAID, CORE_BOND_REFRESH, CORE_BOND_PULSE, GARRISON_REFRESH, INSIDER_AMMO, OPERATOR_REVEAL, PODEGO_AURA, PODEGO_ZONE, TINMAN_ZONE };

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
