
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "edge/text_editor.h"
#include "edge/text_layout.h"
#include "edge/om/value_collection_property.h"
#include "item.h"

namespace pg
{
	static constexpr float text_lr_padding = 3;
	static constexpr float title_lr_padding = 4;
	static constexpr float title_ud_padding = 2;

	struct property_editor_i
	{
		virtual ~property_editor_i() = default;
		virtual bool show (edge::win32_window_i* parent) = 0; // return IDOK, IDCANCEL, -1 (some error), 0 (hWndParent invalid or closed)
		virtual void cancel() = 0;
	};

	struct __declspec(novtable) pg_custom_editor_i
	{
		// Implementation must listen to the collection change events and
		// at least not crash when objects are removed from the list.
		// The caller is responsible to keep the list object alive until after this function returns.
		virtual std::unique_ptr<property_editor_i> create_editor (object_list_i& objects) const = 0;
	};

	// ---------------------------------------

	// Visual C++ seems to have a bug that causes it to generate an incorrect object layout for this class for constexpr variables.
	// See bug report at https://developercommunity.visualstudio.com/content/problem/974911/bad-code-gen-with-constexpr-variable-of-mi-class.html
	// The workaround is to make this type non-literal, thus force all variables to be non-constexpr and be initialized by constructor code
	// (rather than be initialized by the compiler at compile time and placed directly in linker sections, as it happens with constexpr)
	//
	// Note that they shouldn't be static inline either, or else we may run into another compiler bug, something about
	// bad thunks when taking the address of a virtual function during the initialization of a static inline var.
	//
	// So whenever you use this class, declare the static variable in the class, and define it outside the class.
	// TODO: rename to something less dumb.
	template<typename property_t, typename... interfaces_t>
	struct prop_wrapper : property_t, interfaces_t...
	{
		template<typename... args_t>
		prop_wrapper (args_t... args)
			: property_t(std::forward<args_t>(args)...)
		{ }
	};

	struct render_context
	{
		ID2D1DeviceContext* dc;

		edge::com_ptr<ID2D1SolidColorBrush> back;
		edge::com_ptr<ID2D1SolidColorBrush> fore;
		edge::com_ptr<ID2D1SolidColorBrush> border;
		edge::com_ptr<ID2D1SolidColorBrush> disabled_fore;
		edge::com_ptr<ID2D1SolidColorBrush> selected_back_focused;
		edge::com_ptr<ID2D1SolidColorBrush> selected_back_not_focused;
		edge::com_ptr<ID2D1SolidColorBrush> selected_fore;
		edge::com_ptr<ID2D1SolidColorBrush> root_item_back;
		edge::com_ptr<ID2D1SolidColorBrush> root_item_fore;
		edge::com_ptr<ID2D1SolidColorBrush> tooltip_back;
		edge::com_ptr<ID2D1SolidColorBrush> tooltip_fore;

		edge::com_ptr<ID2D1LinearGradientBrush> item_gradient_brush;
		edge::com_ptr<ID2D1LinearGradientBrush> item_gradient_brush_hot;
	};

	enum class htcode { none, expand, name, value, output };

	struct htresult
	{
		item_i* item;
		float   render_y;
		htcode  code;

		operator bool() const { return item != nullptr; }
	};

	struct __declspec(novtable) property_grid_i
	{
		virtual ~property_grid_i() = default;
		virtual edge::d2d_renderer_i* renderer() const = 0;
		virtual edge::win32_window_i& window() const = 0;
		virtual D2D1_RECT_F bounds() const = 0;
		virtual void set_bounds (const D2D1_RECT_F& rectp) = 0;
		virtual void set_border_width (float bw) = 0;
		
		// The caller is responsible to ensure that the "objects" parameter and all the objects it references
		// and all their descendants stay alive as long as the section is used by the property grid.
		// (A safer alternative would have been to hold everything in smart pointers with shared ownership
		// (std::shared_ptr, IUnknown etc.) That, however, would have introduced complex requirements for the application,
		// and would also introduce performance issues (think area-selection in a drawing with hundreds of lines,
		// the property grid tracking the selection, mouse moving at 60 fps).
		virtual void add_section (std::string_view heading, object_list_i& objects, edge::string_convert_context_i* scc) = 0;
		virtual std::span<const std::unique_ptr<root_item_i>> sections() const = 0;
		virtual void clear_sections() = 0;

