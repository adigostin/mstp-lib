
#pragma once
#include "reflection.h"
#include "events.h"
#include "span.hpp"

namespace edge
{
	using tcb::span;

	struct type
	{
		static std::vector<const type*>* known_types;

		const char* const name;
		const type* const base_type;
		span<const property* const> const props;

		type(const char* name, const type* base_type, span<const property* const> props);;
		~type();

		static const type* find_type (const char* name);

		std::vector<const property*> make_property_list() const;
		const property* find_property (const char* name) const;

		virtual bool has_factory() const = 0;
		virtual span<const value_property* const> factory_props() const = 0;
		virtual object* create (const span<std::string_view>& string_values) const = 0;
	private:
		void add_properties (std::vector<const property*>& properties) const;
	};

	template<typename object_type, typename... factory_arg_props>
	class xtype : public type
	{
		static constexpr size_t parameter_count = sizeof...(factory_arg_props);

		static_assert (std::conjunction_v<std::is_base_of<value_property, factory_arg_props>...>, "factory params must derive from value_property");
		static_assert (std::is_convertible_v<object_type*, object*>);

		using factory_t = object_type*(*const)(typename factory_arg_props::param_t... factory_args);
		
		factory_t const _factory;
		std::array<const value_property*, parameter_count> const _factory_props;

	public:
		xtype (const char* name, const type* base, span<const property* const> props,
			factory_t factory, const factory_arg_props*... factory_props)
			: type(name, base, props)
			, _factory(factory)
			, _factory_props(std::array<const value_property*, parameter_count>{ factory_props... })
		{ }

	private:
		virtual bool has_factory() const override { return _factory != nullptr; }

		virtual span<const value_property* const> factory_props() const override { return _factory_props; }
		
		template<std::size_t... I>
		static std::tuple<typename factory_arg_props::value_t...> strings_to_values (const span<std::string_view>& string_values, std::index_sequence<I...>)
		{
			std::tuple<typename factory_arg_props::value_t...> result;
			bool cast_ok = (true && ... && factory_arg_props::from_string(string_values[I], std::get<I>(result)));
			assert(cast_ok);
			return result;
		}

		virtual object* create (const span<std::string_view>& string_values) const override
		{
			assert (_factory != nullptr);
			assert (string_values.size() == parameter_count);
			auto values = strings_to_values (string_values, std::make_index_sequence<parameter_count>());
			object_type* obj = std::apply (_factory, values);
			return static_cast<object*>(obj);
		}
	};

	class object;

	template<typename child_t>
	struct __declspec(novtable) collection_i
	{
	private:
		virtual std::vector<std::unique_ptr<child_t>>& children_store() = 0;
		virtual object* as_object() = 0;

	protected:
		virtual void on_child_inserted (size_t index, child_t* child)
		{
			this->as_object()->event_invoker<child_inserted_event>()(this, index, child);
		}

		virtual void on_child_removing (size_t index, child_t* child)
		{
			this->as_object()->event_invoker<child_removing_event>()(this, index, child);
		}

	public:
		struct child_inserted_event : public event<child_inserted_event, collection_i*, size_t, child_t*> { };
		struct child_removing_event : public event<child_removing_event, collection_i*, size_t, child_t*> { };

		const std::vector<std::unique_ptr<child_t>>& children() const
		{
			return const_cast<collection_i*>(this)->children_store();
		}

		void insert (size_t index, std::unique_ptr<child_t>&& o)
		{
			auto& children = children_store();
			assert (index <= children.size());
			child_t* raw = o.get();
			children.insert (children.begin() + index, std::move(o));
			assert (raw->_parent == nullptr);
			raw->_parent = this;
			this->on_child_inserted (index, raw);
		}

		std::unique_ptr<child_t> remove(size_t index)
		{
			auto& children = children_store();
			assert (index < children.size());
			child_t* raw = children[index].get();
			this->on_child_removing (index, raw);
			assert (raw->_parent == this->as_object());
			raw->_parent = nullptr;
			auto result = std::move (children[index]);
			children.erase (children.begin() + index);
			return result;
		}

		typename child_inserted_event::subscriber get_inserted_event()
		{
			return collection_i::object_inserted_event::subscriber(this->as_object());
		}

		typename child_removing_event::subscriber get_removing_event()
		{
			return collection_i::object_removing_event::subscriber(this->as_object());
		}
	};

	enum class collection_property_change_type { set, insert, remove };

	struct property_change_args
	{
		const struct property* property;
		size_t index;
		collection_property_change_type type;

		property_change_args (const value_property* property)
			: property(property)
		{ }

		property_change_args (const value_property& property)
			: property(&property)
		{ }

		property_change_args (const value_collection_property* property, size_t index, collection_property_change_type type)
			: property(property), index(index), type(type)
		{ }

		property_change_args (const object_collection_property* property, size_t index, collection_property_change_type type)
			: property(property), index(index), type(type)
		{ }
	};

	struct property_changing_e : event<property_changing_e, object*, const property_change_args&> { };
	struct property_changed_e  : event<property_changed_e , object*, const property_change_args&> { };

	class object : public event_manager
	{
		template<typename child_type>
		friend struct collection_i;

	public:
		virtual ~object() = default;

		template<typename T>
		bool is() const { return dynamic_cast<const T*>(this) != nullptr; }

		property_changing_e::subscriber property_changing() { return property_changing_e::subscriber(this); }
		property_changed_e::subscriber property_changed() { return property_changed_e::subscriber(this); }

	protected:
		virtual void on_property_changing (const property_change_args&);
		virtual void on_property_changed (const property_change_args&);

	public:
		static const xtype<object> _type;
		virtual const type* type() const { return &_type; }
	};
}
