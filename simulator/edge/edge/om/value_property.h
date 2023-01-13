
#pragma once
#include "property.h"
#include "property_accessors.h"
#include "../rassert.h"

namespace edge
{
	struct __declspec(novtable) string_convert_context_i
	{
		virtual ~string_convert_context_i() = default;
	};

	class string_convert_exception : public std::exception
	{
		std::string const _message;

		static std::string make_string (std::string_view str, const char* type_name);

	public:
		string_convert_exception (const char* str);
		string_convert_exception (std::string_view str, const char* type_name);
		virtual char const* what() const noexcept override { return _message.c_str(); }
	};

	struct nvp
	{
		const char* name;
		uint32_t value;
	};

	struct value_property : property
	{
		using property::property;

		virtual const char* type_name() const = 0;
		virtual bool can_set (const object* obj) const = 0;
		virtual void get_to_string (const object* from, out_sstream_i* to, const string_convert_context_i* context) const = 0;
		virtual void set_from_string (std::string_view from, object* to, const string_convert_context_i* context) const = 0;
		virtual const nvp* nvps() const = 0;
		virtual uint32_t enum_value_as_int (const object* obj) const = 0;
		virtual bool equal (const object* obj1, const object* obj2) const = 0;
		virtual bool has_default() const = 0;
		virtual bool changed_from_default(const object* obj) const = 0;
		virtual void reset_to_default(object* obj) const = 0;
		std::string get_to_string (const object* from, const string_convert_context_i* context) const;
	};

	struct value_property_change_args : property_change_args
	{
		using base = property_change_args;

		value_property_change_args (const value_property* property)
			: base(property)
		{ }

		value_property_change_args (const value_property& property)
			: base(&property)
		{ }

		const value_property* property() const { return static_cast<const value_property*>(base::property); }
	};

	// ========================================================================

	template<typename property_traits>
	struct typed_value_property : value_property
	{
		using base = value_property;
		using base::base;

		using value_t  = typename property_traits::value_t;

		static_assert (std::is_same_v<
			decltype(property_traits::to_string(
				std::declval<const value_t&>(),
				std::declval<out_sstream_i*>(),
				std::declval<const string_convert_context_i*>())
				),
			void
		>);

		static_assert (std::is_same_v<
			decltype(property_traits::from_string(
				std::declval<std::string_view>(),
				std::declval<value_t&>(),
				std::declval<const string_convert_context_i*>())
				),
			void
		>);

	private:
		// https://stackoverflow.com/a/17534399/451036
		template <typename T, typename = void>
		struct nvps_helper
		{
			static const nvp* nvps() { return nullptr; }
			static int value(value_t v) { rassert(false); return -1; }
		};

		template <typename T>
		struct nvps_helper<T, typename std::enable_if<bool(sizeof(&T::nvps))>::type>
		{
			static const nvp* nvps() { return T::nvps; }
			static int value(value_t v) { return (int)v; }
		};

	public:
		virtual const char* type_name() const override final { return property_traits::type_name; }

		virtual const nvp* nvps() const override final { return nvps_helper<property_traits>::nvps(); }

		virtual uint32_t enum_value_as_int (const object* obj) const override final { return nvps_helper<property_traits>::value(get(obj)); }

		virtual value_t get (const object* from) const = 0;

		virtual void set (value_t from, object* to) const = 0;

		virtual void get_to_string (const object* from, out_sstream_i* to, const string_convert_context_i* context) const override final
		{
			property_traits::to_string(this->get(from), to, context);
		}

		using base::get_to_string;

		virtual void set_from_string (std::string_view from, object* to, const string_convert_context_i* context) const override final
		{
			value_t value;
			property_traits::from_string (from, value, context);
			this->set(std::move(value), to);
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
		using getter_t = pointer_to_derived_member_function_t<value_t, true>;
		using setter_t = pointer_to_derived_member_function_t<void, false, value_t>;

		const char* const _name;
		getter_t const _getter;
		setter_t const _setter;
		std::optional<value_t> const _default_value;

		constexpr static_value_property (const char* name, getter_t getter, setter_t setter, std::optional<value_t> default_value = std::nullopt)
			: _name(name), _getter(getter), _setter(setter), _default_value(std::move(default_value))
		{ }

		const std::optional<value_t>& default_value() const { return _default_value; }

		virtual const char* name() const override final { return _name; }

		virtual bool can_set (const object* obj) const override final { return _setter.mg; }

		virtual value_t get (const object* obj) const override final { return (obj->*(_getter.mg))(); }

		virtual void set (value_t value, object* obj) const override final { return (obj->*(_setter.mg))(std::move(value)); }

		virtual bool has_default() const override final
		{
			return _default_value.has_value();
		}

		virtual bool changed_from_default (const object* obj) const override final
		{
			return !_default_value || ((obj->*(_getter.mg))() != _default_value.value());
		}

		virtual void reset_to_default(object* obj) const override
		{
			(obj->*(_setter.mg))(_default_value.value());
		}
	};

