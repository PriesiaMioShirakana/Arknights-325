#include <stronghold/simulation/battle.hpp>

namespace Stronghold
{
	void Battle::ValidateContent(ContentReference _reference, ContentTag _expected) const
	{
		if (_reference.MyTag == ContentTag::BUILTIN && _reference.MyRegistration == 0) return;
		if (!_MyInput.MyContentRegistry) throw std::invalid_argument("custom content requires a registry");
		_MyInput.MyContentRegistry->get().Validate(_reference, _expected);
	}

	void Battle::AttachContent(ContentReference _reference, UnitId _unit, std::uint64_t _buff, std::size_t _owner)
	{
		if (_reference.MyTag == ContentTag::BUILTIN) return;
		// 只在显式 CUSTOM 路径创建对象。工厂返回独占实例，实例状态不会跨单位或跨对局共享。
		auto handler = _MyInput.MyContentRegistry->get().Create(_reference);
		_MyContentInstances.emplace_back(_reference, std::move(handler), _unit, _buff, _owner);
	}

	void Battle::RetireContent(UnitId _unit, std::uint64_t _buff)
	{
		for (auto& instance : _MyContentInstances)
			if (instance.MyUnit == _unit && instance.MyBuff == _buff)
			{
				instance.MyRetired = true;
				// 扩展可以在自己的回调中移除自己；必须等最外层回调返回后再析构派生对象。
				if (_MyContentDepth == 0) instance.MyHandler.reset();
			}
		if (_MyContentDepth == 0) std::erase_if(_MyContentInstances, [](const ContentInstance& _instance) { return _instance.MyRetired; });
	}

	void Battle::RecordContentError(ContentReference _reference, std::string _message)
	{
		constexpr std::size_t ErrorLimit = 64;
		if (_MyContentErrors.size() < ErrorLimit)
			_MyContentErrors.emplace_back(Tick(), _reference.MyTag, _reference.MyRegistration, std::move(_message));
		if (_MyContentErrors.size() == ErrorLimit) _MyContentFault = true;
	}

	void Battle::NotifyContent(ContentEvent& _event)
	{
		NotifyProfession(_event);
		if (_MyContentInstances.empty()) { NotifyChoices(_event); NotifyProfessionLate(_event); return; }
		constexpr unsigned DepthLimit = 32;
		if (_MyContentDepth == DepthLimit) throw std::runtime_error("custom content recursion limit");
		++_MyContentDepth;
		// 新增的处理器不接收当前正在广播的事件，避免一次部署递归扩展成无限广播。
		const auto count = _MyContentInstances.size();
		for (std::size_t i = 0; i < count; ++i)
		{
			const auto& instance = _MyContentInstances[i];
			if (instance.MyRetired || !instance.MyHandler) continue;
			const auto tag = instance.MyReference.MyTag;
			const bool buffEvent = _event.MyKind >= ContentEventKind::BUFF_APPLIED && _event.MyKind <= ContentEventKind::BUFF_EXPIRED;
			if (tag == ContentTag::CUSTOM_BUFF)
			{
				if (buffEvent && instance.MyBuff != _event.MyBuff) continue;
				if (_event.MyKind == ContentEventKind::TICK) continue; // Buff 按自身 interval 单独计时。
			}
			else if (buffEvent && instance.MyUnit != 0) continue;
			if (instance.MyUnit != 0)
			{
				if (_event.MyKind == ContentEventKind::TICK && !Unit(instance.MyUnit).MyAlive) continue;
				if (_event.MyKind != ContentEventKind::TICK && _event.MyKind != ContentEventKind::BATTLE_START &&
					_event.MyKind != ContentEventKind::BATTLE_END && _event.MyKind != ContentEventKind::LAYER_GAIN && instance.MyUnit != _event.MyUnit &&
					instance.MyUnit != _event.MySource && instance.MyUnit != _event.MyTarget) continue;
			}
			_event.MyHandlerUnit = instance.MyUnit;
			_event.MyHandlerOwner = instance.MyOwner;
			const auto reference = instance.MyReference;
			// 句柄对象在堆上的地址稳定；回调可能追加实例使 vector 重分配，返回后不再读取 instance。
			try { instance.MyHandler->Handle(*this, _event); }
			catch (const std::exception& error) { RecordContentError(reference, error.what()); }
			catch (...) { RecordContentError(reference, "unknown custom content exception"); }
		}
		NotifyChoices(_event);
		NotifyProfessionLate(_event);
		--_MyContentDepth;
		if (_MyContentDepth == 0)
			std::erase_if(_MyContentInstances, [](const ContentInstance& _instance) { return _instance.MyRetired; });
	}
}
