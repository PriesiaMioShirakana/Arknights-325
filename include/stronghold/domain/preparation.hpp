#ifndef STRONGHOLD_DOMAIN_PREPARATION_HPP
#define STRONGHOLD_DOMAIN_PREPARATION_HPP
#include <expected>
#include <functional>
#include <stronghold/domain/summon_catalog.hpp>
#include <stronghold/domain/pool.hpp>
#include <variant>

namespace Stronghold
{
	using PieceUid = std::uint64_t;

	struct Piece
	{
		PieceUid MyUid{};
		std::string MyId;
		int MyPoolCopies{};
		Facing MyFacing{Facing::RIGHT};
		std::vector<Piece> MyItems{}; // 仅干员持有装备；装备本身不再嵌套其它物品。
		std::optional<unsigned> MyTemporaryDue{};
		bool MyDeferredMerge{};
		PieceUid MyOwnerUid{}; // 非零表示召唤物，普通棋子不借用这个标记。
		unsigned MyCount{1};
		[[nodiscard]] bool IsToken() const noexcept { return MyOwnerUid != 0; }
	};

	using BoardPieces = std::array<std::optional<Piece>, 36>;

	struct ShopSlot
	{
		std::string MyId;
		int MyPrice{};
		bool MyFrozen{};
		bool MySold{};
	};

	struct PrivateStockSelection
	{
		std::string_view MyId{};
		int MyShopLevel{1};
		bool MyEnabled{true}; // 所有盟约被禁用时仍保留选择，但不生成库存。
	};

	struct PlacementOverride
	{
		std::string MyId;
		PlacementClass MyPlacement{};
	};

	// 规则计数与展示统计分别保存；开局收入之前重置本回合计数，整局统计跨回合保留。
	struct PreparationRoundStatistics
	{
		std::uint64_t MyRefreshes{};
		std::uint64_t MyBuys{};
		std::uint64_t MySells{};
		std::uint64_t MySpent{};
		std::uint64_t MyGainedChess{};
		std::uint64_t MyArts{};
	};

	struct EconomyStatistics
	{
		std::uint64_t MySpent{};
		std::uint64_t MyFundsGained{};
		std::uint64_t MyRefreshes{};
		std::uint64_t MyBuys{};
		std::uint64_t MySells{};
		std::uint64_t MyChessMerges{};
		std::uint64_t MyItemMerges{};
		std::uint64_t MyItemsEquipped{};
	};

	struct PurchaseUpgrade
	{
		PieceKind MyKind{};
		std::uint64_t MyRemaining{};
		std::string MySource{};
	};

	struct PlayerView
	{
		std::string MyPlayerId;
		std::int64_t MyFunds{};
		int MyLevel{1};
		int MyUpgradePrice{};
		bool MyFrozen{};
		bool MyReady{};
		std::vector<std::optional<Piece>> MyHand;
		BoardPieces MyBoard;
		BoardLayout MyLayout;
		std::size_t MyDeployCap{};

		[[nodiscard]] std::size_t DeployCount() const noexcept
		{
			return static_cast<std::size_t>(
				std::ranges::count_if(MyBoard, [](const auto& _piece) { return _piece && !_piece->IsToken(); }));
		}

		std::vector<std::optional<ShopSlot>> MyShop;
		std::vector<std::vector<ShopSlot>> MyOffers;
		std::int64_t MyPendingFunds{};
		bool MyAlive{true};
		std::vector<std::optional<Piece>> MyTemporary{};
		unsigned MyPreparationsEnded{};
		std::vector<PrivatePoolEntry> MyPrivateStock{};
		std::vector<PlacementOverride> MyPlacementOverrides{};
		// 物理棋盘仍是固定数组；另存原版 Map 的键插入顺序，用于召唤物回收与回合补发。
		std::array<std::size_t, 36> MyBoardOrder{};
		std::size_t MyBoardOrderSize{};
		std::uint64_t MyFreeRefreshes{};
		std::vector<PurchaseUpgrade> MyPurchaseUpgrades{};
		bool MyKeepRemainingFunds{};
		PreparationRoundStatistics MyRoundStatistics{};
		EconomyStatistics MyStatistics{};
	};

	struct PublicPlayerView
	{
		std::string MyPlayerId;
		int MyLevel{};
		bool MyReady{};
		BoardPieces MyBoard;
		bool MyAlive{true};
	};

	// 一次完整回合的经济提交；待救援玩家须先在结算层完成救援或最终淘汰。
	struct EconomySettlement
	{
		std::string_view MyPlayerId{};
		std::int64_t MyFunds{};
		bool MyEliminated{};
	};

	enum class PreparationPhase
	{
		IDLE,
		PREPARING,
		CLOSED,
		ROUND_START
	};

