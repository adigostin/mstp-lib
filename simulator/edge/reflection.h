
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include <string>
#include <string_view>
#include <memory>
#include <type_traits>
#include <cstdint>
#include <cstdio>
#include <optional>

#define TCB_SPAN_NAMESPACE_NAME std
#define TCB_SPAN_NO_CONTRACT_CHECKING
#include "tcb/span.hpp"

#include "assert.h"

namespace edge
{
	class object;
	class type;
	struct concrete_type;
	template<typename... factory_arg_property_traits> struct xtype;

	struct property_group
	{
		int32_t prio;
		const char* name;
	};

	extern const property_group misc_group;

	class string_convert_exception : public std::exception
	{
		std::string const _message;

		static std::string make_string (std::string_view str, const char* type_name);

	public:
		string_convert_exception (const char* str);
		string_convert_exception (std::string_view str, const char* type_name);
		virtual char const* what() const noexcept override { return _message.c_str(); }
	};

	class not_implemented_exception : public std::exception
	{
		virtual const char* what() const noexcept override { return "Not implemented"; }
	};

	// Note that we want this type to be polymorphic so we can dynamic_cast<> on it.
	// It should contain at least one virtual function (in C++20 that could be the destructor).
	struct property
	{
		const char* const _name;
		const property_group* const _group;
		const char* const _description;

		constexpr property (const char* name, const property_group* group, const char* description)
			: _name(name), _group((group != nullptr) ? group : &misc_group), _description(description)
		{ }
		property (const property&) = delete;
		property& operator= (const property&) = delete;

	private:
		virtual void dummy_to_make_polymorphic() { }
	};

	struct nvp
	{
		const char* name;
		int value;
	};

	struct out_stream_i
	{
		virtual void write (const void* data, size_t size) = 0;

		void write (uint8_t data) { write(&data, sizeof(data)); }
	};

	struct binary_reader
	{
		const uint8_t* ptr;
		const uint8_t* const end;
	};

	struct value_property : property
	{
		using property::property;

		virtual const char* type_name() const = 0;
		virtual bool can_set (const object* obj) const = 0;
		virtual void get_to_string (const object* from, std::string& to) const = 0;
		virtual void set_from_string (std::string_view from, object* to) const = 0;
		virtual void serialize (const object* from, out_stream_i* to) const = 0;
		virtual void deserialize (binary_reader& from, object* to) const = 0;
		virtual const nvp* nvps() const = 0;
		virtual bool equal (const object* obj1, const object* obj2) const = 0;
		virtual bool changed_from_default(const object* obj) const = 0;
		virtual void reset_to_default(object* obj) const = 0;

		std::string get_to_string (const object* from) const
		{
			std::string to;
			get_to_string (from, to);
			return to;
		}
	};

	// ========================================================================

	template<typename property_traits>
	struct typed_value_property : value_property
	{
		using base = value_property;
		using base::base;

		using value_t  = typename property_traits::value_t;

	private:
		// https://stackoverflow.com/a/17534399/451036
		template <typename T, typename = void>
		struct nvps_helper
		{
			static const nvp* nvps() { return nullptr; }
		};

		template <typename T>
		struct nvps_helper<T, typename std::enable_if<bool(sizeof(&T::nvps))>::type>
		{
			static const nvp* nvps() { return T::nvps; }
		};

	public:
		virtual const char* type_name() const override final { return property_traits::type_name; }

		virtual const nvp* nvps() const override final { return nvps_helper<property_traits>::nvps(); }

		virtual value_t get (const object* from) const = 0;

		virtual void set (value_t from, object* to) const = 0;

		virtual void get_to_string (const object* from, std::string& to) const override final
		{
			property_traits::to_string(this->get(from), to);
		}

		virtual void set_from_string (std::string_view from, object* to) const override final
		{
			value_t value;
			property_traits::from_string (from, value);
			this->set(value, to);
		}

		virtual void serialize (const object* from, out_stream_i* to) const override final
		{
			property_traits::serialize (this->get(from), to);
		}

		virtual void deserialize (binary_reader& from, object* to) const override final
		{
			value_t value;
			property_traits::deserialize(from, value);
			if (to)
				this->set (value, to);
		}

		virtual bool equal (const object* obj1, const object* obj2) const override final
		{
			return this->get(obj1) == this->get(obj2);
		}
	};

	// ========================================================================

	template<typename property_traits>
	struct static_value_property : typed_value_property<property_traits>
	{
		using base = typed_value_property<property_traits>;

		using value_t  = typename property_traits::value_t;

		using member_getter_t = value_t (object::*)() const;
		using member_setter_t = void (object::*)(value_t value);
		using static_getter_t = value_t(*)(const object*);
		using static_setter_t = void(*)(object*, value_t);
		using member_var_t = value_t object::*;

