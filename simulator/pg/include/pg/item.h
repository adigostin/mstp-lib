#pragma once
#include "edge/events.h"
#include "edge/edge.h"
#include "edge/text_layout.h"
#include "edge/om/object_property.h"
#include "edge/om/object_collection_property.h"
#include "edge/om/value_collection_property.h"
#include "object_list.h"

namespace pg
{
	struct render_context;
	struct property_grid_i;
	struct root_item_i;
	struct property_item_i;
	struct expandable_item_i;
	struct collection_existing_child_item_i;
	struct collection_new_child_item_i;

	struct item_i
	{
		virtual ~item_i() = default;
		virtual expandable_item_i* parent() const = 0;
		virtual void perform_layout() = 0;
		virtual void render (const render_context& rc, float y, bool selected, bool hot, bool focused) const = 0;

		// Returns the height of the item's content, such as the height of the item's text layout.
		// The implementation must not pad this to align it to pixel edge. Alignment is done by the caller, only when needed.
		// If the implementation needs the item to be hidden, it must return zero from this function.
		virtual float content_height() const = 0;

		virtual HCURSOR cursor_at(D2D1_POINT_2F pd, float item_y) const = 0;
		virtual bool selectable() const = 0;
		virtual void on_mouse_down (const edge::mouse_ud_args& ma, float item_y) = 0;
		virtual void on_mouse_up   (const edge::mouse_ud_args& ma, float item_y) = 0;
		virtual std::string description_title() const = 0;
		virtual std::string description_text() const = 0;
		virtual root_item_i* as_root() { return nullptr; }
		struct item_removing_e : edge::event<item_removing_e, item_i*> { };
		virtual item_removing_e::subscriber item_removing() = 0;

		root_item_i* root();
		const root_item_i* root() const { return const_cast<item_i*>(this)->root(); }
		property_grid_i* grid() const;
		size_t indent() const;
		void render_default_background (const render_context& rc, float y, bool selected, bool hot, bool focused) const;
	};

	struct value_layout_t
	{
		enum class read_state { ok, multiple_values, read_exception };

		edge::text_layout_with_metrics tl;
		read_state state;
	};

	struct __declspec(novtable) expandable_item_i
	{
		virtual item_i* as_item() = 0;
		virtual size_t child_count() const = 0;
		virtual item_i* child_at(size_t index) const = 0;

		const item_i* as_item() const { return const_cast<expandable_item_i*>(this)->as_item(); }

		size_t index_of (const item_i* child) const 
		{
			for (size_t i = 0; i < child_count(); i++)
			{
				if (child_at(i) == child)
					return i;
			}

			rassert(false); return -1;
		}

		virtual bool expanded() const = 0;
		virtual void expand() = 0;
		virtual void collapse() = 0;

		void render_expand_button (const render_context& rc, float item_y) const;

		void expand_all();
	};

	struct object_item_i : expandable_item_i
	{
		virtual object_list_i& objects() = 0;
	};

	struct property_group
	{
		int32_t prio;
		const char* name;
	};

	struct ui_property_i
	{
		virtual const char* description() const = 0;
		virtual const property_group* group() const = 0;
		virtual bool ui_visible() const = 0;
	};

	struct group_item_i : item_i, expandable_item_i
	{
		virtual object_item_i* parent() const = 0;
		virtual const property_group* const group() const = 0;
		virtual std::span<std::unique_ptr<property_item_i> const> children() const = 0;
	};

	struct root_item_i : item_i, object_item_i, edge::hierarchy_root_i
	{
		virtual property_grid_i* grid() const = 0;
		virtual edge::string_convert_context_i* app_context() const = 0;
	};

	struct __declspec(novtable) property_item_i : item_i
	{
		virtual group_item_i* parent() const = 0;
		virtual const edge::property* property() const = 0;

		// These two functions are called from code in the object_item class, which listens to corresponding events.
		virtual void on_property_changing (size_t object_index, const edge::property_change_args& args) = 0;
		virtual void on_property_changed (size_t object_index, const edge::property_change_args& args) = 0;

		edge::text_layout_with_metrics make_name_layout() const;
	};

	// This interface is meant to be implemented by properties that need property grid items with more
	// functionality than that provided by the property grid project. An example could be a "color" property,
	// which would implement this interface to provide a property grid item with colored background.
	struct __declspec(novtable) custom_item_property_i
	{
		virtual std::unique_ptr<property_item_i> create_item (group_item_i* parent, const edge::property* prop) const = 0;
	};

	struct value_property_item_i : property_item_i
	{
		virtual const edge::value_property* property() const = 0;

		value_layout_t make_value_layout() const;
	};

	struct object_property_item_i : property_item_i, object_item_i
	{
		virtual const edge::object_property* property() const = 0;
	};

	struct collection_item_i : property_item_i, expandable_item_i
	{
		virtual const edge::collection_property* property() const = 0;
		virtual size_t collection_entry_count() const = 0;
		virtual collection_existing_child_item_i* collection_entry_at (size_t index) const = 0;
		virtual collection_new_child_item_i* collection_new_entry() const = 0;
	};

	struct value_collection_item_i : collection_item_i
	{
		virtual const edge::value_collection_property* property() const = 0;
	};

	struct object_collection_item_i : collection_item_i
	{
		virtual const edge::object_collection_property* property() const = 0;
	};

	struct collection_new_child_item_i : item_i
	{
		virtual collection_item_i* parent() const = 0;
	};

	struct collection_child_item_i : item_i
	{
	};

	struct collection_existing_child_item_i : collection_child_item_i
	{
		virtual size_t collection_entry_count() const = 0;
		virtual collection_existing_child_item_i* collection_entry_at (size_t index) const = 0;
		virtual collection_new_child_item_i* collection_new_entry() const = 0;
		virtual void on_property_setting (size_t object_index) = 0;
		virtual void on_property_set     (size_t object_index) = 0;
	};

	struct __declspec(novtable) value_collection_child_item_i : collection_existing_child_item_i
	{
		virtual value_collection_item_i* parent() const = 0;
	};

	edge::text_layout_with_metrics make_name_layout(property_item_i* item);
	value_layout_t make_value_layout(value_property_item_i* item);
}
