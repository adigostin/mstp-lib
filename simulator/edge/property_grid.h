
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "object.h"
#include "text_editor.h"
#include "text_layout.h"

namespace edge
{
	struct pgitem_i;
	struct expandable_item_i;
	struct group_item_i;
	struct object_item_i;
	struct root_item_i;
	struct value_property_item_i;

	struct property_editor_i
	{
		virtual ~property_editor_i() = default;
		virtual bool show (win32_window_i* parent) = 0; // return IDOK, IDCANCEL, -1 (some error), 0 (hWndParent invalid or closed)
		virtual void cancel() = 0;
	};

	struct __declspec(novtable) pg_custom_editor_i
	{
		virtual std::unique_ptr<property_editor_i> create_editor (std::span<object* const> objects) const = 0;
	};

	struct __declspec(novtable) pg_custom_item_i
	{
		virtual std::unique_ptr<pgitem_i> create_item (group_item_i* parent, const property* prop) const = 0;
	};

	struct __declspec(novtable) pg_bindable_property_i
	{
		virtual bool bound (const object* o) const = 0;
	};

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

	struct __declspec(novtable) property_grid_i
	{
		virtual ~property_grid_i() = default;
		virtual d2d_window_i* window() const = 0;
		virtual D2D1_RECT_F rectd() const = 0;
		virtual void set_bounds (const D2D1_RECT_F& rectp) = 0;
		virtual void set_border_width (float bw) = 0;
		virtual float border_width() const = 0;
		virtual void on_dpi_changed() = 0;
		virtual void clear() = 0;
		virtual void add_section (const char* heading, std::span<object* const> objects) = 0;
		void add_section (const char* heading, object* obj) { add_section(heading, { &obj, 1 }); }
		virtual std::span<const std::unique_ptr<root_item_i>> sections() const = 0;
		virtual bool read_only() const = 0;
		virtual void render (const d2d_render_args& ra) const = 0;
		virtual handled on_mouse_down (const mouse_ud_args& args) = 0;
		virtual handled on_mouse_up   (const mouse_ud_args& args) = 0;
		virtual void    on_mouse_move (const mouse_move_args& args) = 0;
		virtual handled on_key_down (uint32_t vkey, modifier_key mks) = 0;
		virtual handled on_key_up (uint32_t vkey, modifier_key mks) = 0;
		virtual handled on_char_key (uint32_t ch) = 0;
		virtual HCURSOR cursor_at (POINT pp, D2D1_POINT_2F pd) const = 0;
		virtual D2D1_POINT_2F input_of (value_property_item_i* vi) const = 0;
		virtual D2D1_POINT_2F output_of (value_property_item_i* vi) const = 0;
		virtual value_property_item_i* find_item (const value_property* prop) const = 0;
		virtual bool editing_text() const = 0;

		enum class htcode { none, input, expand, name, value, output };

		struct htresult
		{
			pgitem_i* item;
			float   y;
			htcode  code;

			operator bool() const { return item != nullptr; }
		};

		virtual htresult hit_test (D2D1_POINT_2F pd) const = 0;

		struct property_edited_args
		{
			const std::vector<object*>& objects;
			std::vector<std::string> old_values;
			std::string new_value;
		};

		struct property_edited_e : event<property_edited_e, property_edited_args&&> { };
		virtual property_edited_e::subscriber property_changed() = 0;

		// TODO: make these internal to property_grid.cpp / property_grid_items.cpp
		virtual IDWriteTextFormat* text_format() const = 0;
		virtual IDWriteTextFormat* bold_text_format() const = 0;
		virtual void invalidate() = 0;
		virtual text_editor_i* show_text_editor (const D2D1_RECT_F& rect, bool bold, float lr_padding, std::string_view str) = 0;
		virtual int show_enum_editor (D2D1_POINT_2F dip, const nvp* nvps) = 0;
		virtual void change_property (const std::vector<object*>& objects, const value_property* prop, std::string_view new_value_str) = 0;
		virtual float line_thickness() const = 0;
		virtual float name_column_x (size_t indent) const = 0;
		virtual float value_column_x() const = 0;
		virtual float indent_width() const = 0;

