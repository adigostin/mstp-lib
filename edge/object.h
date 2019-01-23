
#pragma once
#include "property_descriptor.h"
#include "events.h"

namespace edge
{
	struct type_t
	{
		const char* name;
		const type_t* base;
		view<const property* const> props;

	private:
		void add_properties (std::vector<const property*>& properties) const;

	public:
		std::vector<const property*> make_property_list() const;
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

	class object : public event_manager
	{
		template<typename child_type>
		friend struct collection_i;

	public:
		virtual ~object() = default;

		template<typename T>
		bool is() const { return dynamic_cast<const T*>(this) != nullptr; }

		struct property_changing_e : event<property_changing_e, object*, const property*> { };
		struct property_changed_e  : event<property_changed_e , object*, const property*> { };

		property_changing_e::subscriber property_changing() { return property_changing_e::subscriber(this); }
		property_changed_e::subscriber property_changed() { return property_changed_e::subscriber(this); }

		uint32_t tag() const { return 0; }
		void set_tag (uint32_t tag);

	protected:
		virtual void on_property_changing (const property* property);
		virtual void on_property_changed (const property* property);

	public:
		static const uint32_property _tag_property;
		static constexpr const property* const _properties[] = { &_tag_property };
		static constexpr type_t _type = { "object", nullptr, _properties };
		virtual const type_t* type() const { return &_type; }
	};
}