	enum class CommandError
	{
		UNKNOWN_PLAYER,
		STALE_ROUND,
		WRONG_PHASE,
		READY,
		BAD_TARGET,
		SOLD_OUT,
		NO_FUNDS,
		HAND_FULL,
		MAX_LEVEL,
		BAD_TILE,
		BOARD_FULL,
		ELIMINATED,
		TEMP_NOT_EMPTY
	};

	struct Buy
	{
		std::size_t MySlot{};
	};

	struct Refresh
	{};

	struct Freeze
	{};

	struct LevelUp
	{};

	struct Sell
	{
		PieceUid MyUid{};
	};

	struct DestroyItem
	{
		PieceUid MyUid{};
	};

	struct SetReady
	{
		bool MyReady{};
	};

	struct PickReward
	{
		std::size_t MySlot{};
	};

	struct MoveToBoard
	{
		PieceUid MyUid{};
		BoardPosition MyPosition;
		Facing MyFacing{Facing::RIGHT};
	};

	struct MoveToHand
	{
		PieceUid MyUid{};
		std::size_t MySlot{};
	};

	struct EquipItem
	{
		PieceUid MyItem{};
		PieceUid MyTarget{};
		std::optional<PieceUid> MyReplace{};
	};

	struct GrantOptions
	{
		bool MyFromPool{true};
		bool MyToTemporary{};
		bool MyDeferItemMerge{};
	};

	struct PromotePiece { PieceUid MyUid{}; };
	struct UpgradeOwnedItem { PieceUid MyUid{}; };
	struct RemoveOwnedPiece { PieceUid MyUid{}; };
	struct TransformChess { PieceUid MyUid{}; std::string MyDefinitionId; };
	struct AttachItemDirect { PieceUid MyItem{}; PieceUid MyTarget{}; };
	using InventoryEffect = std::variant<PromotePiece, UpgradeOwnedItem, RemoveOwnedPiece, TransformChess, AttachItemDirect>;

	struct AdjustFunds { std::int64_t MyAmount{}; };
	struct AddPendingFunds { std::int64_t MyAmount{}; };
	struct GrantFreeRefreshes { std::uint64_t MyCount{}; };
	struct GrantPurchaseUpgrade { PurchaseUpgrade MyUpgrade{}; };
	struct KeepRemainingFunds { bool MyEnabled{}; };
	struct RaiseDeployCap { std::size_t MyMinimum{}; };
	using EconomyEffect = std::variant<AdjustFunds, AddPendingFunds, GrantFreeRefreshes,
		GrantPurchaseUpgrade, KeepRemainingFunds, RaiseDeployCap>;

	using PreparationCommand =
		std::variant<Buy, Refresh, Freeze, LevelUp, Sell, DestroyItem, SetReady, PickReward, MoveToBoard, MoveToHand, EquipItem>;

	struct CommandEnvelope
	{
		std::string MyPlayerId;
		int MyRound{};
		PreparationCommand MyCommand;
	};

	enum class EventKind
	{
		PURCHASED,
		REFRESHED,
		FROZEN,
		LEVELLED,
		SOLD,
		DESTROYED,
		READINESS,
		MERGED,
		REWARD_PICKED,
		MOVED,
		GRANTED,
		EQUIPPED,
		PROMOTED,
		ITEM_UPGRADED,
		ECONOMY_EFFECT
	};

	struct EconomyEvent
	{
		EventKind MyKind{};
		std::string MyDefinitionId;
		PieceUid MyUid{};
		std::int64_t MyAmount{};
	};

	struct ChangeSet
	{
		std::uint64_t MyRevision{};
		std::vector<EconomyEvent> MyEvents;
	};

	struct GrantResult
	{
		std::optional<PieceUid> MyPiece{}; // 手牌/临时区均满且不能合成时为空；干员占用的副本已归还。
		ChangeSet MyChanges{};
	};

	// 准备经济、库存、布阵与召唤物；具体内容效果和战斗由宿主组合。单个所有者串行调用。
	// 失败命令回滚资金、库存和随机流；原版满手召唤物撤回失败仍会把棋盘键移至遍历末尾。
	class EconomySession final
	{
	public:
		// The caller-owned catalog must outlive this session and must not be moved or reassigned while in use.
		EconomySession(
			const Catalog& _catalog,
			std::string_view _mode,
			std::span<const Seat> _players,
			std::uint32_t _seed,
			bool _experimental = false,
			bool _independentPools = false,
			const std::set<std::string, std::less<>>& _banned = {},
			BoardLayout _board = BoardLayout::Fallback());

		EconomySession(
			const Catalog&& _catalog,
			std::string_view _mode,
			std::span<const Seat> _players,
			std::uint32_t _seed,
			bool _experimental = false,
			bool _independentPools = false,
			const std::set<std::string, std::less<>>& _banned = {},
			BoardLayout _board = BoardLayout::Fallback()) = delete;

