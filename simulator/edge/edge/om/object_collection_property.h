
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "collection_property.h"
#include "property_accessors.h"

namespace edge
{
	struct object_collection_property : collection_property
	{
		virtual bool preallocated() const = 0;
		virtual object* at (const object* obj, size_t index) const = 0;
		virtual void insert (object* obj, size_t index, std::unique_ptr<object> child) const = 0;
		virtual std::unique_ptr<object> remove (object* obj, size_t index) const = 0;

		void append (object* obj, std::unique_ptr<object> child) const { insert (obj, size(obj), std::move(child)); }

		std::unique_ptr<object> remove_back (object* obj) const { return remove (obj, this->size(obj) - 1); }
		
		size_t index_of (const object* obj, const object* child) const
		{
			for (size_t i = 0; i < this->size(obj); i++)
			{
				if (at(obj, i) == child)
					return i;
			}

			rassert(false); return -1;
		}

		std::unique_ptr<object> remove (object* obj, object* obj_to_remove) const { return remove (obj, index_of(obj, obj_to_remove)); }
	};

	struct object_collection_property_change_args : collection_property_change_args
	{
		using base = collection_property_change_args;
		
		// inserting - these are the children to be inserted
		// inserted  - unused
		// removing  - unused
		// removed   - these are the removed children
		// setting   - these are the children to replace the existing ones
		// set       - these are the replaced children
		// rotating  - unused
		// rotated   - unused
		object* child;

		object_collection_property_change_args (const object_collection_property* property, size_t index, collection_property_change_type type, object* child)
			: base(property, index, type), child(child)
		{ }

		object_collection_property_change_args (const object_collection_property* property, size_t first, size_t n_first, size_t last)
			: base(property, first, n_first, last), child(nullptr)
		{ }

		const object_collection_property* property() const { return static_cast<const object_collection_property*>(base::property()); }
	};
	/*
	template<typename child_t>
	struct typed_object_collection_property : object_collection_property
	{
		using base = object_collection_property;

		using backing_field_t = pointer_to_derived_member_var_t<std::vector<std::unique_ptr<child_t>>>;

		const char* const _name;
		backing_field_t const _backing_field;

		constexpr typed_object_collection_property (const char* name, bool preallocated, backing_field_t backing_field)
			: base (preallocated)
			, _name(name)
			, _backing_field(backing_field)
		{
			static_assert (std::is_base_of_v<object, child_t>);
		}

		virtual const char* name() const override final { return _name; }

		virtual size_t size (const object* obj) const override final
		{
			return (obj->*(_backing_field.mv)).size();
		}

		virtual child_t* at (const object* obj, size_t index) const override final
		{
			return (obj->*(_backing_field.mv))[index].get();
		}

		child_t* back (const object* obj) const
		{
			auto& children = obj->*(_backing_field.bf);
			return children.back().get();
		}

		void insert (object* obj, size_t index, std::unique_ptr<child_t> child) const
		{
			rassert(!child->parent());
			std::vector<std::unique_ptr<child_t>>& store = obj->*(_backing_field.mv);
			rassert(index <= store.size());

			object_collection_property_change_args args (this, index, collection_property_change_type::insert, child.get());
			obj->on_property_changing(args);

			hierarchy_root_i* root = obj->hierarchy_root();
			if (root)
			{
				// I determined the walk order to be the most useful if inner nodes such as bindings are
				// "fully visited" first (fully visited" meaning "inserting"+"inserted", or "removing"+"removed").
				// This means pre_order in the xxx-ing part and post_order in the xxx-ed part.
				child->enum_descendants (true, walk_tree_order::pre_order, [root](hierarchy_object_i* n)
				{
					n->on_inserting_into_hierarchy(root);
				});
			}

			store.insert (store.begin() + index, nullptr);
			store[index] = std::move(child);
			store[index]->set_parent(obj);

			if (root)
			{
				store[index]->enum_descendants (true, walk_tree_order::post_order, [root](hierarchy_object_i* n)
				{
					n->on_inserted_into_hierarchy(root);
				});
			}

			args.child = nullptr;
			obj->on_property_changed(args);
		}

		virtual void insert (object* obj, size_t index, std::unique_ptr<object> child) const override final
		{
			auto child_obj = child.release();
			auto typed_child = static_cast<child_t*>(child_obj);
			rassert (typed_child == dynamic_cast<child_t*>(child_obj));
			insert (obj, index, std::unique_ptr<child_t>(typed_child));
		}

		// TODO: rename to push_back
		void append (object* obj, std::unique_ptr<child_t> child) const { insert (obj, size(obj), std::move(child)); }

		virtual std::unique_ptr<object> remove (object* obj, size_t index) const override final
		{
			std::vector<std::unique_ptr<child_t>>& store = obj->*(_backing_field.mv);
			rassert (store[index]->parent() == obj);

			object_collection_property_change_args args (this, index, collection_property_change_type::remove, nullptr);
			obj->on_property_changing(args);

			hierarchy_root_i* root = obj->hierarchy_root();
			if (root)
			{
				store[index]->enum_descendants (true, edge::walk_tree_order::pre_order, [root](hierarchy_object_i* n)
					{
						n->on_removing_from_hierarchy(root);
					});
			}

			store[index]->set_parent(nullptr);
			auto result = std::move(store[index]);
			store.erase(store.begin() + index);

			if (root)
			{
				result->enum_descendants (true, edge::walk_tree_order::post_order, [root](hierarchy_object_i* n)
					{
						n->on_removed_from_hierarchy(root);
					});
			}

			args.child = result.get();
			obj->on_property_changed(args);

			return result;
		}

		virtual void rotate (object* obj, size_t first, size_t n_first, size_t last) const override final
		{
			auto& children = obj->*(_backing_field.mv);
			object_collection_property_change_args args (this, first, n_first, last);

			rassert(false);
			//obj->on_property_changing(args);
			//std::rotate (children.begin() + first, children.begin() + n_first, children.begin() + last);
			//obj->on_property_changed(args);
		}

		size_t index_of (const object* obj, const child_t* child) const
		{
			auto& children = obj->*(_backing_field.mv);
			for (size_t i = 0; i < children.size(); i++)
			{
				if (children[i].get() == child)
					return i;
			}

			rassert(false);
			return -1;
		}

		auto& children (const object* obj) const
		{
			return obj->*(_backing_field.mv);
		}

		using base::remove;
		using base::index_of;
	};
	*/
	template<typename child_t>
	struct typed_object_collection_property1 : object_collection_property
	{
		using static_size_getter_t = size_t(*)(const object* obj);
		using member_size_getter_t = pointer_to_derived_member_function_t<size_t, true>;
		using size_getter_t = std::variant<static_size_getter_t, member_size_getter_t>;

