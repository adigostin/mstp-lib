
#pragma once
#include "pg/item.h"

template<typename property_traits>
struct static_ui_prop : edge::static_value_property<property_traits>, pg::ui_property_i
{
	using base = edge::static_value_property<property_traits>;

	const pg::property_group* const _group;
	const char* const _description;
	bool const _ui_visible;

	static_ui_prop (const char* name, const pg::property_group* const group, const char* const description, bool ui_visible,
		base::getter_t getter, base::setter_t setter, std::optional<typename property_traits::value_t> default_value = std::nullopt)
		: base (name, getter, setter, std::move(default_value))
		, _group(group), _description(description), _ui_visible(ui_visible)
	{ }

	virtual const char* description() const override { return _description; }
	virtual const pg::property_group* group() const override { return _group; }
	virtual bool ui_visible() const override { return _ui_visible; }
};

using bool_p   = static_ui_prop<edge::bool_property_traits>;
using uint32_p = static_ui_prop<edge::uint32_property_traits>;
using size_p   = static_ui_prop<edge::size_t_property_traits>;
using float_p  = static_ui_prop<edge::float_property_traits>;
using string_p = static_ui_prop<edge::temp_string_property_traits>;
using side_p   = static_ui_prop<edge::side_property_traits>;
using edge::property_change_args;

namespace pg
{
	template<typename child_t>
	struct typed_object_collection_property : edge::typed_object_collection_property1<child_t>, ui_property_i
	{
		using base = edge::typed_object_collection_property1<child_t>;

		const pg::property_group* const _group;
		const char* const _description;

		typed_object_collection_property (const char* name, const pg::property_group* const group, const char* const description, 
			bool preallocated, base::backing_field_t backing_field)
			: base(name, preallocated, backing_field)
			, _group(group)
			, _description(description)
		{ }

		virtual const char* description() const override { return _description; }
		virtual const pg::property_group* group() const override { return _group; }
	};
}