		// 默认同时进入准备，供独立经济宿主使用；完整对局传 false，发收入后先处理机变。
		void BeginRound(int _round, bool _openPreparation = true);
		// 仅开局前、未发放棋子时配置；复制输入，不借用调用方缓存。
		[[nodiscard]] std::expected<void, CommandError> ConfigureRoster(
			std::string_view _player,
			std::span<const PrivateStockSelection> _stock,
			std::span<const PlacementOverride> _placements
		);
		// 目录须比会话活得更久，且借用期间不可移动/赋值；禁止临时对象以免悬空。
		[[nodiscard]] std::expected<void, CommandError> ConfigureSummons(std::string_view _player, const SummonCatalog& _catalog);
		std::expected<void, CommandError> ConfigureSummons(std::string_view, const SummonCatalog&&) = delete;
		void BeginPreparation();
		// 准备倒计时到期：先清理未准备者的到期临时栏并锁定操作，再由宿主执行 onPrepEnd。
		void ApplyPreparationDeadline();
		[[nodiscard]] std::expected<ChangeSet, CommandError> SetBoardLayout(std::string_view _player, BoardLayout _layout);

		void EndPreparation();
		// 原子接收全体玩家结果；同回合只允许一次，资金在下一次 BeginRound 才发放。
		void ApplySettlement(int _round, std::span<const EconomySettlement> _players);

		[[nodiscard]] std::expected<ChangeSet, CommandError> Execute(const CommandEnvelope& _command);
		// 宿主内容奖励入口，不受 Ready 限制，可在战斗/结算中发放；不能直接作为客户端命令暴露。
		[[nodiscard]] std::expected<GrantResult, CommandError> GrantPiece(std::string_view _player, std::string_view _definition, GrantOptions _options = {});
		// 内容效果专用：晋升保留 UID/装备，变形先回收装备再获得新干员；销毁可作用于已装备物品。
		// 与客户端命令分开，允许准备结束后执行；调用失败时整个库存、卡池和随机状态回滚。
		[[nodiscard]] std::expected<GrantResult, CommandError> ApplyInventoryEffect(std::string_view _player, const InventoryEffect& _effect);

		// 可信内容入口：资金增减（负数最低减至零）、下轮资金、免费刷新和后续购买晋升。
		// 不依赖准备阶段或 Ready；淘汰者拒绝，异常和非法数据不提交事务。
		[[nodiscard]] std::expected<ChangeSet, CommandError> ApplyEconomyEffect(std::string_view _player, const EconomyEffect& _effect);
		// 内容创建的选一奖励不预占库存；去重、过滤错误类型后最多六项，无有效项时不入队。
		[[nodiscard]] std::expected<ChangeSet, CommandError> OfferPieces(std::string_view _player, PieceKind _kind, std::span<const std::string_view> _ids);

		// 内容随机流由宿主持有；抽取本身不扣库存，发放时再扣。额外库存固定使用该玩家的 DIY 库存。
		[[nodiscard]] std::optional<std::string> RollChess(std::string_view _player, Random& _random, RollOptions _options = {}) const;
		[[nodiscard]] std::optional<PoolEntry> CopyStock(std::string_view _player, std::string_view _baseId) const;

		[[nodiscard]] std::optional<PlayerView> View(std::string_view _playerId) const;

		[[nodiscard]] std::vector<PublicPlayerView> PublicView() const;

		[[nodiscard]] PreparationPhase Phase() const noexcept { return _MyPhase; }

		[[nodiscard]] int Round() const noexcept { return _MyRound; }

		[[nodiscard]] std::uint64_t Revision() const noexcept { return _MyRevision; }

		[[nodiscard]] bool PoolConservationHolds() const;

	private:
		friend class PreparationContent; // 跨玩家机变奖励使用同一经济事务与 UID 流。
		struct Player
		{
			PlayerView MyView;
			std::size_t MyPool{};
			ShopLayout MyLayout;
			std::optional<std::reference_wrapper<const SummonCatalog>> MySummons{};
		};

		[[nodiscard]] std::optional<CommandError>
		Apply(Player& _player, const PreparationCommand& _command, std::vector<EconomyEvent>& _events);

		[[nodiscard]] std::optional<ShopSlot> RollSlot(Player& _player, PieceKind _kind);

		void RollShop(Player& _player, bool _keepFrozen, bool _onlyNew = false);

