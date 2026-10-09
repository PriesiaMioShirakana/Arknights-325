#ifndef STRONGHOLD_SIMULATION_CONTENT_REGISTRY_HPP
#define STRONGHOLD_SIMULATION_CONTENT_REGISTRY_HPP
#include <concepts>
#include <memory>
#include <string_view>
#include <stronghold/simulation/combat_types.hpp>
#include <type_traits>

namespace Stronghold
{
	class Battle;

	enum class ContentEventKind
	{
		DEPLOY,
		BATTLE_START,
		TICK,
		BEFORE_DAMAGE,
		DAMAGED,
		HEALED,
		ATTACK,
		DEATH,
		BEFORE_STATUS,
		STATUS_APPLIED,
		BUFF_APPLIED,
		BUFF_TICK,
		BUFF_REMOVED,
		BUFF_EXPIRED,
		BATTLE_END,
		SKILL_START,
		SKILL_ENDING,
		SKILL_END,
		SKILL_TICK,
		SP_GAIN,
		AMMO_USED,
		ELEMENT_HIT,
		ELEMENT_BURST,
		ATTACK_HIT,
		BEFORE_HEALTH_DAMAGE,
		FATAL,
		BEFORE_KILL,
		DODGED,
		BEFORE_HEAL,
		MERCHANT_PAY,
		BARD_REGEN,
		BOOMERANG_CAUGHT,
		DOLL_SWAP,
		ENEMY_LEAK,
		LAYER_GAIN,

	};

	// 事件值在当前调用链中共享；BeforeDamage / BeforeStatus 可以修改数值或取消。
	// 其余通知是结果事件，修改字段不会倒改已经提交的伤害、死亡或状态。
	// MyHandlerUnit / MyHandlerOwner 标识处理器所有者，MySource / MyTarget 标识事件参与者。
	struct ContentEvent
	{
		ContentEventKind MyKind{};
		UnitId MyUnit{};
		UnitId MySource{};
		UnitId MyTarget{};
		UnitId MyHandlerUnit{};
		std::size_t MyHandlerOwner{};
		std::uint64_t MyBuff{};
		double MyAmount{};
		double MyDelta{};
		DamageInfo MyDamage{};
		CombatStatus MyStatus{};
		StatusApplication MyApplication{};
		bool MyCancel{};
		bool MyNoAmmo{};
		SkillReason MySkillReason{};
		SpReason MySpReason{};
		ElementHit MyElementHit{};
		std::optional<Element> MyElement{};
		bool MySplash{};
		bool MyChain{};
		UnitId MyCredit{};
		bool MyPrevented{};
		RemovalReason MyRemovalReason{};
		bool MyInitial{};
		bool MyMove{};
		HealOptions MyHealOptions{};
		std::size_t MyTargetCount{};
		std::uint64_t MyAttackId{};
		bool MySkillAttack{};
		WorldPoint MyPoint{};
		bool MyDoll{};
		std::size_t MyPlayer{NoPlayer};
		std::string_view MyBondId{};
		std::string_view MyReason{};
		std::optional<WorldPoint> MySourceTile{};
		bool MyDying{};
		std::span<const UnitId> MyTargets{}; // 仅本次攻击回调借用；保留主攻击的目标次序。
		bool MyEquipmentSilenced{};
		bool MyStatusEntered{};
		bool MyRevived{}; // 成功的装备／战略复活不占用阿戈尔首次击倒名额。
	};

	// 多态只存在于 CUSTOM 实例中：内置单位不创建该对象，也不经由此接口行动。
	// 实例由所属 Battle 独占，注册表只保存工厂。处理器不得保留事件引用或跨移动缓存 Battle 地址。
	class CustomContent
	{
	public:
		virtual ~CustomContent() = default;
		virtual void Handle(Battle& _battle, ContentEvent& _event) = 0;
	};

	// CRTP 将有实现的钩子静态绑定到派生类；没有实现的钩子在编译期去掉。
	// 运行时只有一个擦除类型的 Handle 入口，不为每个可选钩子建立一组虚函数。
	template <class _Derived, ContentTag _Tag>
	class CustomContentHandler : public CustomContent
	{
	public:
		static_assert(IsCustom(_Tag));
		static constexpr ContentTag Tag = _Tag;

