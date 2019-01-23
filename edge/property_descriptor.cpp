
#include "property_descriptor.h"

namespace edge
{
	const char* GetEnumName (const NVP* nvps, int value)
	{
		for (auto nvp = nvps; nvp->first != nullptr; nvp++)
		{
			if (nvp->second == value)
				return nvp->first;
		}

		return nullptr;
	}

	bool TryGetEnumValue (const NVP* nvps, std::string_view name, int* value_out)
	{
		assert(false); return false;
	}

	bool TryGetEnumValue (const NVP* nvps, std::wstring_view name, int* value_out)
	{
		assert(false); return false;
	}

	bool bool_from_string (std::string_view from, bool& to)
	{
		assert(false); return false;
	}

	bool uint32_from_string (std::string_view from, uint32_t& to)
	{
		if (from.empty())
			return false;

		char* endPtr;
		unsigned long value = std::strtoul (from.data(), &endPtr, 10);

		if (endPtr != from.data() + from.length())
			return false;

		to = value;
		return true;
	}

	bool int32_from_string (std::string_view from, int32_t& to)
	{
		if (from.empty())
			return false;

		char* endPtr;
		long value = std::strtol (from.data(), &endPtr, 10);

		if (endPtr != from.data() + from.length())
			return false;

		to = value;
		return true;
	}

	bool float_from_string (std::string_view from, float& to)
	{
		assert(false); return false; // not implemented
	}
}
