
#include "pch.h"
#include "Object.h"

const wchar_t* GetEnumName (const NVP* nvps, int value)
{
	for (auto nvp = nvps; nvp->first != nullptr; nvp++)
	{
		if (nvp->second == value)
			return nvp->first;
	}

	return nullptr;
}

int GetEnumValue (const NVP* nvps, const wchar_t* name)
{
	for (auto nvp = nvps; nvp->first != nullptr; nvp++)
	{
		if (wcscmp (nvp->first, name) == 0)
			return nvp->second;
	}

	throw std::invalid_argument(u8"Name not valid");
}

std::wstring EnumProperty::to_wstring (const Object* obj) const
{
	auto value = (obj->*_getter)();
	return GetEnumName (_nameValuePairs, value);
}
