
#pragma once
#include "om/object.h"

namespace edge
{
	struct object_reference_i;

	// Implemented by objects that hold references to other objects, for example
	// by an "action" object that holds references to one or more targets of the action.
	//
	// Useful, for example, to the GUI code that deletes a random node (a sub-hierarchy) from a project (a hierachy):
	// the GUI code will enumerate all removed nodes to see if there are references between the sub-hierarchy
	// and the remaining hierarchy, and will remove these references before removing the sub-hierarchy; this operation
	// can later be undone by adding back the sub-hierarchy and then the removed references.
	struct __declspec(novtable) reference_holder_i
	{
		virtual size_t reference_count() const = 0;
		virtual object_reference_i* reference_at (size_t index) const = 0;
		virtual std::unique_ptr<object_reference_i> remove_reference (size_t index) = 0;
		virtual void insert_reference (size_t index, std::unique_ptr<object_reference_i> ref) = 0;

		size_t index_of_reference (object_reference_i* ref) const
		{
			for (size_t i = 0; i < reference_count(); i++)
			{
				if (reference_at(i) == ref)
					return i;
			}

			_ASSERT(false); return -1;
		}
	};

	// This is a reference held by reference_holder_i.
	struct __declspec(novtable) object_reference_i
	{
		// Destructor is needed here in order to allow code to hold objects that implement this
		// interface in a unique_ptr. Example of code that does this is make_delete_elements_action().
		virtual ~object_reference_i() = default;

		// Returns the parent that holds this reference. References are removed from it
		// when the user deletes things in the editor, and added back when the user does Undo.
		virtual reference_holder_i* reference_holder() const = 0;
	};

	// This is the target of a reference.
	struct referenced_object_i
	{
		virtual void register_reference (object_reference_i* ref) = 0;
		virtual void unregister_reference (object_reference_i* ref) = 0;
		virtual std::vector<object_reference_i*> get_references_into() const = 0;
		virtual const static_value_property<uint32_property_traits>* get_id_prop() const = 0;
		virtual object* as_object() = 0;
		const object* as_object() const { return const_cast<referenced_object_i*>(this)->as_object(); }
		uint32_t ref_id() const { return get_id_prop()->get(this->as_object()); }
		uint32_t set_ref_id (uint32_t value) { return get_id_prop()->set(value, this->as_object()); }
	};

	struct reference_root_i : hierarchy_root_i
	{
		virtual referenced_object_i* find_referenced_node (object* search_from, uint32_t id) const = 0;
	};
}
