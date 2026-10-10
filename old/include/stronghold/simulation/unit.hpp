#ifndef STRONGHOLD_SIMULATION_UNIT_HPP
#define STRONGHOLD_SIMULATION_UNIT_HPP
#include <stronghold/simulation/skill.hpp>

namespace Stronghold
{
	class Environment;
	struct ContentEvent;

	// 单位行为引用环境中的唯一状态，不复制生命、属性或技能状态。
	// 组件地址跨新增单位和外层 Battle 移动稳定；生命周期由环境持有。
	class Unit
	{
	public:
		Unit(Battle& _battle, UnitId _unit);
		Unit(Environment& _environment, UnitId _unit);
		virtual ~Unit() = default;
		Unit(const Unit&) = delete;
		Unit& operator=(const Unit&) = delete;

		[[nodiscard]] UnitId Id() const noexcept { return _MyState.MyId; }

		[[nodiscard]] const CombatUnit& State() const noexcept { return _MyState; }

		[[nodiscard]] const CombatDefinition& Definition() const noexcept { return _MyState.MyDefinition; }

		[[nodiscard]] Environment& World() const noexcept { return _MyEnvironment; }

		[[nodiscard]] const BattlePlayerState& Owner() const;
		[[nodiscard]] SkillBase& Skill() const;
		[[nodiscard]] WorldPoint RulePosition() const;
		[[nodiscard]] const std::bitset<FieldTiles>& Range() const;
		bool Attack(std::span<const UnitId> _targets = {}, bool _noAmmo = false);
		bool Relocate(WorldPoint _position);
		// 非死亡离场；死亡与漏怪分别由伤害及路线结算处理。
		bool Exit(RemovalReason _reason = RemovalReason::REMOVED, bool _permanent = true);

		// 来源单位发出效果，接受者执行结算；这些入口不重写公式，扩展使用回调。
		double DealDamage(UnitId _target, const DamageInfo& _damage);
		double TakeDamage(const DamageInfo& _damage, UnitId _source = 0);
		double Heal(UnitId _target, double _amount, HealOptions _options = {});
		double ReceiveHealing(double _amount, UnitId _source = 0, HealOptions _options = {});
		double LoseHealth(double _amount, UnitId _source = 0, bool _silent = false);
		void AddShield(Shield _shield);
		bool ApplyStatus(CombatStatus _status, const StatusApplication& _application);
		bool RemoveStatus(CombatStatus _status);
		std::uint64_t AddBuff(BuffDefinition _definition);
		std::size_t RemoveBuff(std::string_view _key);
		bool RemoveBuff(std::uint64_t _handle);
		bool ApplyStrongest(std::string _key, double _duration, BuffStrength _strength, UnitId _source = 0);
		double TakeElement(ElementHit _hit, UnitId _source = 0);
		double ReduceElement(double _amount, std::optional<Element> _element = std::nullopt);
		double Push(double _force, const PushOptions& _options = {});
		double PullToFront(UnitId _source, double _force);
		void Handle(ContentEvent& _event);

	protected:
		[[nodiscard]] BattleCore& Core() const noexcept;
		[[nodiscard]] Battle& BattleView() const noexcept; // 旧扩展兼容入口，新效果使用 World() 和目标单位。
		virtual bool DoAttack(std::span<const UnitId> _targets, bool _noAmmo);
		virtual bool DoRelocate(WorldPoint _position);
		virtual bool DoExit(RemovalReason _reason, bool _permanent);

		// 来源与接受者各收到一次，自身作用只收到一次；通过事件 ID 区分角色。
		// 前置伤害修改 MyDamage，前置治疗修改 MyAmount；MyCancel 取消结算。
		// 后置 MyAmount 为实际生命变化；元素量表使用独立事件。
		virtual void OnBeforeDamage(ContentEvent& _event);
		virtual void OnAfterDamage(const ContentEvent& _event);
		virtual void OnBeforeHeal(ContentEvent& _event);
		virtual void OnAfterHeal(const ContentEvent& _event);

		virtual void OnDeploy(ContentEvent& _event);
		virtual void OnTick(ContentEvent& _event);
		virtual void OnDamaged(ContentEvent& _event);
		virtual void OnDeath(ContentEvent& _event);
		virtual void OnEvent(ContentEvent& _event);

	private:
		friend class BattleCore;
		double CommitHealthLoss(UnitId _source, double _amount, bool _recoverSp, const DamageInfo& _damage);
		Environment& _MyEnvironment;
		CombatUnit& _MyState;
	};

	// 敌人仍由环境自动推进路线；位置入口用于效果位移，不增加玩家直接控制。
	class EnemyBase : public Unit
	{
	public:
		using Unit::Unit;

	protected:
		bool DoRelocate(WorldPoint _position) override;
	};

	class SummonBase : public Unit
	{
	public:
		using Unit::Unit;
	};

	class DeviceBase : public Unit
	{
	public:
		using Unit::Unit;
	};
}
#endif
