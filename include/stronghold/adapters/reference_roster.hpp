#ifndef STRONGHOLD_ADAPTERS_REFERENCE_ROSTER_HPP
#define STRONGHOLD_ADAPTERS_REFERENCE_ROSTER_HPP
#include <stronghold/domain/content_pools.hpp>
#include <expected>
#include <stronghold/adapters/reference_ally.hpp>
#include <stronghold/domain/preparation.hpp>

namespace Stronghold
{
	struct RosterRule
	{
		std::string_view MyId{};
		std::string_view MyGoldenId{};
		bool MySelectable{};
		bool MyOwnable{};
		std::span<const int> MySkills{};
		int MyDefaultSkill{-1};
		std::span<const std::string_view> MyModules{};
		std::string_view MyDefaultModule{};
	};

	struct DiySlotRule
	{
		std::string_view MyId{};
		std::string_view MyGoldenId{};
		unsigned MyTier{};
		unsigned MyShopLevel{};
	};

	struct TokenSkillSource { int MySkill{}; bool MyAvailable{}; };
	struct TokenModuleCount { std::string_view MyModule{}; unsigned MyCount{1}; };
	struct TokenSupplyVariant
	{
		std::string_view MyOwner{};
		bool MyAvailable{};
		unsigned MyCount{1};
		std::span<const TokenSkillSource> MySkills{};
		std::span<const TokenModuleCount> MyModules{};
	};
	struct TokenSupplyRule
	{
		std::string_view MyId{};
		bool MyOwnerRange{};
		bool MyOwnerRangeOutside{};
		unsigned MyCount{1};
		std::span<const TokenSupplyVariant> MyVariants{};
	};

	[[nodiscard]] std::span<const RosterRule> ReferenceRosterRules() noexcept;
	[[nodiscard]] std::span<const DiySlotRule> ReferenceDiySlots() noexcept;
	[[nodiscard]] std::span<const std::string_view> ReferenceExcludedDiyModules() noexcept;
	[[nodiscard]] std::span<const TokenSupplyRule> ReferenceTokenSupplies() noexcept;

	enum class RosterError { BAD_MESSAGE, BAD_TARGET, BOT };
	struct LoadoutChoice
	{
		std::string_view MyId{};
		std::optional<int> MySkill{};
		std::optional<std::string_view> MyModule{};
	};
	struct SelectedLoadout
	{
		std::string_view MyId{};
		int MySkill{};
		std::string_view MyModule{}; // 空串对应无精锐记录时的 null。
	};
	struct DiyPick
	{
		std::string_view MySlot{};
		std::string_view MyCharacter{}; // 空串表示显式清空槽位。
		std::optional<int> MySkill{};
		std::optional<std::string_view> MyModule{};
	};
	struct SelectedDiy
	{
		std::string_view MySlot{};
		std::string_view MyCharacter{};
		int MySkill{};
		std::string_view MyModule{};
	};
	struct RosterOperator
	{
		const OperatorRecord& MyIdentity;
		const AllyLoadoutRecord& MyLoadout;
		std::span<const std::string_view> MyBonds{};
		std::string_view MyTokenOwner{};
		bool MyStandIn{};
		bool MyDiySelected{};

		// 协议中的精锐形态总有“不装备”选项，即使空 DIY 身体没有 modules 表。
		[[nodiscard]] std::string_view ModuleId() const noexcept
		{ return MyIdentity.MyGolden ? (MyLoadout.MyModuleId.empty() ? "none" : MyLoadout.MyModuleId) : ""; }
	};
	struct TokenAllowance
	{
		std::string_view MyId{};
		unsigned MyCount{};
		bool MyOwnerRange{};
		bool MyOwnerRangeOutside{};
	};

	// 开局前配置的值类型。成功保存的字符串/查询结果均引用编译期数据，不借用请求缓冲区。
	// 阶段权限由对局宿主管理；机器人始终保持默认配置。自选的 kit 白名单由实际内容注册层提供。
	class PlayerRoster final
	{
	public:
		explicit PlayerRoster(bool _bot = false) : _MyBot(_bot) { }
		[[nodiscard]] std::expected<void, RosterError> SetLoadout(std::span<const LoadoutChoice> _choices);
		[[nodiscard]] std::expected<std::size_t, RosterError> SetNotOwned(std::span<const std::string_view> _ids);
		[[nodiscard]] std::expected<std::size_t, RosterError> SetDiy(
			std::span<const DiyPick> _picks,
			std::span<const std::string_view> _implementedCharacters
		);
		[[nodiscard]] RosterOperator Resolve(std::string_view _id) const;
		// 清空并复用调用方缓冲区。数量是准备阶段的部署上限，与战斗召唤技能的 count 无关。
		void PlaceableTokens(std::string_view _id, std::vector<TokenAllowance>& _out) const;
		[[nodiscard]] SummonCatalog MakeSummonCatalog() const;
		[[nodiscard]] std::vector<ContentPoolRoster> MakeContentPoolRoster() const;
		[[nodiscard]] std::span<const SelectedLoadout> Loadouts() const noexcept { return _MyLoadouts; }
		[[nodiscard]] std::span<const std::string_view> NotOwned() const noexcept { return _MyNotOwned; }
		[[nodiscard]] std::span<const SelectedDiy> Diy() const noexcept { return _MyDiy; }

	private:
		bool _MyBot{};
		std::vector<SelectedLoadout> _MyLoadouts;
		std::vector<std::string_view> _MyNotOwned;
		std::vector<SelectedDiy> _MyDiy;
	};

	// 将已经验证的玩家配置转换为独立经济核心的值类型规则；禁用盟约由对局抽取后传入。
	[[nodiscard]] std::expected<void, CommandError> ConfigurePreparationRoster(
		EconomySession& _session,
		std::string_view _player,
		const PlayerRoster& _roster,
		std::span<const std::string_view> _inactiveBonds = {}
	);
}
#endif