		virtual void set_read_only (bool read_only) = 0;
		virtual bool read_only() const = 0;
		virtual D2D1_POINT_2F output_of (value_property_item_i* vi) const = 0;
		virtual value_property_item_i* find_item (const edge::value_property* prop) const = 0;
		virtual bool editing_text() const = 0;
		virtual void expand_all() = 0;
		struct creating_object_e : edge::cancelable_event<creating_object_e, std::unique_ptr<edge::object>, edge::object*, const edge::object_property*, const edge::concrete_type*> { };
		virtual creating_object_e::subscriber creating_object() = 0;

		virtual htresult hit_test (D2D1_POINT_2F pd) const = 0;

		struct property_edited_args
		{
			const edge::property* prop;
			const object_list_i& objects;
		};

		struct value_property_edited_args : property_edited_args
		{
			std::vector<std::string> old_values;
			std::string new_value;
			edge::string_convert_context_i* context;
		};

		struct object_property_edited_args : property_edited_args
		{
			std::vector<std::unique_ptr<edge::object>> old_values;
		};

		struct value_collection_property_edited_args : property_edited_args
		{
			size_t value_index;
			std::vector<std::string> old_values;
			std::string new_value;
			edge::string_convert_context_i* context;
		};

		struct property_edited_e : edge::event<property_edited_e, property_edited_args&&> { };
		virtual property_edited_e::subscriber property_changed() = 0;

		struct item_set_cursor_e : edge::cancelable_event<item_set_cursor_e, HCURSOR, const htresult&> { };
		virtual item_set_cursor_e::subscriber item_set_cursor() = 0;

		struct item_clicked_e : edge::cancelable_event<item_clicked_e, std::optional<LRESULT>, const htresult&> { };
		virtual item_clicked_e::subscriber item_clicked() = 0;

		// TODO: make these internal to property_grid.cpp / property_grid_items.cpp
		virtual IDWriteTextFormat* text_format() const = 0;
		virtual IDWriteTextFormat* bold_text_format() const = 0;
		virtual void invalidate() = 0;
		virtual void invalidate_item (item_i* i) = 0;
		virtual bool try_show_text_editor_on_selected_item (bool bold, std::string_view str) = 0;
		virtual int show_enum_editor (D2D1_POINT_2F dip, const edge::nvp* nvps) = 0;
		virtual void change_property (const object_list_i& objects, const edge::value_property* prop, std::string new_value_str, edge::string_convert_context_i* scc) = 0;
		virtual void change_property (const object_list_i& objects, const edge::object_property* prop, const edge::concrete_type* type) = 0;
		virtual void change_property (const object_list_i& objects, const edge::value_collection_property* prop, size_t value_index, std::string new_value_str, edge::string_convert_context_i* scc) = 0;
		virtual float line_width (uint32_t dpi) const = 0;
		virtual float expand_column_left (uint32_t dpi) const = 0;
		virtual float name_column_left (size_t indent) const = 0;
		virtual float value_column_left (uint32_t dpi) const = 0;
		virtual float value_column_right (uint32_t dpi) const = 0;
		virtual float indent_width() const = 0;
		virtual const edge::theme_color_provider_i* tcp() const = 0;
		virtual RECT calc_popup_window_pos (item_i* item, float item_y, D2D1_SIZE_F client_size_requested, DWORD style, DWORD ex_style) const = 0;
		virtual item_i* selected_item() const = 0;
	};

	using property_grid_factory_t = std::unique_ptr<property_grid_i>(edge::d2d_renderer_i* renderer, const D2D1_RECT_F& bounds, edge::theme_color_provider_i* tp);
	extern property_grid_factory_t* const property_grid_factory;
}
