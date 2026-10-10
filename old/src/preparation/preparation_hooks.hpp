#ifndef STRONGHOLD_PREPARATION_HOOKS_HPP
#define STRONGHOLD_PREPARATION_HOOKS_HPP
#include <stronghold/domain/preparation_content.hpp>

namespace Stronghold
{
	// 静态内置分派桥；绑定事务副本，经济层单独使用时没有内容回调。
	// 经济方法内部再次复制时，回调显式接收正在修改的会话，不依赖原会话地址。
	struct PreparationHooks final
	{
		struct Scope
		{
			explicit Scope(PreparationHooks& _hooks, bool _prepEnd = false)
				: MyHooks(_hooks), MyEntered(_hooks.MyDepth < 6), MyPrepEnd(_prepEnd)
			{
				if (MyEntered) { ++MyHooks.MyDepth; if (MyPrepEnd) ++MyHooks.MyPrepEndDepth; }
			}

			~Scope()
			{
				if (MyEntered) { --MyHooks.MyDepth; if (MyPrepEnd) --MyHooks.MyPrepEndDepth; }
			}

			Scope(const Scope&) = delete;
			Scope& operator=(const Scope&) = delete;
			PreparationHooks& MyHooks;
			bool MyEntered{};
			bool MyPrepEnd{};
		};

		PreparationHooks(const PreparationContent& _content, EconomySession& _economy,
			std::vector<PreparationContent::Player>& _players, Random& _random)
			: MyContent(_content), MyEconomy(_economy), MyPlayers(_players), MyRandom(_random), MyPrevious(_economy._MyHooks)
		{
			if (!_content._MyBondEffects.empty() || !_content._MyGarrisons.MyEffects.empty())
			{
				MyBondStates.reserve(_players.size());
				for (std::size_t i = 0; i < _players.size(); ++i)
					MyBondStates.emplace_back(_content.ComputeItemBondStates(_economy, _players[i], i));
			}
			MyEconomy._MyHooks = this;
		}

		~PreparationHooks() { MyEconomy._MyHooks = MyPrevious; }

		PreparationHooks(const PreparationHooks&) = delete;
		PreparationHooks& operator=(const PreparationHooks&) = delete;

		void Acquired(EconomySession& _economy, std::string_view _player, PieceUid _uid, std::vector<EconomyEvent>& _events) const
		{
			const auto index = static_cast<std::size_t>(&Player(_player) - MyPlayers.data());
			RefreshBonds(_economy, index);
			Scope scope(*_economy._MyHooks); if (!scope.MyEntered) return;
			auto& view = _economy._MyPlayers[index].MyView;
			const auto location = _economy.Locate(view, _uid);
			const auto piece = location ? std::optional(location->Get(view)) : std::nullopt;
			MyContent.ApplyBonds(_economy, MyPlayers, index, PreparationContent::BondEvent::GAIN, MyRandom, _events);
			if (!piece) return;
			MyContent.RunGarrisons(_economy, MyPlayers, index, GarrisonEvent::GAIN, MyRandom, _events, &*piece);
			if (_economy._MyCatalog.At(piece->MyId).MyKind == PieceKind::CHESS)
				MyContent.OnItemGain(_economy, MyPlayers, _player, _uid, _events);
		}

		void Sold(EconomySession& _economy, std::string_view _player, const Piece& _piece,
			std::int64_t& _gain, std::vector<EconomyEvent>& _events) const
		{
			Scope scope(*_economy._MyHooks); if (!scope.MyEntered) return;
			const auto index = static_cast<std::size_t>(&Player(_player) - MyPlayers.data());
			MyContent.ApplyBand(_economy, MyPlayers, index, PreparationContent::BandEvent::SOLD, MyRandom, _events, _piece.MyId, &_gain);
			MyContent.ApplyBonds(_economy, MyPlayers, index, PreparationContent::BondEvent::SOLD, MyRandom, _events);
			MyContent.RunGarrisons(_economy, MyPlayers, index, GarrisonEvent::SOLD, MyRandom, _events, &_piece);
			MyContent.OnItemSold(_economy, MyPlayers, _player, MyRandom, _events);
		}

		void BondEvent(EconomySession& _economy, std::string_view _player, PreparationContent::BondEvent _event,
			std::vector<EconomyEvent>& _events) const
		{
			Scope scope(*_economy._MyHooks); if (!scope.MyEntered) return;
			const auto index = static_cast<std::size_t>(&Player(_player) - MyPlayers.data());
			MyContent.ApplyBonds(_economy, MyPlayers, index, _event, MyRandom, _events);
		}

