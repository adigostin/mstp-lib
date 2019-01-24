
#include "property_descriptor.h"

namespace edge
{
	std::string bool_to_string (bool from)
	{
		return from ? "true" : "false";
	}

	bool bool_from_string (std::string_view from, bool& to)
	{
		if (from == "true")
		{
			to = true;
			return true;
		}
		else if (from == "false")
		{
			to = false;
			return true;
		}
		else
			return false;
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