		template <typename From, typename To, typename = void>
		struct is_static_castable
		{
			static constexpr bool value = false;
		};

		template <typename From, typename To>
		struct is_static_castable<From, To, std::void_t<decltype(static_cast<To>(*(From*)(nullptr)))>>
		{
			static constexpr bool value = true;
		};

		class getter_t
		{
			enum getter_type { none, member_function, static_function, member_var };

			getter_type const type;
			union
			{
				std::nullptr_t  np;
				member_getter_t mg;
				static_getter_t sg;
				member_var_t    mv;
			};

		public:
			constexpr getter_t (std::nullptr_t np) noexcept : type(none), np(np) { }

			template<typename MemberGetter, std::enable_if_t<is_static_castable<MemberGetter, member_getter_t>::value, int> = 0>
			constexpr getter_t (MemberGetter mg) noexcept : type(member_function), mg(static_cast<member_getter_t>(mg)) { }

			template<typename StaticGetter, std::enable_if_t<is_static_castable<StaticGetter, static_getter_t>::value, int> = 0>
			constexpr getter_t (StaticGetter sg) noexcept : type(static_function), sg(static_cast<static_getter_t>(sg)) { }

			constexpr getter_t (member_var_t mv) noexcept : type(member_var), mv(mv) { }

			value_t get (const object* obj) const
			{
				if (type == member_function)
					return (obj->*mg)();

				if (type == static_function)
					return sg(obj);

				if (type == member_var)
					return obj->*mv;

				throw nullptr;
			}
		};

		class setter_t
		{
			enum setter_type { member_function, static_function, none };

			setter_type const type;
			union
			{
				std::nullptr_t  np;
				member_setter_t ms;
				static_setter_t ss;
			};

		public:
			constexpr setter_t (std::nullptr_t np) noexcept : type(none), np(np) { }

			template<typename MemberSetter, std::enable_if_t<is_static_castable<MemberSetter, member_setter_t>::value, int> = 0>
			constexpr setter_t (MemberSetter ms) noexcept : type(member_function), ms(static_cast<member_setter_t>(ms)) { }

			template<typename StaticSetter, std::enable_if_t<is_static_castable<StaticSetter, static_setter_t>::value, int> = 0>
			constexpr setter_t (StaticSetter ss) noexcept : type(static_function), ss(static_cast<static_setter_t>(ss)) { }

			void set (value_t from, object* to) const
			{
				if (type == member_function)
					(to->*ms)(from);
				else if (type == static_function)
					ss(to, from);
				else
					assert(false);
			}

			bool is_null() const { return type == none; }
		};

		getter_t const _getter;
		setter_t const _setter;
		std::optional<value_t> const default_value;

		constexpr static_value_property (const char* name, const property_group* group, const char* description, getter_t getter, setter_t setter, std::optional<value_t>&& default_value = std::nullopt)
			: base(name, group, description), _getter(getter), _setter(setter), default_value(std::move(default_value))
		{ }

		virtual bool can_set (const object* obj) const override final { return !_setter.is_null(); }

		virtual value_t get (const object* from) const override final { return _getter.get(from); }

		virtual void set (value_t from, object* to) const override final { _setter.set(from, to); }

		virtual bool changed_from_default (const object* obj) const override
		{
			return !default_value || (_getter.get(obj) != default_value.value());
		}

		virtual void reset_to_default(object* obj) const override
		{
			_setter.set(default_value.value(), obj);
		}
	};

	// ===========================================

	struct bool_property_traits
	{
		static const char type_name[];
		using value_t = bool;
		static constexpr nvp nvps[] = { { "False", 0 }, { "True", 1 }, { nullptr, -1 }, };
		static void to_string (value_t from, std::string& to);
		static void from_string (std::string_view from, value_t& to);
		static void serialize (value_t from, out_stream_i* to) { assert(false); } // not implemented
		static void deserialize (binary_reader& from, value_t& to) { assert(false); } // not implemented
	};
	using bool_p = static_value_property<bool_property_traits>;

	template<typename t_, const char* type_name_>
	struct arithmetic_property_traits
	{
		//static_assert (std::is_arithmetic_v<t_>);
		static constexpr const char* type_name = type_name_;
		using value_t = t_;
		static void to_string (value_t from, std::string& to); // needs specialization
		static void from_string (std::string_view from, value_t& to); // needs specialization
		static void serialize (value_t from, out_stream_i* to); // needs specialization
		static void deserialize (binary_reader& from, value_t& to); // needs specialization
	};