		void Merged(EconomySession& _economy, std::string_view _player, std::vector<EconomyEvent>& _events) const
		{
			BondEvent(_economy, _player, PreparationContent::BondEvent::MERGE, _events);
		}

		void PreparationStarted(EconomySession& _economy, std::string_view _player, std::vector<EconomyEvent>& _events) const
		{
			RefreshBonds(_economy, static_cast<std::size_t>(&Player(_player) - MyPlayers.data()));
			BondEvent(_economy, _player, PreparationContent::BondEvent::PREP_START, _events);
		}

		void RefreshBonds(const EconomySession& _economy, std::size_t _index) const
		{
			if (!MyBondStates.empty()) MyBondStates[_index] = MyContent.ComputeItemBondStates(_economy, MyPlayers[_index], _index);
		}

		[[nodiscard]] const PreparationContent::Player& Player(std::string_view _id) const
		{
			return *std::ranges::find(MyPlayers, _id, &PreparationContent::Player::MyId);
		}

		void Income(const EconomySession& _economy, std::string_view _player, std::int64_t& _income) const
		{
			MyContent.BandIncome(_economy, Player(_player), _income);
		}

		[[nodiscard]] std::int64_t Price(const EconomySession& _economy, std::string_view _player, const ShopSlot& _slot) const
		{
			return MyContent.BandPrice(_economy, Player(_player), _slot);
		}

		void Bought(EconomySession& _economy, std::string_view _player, std::string_view _definition, std::vector<EconomyEvent>& _events) const
		{
			Dispatch(_economy, _player, PreparationContent::BandEvent::BUY, _events, _definition);
		}

		void Refreshed(EconomySession& _economy, std::string_view _player, std::vector<EconomyEvent>& _events) const
		{
			Dispatch(_economy, _player, PreparationContent::BandEvent::REFRESH, _events);
		}

		void Levelled(EconomySession& _economy, std::string_view _player, std::vector<EconomyEvent>& _events) const
		{
			Dispatch(_economy, _player, PreparationContent::BandEvent::LEVEL_UP, _events);
		}

		void Spent(EconomySession& _economy, std::string_view _player, std::int64_t _amount, std::vector<EconomyEvent>& _events) const
		{
			if (_amount > 0) Dispatch(_economy, _player, PreparationContent::BandEvent::SPEND, _events, {}, &_amount);
		}

		void Dispatch(EconomySession& _economy, std::string_view _player, PreparationContent::BandEvent _event,
			std::vector<EconomyEvent>& _events, std::string_view _definition = {}, std::int64_t* _amount = nullptr) const
		{
			const auto index = static_cast<std::size_t>(&Player(_player) - MyPlayers.data());
			Scope scope(*_economy._MyHooks); if (!scope.MyEntered) return;
			std::vector<PieceUid> refreshed;
			if (_event == PreparationContent::BandEvent::REFRESH && !MyContent._MyGarrisons.MyEffects.empty())
			{
				auto& view = _economy._MyPlayers[index].MyView;
				refreshed.reserve(view.MyBoard.size() + view.MyHand.size());
				const auto add = [&](const auto& _pieces)
				{
					for (const auto& piece : _pieces)
						if (piece && !piece->IsToken() && _economy._MyCatalog.At(piece->MyId).MyKind == PieceKind::CHESS)
							refreshed.push_back(piece->MyUid);
				};
				add(view.MyBoard); add(view.MyHand);
			}
			MyContent.ApplyBand(_economy, MyPlayers, index, _event, MyRandom, _events, _definition, _amount);
			if (_event == PreparationContent::BandEvent::BUY || _event == PreparationContent::BandEvent::REFRESH)
				MyContent.ApplyBonds(_economy, MyPlayers, index, _event == PreparationContent::BandEvent::BUY ?
					PreparationContent::BondEvent::BUY : PreparationContent::BondEvent::REFRESH, MyRandom, _events);
			if (_event == PreparationContent::BandEvent::REFRESH)
				MyContent.RunGarrisons(_economy, MyPlayers, index, GarrisonEvent::REFRESH, MyRandom, _events, nullptr, refreshed);
		}

		const PreparationContent& MyContent;
		EconomySession& MyEconomy;
		std::vector<PreparationContent::Player>& MyPlayers;
		Random& MyRandom;
		PreparationHooks* MyPrevious{};
		// 模拟原版 recompute 边界；出售/合成事件保留上次成员快照，层数读取仍来自实时账本。
		mutable std::vector<std::vector<BondState>> MyBondStates;
		unsigned MyDepth{};
		unsigned MyPrepEndDepth{};
	};
}
#endif
