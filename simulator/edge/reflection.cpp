
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "reflection.h"
#include <ctype.h>

namespace edge
{
	extern const property_group misc_group = { 0, "Misc" };

	//static
	std::string string_convert_exception::make_string (std::string_view str, const char* type_name)
	{
		auto res = std::string("Cannot convert \"");
		res += str;
		res += "\" to type \"";
		res += type_name;
		res += "\".";
		return res;
	}

	string_convert_exception::string_convert_exception (const char* str)
		: _message(str)
	{ }

	string_convert_exception::string_convert_exception (std::string_view str, const char* type_name)
		: _message(make_string(str, type_name))
	{ }

	// ========================================================================

	const char bool_property_traits::type_name[] = "bool";

	void bool_property_traits::to_string (value_t from, std::string& to)
	{
		to = from ? "True" : "False";
	}

	void bool_property_traits::from_string (std::string_view from, value_t& to)
	{
		if ((from.size() == 4) && (tolower(from[0]) == 't') && (tolower(from[1]) == 'r') && (tolower(from[2]) == 'u') && (tolower(from[3]) == 'e'))
		{
			to = true;
			return;
		}

		if ((from.size() == 5) && (tolower(from[0]) == 'f') && (tolower(from[1]) == 'a') && (tolower(from[2]) == 'l') && (tolower(from[3]) == 's') && (tolower(from[4]) == 'e'))
		{
			to = false;
			return;
		}

		throw string_convert_exception(from, type_name);
	}

	// ========================================================================

	extern const char int32_type_name[] = "int32";

	template<> void int32_property_traits::to_string (value_t from, std::string& to)
	{
		char buffer[16];
		#ifdef _MSC_VER
			sprintf_s (buffer, "%d", from);
		#else
			sprintf (buffer, "%d", from);
		#endif
		to = buffer;
	}

	template<> void int32_property_traits::from_string (std::string_view from, int32_t& to)
	{
		if (from.empty())
			throw string_convert_exception(from, type_name);

		char* endPtr;
		long value = strtol (from.data(), &endPtr, 10);

		if (endPtr != from.data() + from.size())
			throw string_convert_exception(from, type_name);

		to = value;
	}

	template<> void int32_property_traits::serialize (value_t from, out_stream_i* to)
	{
		assert(false); // not implemented
	}

	template<> void int32_property_traits::deserialize (binary_reader& from, value_t& to)
	{
		assert(false); // not implemented
	}

	// ========================================================================

	extern const char uint32_type_name[] = "uint32";

	template<> void uint32_property_traits::to_string (value_t from, std::string& to)
	{
		char buffer[16];
		#ifdef _MSC_VER
		sprintf_s (buffer, "%u", from);
		#else
		sprintf (buffer, "%u", from);
		#endif
		to = buffer;
	}

	template<> void uint32_property_traits::from_string (std::string_view from, uint32_t& to)
	{
		if (from.empty())
			throw string_convert_exception(from, type_name);

		char* endPtr;
		unsigned long value = std::strtoul (from.data(), &endPtr, 10);

		if (endPtr != from.data() + from.size())
			throw string_convert_exception(from, type_name);

		to = value;
	}

	template<> void uint32_property_traits::serialize (value_t from, out_stream_i* to)
	{
		assert(false); // not implemented
	}

	template<> void uint32_property_traits::deserialize (binary_reader& from, value_t& to)
	{
		assert(false); // not implemented
	}

	// ========================================================================

	extern const char uint64_type_name[] = "uint64";

	template<> void uint64_property_traits::to_string (value_t from, std::string& to)
	{
		char buffer[32];
		#ifdef _MSC_VER
		sprintf_s (buffer, "%llu", from);
		#else
		sprintf (buffer, "%llu", from);
		#endif
		to = buffer;
	}

