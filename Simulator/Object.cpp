
#include "pch.h"
#include "Object.h"

EnumProperty::EnumProperty (const wchar_t* name, Getter getter, Setter setter, const NVP* nameValuePairs)
	: TypedProperty(name, getter, setter), _nameValuePairs(nameValuePairs)
{ }

std::wstring EnumProperty::to_wstring (const Object* obj, unsigned int vlanNumber) const
{
	auto value = _getter(obj, vlanNumber);
	for (auto nvp = _nameValuePairs; nvp->first != nullptr; nvp++)
	{
		if (nvp->second == value)
			return nvp->first;
	}

	return L"??";
}