		std::optional<PieceUid> Acquire(Player& _player, const Definition& _definition, std::vector<EconomyEvent>& _events, GrantOptions _options = {});
		std::optional<PieceUid> MergePieces(Player& _player, const Definition& _definition, std::optional<Piece> _incoming, std::vector<EconomyEvent>& _events);
		void FillHand(PlayerView& _view) const;
		void ResolveTemporary(Player& _player);
		bool Stow(PlayerView& _view, Piece& _piece, bool _temporary = false) const;
		void CheckItemMerges(Player& _player, std::vector<EconomyEvent>& _events);
		[[nodiscard]] std::optional<CommandError> Equip(Player& _player, const EquipItem& _command, std::vector<EconomyEvent>& _events);
		[[nodiscard]] std::optional<CommandError> ApplyInventoryEffect(Player& _player, const InventoryEffect& _effect, GrantResult& _result);

		void QueueReward(Player& _player);
		static void AddFunds(PlayerView& _view, std::int64_t _amount);
		static bool Spend(PlayerView& _view, std::int64_t _amount);
		void ApplyPurchaseUpgrade(Player& _player, PieceUid _piece, std::vector<EconomyEvent>& _events);
		[[nodiscard]] const PoolEntry* Stock(const Player& _player, std::string_view _base) const;
		int TakeCopies(Player& _player, std::string_view _base, int _count);
		int ReturnCopies(Player& _player, std::string_view _base, int _count);
		[[nodiscard]] static PlacementClass PlacementOf(const PlayerView& _view, const Definition& _definition);
		[[nodiscard]] static bool Selected(const PlayerView& _view, const Definition& _definition);

		[[nodiscard]] std::optional<CommandError> Move(Player& _player, const MoveToBoard& _command);

		[[nodiscard]] std::optional<CommandError> Move(Player& _player, const MoveToHand& _command);

		struct PieceLocation
		{
			bool MyOnBoard{};
			std::size_t MyIndex{};
			bool MyInTemporary{};
			std::optional<std::size_t> MyEquippedIndex{};

			[[nodiscard]] std::optional<Piece>& Slot(PlayerView& _view) const
			{ return MyOnBoard ? _view.MyBoard.at(MyIndex) : MyInTemporary ? _view.MyTemporary.at(MyIndex) : _view.MyHand.at(MyIndex); }

			[[nodiscard]] Piece& Get(PlayerView& _view) const
			{ return MyEquippedIndex ? Slot(_view)->MyItems.at(*MyEquippedIndex) : *Slot(_view); }
		};
		struct OwnerPosition { const Piece& MyPiece; BoardPosition MyPosition; Facing MyFacing; };
		[[nodiscard]] bool Legal(const Player& _player, const Piece& _piece, BoardPosition _position, std::optional<OwnerPosition> _owner = {}) const;
		[[nodiscard]] std::optional<bool> InOwnerRange(const Player& _player, const Piece& _token, BoardPosition _position, std::optional<OwnerPosition> _owner = {}) const;
		[[nodiscard]] bool ReturnToken(PlayerView& _view, Piece& _token, std::optional<std::size_t> _preferred = {}, bool _allowTemporary = true) const;
		void GrantTokens(Player& _player, const Piece& _owner);
		static void RemoveTokens(PlayerView& _view, PieceUid _owner);
		void LiftTokens(Player& _player, PieceUid _owner, PieceUid _keep = 0);
		void LiftOutOfRange(Player& _player);
		[[nodiscard]] bool RoomForReorient(const Player& _player, const Piece& _owner, BoardPosition _position, Facing _facing) const;
		static void ClearBoard(PlayerView& _view, std::size_t _index);
		static void SetBoard(PlayerView& _view, std::size_t _index, Piece _piece);

		[[nodiscard]] static Piece Detach(PlayerView& _view, const PieceLocation& _location);
		[[nodiscard]] std::vector<PieceLocation> MergeLocations(const PlayerView& _view, const Definition& _definition) const;

		[[nodiscard]] static std::optional<PieceLocation> Locate(const PlayerView& _view, PieceUid _uid);

		void Commit(EconomySession&& _next) noexcept
		{
			_MyRandom = _next._MyRandom;
			_MyPlayers.swap(_next._MyPlayers);
			_MyPools.swap(_next._MyPools);
			_MyRound = _next._MyRound;
			_MySettledRound = _next._MySettledRound;
			_MyPhase = _next._MyPhase;
			_MyNextUid = _next._MyNextUid;
			_MyRevision = _next._MyRevision;
		}

		const Catalog& _MyCatalog;
		const EconomyRules& _MyRules;
		Random _MyRandom;
		std::vector<Player> _MyPlayers;
		std::vector<SharedPool> _MyPools;
		int _MyRound{};
		int _MySettledRound{};
		PreparationPhase _MyPhase{PreparationPhase::IDLE};
		PieceUid _MyNextUid{};
		std::uint64_t _MyRevision{};
	};
} // namespace Stronghold
#endif
