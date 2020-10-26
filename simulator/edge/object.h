
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "reflection.h"
#include "events.h"
#include <vector>
#include <array>
#include <tuple>

namespace edge
{
	class type
	{
	public:
		const type* const base_type;
		std::span<const property* const> const props;

		type (const type* base_type, std::span<const property* const> props) noexcept;
		virtual ~type() = default;
		std::vector<const property*> make_property_list() const;
		const property* find_property (const char* name) const;
		bool has_property (const property* p) const;
		bool is_same_or_derived_from (const type* t) const;
		bool is_same_or_derived_from (const type& t) const;

	private:
		void add_properties (std::vector<const property*>& properties) const;
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
		}

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
			return _factory(std::forward<typename factory_arg_property_traits::value_t>(std::get<I>(values))...);
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

	enum class collection_property_change_type { set, insert, remove };

	struct property_change_args
	{
		const edge::property* property;
		size_t index;
		collection_property_change_type type;

		property_change_args (const edge::property* property, size_t index, collection_property_change_type type)
			: property(property), index(index), type(type)
		{ }

		property_change_args (const value_property* property)
			: property(property)
		{ }

		property_change_args (const value_property& property)
			: property(&property)
		{ }

		property_change_args (const value_collection_property* property, size_t index, collection_property_change_type type)
			: property(property), index(index), type(type)
		{ }
	};

	struct property_changing_e : event<property_changing_e, object*, const property_change_args&> { };
	struct property_changed_e  : event<property_changed_e , object*, const property_change_args&> { };

	struct parent_i;
	// TODO: make event_manager a member var, possibly a pointer
	class object : public event_manager
	{
		parent_i* _parent = nullptr;

		friend parent_i;

	public:
		object() = default;

		// We explicitly define the copy constructors and copy-assignment operator cause we don't want _parent to be copied too.
		object(const object&) { }
		object& operator=(const object&) { return *this; }

		// The move constructor and move-assignment operator don't make sense for an ownable object.
		object(object&& other) = delete;
		object& operator=(object&& other) = delete;

		virtual ~object() = default;

		parent_i* parent() const { return _parent; }

		struct inserted_into_parent_e : public event<inserted_into_parent_e> { };
		struct removing_from_parent_e : public event<removing_from_parent_e> { };

		property_changing_e::subscriber property_changing() { return property_changing_e::subscriber(this); }
		property_changed_e::subscriber property_changed() { return property_changed_e::subscriber(this); }
		inserted_into_parent_e::subscriber inserted_into_parent() { return inserted_into_parent_e::subscriber(this); }
		removing_from_parent_e::subscriber removing_from_parent() { return removing_from_parent_e::subscriber(this); }

		virtual void walk_tree (const std::function<void(object*)>& f);

	protected:
		virtual void on_property_changing (const property_change_args&);
		virtual void on_property_changed (const property_change_args&);
		virtual void on_inserted_into_parent() { this->event_invoker<inserted_into_parent_e>()(); }
		virtual void on_removing_from_parent() { this->event_invoker<removing_from_parent_e>()(); }

		static const edge::type _type;
	public:
		virtual const edge::concrete_type* type() const = 0;
	};

	struct parent_i
	{
	protected:
		void set_this_as_parent(object* child);
		void call_inserted_into_parent(object* child);

		void call_removing_from_parent(object* child);
		void clear_this_as_parent(object* child);
	};
}
