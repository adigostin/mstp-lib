
#pragma once
#include "../rassert.h"
#include "enumerator.h"

namespace edge
{
	struct __declspec(novtable) hierarchy_root_i
	{
		virtual ~hierarchy_root_i() = default;
	};

	enum class walk_tree_order
	{
		pre_order, // Object is visited _before_ its descendants.
		post_order // Object is visited _after_ its descendants.
	};

	class hierarchy_object_i
	{
	public:
		virtual ~hierarchy_object_i() = default;

		virtual hierarchy_object_i* parent() const = 0;

		// This is meant to be overridden in derived classes that implement hierarchy_root_i, and to return "this".
		// Note that such a class is also responsible for calling on_inserting_into_hierarchy/on_inserted_into_hierarchy
		// from its constructor and on_removing_from_hierarchy/on_removed_from_hierarchy from its destructor; maybe
		// we should do some refactoring to remove this requirement which is easy to miss.
		virtual hierarchy_root_i* as_hierarchy_root() { return nullptr; }
		const hierarchy_root_i* as_hierarchy_root() const { return const_cast<hierarchy_object_i*>(this)->as_hierarchy_root(); }

	private:
		virtual enumerator_i<hierarchy_object_i*>* make_enumerator() = 0;
	public:
		// This is called by whoever inserts a sub-hierarchy in a rooted hierarchy, before the insertion, on every node of the sub-hierarchy.
		virtual void on_inserting_into_hierarchy (hierarchy_root_i* root) { }

		// This is called by whoever inserts a sub-hierarchy in a rooted hierarchy, after the insertion, on every node of the sub-hierarchy.
		virtual void on_inserted_into_hierarchy  (hierarchy_root_i* root) { }

		// This is called by whoever removes a sub-hierarchy from a rooted hierarchy, before the removal, on every node of the sub-hierarchy.
		virtual void on_removing_from_hierarchy  (hierarchy_root_i* root) { }

		// This is called by whoever removes a sub-hierarchy from a rooted hierarchy, after the removal, on every node of the sub-hierarchy.
		virtual void on_removed_from_hierarchy   (hierarchy_root_i* root) { }

		hierarchy_root_i* hierarchy_root()
		{
			auto current = this;
			while (true)
			{
				if (auto r = current->as_hierarchy_root())
					return r;
				auto parent = current->parent();
				if (!parent)
					return nullptr;
				current = parent;
			}
		}

		const hierarchy_root_i* hierarchy_root() const
		{
			return const_cast<hierarchy_object_i*>(this)->hierarchy_root();
		}

		template<typename func_t> requires std::is_invocable_v<func_t, hierarchy_object_i*>
		void enum_descendants (bool include_this, walk_tree_order order, const func_t& f)
		{
			if (include_this && (order == walk_tree_order::pre_order))
				f(this);

			auto children = edge::enumerable<hierarchy_object_i*, enumerator_i<hierarchy_object_i*>>(make_enumerator());
			for (auto c : children)
				c->enum_descendants(true, order, f);

			if (include_this && (order == walk_tree_order::post_order))
				f(this);
		}

		// Returns the canceled object, or nullptr if no object was canceled.
		template<typename func_t> requires std::is_invocable_v<func_t, hierarchy_object_i*, bool&>
		hierarchy_object_i* enum_descendants (bool include_this, walk_tree_order order, const func_t& f)
		{
			if (include_this && (order == walk_tree_order::pre_order))
			{
				bool cancel = false;
				f(this, cancel);
				if (cancel)
					return this;
			}

			auto children = edge::enumerable<hierarchy_object_i*, enumerator_i<hierarchy_object_i*>>(make_enumerator());
			for (auto c : children)
			{
				hierarchy_object_i* canceled_object = c->enum_descendants(true, order, f);
				if (canceled_object)
					return canceled_object;
			}

			if (include_this && (order == walk_tree_order::post_order))
			{
				bool cancel = false;
				f(this, cancel);
				if (cancel)
					return this;
			}

			return nullptr;
		}
	};
}
