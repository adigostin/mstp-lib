
#pragma once
#include <cstdint>
#include <utility>
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "assert.h"

namespace edge
{
	class object;

	struct property_editor_parent_i
	{
		virtual ~property_editor_parent_i() { }
	};

	struct property_editor_i
	{
		virtual ~property_editor_i() { }
		virtual bool show (property_editor_parent_i* parent) = 0; // return IDOK, IDCANCEL, -1 (some error), 0 (hWndParent invalid or closed)
		virtual void cancel() = 0;
	};

	using property_editor_factory_t = std::unique_ptr<property_editor_i>(const std::vector<object*>& objects);

	struct property_group
	{
		int32_t prio;
		const char* name;
	};

	extern const property_group misc_group;

	struct property abstract
	{
		const char* const _name;
		const property_group* const _group;
		const char* const _description;

		constexpr property (const char* name, const property_group* group, const char* description)
			: _name(name), _group((group != nullptr) ? group : &misc_group), _description(description)
		{ }
		property (const property&) = delete;
		property& operator= (const property&) = delete;

		virtual const char* type_name() const = 0;
		virtual bool has_setter() const = 0;
		virtual property_editor_factory_t* custom_editor() const = 0;
	};

	using NVP = std::pair<const char*, int>;

	template<typename t>
	struct view
	{
		t* from;
		t* to;

		constexpr view()
			: from(nullptr), to(nullptr)
		{ }

		template<size_t size>
		constexpr view (const t(&array)[size])
			: from(array), to(&array[size])
		{ }

		t* data() const { return from; }
		size_t size() const { return to - from; }
	};

	struct value_property : property
	{
		using property::property;
	
		virtual std::string get_to_string (const object* obj) const = 0;
		virtual bool try_set_from_string (object* obj, std::string_view str) const = 0;
		virtual const NVP* nvps() const = 0;
		virtual bool equal (object* obj1, object* obj2) const = 0;
	};

	// ========================================================================

	template<
		typename value_t,
		typename param_t,
		typename return_t,
		const char* type_name_,
		std::string (*to_string)(param_t value),
		bool (*from_string)(std::string_view str, value_t& out),
		const NVP* nvps_ = nullptr,
		property_editor_factory_t* custom_editor_ = nullptr
	>
	struct typed_property : value_property
	{
		using base = value_property;

		using getter_t = return_t (object::*)() const;
		using setter_t = void (object::*)(param_t);

		getter_t const _getter;
		setter_t const _setter;
		std::optional<value_t> const _default_value;

		constexpr typed_property (const char* name, const property_group* group, const char* description, getter_t getter, setter_t setter, std::optional<value_t> default_value)
			: base(name, group, description), _getter(getter), _setter(setter), _default_value(default_value)
		{ }

		virtual const char* type_name() const override final { return type_name_; }

		virtual bool has_setter() const override final { return _setter != nullptr; }

		virtual property_editor_factory_t* custom_editor() const override { return custom_editor_; }

	public:
		std::string get_to_string (const object* obj) const final
		{
			auto value = (obj->*_getter)();
			return to_string (value);
		}

		virtual bool try_set_from_string (object* obj, std::string_view str_in) const override final
		{
			value_t value;
			bool ok = from_string(str_in, value);
			if (ok)
				(obj->*_setter)(value);
			return ok;
		}

		virtual const NVP* nvps() const override final
		{
			return nvps_;
		}

		virtual bool equal (object* obj1, object* obj2) const override final
		{
			return (obj1->*_getter)() == (obj2->*_getter)();
		}
	};

	template<typename enum_t, const NVP* nvps>
	struct enum_converters
	{
		static std::string enum_to_string (enum_t from)
		{
			for (auto nvp = nvps; nvp->first != nullptr; nvp++)
			{
				if (nvp->second == (int)from)
					return nvp->first;
			}

			return "(unknown)";
		}

	private:
		template<typename char_type>
		static bool equals (std::basic_string_view<char_type> one, const char* other)
		{
			static_assert(false);
		}

		static bool equals (std::string_view one, const char* other)
		{
			return one == other;
		}

		static bool equals (std::wstring_view one, const char* other)
		{
			for (auto o = one.begin(); o != one.end(); o++)
			{
				if ((*other == 0) || (*other != *o))
					return false;
			}

			return *other == 0;
		}

	public:
		template<typename char_type>
		static bool enum_from_string (std::basic_string_view<char_type> from, enum_t& to)
		{
			for (auto nvp = nvps; nvp->first != nullptr; nvp++)
			{
				if (equals(from, nvp->first))
				{
					to = static_cast<enum_t>(nvp->second);
					return true;
				}
			}

			return false;
		}
	};

	template<
		typename enum_t,
		const char* type_name,
		const NVP* nvps_
	>
	using enum_property = typed_property<enum_t, enum_t, enum_t, type_name, enum_converters<enum_t, nvps_>::enum_to_string, enum_converters<enum_t, nvps_>::enum_from_string, nvps_>;

	// ===========================================

	std::string bool_to_string (bool from);
	bool bool_from_string (std::string_view from, bool& to);
	static constexpr char bool_type_name[] = "bool";
	using bool_p = typed_property<bool, bool, bool, bool_type_name, bool_to_string, bool_from_string>;

	inline std::string uint32_to_string (uint32_t from) { return std::to_string(from); }
	bool uint32_from_string (std::string_view from, uint32_t& to);
	static constexpr char uint32_type_name[] = "uint32";
	using uint32_p = typed_property<uint32_t, uint32_t, uint32_t, uint32_type_name, uint32_to_string, uint32_from_string>;

	inline std::string int32_to_string (int32_t from) { return std::to_string(from); }
	bool int32_from_string (std::string_view from, int32_t& to);
	static constexpr char int32_type_name[] = "int32";
	using int32_property = typed_property<int32_t, int32_t, int32_t, int32_type_name, int32_to_string, int32_from_string>;

	inline std::string float_to_string (float from) { return std::to_string(from); }
	bool float_from_string (std::string_view from, float& to);
	static constexpr char float_type_name[] = "float";
	using float_property = typed_property<float, float, float, float_type_name, float_to_string, float_from_string>;

	inline std::string temp_string_to_string (std::string_view from) { return std::string(from); }
	inline bool temp_string_from_string (std::string_view from, std::string& to) { to = from; return true; }
	static constexpr char temp_string_type_name[] = "temp_string";
	using temp_string_p = typed_property<std::string, std::string_view, std::string, temp_string_type_name, temp_string_to_string, temp_string_from_string>;

	inline std::string backed_string_to_string (std::string_view from) { return std::string(from); }
	inline bool backed_string_from_string (std::string_view from, std::string& to) { to = from; return true; }
	static constexpr char backed_string_type_name[] = "backed_string";
	using backed_string_property = typed_property<std::string, std::string_view, const std::string&, backed_string_type_name, backed_string_to_string, backed_string_from_string>;
}