		void Handle(Battle& _battle, ContentEvent& _event) final
		{
			auto& handler = static_cast<_Derived&>(*this);
			switch (_event.MyKind)
			{
			case ContentEventKind::LAYER_GAIN:
				if constexpr (requires { handler.OnLayerGain(_battle, _event); }) handler.OnLayerGain(_battle, _event);
				break;
			case ContentEventKind::SKILL_START:
				if constexpr (requires { handler.OnSkillStart(_battle, _event); }) handler.OnSkillStart(_battle, _event);
				break;
			case ContentEventKind::SKILL_ENDING:
				if constexpr (requires { handler.OnSkillEnding(_battle, _event); }) handler.OnSkillEnding(_battle, _event);
				break;
			case ContentEventKind::SKILL_END:
				if constexpr (requires { handler.OnSkillEnd(_battle, _event); }) handler.OnSkillEnd(_battle, _event);
				break;
			case ContentEventKind::SKILL_TICK:
				if constexpr (requires { handler.OnSkillTick(_battle, _event); }) handler.OnSkillTick(_battle, _event);
				break;
			case ContentEventKind::SP_GAIN:
				if constexpr (requires { handler.OnSpGain(_battle, _event); }) handler.OnSpGain(_battle, _event);
				break;
			case ContentEventKind::AMMO_USED:
				if constexpr (requires { handler.OnAmmoUsed(_battle, _event); }) handler.OnAmmoUsed(_battle, _event);
				break;
			case ContentEventKind::ELEMENT_HIT:
				if constexpr (requires { handler.OnElementHit(_battle, _event); }) handler.OnElementHit(_battle, _event);
				break;
			case ContentEventKind::ELEMENT_BURST:
				if constexpr (requires { handler.OnElementBurst(_battle, _event); }) handler.OnElementBurst(_battle, _event);
				break;
			case ContentEventKind::ATTACK_HIT:
				if constexpr (requires { handler.OnAttackHit(_battle, _event); }) handler.OnAttackHit(_battle, _event);
				break;
			case ContentEventKind::BEFORE_HEALTH_DAMAGE:
				if constexpr (requires { handler.OnBeforeHealthDamage(_battle, _event); }) handler.OnBeforeHealthDamage(_battle, _event);
				break;
			case ContentEventKind::FATAL:
				if constexpr (requires { handler.OnFatal(_battle, _event); }) handler.OnFatal(_battle, _event);
				break;
			case ContentEventKind::BEFORE_KILL:
				if constexpr (requires { handler.OnBeforeKill(_battle, _event); }) handler.OnBeforeKill(_battle, _event);
				break;
			case ContentEventKind::DODGED:
				if constexpr (requires { handler.OnDodged(_battle, _event); }) handler.OnDodged(_battle, _event);
				break;
			case ContentEventKind::ENEMY_LEAK:
				if constexpr (requires { handler.OnEnemyLeak(_battle, _event); }) handler.OnEnemyLeak(_battle, _event);
				break;
			case ContentEventKind::DOLL_SWAP:
				if constexpr (requires { handler.OnDollSwap(_battle, _event); }) handler.OnDollSwap(_battle, _event);
				break;
			case ContentEventKind::BOOMERANG_CAUGHT:
				if constexpr (requires { handler.OnBoomerangCaught(_battle, _event); }) handler.OnBoomerangCaught(_battle, _event);
				break;
			case ContentEventKind::MERCHANT_PAY:
				if constexpr (requires { handler.OnMerchantPay(_battle, _event); }) handler.OnMerchantPay(_battle, _event);
				break;
			case ContentEventKind::BARD_REGEN:
				if constexpr (requires { handler.OnBardRegen(_battle, _event); }) handler.OnBardRegen(_battle, _event);
				break;
			case ContentEventKind::BEFORE_HEAL:
				if constexpr (requires { handler.OnBeforeHeal(_battle, _event); }) handler.OnBeforeHeal(_battle, _event);
				break;
			case ContentEventKind::DEPLOY:
				if constexpr (requires { handler.OnDeploy(_battle, _event); }) handler.OnDeploy(_battle, _event);
				break;
			case ContentEventKind::BATTLE_START:
				if constexpr (requires { handler.OnBattleStart(_battle, _event); }) handler.OnBattleStart(_battle, _event);
				break;
			case ContentEventKind::TICK:
				if constexpr (requires { handler.OnTick(_battle, _event); }) handler.OnTick(_battle, _event);
				break;
			case ContentEventKind::BEFORE_DAMAGE:
				if constexpr (requires { handler.OnBeforeDamage(_battle, _event); }) handler.OnBeforeDamage(_battle, _event);
				break;
			case ContentEventKind::DAMAGED:
				if constexpr (requires { handler.OnDamaged(_battle, _event); }) handler.OnDamaged(_battle, _event);
				break;
			case ContentEventKind::HEALED:
				if constexpr (requires { handler.OnHealed(_battle, _event); }) handler.OnHealed(_battle, _event);
				break;
			case ContentEventKind::ATTACK:
				if constexpr (requires { handler.OnAttack(_battle, _event); }) handler.OnAttack(_battle, _event);
				break;
			case ContentEventKind::DEATH:
				if constexpr (requires { handler.OnDeath(_battle, _event); }) handler.OnDeath(_battle, _event);
				break;
			case ContentEventKind::BEFORE_STATUS:
				if constexpr (requires { handler.OnBeforeStatus(_battle, _event); }) handler.OnBeforeStatus(_battle, _event);
				break;
			case ContentEventKind::STATUS_APPLIED:
				if constexpr (requires { handler.OnStatusApplied(_battle, _event); }) handler.OnStatusApplied(_battle, _event);
				break;
			case ContentEventKind::BUFF_APPLIED:
				if constexpr (requires { handler.OnApply(_battle, _event); }) handler.OnApply(_battle, _event);
				break;
			case ContentEventKind::BUFF_TICK:
				if constexpr (requires { handler.OnTick(_battle, _event); }) handler.OnTick(_battle, _event);
				break;
			case ContentEventKind::BUFF_REMOVED:
				if constexpr (requires { handler.OnRemove(_battle, _event); }) handler.OnRemove(_battle, _event);
				break;
			case ContentEventKind::BUFF_EXPIRED:
				if constexpr (requires { handler.OnExpire(_battle, _event); }) handler.OnExpire(_battle, _event);
				break;
			case ContentEventKind::BATTLE_END:
				if constexpr (requires { handler.OnBattleEnd(_battle, _event); }) handler.OnBattleEnd(_battle, _event);
				break;
			}
		}
	};