	extern const char int32_type_name[];
	using int32_property_traits = arithmetic_property_traits<int32_t, int32_type_name>;
	using int32_p = static_value_property<int32_property_traits>;

	extern const char uint32_type_name[];
	using uint32_property_traits = arithmetic_property_traits<uint32_t, uint32_type_name>;
	using uint32_p = static_value_property<uint32_property_traits>;

	extern const char uint64_type_name[];
	using uint64_property_traits = arithmetic_property_traits<uint64_t, uint64_type_name>;
	using uint64_p = static_value_property<uint64_property_traits>;

	extern const char size_t_type_name[];
	using size_t_property_traits = arithmetic_property_traits<size_t, size_t_type_name>;
	using size_t_p = static_value_property<size_t_property_traits>;

	extern const char float_type_name[];
	using float_property_traits = arithmetic_property_traits<float, float_type_name>;
	using float_p = static_value_property<float_property_traits>;

	struct backed_string_property_traits
	{
		static const char type_name[];
		using value_t = std::string_view;
		static void to_string (value_t from, std::string& to) { to = from; }
		static void from_string (std::string_view from, value_t& to) { to = from; }
		static void serialize (value_t from, out_stream_i* to);
		static void deserialize (binary_reader& from, value_t& to);
	};
	using backed_string_p = static_value_property<backed_string_property_traits>;

	struct temp_string_property_traits
	{
		static constexpr char type_name[] = "temp_string";
		using value_t = std::string;
		static void to_string (value_t from, std::string& to) { to = from; }
		static void from_string (std::string_view from, value_t& to) { to = from; }
		static void serialize (value_t from, out_stream_i* to);
		static void deserialize (binary_reader& from, value_t& to);
	};
	using temp_string_p = static_value_property<temp_string_property_traits>;

	// ========================================================================

	template<typename enum_t, const char* type_name_, const nvp* nvps_, bool serialize_as_integer, const char* unknown_str>
	struct enum_property_traits
	{
		static constexpr const char* type_name = type_name_;
		using value_t = enum_t;

		static constexpr const nvp* nvps = nvps_;

		static void to_string (enum_t from, std::string& to)
		{
			if (serialize_as_integer)
			{
				int32_property_traits::to_string((int32_t)from, to);
				return;
			}

			for (auto nvp = nvps_; nvp->name != nullptr; nvp++)
			{
				if (nvp->value == (int)from)
				{
					to = nvp->name;
					return;
				}
			}

			to = unknown_str;
		}

		static void from_string (std::string_view from, enum_t& to)
		{
			if (serialize_as_integer)
			{
				try
				{
					int32_t val;
					int32_property_traits::from_string(from, val);
					to = (enum_t)val;
					return;
				}
				catch (const string_convert_exception&)
				{
				}
			}

			for (auto nvp = nvps_; nvp->name != nullptr; nvp++)
			{
				if (from == nvp->name)
				{
					to = static_cast<enum_t>(nvp->value);
					return;
				}
			}

			throw string_convert_exception(from, type_name);
		}

		static void serialize (value_t from, out_stream_i* to) { assert(false); }
		static void deserialize (binary_reader& from, value_t& to) { assert(false); }
	};

	extern const char unknown_enum_value_str[];

	template<typename enum_t, const char* type_name, const nvp* nvps_, bool serialize_as_integer = false, const char* unknown_str = unknown_enum_value_str>
	using enum_property = static_value_property<enum_property_traits<enum_t, type_name, nvps_, serialize_as_integer, unknown_str>>;

	// ========================================================================

	enum class side { left, top, right, bottom };
	static constexpr const char side_type_name[] = "side";
	static constexpr const nvp side_nvps[] = {
		{ "Left",   (int) side::left },
		{ "Top",    (int) side::top },
		{ "Right",  (int) side::right },
		{ "Bottom", (int) side::bottom },
		{ 0, 0 },
	};
	using side_p = edge::enum_property<side, side_type_name, side_nvps>;

	// ========================================================================

	struct object_property : property
	{
		using base = property;

	public:
		using base::base;
		virtual object* get (const object* obj) const = 0;
		virtual std::unique_ptr<object> set (object* obj, std::unique_ptr<object>&& value) const = 0;
	};

	template<typename object_t>
	struct typed_object_property : object_property
	{
		using base = object_property;

		using getter_t = object_t* (object::*)() const;
		using setter_t = std::unique_ptr<object_t> (object::*)(std::unique_ptr<object_t>&& value);

		getter_t const _getter;
		setter_t const _setter;

		typed_object_property (const char* name, const property_group* group, const char* description, getter_t getter, setter_t setter)
			: base (name, group, description), _getter(getter), _setter(setter)
		{
			static_assert (std::is_base_of<object, object_t>::value);
		}