	// ===========================================

	struct bool_property_traits
	{
		static const char type_name[];
		using value_t = bool;
		static constexpr nvp nvps[] = { { "False", 0 }, { "True", 1 }, { nullptr }, };
		static void to_string (value_t from, out_sstream_i* to, const string_convert_context_i* context);
		static void from_string (std::string_view from, value_t& to, const string_convert_context_i* context);
	};

	template<typename t_, const char* type_name_>
	struct arithmetic_property_traits
	{
		//static_assert (std::is_arithmetic_v<t_>);
		static constexpr const char* type_name = type_name_;
		using value_t = t_;
		static void to_string (value_t from, out_sstream_i* to, const string_convert_context_i* context); // needs specialization
		static void from_string (std::string_view from, value_t& to, const string_convert_context_i* context); // needs specialization
	};

	extern const char int32_type_name[];
	using int32_property_traits = arithmetic_property_traits<int32_t, int32_type_name>;

	extern const char uint8_type_name[];
	using uint8_property_traits = arithmetic_property_traits<uint8_t, uint8_type_name>;

	extern const char uint32_type_name[];
	using uint32_property_traits = arithmetic_property_traits<uint32_t, uint32_type_name>;

	extern const char uint64_type_name[];
	using uint64_property_traits = arithmetic_property_traits<uint64_t, uint64_type_name>;

	extern const char size_t_type_name[];
	using size_t_property_traits = arithmetic_property_traits<size_t, size_t_type_name>;

	extern const char float_type_name[];
	using float_property_traits = arithmetic_property_traits<float, float_type_name>;

	// TODO: class string_or_view
	struct backed_string_property_traits
	{
		static constexpr char type_name[] = "backed_string";
		using value_t = std::string_view;
		static void to_string (value_t from, out_sstream_i* to, const string_convert_context_i* context) { to->write(from); }
		static void from_string (std::string_view from, value_t& to, const string_convert_context_i* context) { to = from; }
	};
	using backed_string_p = static_value_property<backed_string_property_traits>;

	struct temp_string_property_traits
	{
		static constexpr char type_name[] = "temp_string";
		using value_t = std::string;
		static void to_string (value_t from, out_sstream_i* to, const string_convert_context_i* context) { to->write(from); }
		static void from_string (std::string_view from, value_t& to, const string_convert_context_i* context) { to = from; }
	};
	using temp_string_p = static_value_property<temp_string_property_traits>;

	// ========================================================================

	extern const char unknown_enum_value_str[];

	template<typename value_t_, const char* type_name_, const nvp* nvps_, bool serialize_as_integer = false, const char* unknown_str = unknown_enum_value_str>
	struct enum_property_traits
	{
		static constexpr const char* type_name = type_name_;
		using value_t = value_t_;

		static constexpr const nvp* nvps = nvps_;

		static void to_string (value_t from, out_sstream_i* to, const string_convert_context_i* context)
		{
			if (serialize_as_integer)
			{
				int32_property_traits::to_string((int32_t)from, to, context);
				return;
			}

			for (auto nvp = nvps_; nvp->name != nullptr; nvp++)
			{
				if (nvp->value == (int)from)
				{
					to->write(nvp->name);
					return;
				}
			}

			to->write(unknown_str);
		}

		static void from_string (std::string_view from, value_t& to, const string_convert_context_i* context)
		{
			if (serialize_as_integer)
			{
				try
				{
					int32_t val;
					int32_property_traits::from_string(from, val, context);
					to = (value_t)val;
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
					to = static_cast<value_t>(nvp->value);
					return;
				}
			}

			throw string_convert_exception(from, type_name);
		}
	};

	extern const char unknown_enum_value_str[];

	// ========================================================================

	enum class side { left, top, right, bottom };
	extern const char side_type_name[];
	extern const nvp side_nvps[];
	using side_property_traits = enum_property_traits<side, side_type_name, side_nvps, false, unknown_enum_value_str>;
}
