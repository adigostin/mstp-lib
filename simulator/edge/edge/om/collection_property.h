#pragma once
#include "object.h"

namespace edge
{
	struct collection_property : property
	{
		virtual size_t size (const object* obj) const = 0;
		virtual void rotate (object* obj, size_t first, size_t n_first, size_t last) const = 0;
	};

	enum class collection_property_change_type { set, insert, remove, rotate };

	struct collection_property_change_args : property_change_args
	{
		using base = property_change_args;

		union
		{
			size_t const index; // -1 is used with type=set and signifies that all elements are being set
			struct
			{
				size_t const first;
				size_t const n_first;
				size_t const last;
			}; // these have the same meaning as the parameters of std::rotate
		};

		collection_property_change_type const type;

	protected:
		collection_property_change_args (const collection_property* property, size_t index, collection_property_change_type type)
			: base(property), index(index), type(type)
		{
			rassert ((type == collection_property_change_type::insert)
				|| (type == collection_property_change_type::set)
				|| (type == collection_property_change_type::remove));
		}

		collection_property_change_args (const collection_property* property, size_t first, size_t n_first, size_t last)
			: base(property), first(first), n_first(n_first), last(last), type(collection_property_change_type::rotate)
		{ }

	public:
		const collection_property* property() const { return static_cast<const collection_property*>(base::property); }
	};
}
