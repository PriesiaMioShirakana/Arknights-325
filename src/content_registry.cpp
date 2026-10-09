#include <limits>
#include <stronghold/simulation/content_registry.hpp>

namespace Stronghold
{
	ContentReference ContentRegistry::RegisterEntry(std::string _id, ContentTag _tag, Factory _factory)
	{
		if (_MySealed) throw std::logic_error("content registry is sealed");
		if (_id.empty() || _id.size() > 256 || !IsCustom(_tag) || !_factory)
			throw std::invalid_argument("invalid custom content registration");
		for (const auto& entry : _MyEntries)
			if (entry.MyTag == _tag && entry.MyId == _id)
				throw std::invalid_argument("duplicate custom content registration: " + _id);
		if (_MyEntries.size() >= std::numeric_limits<std::uint32_t>::max())
			throw std::length_error("content registry is full");
		_MyEntries.emplace_back(std::move(_id), _tag, _factory);
		return ContentReference{.MyTag = _tag, .MyRegistration = static_cast<std::uint32_t>(_MyEntries.size())};
	}

	ContentReference ContentRegistry::Find(ContentTag _tag, std::string_view _id) const
	{
		for (std::size_t i = 0; i < _MyEntries.size(); ++i)
			if (_MyEntries[i].MyTag == _tag && _MyEntries[i].MyId == _id)
				return ContentReference{.MyTag = _tag, .MyRegistration = static_cast<std::uint32_t>(i + 1)};
		throw std::out_of_range("unregistered custom content");
	}

	void ContentRegistry::Validate(ContentReference _reference, ContentTag _expected) const
	{
		if (!_MySealed) throw std::logic_error("seal content registry before starting a battle");
		if (_reference.MyTag != _expected || !IsCustom(_reference.MyTag) || _reference.MyRegistration == 0 ||
			_reference.MyRegistration > _MyEntries.size() ||
			_MyEntries[_reference.MyRegistration - 1].MyTag != _reference.MyTag)
			throw std::invalid_argument("custom content tag/registration mismatch");
	}

	std::unique_ptr<CustomContent> ContentRegistry::Create(ContentReference _reference) const
	{
		Validate(_reference, _reference.MyTag);
		return _MyEntries[_reference.MyRegistration - 1].MyFactory();
	}
}