		float width() const { auto r = rectd(); return r.right - r.left; }
		float height() const { auto r = rectd(); return r.bottom - r.top; }
	};

	using property_grid_factory_t = std::unique_ptr<property_grid_i>(d2d_window_i* window, const D2D1_RECT_F& bounds);
	extern property_grid_factory_t* const property_grid_factory;


	struct pg_render_context
	{
		const d2d_render_args* ra;
		com_ptr<ID2D1SolidColorBrush> disabled_fore_brush;
		com_ptr<ID2D1SolidColorBrush> data_bind_fore_brush;
		com_ptr<ID2D1LinearGradientBrush> item_gradient_brush;
		com_ptr<ID2D1PathGeometry> triangle_geo;
	};

	struct __declspec(novtable) pgitem_i
	{
		virtual ~pgitem_i() = default;

		static constexpr float text_lr_padding = 3;
		static constexpr float title_lr_padding = 4;
		static constexpr float title_ud_padding = 2;

		virtual root_item_i* as_root() { return nullptr; }
		virtual expandable_item_i* parent() const = 0;
		virtual void perform_layout() = 0;
		virtual void render (const pg_render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const = 0;
		virtual float content_height() const = 0;
		virtual HCURSOR cursor_at(D2D1_POINT_2F pd, float item_y) const;
		virtual bool selectable() const = 0;
		virtual void on_mouse_down (const mouse_ud_args& ma, float item_y) = 0;
		virtual void on_mouse_up   (const mouse_ud_args& ma, float item_y) = 0;
		virtual std::string description_title() const = 0;
		virtual std::string description_text() const = 0;

		const root_item_i* as_root() const { return const_cast<pgitem_i*>(this)->as_root(); }
		root_item_i* root();
		const root_item_i* root() const { return const_cast<pgitem_i*>(this)->root(); }
		size_t indent() const;
		float content_height_aligned() const;
	};

	struct __declspec(novtable) expandable_item_i : pgitem_i
	{
		virtual size_t child_count() const = 0;
		virtual pgitem_i* child_at(size_t index) const = 0;
		virtual void expand() = 0;
		virtual void collapse() = 0;
		virtual bool expanded() const = 0;
	};

	struct __declspec(novtable) object_item_i : expandable_item_i
	{
		virtual const std::vector<object*>& objects() const = 0;
	};

	struct __declspec(novtable) root_item_i : object_item_i
	{
		virtual property_grid_i* grid() const = 0;
	};

	struct __declspec(novtable) group_item_i : expandable_item_i
	{
		virtual root_item_i* as_root() override final { return nullptr; }
		virtual object_item_i* parent() const = 0;
	};

	struct __declspec(novtable) value_item_i : pgitem_i
	{
		struct value_layout
		{
			text_layout_with_metrics tl;
			bool readable;
		};

		virtual root_item_i* as_root() override final { return nullptr; }
		virtual float content_height() const override final;
		virtual bool selectable() const override final { return true; }
		virtual void perform_layout() override final;

		virtual void perform_name_layout() = 0;
		virtual void perform_value_layout() = 0;
		virtual const text_layout_with_metrics& name() const = 0;
		virtual const value_layout& value() const = 0;
	};

	struct __declspec(novtable) value_property_item_i : value_item_i
	{
		virtual void render (const pg_render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const override final;
		virtual HCURSOR cursor_at(D2D1_POINT_2F pd, float item_y) const override final;
		virtual void on_mouse_down (const mouse_ud_args& ma, float item_y) override final;
		virtual void on_mouse_up   (const mouse_ud_args& ma, float item_y) override final;
		virtual std::string description_title() const override final;
		virtual std::string description_text() const override final;

		virtual group_item_i* parent() const = 0;
		virtual const value_property* property() const = 0;
		virtual void render_value (const pg_render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused, bool data_bound) const = 0;

		bool changed_from_default() const;
		bool multiple_values() const;
		bool can_edit() const;
		text_layout_with_metrics make_name_layout() const;
		value_layout make_value_layout() const;
	};

	struct __declspec(novtable) value_collection_entry_item_i : value_item_i
	{
	};

}
