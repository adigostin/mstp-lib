
#pragma once
#include "object.h"
#include "property_accessors.h"

namespace edge
{
	struct object_property : property
	{
		virtual const type* child_type() const = 0;
		virtual object* get (const object* obj) const = 0;
		virtual std::unique_ptr<object> set (object* obj, std::unique_ptr<object> value) const = 0;
	};

	struct object_property_change_args : property_change_args
	{
		using base = property_change_args;

		// property_changing - this is the object to insert.
		// property_changed  - this is the object removed.
		object* other_child;

		object_property_change_args (const object_property* prop, object* other_child)
			: property_change_args(prop), other_child(other_child)
		{ }
	};
	/*
	// This is an implementation of "object_property" useful for those objects that hold the child
	// in an std::unique_ptr (covers virtually all use cases).
	template<typename child_t, bool read_only = false>
	struct typed_object_property : object_property
	{
		using base = object_property;

		using backing_field_t = pointer_to_derived_member_var_t<std::unique_ptr<child_t>>;

		const char* const _name;
		const type* const _child_type;
		backing_field_t const _backing_field;

		typed_object_property (const char* name, const type* base_type, backing_field_t backing_field)
			: _name(name)
			, _child_type(base_type)
			, _backing_field(backing_field)
		{
			static_assert (std::is_base_of<object, child_t>::value);
		}

		virtual const char* name() const override final { return _name; }

		std::unique_ptr<child_t> set (object* obj, std::unique_ptr<child_t> new_value) const requires (read_only==false)
		{
			static_assert(std::is_base_of_v<object, child_t>);
			auto& backing_field = obj->*(_backing_field.mv);

			hierarchy_root_i* root = obj->hierarchy_root();

			obj->on_property_changing(object_property_change_args(this, new_value.get()));

			if (root)
			{
				// I determined the walk order to be the most useful if inner nodes such as bindings are
				// "fully visited" first (fully visited" meaning "inserting"+"inserted", or "removing"+"removed").
				// This means pre_order in the xxx-ing part and post_order in the xxx-ed part.

				if (backing_field)
					backing_field->enum_descendants (true, edge::walk_tree_order::pre_order, [root](hierarchy_object_i* n)
					{
						n->on_removing_from_hierarchy(root);
					});

				if (new_value)
					new_value->enum_descendants (true, walk_tree_order::pre_order, [root](hierarchy_object_i* n)
					{
						n->on_inserting_into_hierarchy(root);
					});
			}

			std::unique_ptr<child_t> res;
			if (backing_field)
			{
				backing_field->set_parent(nullptr);
				res = std::move(backing_field);
			}

			if (new_value)
			{
				backing_field = std::move(new_value);
				backing_field->set_parent(obj);
			}

			if (root)
			{
				if (res)
					res->enum_descendants (true, edge::walk_tree_order::post_order, [root](hierarchy_object_i* n)
					{
						n->on_removed_from_hierarchy(root);
					});

				if (backing_field)
					backing_field->enum_descendants (true, walk_tree_order::post_order, [root](hierarchy_object_i* n)
					{
						n->on_inserted_into_hierarchy(root);
					});
			}

			obj->on_property_changed(object_property_change_args(this, res.get()));

			return res;
		}

		std::unique_ptr<child_t> set (object* obj, nullptr_t) const
		{
			return set (obj, std::unique_ptr<child_t>());
		}

		virtual child_t* get (const object* obj) const override final
		{
			return (obj->*(_backing_field.mv)).get();
		}

		virtual std::unique_ptr<object> set (object* obj, std::unique_ptr<object> value) const override final
		{
			return set (obj, std::unique_ptr<child_t>(static_cast<child_t*>(value.release())));
		}

		virtual const type* child_type() const override { return _child_type; }
	};
	*/
	/*
	// Force an instantiation to catch errors early during compilation.
	namespace
	{
		class dummy_object : public object { };
		template struct typed_object_property<dummy_object>;
	}
	*/
}
