
#pragma once
#include <cstdint>
#include <utility>
#include <string>
#include <vector>
#include <optional>
#include "assert.h"

namespace edge
{
	class object;

	struct property_editor_parent_i
	{
		virtual ~property_editor_parent_i() { }
	};

	struct IPropertyEditor
	{
		virtual ~IPropertyEditor() { }
		virtual bool show (property_editor_parent_i* parent) = 0; // return IDOK, IDCANCEL, -1 (some error), 0 (hWndParent invalid or closed)
		virtual void Cancel() = 0;
	};

	using PropertyEditorFactory = std::unique_ptr<IPropertyEditor>(*const)(const std::vector<object*>& objects);

	struct property abstract
	{
		const char*           const _name;
		PropertyEditorFactory const _customEditor; // TODO: make it a template parameter

		constexpr property (const char* name, PropertyEditorFactory customEditor = nullptr)
			: _name(name), _customEditor(customEditor)
		{ }
		property (const property&) = delete;
		property& operator= (const property&) = delete;

		virtual const char* type_name() const = 0;
		virtual bool has_setter() const = 0;
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
		using base = property;

		constexpr value_property (const char* name, PropertyEditorFactory customEditor)
			: base(name, customEditor)
		{ }
	
		virtual std::string get_to_string (const object* obj) const = 0;
		virtual bool try_set_from_string (object* obj, std::string_view str) const = 0;
	};

	// ========================================================================

	template<
		typename value_t,
		typename param_t,
		typename return_t,
		const char* type_name_,
		std::string (*to_string)(param_t value),
		bool (*from_string)(std::string_view str, value_t& out)
	>
	struct typed_property : value_property
	{
		using base = value_property;

		using getter_t = return_t (object::*)() const;
		using setter_t = void (object::*)(param_t);

		getter_t const _getter;
		setter_t const _setter;
		std::optional<value_t> const _default_value;

		constexpr typed_property (const char* name, getter_t getter, setter_t setter, std::optional<value_t> default_value, PropertyEditorFactory customEditor = nullptr)
			: base(name, customEditor/*, {}*/), _getter(getter), _setter(setter), _default_value(default_value)
		{ }

		virtual const char* type_name() const override final { return type_name_; }

		virtual bool has_setter() const override final { return _setter != nullptr; }

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
	};

	extern const char* GetEnumName (const NVP* nvps, int value);

	template<typename char_type>
	bool TryGetEnumValue (const NVP* nvps, std::basic_string_view<char_type> name, int* value_out);

	template bool TryGetEnumValue (const NVP* nvps, std::string_view name, int* value_out);
	template bool TryGetEnumValue (const NVP* nvps, std::wstring_view name, int* value_out);

	template<typename enum_t, const NVP* nvps>
	struct enum_converters
	{
		static std::string enum_to_string (enum_t from)
		{
			return GetEnumName (nvps, (int) from);
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
		const NVP* nvps
	>
	using enum_property = typed_property<enum_t, enum_t, enum_t, type_name, enum_converters<enum_t, nvps>::enum_to_string, enum_converters<enum_t, nvps>::enum_from_string>;

	// ===========================================

	std::string bool_to_string (bool from);
	bool bool_from_string (std::string_view from, bool& to);
	static constexpr char bool_type_name[] = "bool";
	using bool_property = typed_property<bool, bool, bool, bool_type_name, bool_to_string, bool_from_string>;

	inline std::string uint32_to_string (uint32_t from) { return std::to_string(from); }
	bool uint32_from_string (std::string_view from, uint32_t& to);
	static constexpr char uint32_type_name[] = "uint32";
	using uint32_property = typed_property<uint32_t, uint32_t, uint32_t, uint32_type_name, uint32_to_string, uint32_from_string>;

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
	using temp_string_property = typed_property<std::string, std::string_view, std::string, temp_string_type_name, temp_string_to_string, temp_string_from_string>;

	inline std::string backed_string_to_string (std::string_view from) { return std::string(from); }
	inline bool backed_string_from_string (std::string_view from, std::string& to) { to = from; return true; }
	static constexpr char backed_string_type_name[] = "backed_string";
	using backed_string_property = typed_property<std::string, std::string_view, const std::string&, backed_string_type_name, backed_string_to_string, backed_string_from_string>;
}
