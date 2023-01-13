
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "hierarchy_object.h"
#include "value_property.h"
#include "../events.h"

namespace edge
{
	struct object;

	class not_implemented_exception : public std::exception
	{
		virtual const char* what() const noexcept override { return "Not implemented"; }
	};

	class type
	{
	public:
		const type* const base_type;
		std::span<const property* const> const props;

		type (const type* base_type, std::span<const property* const> props) noexcept;
		virtual ~type() = default;
		const property* find_property (const char* name) const;
		bool has_property (const property* p) const;
		bool is_same_or_derived_from (const type* t) const;
		bool is_same_or_derived_from (const type& t) const;
	};

	class concrete_type : public type
	{
		using base = type;
		static std::vector<const concrete_type*>* _known_types;

	public:
		const char* const name;

		concrete_type (const char* name, const type* base_type, std::span<const property* const> props) noexcept;
		virtual ~concrete_type();
		virtual std::span<const value_property* const> factory_props() const = 0;
		virtual std::unique_ptr<object> create (std::span<std::string_view> string_values, string_convert_context_i* context) const = 0;
		std::unique_ptr<object> create() const;

		static const std::vector<const concrete_type*>& known_types();

		class property_enumerator
		{
			size_t _prop_index;
			const type* _type;

		public:
			property_enumerator (const concrete_type* type);

			property_enumerator& operator++();

			property_enumerator& operator++(int) { return this->operator++(); }

			operator bool() const { return !!_type; }

			const property* operator*() const
			{
				rassert(_type);
				return _type->props[_prop_index];
			}

			const property* operator->() const
			{
				rassert(_type);
				return _type->props[_prop_index];
			}

			std::vector<const property*> to_vector();
		};

		property_enumerator make_property_enumerator() const
		{
			return property_enumerator(this);
		}
	};

	template<typename t, typename... factory_arg_property_traits>
	struct xtype : concrete_type
	{
		static constexpr size_t parameter_count = sizeof...(factory_arg_property_traits);

		using factory_t = std::unique_ptr<t>(typename factory_arg_property_traits::value_t&&... factory_args);

		factory_t* const _factory;
		std::array<const value_property*, parameter_count> const _factory_props; // TODO: change to tuple

	public:
		constexpr xtype (const char* name, const type* base, std::span<const property* const> props,
			factory_t* factory = nullptr, const typed_value_property<factory_arg_property_traits>*... factory_props)
			: concrete_type(name, base, props)
			, _factory(factory)
			, _factory_props(std::array<const value_property*, parameter_count>{ factory_props... })
		{
			static_assert(std::is_base_of_v<object, t>);
			rassert ((!factory_props->has_default()) && ...);
		}

		xtype(const xtype&) = delete;
		xtype& operator=(const xtype&) = delete;

		factory_t* factory() const { return _factory; }

		virtual std::span<const value_property* const> factory_props() const override { return _factory_props; }

		std::unique_ptr<object> create (typename factory_arg_property_traits::value_t&&... factory_args) const
		{
			return _factory(std::forward<typename factory_arg_property_traits::value_t>(factory_args)...);
		}

	private:
		template<size_t... I>
		std::unique_ptr<object> create_internal (std::span<std::string_view> string_values, std::tuple<typename factory_arg_property_traits::value_t...>& values, std::index_sequence<I...>, string_convert_context_i* context) const
		{
			static_assert(std::is_base_of_v<object, t>);
			(factory_arg_property_traits::from_string(string_values[I], std::get<I>(values), context), ...);
			return _factory(std::forward<typename factory_arg_property_traits::value_t>(std::move(std::get<I>(values)))...);
		}

	public:
		virtual std::unique_ptr<object> create (std::span<std::string_view> string_values, string_convert_context_i* context) const override
		{
			rassert (_factory);
			rassert (string_values.size() == parameter_count);
			std::tuple<typename factory_arg_property_traits::value_t...> values;
			return create_internal(string_values, values, std::make_index_sequence<parameter_count>(), context);
		}
	};

	struct __declspec(novtable) object : hierarchy_object_i
	{
		virtual const edge::concrete_type* type() const = 0;

		#pragma region enumerators
		struct enumerator_i : edge::enumerator_i<hierarchy_object_i*>
		{
			virtual object* get() const = 0;
		};

		virtual enumerator_i* make_enumerator() override final;
		auto enum_children() { return enumerable<object*, enumerator_i>(make_enumerator()); }
		#pragma endregion
	};

	struct property_changing_e : event<property_changing_e, object*, const property_change_args&> { };
	struct property_changed_e  : event<property_changed_e , object*, const property_change_args&> { };

	struct __declspec(novtable) notify_property_change
	{
		virtual property_changing_e::subscriber property_changing() = 0;
		virtual property_changed_e::subscriber property_changed() = 0;
	};

	// ========================================================================
	
	bool same_type (const object* obj1, const object* obj2);
	bool same_type (const concrete_type* type1, const concrete_type* type2);
	bool same_type (const char* type_name1, const char* type_name2);
}

template<typename to_t, typename from_t>
to_t checked_static_cast (from_t from)
{
	static_assert (std::is_pointer_v<to_t> || std::is_reference_v<to_t>);
	static_assert (std::is_pointer_v<from_t> || std::is_reference_v<from_t>);
	static_assert (std::is_polymorphic_v<std::conditional_t<std::is_pointer_v<to_t  >, std::remove_pointer_t<to_t  >, std::remove_reference_t<to_t  >>>);
	static_assert (std::is_polymorphic_v<std::conditional_t<std::is_pointer_v<from_t>, std::remove_pointer_t<from_t>, std::remove_reference_t<from_t>>>);

	to_t res = static_cast<to_t>(from);
	dassert(res == dynamic_cast<to_t>(from));
	return res;
}
