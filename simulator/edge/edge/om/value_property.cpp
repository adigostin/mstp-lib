
#include "value_property.h"

namespace edge
{
	// Let's instantiate some templates to catch compilation errors earlier.
	template struct static_value_property<uint32_property_traits>;

	std::string value_property::get_to_string (const object* from, const string_convert_context_i* context) const
	{
		struct oss : out_sstream_i
		{
			std::string buffer;
			virtual void write (const char* data, size_t size) override { buffer.append(data, size); }
		} s;
		this->get_to_string(from, &s, context);
		return std::move(s.buffer);
	}

	// ========================================================================

	const char bool_property_traits::type_name[] = "bool";

	void bool_property_traits::to_string (value_t from, out_sstream_i* to, const string_convert_context_i* context)
	{
		to->write(from ? "True" : "False");
	}

	void bool_property_traits::from_string (std::string_view from, value_t& to, const string_convert_context_i* context)
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

	template<> void int32_property_traits::to_string (value_t from, out_sstream_i* to, const string_convert_context_i* context)
	{
		char buffer[16];
		#ifdef _MSC_VER
		sprintf_s (buffer, "%d", from);
		#else
		sprintf (buffer, "%d", from);
		#endif
		to->write(buffer);
	}

	template<> void int32_property_traits::from_string (std::string_view from, value_t& to, const string_convert_context_i* context)
	{
		if (from.empty())
			throw string_convert_exception(from, type_name);

		char* endPtr;
		long value = strtol (from.data(), &endPtr, 10);

		if (endPtr != from.data() + from.size())
			throw string_convert_exception(from, type_name);

		to = value;
	}

	// ========================================================================

	extern const char uint8_type_name[] = "uint8";

	template<> void uint8_property_traits::to_string (value_t from, out_sstream_i* to, const string_convert_context_i* context)
	{
		return uint32_property_traits::to_string(from, to, context);
	}

	template<> void uint8_property_traits::from_string (std::string_view from, value_t& to, const string_convert_context_i* context)
	{
		uint32_t value;
		uint32_property_traits::from_string(from, value, context);
		if (value > 0xFF)
			throw std::range_error("Value out of range (expected 0..255)");
		to = (uint8_t)value;
	}

	// ========================================================================

	extern const char uint32_type_name[] = "uint32";

	template<> void uint32_property_traits::to_string (value_t from, out_sstream_i* to, const string_convert_context_i* context)
	{
		char buffer[16];
		#ifdef _MSC_VER
		sprintf_s (buffer, "%u", from);
		#else
		sprintf (buffer, "%u", from);
		#endif
		to->write(buffer);
	}

	template<> void uint32_property_traits::from_string (std::string_view from, value_t& to, const string_convert_context_i* context)
	{
		if (from.empty())
			throw string_convert_exception(from, type_name);

		char* endPtr;
		unsigned long value = std::strtoul (from.data(), &endPtr, 10);

		if (endPtr != from.data() + from.size())
			throw string_convert_exception(from, type_name);

		to = value;
	}

	// ========================================================================

	extern const char uint64_type_name[] = "uint64";

	template<> void uint64_property_traits::to_string (value_t from, out_sstream_i* to, const string_convert_context_i* context)
	{
		char buffer[32];
		#ifdef _MSC_VER
		sprintf_s (buffer, "%llu", from);
		#else
		sprintf (buffer, "%llu", from);
		#endif
		to->write(buffer);
	}

	template<> void uint64_property_traits::from_string (std::string_view from, value_t& to, const string_convert_context_i* context)
	{
		if (from.empty())
			throw string_convert_exception(from, type_name);

		char* endPtr;
		unsigned long long value = std::strtoull (from.data(), &endPtr, 10);

		if (endPtr != from.data() + from.size())
			throw string_convert_exception(from, type_name);

		to = value;
	}

	// ========================================================================

	extern const char size_t_type_name[] = "size_t";

	template<> void size_t_property_traits::to_string (value_t from, out_sstream_i* to, const string_convert_context_i* context)
	{
		uint32_property_traits::to_string((uint32_t)from, to, context);
	}

	template<> void size_t_property_traits::from_string (std::string_view from, value_t& to, const string_convert_context_i* context)
	{
		uint32_t val;
		uint32_property_traits::from_string (from, val, context);
		to = val;
	}

	// ========================================================================

	extern const char float_type_name[] = "float";

	template<> void float_property_traits::to_string (value_t from, out_sstream_i* to, const string_convert_context_i* context)
	{
		char buffer[32];
		#ifdef _MSC_VER
		sprintf_s (buffer, "%g", from);
		#else
		sprintf (buffer, "%g", from);
		#endif
		to->write(buffer);
	}

	template<> void float_property_traits::from_string (std::string_view from, float& to, const string_convert_context_i* context)
	{
		if (from.empty())
			throw string_convert_exception(from, type_name);

		char* end_ptr;
		float value = strtof (from.data(), &end_ptr);

		if (end_ptr != from.data() + from.size())
			throw string_convert_exception(from, type_name);

		to = value;
	}

	// ========================================================================

	const char unknown_enum_value_str[] = "(unknown)";

	const char side_type_name[] = "side";
	const nvp side_nvps[] = {
		{ "Left",   (int) side::left },
		{ "Top",    (int) side::top },
		{ "Right",  (int) side::right },
		{ "Bottom", (int) side::bottom },
		{ 0, 0 },
	};
}