	template <class _Derived>
	using CustomOperator = CustomContentHandler<_Derived, ContentTag::CUSTOM_OPERATOR>;
	template <class _Derived>
	using CustomEnemy = CustomContentHandler<_Derived, ContentTag::CUSTOM_ENEMY>;
	template <class _Derived>
	using CustomBuff = CustomContentHandler<_Derived, ContentTag::CUSTOM_BUFF>;
	template <class _Derived>
	using CustomBond = CustomContentHandler<_Derived, ContentTag::CUSTOM_BOND>;
	template <class _Derived>
	using CustomBondEffect = CustomContentHandler<_Derived, ContentTag::CUSTOM_BOND_EFFECT>;

	// 启动阶段注册并 Seal，之后可被多局只读共享。调用方必须保持注册表的地址和生命周期。
	// 注册时允许分配；战斗查找以编号直接索引，避免在每帧按字符串检索或分配 std::function。
	class ContentRegistry final
	{
	public:
		explicit ContentRegistry(std::size_t _capacity = 0) { _MyEntries.reserve(_capacity); }

		template <class _Handler>
			requires std::derived_from<_Handler, CustomContent> && std::default_initializable<_Handler>
		ContentReference Register(std::string _id)
		{
			static_assert(IsCustom(_Handler::Tag));
			return RegisterEntry(std::move(_id), _Handler::Tag, []() -> std::unique_ptr<CustomContent>
			{
				return std::make_unique<_Handler>();
			});
		}

		void Seal() noexcept { _MySealed = true; }
		[[nodiscard]] bool Sealed() const noexcept { return _MySealed; }
		[[nodiscard]] std::size_t Size() const noexcept { return _MyEntries.size(); }
		[[nodiscard]] ContentReference Find(ContentTag _tag, std::string_view _id) const;
		void Validate(ContentReference _reference, ContentTag _expected) const;
		[[nodiscard]] std::unique_ptr<CustomContent> Create(ContentReference _reference) const;

	private:
		using Factory = std::unique_ptr<CustomContent> (*)();
		struct Entry
		{
			std::string MyId{};
			ContentTag MyTag{};
			Factory MyFactory{};
		};

		ContentReference RegisterEntry(std::string _id, ContentTag _tag, Factory _factory);
		std::vector<Entry> _MyEntries;
		bool _MySealed{};
	};

	struct ContentError
	{
		std::uint64_t MyTick{};
		ContentTag MyTag{};
		std::uint32_t MyRegistration{};
		std::string MyMessage{};
	};
}
#endif