		using static_getter_t = child_t*(*)(const object* obj, size_t index);
		using member_getter_t = pointer_to_derived_member_function_t<child_t*, true, size_t>;
		using getter_t = std::variant<static_getter_t, member_getter_t>;

		using inserter_t = pointer_to_derived_member_function_t<void, false, size_t, std::unique_ptr<child_t>>;

		using remover_t = pointer_to_derived_member_function_t<std::unique_ptr<child_t>, false, size_t>;

		const char*    const _name;
		size_getter_t  const _size_getter;
		getter_t       const _getter;
		inserter_t     const _inserter;
		remover_t      const _remover;

		typed_object_collection_property1(const char* name, size_getter_t size_getter, getter_t getter, inserter_t inserter, remover_t remover)
			: _name(name), _size_getter(size_getter), _getter(getter), _inserter(inserter), _remover(remover)
		{ }

		virtual const char* name() const override { return _name; }
		
		virtual bool preallocated() const override
		{
			rassert(false); return { }; // TODO
		}

		virtual size_t size (const object* obj) const override
		{
			if (std::holds_alternative<static_size_getter_t>(_size_getter))
				return std::get<static_size_getter_t>(_size_getter)(obj);
			else
				return (obj->*std::get<member_size_getter_t>(_size_getter))();
		}

		virtual void rotate (object* obj, size_t first, size_t n_first, size_t last) const override
		{
			rassert(false); // TODO
		}

		virtual object* at (const object* obj, size_t index) const override
		{
			if (std::holds_alternative<static_getter_t>(_getter))
				return std::get<static_getter_t>(_getter)(obj, index);
			else
				return (obj->* std::get<member_getter_t>(_getter))(index);
		}

		virtual void insert (object* obj, size_t index, std::unique_ptr<object> child) const override
		{
			auto typed_child = checked_static_cast<child_t*>(child.get());
			child.release();
			(obj->*_inserter)(index, std::unique_ptr<child_t>(typed_child));
		}

		virtual std::unique_ptr<object> remove (object* obj, size_t index) const override
		{
			auto removed = (obj->*_remover)(index);
			return std::unique_ptr<object>(removed.release());
		}

		using object_collection_property::remove;
		using object_collection_property::index_of;
	};
}
