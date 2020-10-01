
#pragma once
#include "object.h"

namespace edge
{
	struct object_collection_property;

	struct object_collection_i : parent_i
	{
		virtual size_t child_count() const = 0;
		virtual object* child_at(size_t index) const = 0;
		virtual void reserve (size_t capacity) = 0;
		virtual void insert (size_t index, std::unique_ptr<object>&& child) = 0;
		void append (std::unique_ptr<object>&& child) { insert(child_count(), std::move(child)); }
		virtual const object_collection_property* collection_property() const = 0;
		virtual void call_property_changing (const property_change_args& args) = 0;
		virtual void call_property_changed  (const property_change_args& args) = 0;
	};

	template<typename child_t>
	struct typed_object_collection_property;

	template<typename child_t>
	struct typed_object_collection_i : object_collection_i
	{
	private:
		virtual void children_store (std::vector<std::unique_ptr<child_t>>** out) = 0;

		void children_store (const std::vector<std::unique_ptr<child_t>>** out) const
		{
			std::vector<std::unique_ptr<child_t>>* ncout;
			const_cast<typed_object_collection_i<child_t>*>(this)->children_store(&ncout);
			*out = ncout;
		}

		std::vector<std::unique_ptr<child_t>>& children_store()
		{
			std::vector<std::unique_ptr<child_t>>* children;
			this->children_store(&children);
			return *children;
		}

		const std::vector<std::unique_ptr<child_t>>& children_store() const
		{
			const std::vector<std::unique_ptr<child_t>>* children;
			this->children_store(&children);
			return *children;
		}

		virtual void collection_property (const typed_object_collection_property<child_t>** out) const = 0;

		virtual const object_collection_property* collection_property() const override final
		{
			const typed_object_collection_property<child_t>* prop;
			this->collection_property(&prop);
			return prop;
		}

		void set_this_as_parent(object* child) = delete;
		void call_inserted_into_parent(object* child) = delete;

		void call_removing_from_parent(object* child) = delete;
		void clear_this_as_parent(object* child) = delete;

	protected:
		virtual void on_child_inserted (size_t index, child_t* child) { }
		virtual void on_child_removing (size_t index, child_t* child) { }

	public:
		const std::vector<std::unique_ptr<child_t>>& children() const { return children_store(); }

		virtual size_t child_count() const override final { return children_store().size(); }

		virtual child_t* child_at (size_t index) const override final { return children_store()[index].get(); }

		child_t* last() const { return children_store().back().get(); }

		virtual void reserve (size_t capacity) override final
		{
			children_store().reserve(capacity);
		}

		virtual void insert (size_t index, std::unique_ptr<object>&& child) override final
		{
			auto typed_child_raw = static_cast<child_t*>(child.release());
			this->insert(index, std::unique_ptr<child_t>(typed_child_raw));
		}

		void insert (size_t index, std::unique_ptr<child_t>&& o)
		{
			static_assert (std::is_base_of_v<object, child_t>);

			auto& children = children_store();
			rassert (index <= children.size());
			child_t* raw = o.get();

			property_change_args args = { this->collection_property(), index, collection_property_change_type::insert };

			this->call_property_changing(args);
			children.insert (children.begin() + index, std::move(o));
			this->parent_i::set_this_as_parent(raw);
			this->call_property_changed(args);
			this->on_child_inserted (index, raw);
			this->parent_i::call_inserted_into_parent(raw);
		}

		void append (std::unique_ptr<child_t>&& o)
		{
			insert (children_store().size(), std::move(o));
		}

		std::unique_ptr<child_t> remove(size_t index)
		{
			static_assert (std::is_base_of_v<object, child_t>);

			auto& children = children_store();
			rassert (index < children.size());
			auto it = children.begin() + index;
			child_t* raw = (*it).get();

			property_change_args args = { this->collection_property(), index, collection_property_change_type::remove };

			this->parent_i::call_removing_from_parent(raw);
			this->on_child_removing (index, raw);
			this->call_property_changing(args);
			this->parent_i::clear_this_as_parent(raw);
			auto result = std::move (children[index]);
			children.erase (children.begin() + index);
			this->call_property_changed (args);

			return result;
		}

		std::unique_ptr<child_t> remove_last()
		{
			return remove(children_store().size() - 1);
		}

		std::unique_ptr<child_t> remove(child_t* child)
		{
			auto& children = this->children();
			for (size_t i = 0; i < children.size(); i++)
			{
				if (children[i].get() == child)
					return remove(i);
			}

			rassert(false); return nullptr;
		}

		size_t index_of (child_t* child) const
		{
			auto& children = this->children();
			for (size_t i = 0; i < children.size(); i++)
			{
				if (children[i].get() == child)
					return i;
			}

			rassert(false);
			return -1;
		}
	};

	// ========================================================================

	struct object_collection_property : property
	{
		using base = property;

		bool const preallocated;

		object_collection_property (const char* name, const property_group* group, const char* description, bool ui_visible, bool preallocated)
			: base(name, group, description, ui_visible), preallocated(preallocated)
		{ }

		virtual const object_collection_property* as_oc_prop() const { return this; }

		// Purpose of this is to let the object that holds the property to cast a pointer-to-itself
		// to a ponter to the collection represented by the property. This eliminates the need to do
		// a dynamic_cast, which in turn eliminated the need to include RTTI.
		virtual object_collection_i* collection_cast(object* obj) const = 0;

		const object_collection_i* collection_cast (const object* obj) const { return collection_cast(const_cast<object*>(obj)); }
	};

	template<typename child_t>
	struct typed_object_collection_property : object_collection_property
	{
		using base = object_collection_property;

		using collection_getter_t = typed_object_collection_i<child_t>*(*)(object* obj);

		collection_getter_t const _collection_getter;

		constexpr typed_object_collection_property (const char* name, const property_group* group, const char* description,
			bool ui_visible, bool preallocated, collection_getter_t collection_getter)
			: base (name, group, description, ui_visible, preallocated)
			, _collection_getter(collection_getter)
		{
			static_assert (std::is_base_of_v<object, child_t>);
		}

		virtual typed_object_collection_i<child_t>* collection_cast(object* obj) const override
		{
			return _collection_getter(obj);
		}

		const typed_object_collection_i<child_t>* collection_cast(const object* obj) const
		{
			return _collection_getter(const_cast<object*>(obj));
		}
	};
}
