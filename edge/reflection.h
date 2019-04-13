
#pragma once
#include <cstdint>
#include <utility>
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <variant>
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

	enum class ui_visible { no, yes };

	struct property abstract
	{
		const char* const _name;
		const property_group* const _group;
		const char* const _description;
		ui_visible _ui_visible;

		constexpr property (const char* name, const property_group* group, const char* description, enum ui_visible ui_visible)
			: _name(name), _group((group != nullptr) ? group : &misc_group), _description(description), _ui_visible(ui_visible)
		{ }
		property (const property&) = delete;
		property& operator= (const property&) = delete;

		virtual property_editor_factory_t* custom_editor() const { return nullptr; }
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

		template<size_t size>
		constexpr view (const std::array<t, size>& array)
			: from(array.data()), to(array.data() + array.size())
		{ }

		constexpr view (t* from, t* to)
			: from(from), to(to)
		{ }

		t* data() const { return from; }
		size_t size() const { return to - from; }
		
		const t& operator[](size_t i) const
		{
			assert (i < size());
			return from[i];
		}

		using iterator = t*;
		iterator begin() const { return from; }
		iterator end() const { return to; }
	};

	struct value_property : property
	{
		using property::property;
	
		virtual const char* type_name() const = 0;
		virtual bool has_setter() const = 0;
		virtual std::string get_to_string (const object* obj) const = 0;
		virtual bool try_set_from_string (object* obj, std::string_view str) const = 0;
		virtual const NVP* nvps() const = 0;
		virtual bool equal (object* obj1, object* obj2) const = 0;
		virtual bool changed_from_default(const object* obj) const = 0;
	};

	// ========================================================================

	template<typename property_traits,
		const NVP* nvps_ = nullptr,
		property_editor_factory_t* custom_editor_ = nullptr
	>
	struct typed_property : value_property
	{
		using base = value_property;

		using value_t  = typename property_traits::value_t;
		using param_t  = typename property_traits::param_t;
		using return_t = typename property_traits::return_t;

		static constexpr auto from_string = &property_traits::from_string;

		using member_getter_t = return_t (object::*)() const;
		using member_setter_t = void (object::*)(param_t);
		using static_getter_t = return_t(*)(const object*);
		using static_setter_t = void(*)(object*, param_t);

		using getter_t = std::variant<member_getter_t, static_getter_t, nullptr_t>;
		using setter_t = std::variant<member_setter_t, static_setter_t, nullptr_t>;

		getter_t const _getter;
		setter_t const _setter;
		std::optional<value_t> const _default_value;

		constexpr typed_property (const char* name, const property_group* group, const char* description, enum ui_visible ui_visible, getter_t getter, setter_t setter, std::optional<value_t> default_value)
			: base(name, group, description, ui_visible), _getter(getter), _setter(setter), _default_value(default_value)
		{ }

		virtual const char* type_name() const override final { return property_traits::type_name; }

		virtual bool has_setter() const override final { return !std::holds_alternative<nullptr_t>(_setter); }

		virtual property_editor_factory_t* custom_editor() const override { return custom_editor_; }

	public:
		std::string get_to_string (const object* obj) const final
		{
			auto value = std::holds_alternative<member_getter_t>(_getter) ? (obj->*std::get<member_getter_t>(_getter))() : std::get<static_getter_t>(_getter)(obj);
			return property_traits::to_string (value);
		}

		virtual bool try_set_from_string (object* obj, std::string_view str_in) const override final
		{
			value_t value;
			bool ok = property_traits::from_string(str_in, value);
			if (ok)
			{
				if (std::holds_alternative<member_setter_t>(_setter))
					(obj->*std::get<member_setter_t>(_setter))(value);
				else if (std::holds_alternative<static_setter_t>(_setter))
					std::get<static_setter_t>(_setter)(obj, value);
				else
					assert(false);
			}

			return ok;
		}

		virtual const NVP* nvps() const override final
		{
			return nvps_;
		}

		virtual bool equal (object* obj1, object* obj2) const override final
		{
			auto val1 = std::holds_alternative<member_getter_t>(_getter) ? (obj1->*std::get<member_getter_t>(_getter))() : std::get<static_getter_t>(_getter)(obj1);
			auto val2 = std::holds_alternative<member_getter_t>(_getter) ? (obj2->*std::get<member_getter_t>(_getter))() : std::get<static_getter_t>(_getter)(obj2);
			return val1 == val2;
		}

		virtual bool changed_from_default(const object* obj) const override
		{
			if (!_default_value.has_value())
				return true;

			auto val = std::holds_alternative<member_getter_t>(_getter) ? (obj->*std::get<member_getter_t>(_getter))() : std::get<static_getter_t>(_getter)(obj);
			return val != _default_value.value();
		}
	};

	template<typename enum_t, const char* type_name_, const NVP* nvps>
	struct enum_property_traits
	{
		static constexpr const char* type_name = type_name_;
		using value_t = enum_t;
		using param_t = enum_t;
		using return_t = enum_t;

		static std::string to_string (enum_t from)
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
		static bool from_string (std::string_view from, enum_t& to)
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
	using enum_property = typed_property<enum_property_traits<enum_t, type_name, nvps>, nvps>;

	// ===========================================

	struct bool_property_traits
	{
		static constexpr char type_name[] = "bool";
		using value_t = bool;
		using param_t = bool;
		using return_t = bool;
		static std::string to_string (bool from) { return std::to_string(from); }
		static bool from_string (std::string_view from, bool& to);
	};
	using bool_p = typed_property<bool_property_traits>;

	struct uint32_property_traits
	{
		static constexpr char type_name[] = "uint32";
		using value_t = uint32_t;
		using param_t = uint32_t;
		using return_t = uint32_t;
		static std::string to_string (uint32_t from) { return std::to_string(from); }
		static bool from_string (std::string_view from, uint32_t& to);
	};
	using uint32_p = typed_property<uint32_property_traits>;

	struct int32_property_traits
	{
		static constexpr char type_name[] = "int32";
		using value_t = int32_t;
		using param_t = int32_t;
		using return_t = int32_t;
		static std::string to_string (int32_t from) { return std::to_string(from); }
		static bool from_string (std::string_view from, int32_t& to);
	};
	using int32_p = typed_property<int32_property_traits>;

	struct float_property_traits
	{
		static constexpr char type_name[] = "float";
		using value_t = float;
		using param_t = float;
		using return_t = float;
		static std::string to_string (float from) { return std::to_string(from); }
		static bool from_string (std::string_view from, float& to);
	};
	using float_p = typed_property<float_property_traits>;

	template<bool backed>
	struct string_property_traits
	{
		static constexpr const char* type_name = backed ? "backed_string" : "temp_string";
		using value_t = std::string;
		using param_t = std::string_view;
		using return_t = std::conditional_t<backed, const std::string&, std::string>;
		static std::string to_string (std::string_view from) { return std::string(from); }
		static bool from_string (std::string_view from, std::string& to) { to = from; return true; }
	};
	using temp_string_property_traits = string_property_traits<false>;
	using temp_string_p = typed_property<temp_string_property_traits>;
	using backed_string_property_traits = string_property_traits<true>;
	using backed_string_p = typed_property<backed_string_property_traits>;

	// ===========================================

	struct object_collection_property : property
	{
		using base = property;
		using base::base;

		virtual size_t child_count (const object* parent) const = 0;
		virtual object* child_at (const object* parent, size_t index) const = 0;
		virtual void insert_child (object* parent, size_t index, std::unique_ptr<object>&& child) const = 0;
		virtual std::unique_ptr<object> remove_child (object* parent, size_t index) const = 0;
	};
	
	template<typename parent_t, typename child_t>
	struct typed_object_collection_property : object_collection_property
	{
		static_assert (std::is_base_of_v<object, child_t>);
		static_assert (std::is_base_of_v<object, parent_t>);

		using base = object_collection_property;
		using base::base;

		using get_child_count_t = size_t(parent_t::*)() const;
		using get_child_t       = child_t*(parent_t::*)(size_t) const;
		using insert_child_t    = void(parent_t::*)(size_t, std::unique_ptr<child_t>&&);
		using remove_child_t    = std::unique_ptr<child_t>(parent_t::*)(size_t);

		get_child_count_t const _get_child_count;
		get_child_t       const _get_child;
		insert_child_t    const _insert_child;
		remove_child_t    const _remove_child;

		constexpr typed_object_collection_property (const char* name, const property_group* group, const char* description, enum ui_visible ui_visible,
			get_child_count_t get_child_count, get_child_t get_child, insert_child_t insert_child, remove_child_t remove_child)
			: base (name, group, description, ui_visible)
			, _get_child_count(get_child_count)
			, _get_child(get_child)
			, _insert_child(insert_child)
			, _remove_child(remove_child)
		{ }

		virtual size_t child_count (const object* parent) const override
		{
			auto typed_parent = static_cast<const parent_t*>(parent);
			return (typed_parent->*_get_child_count)();
		}
		
		virtual object* child_at (const object* parent, size_t index) const override
		{
			auto typed_parent = static_cast<const parent_t*>(parent);
			return (typed_parent->*_get_child)(index);
		}
		
		virtual void insert_child (object* parent, size_t index, std::unique_ptr<object>&& child) const override
		{
			auto typed_parent = static_cast<parent_t*>(parent);
			auto raw_child = static_cast<child_t*>(child.release());
			(typed_parent->*_insert_child)(index, std::unique_ptr<child_t>(raw_child));
		}
		
		virtual std::unique_ptr<object> remove_child (object* parent, size_t index) const override
		{
			auto typed_parent = static_cast<parent_t*>(parent);
			auto typed_child = (typed_parent->*_remove_child)(index);
			auto raw_child = typed_child.release();
			return std::unique_ptr<object>(raw_child);
		}
	};
	/*
	struct value_collection_property : property
	{
		using base = property;
		using base::base;

		virtual size_t value_count (const object* parent) const = 0;
		virtual std::string value_at (const object* parent, size_t index) const = 0;
		virtual void insert_value (object* parent, size_t index, std::string_view value) const = 0;
		virtual void remove_child (object* parent, size_t index) const = 0;
	};

	template<typename object_t, typename value_t, typename>
	struct typed_value_collection_property : value_collection_property
	{
		static_assert (std::is_convertible_v<object_t*, object*>);

		using base = value_collection_property;
		using base::base;

		using get_value_count_t = size_t(object_t::*)() const;
		using get_value_t       = value_t*(object_t::*)(size_t) const;
		using insert_value_t    = void(object_t::*)(size_t, std::unique_ptr<value_t>&&);
		using remove_value_t    = std::unique_ptr<value_t>(object_t::*)(size_t);

		get_value_count_t const _get_value_count;
		get_value_t       const _get_value;
		insert_value_t    const _insert_value;
		remove_value_t    const _remove_value;

		constexpr value_collection_property (const char* name, const property_group* group, const char* description, enum ui_visible ui_visible,
			get_child_count_t get_child_count, get_child_t get_child, insert_child_t insert_child, remove_child_t remove_child)
			: base (name, group, description, ui_visible)
			, _get_child_count(get_child_count)
			, _get_child(get_child)
			, _insert_child(insert_child)
			, _remove_child(remove_child)
		{ }
	};
	*/
}