		std::unique_ptr<object_t> set (object* obj, std::unique_ptr<object_t>&& value) const
		{
			(obj->*_setter)(std::move(value));
			assert(false); return nullptr; // TODO: return old value
		}

		virtual object_t* get (const object* obj) const override final { return (obj->*_getter)(); }

		virtual std::unique_ptr<object> set (object* obj, std::unique_ptr<object>&& value) const override final
		{
			return set (obj, std::unique_ptr<object_t>(static_cast<object_t*>(value.release())));
		}
	};

	// ========================================================================

	struct value_collection_property : property
	{
		using property::property;
		virtual size_t size (const object* obj) const = 0;
		virtual bool can_insert_remove() const = 0;
		virtual void get_value (const object* from_obj, size_t from_index, std::string& to) const = 0;
		virtual void set_value (std::string_view from, object* to_obj, size_t to_index) const = 0;
		virtual void insert_value (std::string_view from, object* to_obj, size_t to_index) const = 0;
		virtual void remove_value (object* obj, size_t index) const = 0;
		virtual bool changed (const object* obj) const = 0;
	};

	// TODO: try to get rid of object_t
	template<typename object_t, typename property_traits>
	struct typed_value_collection_property : value_collection_property
	{
		static_assert (std::is_base_of<object, object_t>::value);

		using base = value_collection_property;

		using member_get_size_t = size_t(object_t::*)() const;
		using static_get_size_t = size_t(*)(const object_t*);

		enum accessor_type { member_function, static_function };

		struct get_size_t
		{
			accessor_type const type;
			union
			{
				member_get_size_t mg;
				static_get_size_t sg;
			};

			constexpr get_size_t (member_get_size_t mg) : type(member_function), mg(mg) { }
			constexpr get_size_t (static_get_size_t sg) : type(static_function), sg(sg) { }
		};


		using get_value_t    = typename property_traits::value_t(object_t::*)(size_t) const;
		using set_value_t    = void(object_t::*)(size_t, typename property_traits::value_t);
		using insert_value_t = void(object_t::*)(size_t, typename property_traits::value_t);
		using remove_value_t = void(object_t::*)(size_t);
		using changed_t      = bool(object_t::*)() const;

		get_size_t     const _get_size;
		get_value_t    const _get_value;
		set_value_t    const _set_value;
		insert_value_t const _insert_value;
		remove_value_t const _remove_value;
		changed_t      const _changed;

		constexpr typed_value_collection_property (const char* name, const property_group* group, const char* description,
			get_size_t get_size, get_value_t get_value, set_value_t set_value, insert_value_t insert_value, remove_value_t remove_value, changed_t changed)
			: base (name, group, description)
			, _get_size(get_size)
			, _get_value(get_value)
			, _set_value(set_value)
			, _insert_value(insert_value)
			, _remove_value(remove_value)
			, _changed(changed)
		{
			// At most one of "set_value" and "insert_value" must be non-null.
			//
			// If set_value is non-null, the object must pre-allocate the collection with default values,
			// and the deserializer must call set_value to overwrite some values.
			//
			// If insert_value is non-null, the object must not pre-allocate the collection,
			// and the deserializer must call insert_value to append some values.
			assert (set_value || insert_value);
		}

		virtual size_t size (const object* obj) const override
		{
			auto ot = static_cast<const object_t*>(obj);

			if (_get_size.type == accessor_type::member_function)
				return (ot->*(_get_size.mg))();

			if (_get_size.type == accessor_type::static_function)
				return _get_size.sg(ot);

			assert(false);
			return 0;
		}

		virtual void get_value (const object* from_obj, size_t from_index, std::string& to) const override
		{
			auto value = (static_cast<const object_t*>(from_obj)->*_get_value)(from_index);
			property_traits::to_string(value, to);
		}

		virtual void set_value (std::string_view from, object* to_obj, size_t to_index) const override
		{
			typename property_traits::value_t value;
			property_traits::from_string(from, value);
			(static_cast<object_t*>(to_obj)->*_set_value) (to_index, value);
		}

		virtual void insert_value (std::string_view from, object* to_obj, size_t to_index) const override
		{
			typename property_traits::value_t value;
			property_traits::from_string(from, value);
			(static_cast<object_t*>(to_obj)->*_insert_value) (to_index, value);
		}

		virtual void remove_value (object* obj, size_t index) const override
		{
			assert(false); // not implemented
		}

		virtual bool can_insert_remove() const override { return _insert_value != nullptr; }

		virtual bool changed (const object* obj) const override
		{
			const object_t* ot = static_cast<const object_t*>(obj);
			return (ot->*_changed)();
		}
	};
}