	template<> void uint64_property_traits::from_string (std::string_view from, uint64_t& to)
	{
		if (from.empty())
			throw string_convert_exception(from, type_name);

		char* endPtr;
		unsigned long long value = std::strtoull (from.data(), &endPtr, 10);

		if (endPtr != from.data() + from.size())
			throw string_convert_exception(from, type_name);

		to = value;
	}

	template<> void uint64_property_traits::serialize (value_t from, out_stream_i* to)
	{
		assert(false); // not implemented
	}

	template<> void uint64_property_traits::deserialize (binary_reader& from, value_t& to)
	{
		assert(false); // not implemented
	}

	// ========================================================================

	extern const char size_t_type_name[] = "size_t";

	template<> void size_t_property_traits::to_string (value_t from, std::string& to)
	{
		uint32_property_traits::to_string((uint32_t)from, to);
	}

	template<> void size_t_property_traits::from_string (std::string_view from, size_t&to)
	{
		uint32_t val;
		uint32_property_traits::from_string (from, val);
		to = val;
	}

	template<> void size_t_property_traits::serialize (value_t from, out_stream_i* to)
	{
		assert(false); // not implemented
	}

	template<> void size_t_property_traits::deserialize (binary_reader& from, value_t& to)
	{
		assert(false); // not implemented
	}

	// ========================================================================

	extern const char float_type_name[] = "float";

	template<> void float_property_traits::to_string (value_t from, std::string& to)
	{
		char buffer[32];
		#ifdef _MSC_VER
			sprintf_s (buffer, "%f", from);
		#else
			sprintf (buffer, "%f", from);
		#endif
		to = buffer;
	}

	template<> void float_property_traits::from_string (std::string_view from, float& to)
	{
		if (from.empty())
			throw string_convert_exception(from, type_name);

		char* end_ptr;
		float value = strtof (from.data(), &end_ptr);

		if (end_ptr != from.data() + from.size())
			throw string_convert_exception(from, type_name);

		to = value;
	}

	template<> void float_property_traits::serialize (value_t from, out_stream_i* to)
	{
		assert(false); // not implemented
	}

	template<> void float_property_traits::deserialize (binary_reader& from, value_t& to)
	{
		assert(false); // not implemented
	}

	// ========================================================================

	const char backed_string_property_traits::type_name[] = "backed_string";

	void backed_string_property_traits::serialize (value_t from, out_stream_i* to)
	{
		if (from.size() < 254)
		{
			to->write((uint8_t)from.size());
		}
		else if (from.size() < 65536)
		{
			to->write((uint8_t)254);
			to->write ((uint8_t)(from.size()));
			to->write ((uint8_t)(from.size() >> 8));
		}
		else
		{
			to->write((uint8_t)255);
			to->write ((uint8_t)(from.size()));
			to->write ((uint8_t)(from.size() >> 8));
			to->write ((uint8_t)(from.size() >> 16));
			to->write ((uint8_t)(from.size() >> 24));
		}

		to->write (from.data(), from.size());
	}

	void backed_string_property_traits::deserialize (binary_reader& from, value_t& to)
	{
		assert (from.ptr + 1 <= from.end);
		size_t len = *from.ptr++;

		if (len == 254)
		{
			assert (from.ptr + 2 <= from.end);
			len = *from.ptr++;
			len |= (*from.ptr++ << 8);
		}
		else if (len == 255)
		{
			assert (from.ptr + 4 <= from.end);
			len = *from.ptr++;
			len |= (*from.ptr++ << 8);
			len |= (*from.ptr++ << 16);
			len |= (*from.ptr++ << 24);
		}

		assert (from.ptr + len <= from.end);
		to = std::string_view((const char*)from.ptr, len);
		from.ptr += len;
	}

	// ========================================================================

	void temp_string_property_traits::serialize (value_t from, out_stream_i* to)
	{
		assert(false); // not implemented
	}

	void temp_string_property_traits::deserialize (binary_reader& from, value_t& to)
	{
		assert(false); // not implemented
	}

	// ========================================================================

	const char unknown_enum_value_str[] = "(unknown)";
}
