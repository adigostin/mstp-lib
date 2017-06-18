
#include "pch.h"
#include "Object.h"

std::wstring EnumProperty::to_wstring (const Object* obj) const
{
	auto value = (obj->*_getter)();
	for (auto nvp = _nameValuePairs; nvp->first != nullptr; nvp++)
	{
		if (nvp->second == value)
			return nvp->first;
	}

	return L"??";
}
