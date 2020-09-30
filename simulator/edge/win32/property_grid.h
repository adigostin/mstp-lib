
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "../object.h"
#include "text_editor.h"
#include "property_grid_items.h"

namespace edge
{
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

	struct __declspec(novtable) pg_visible_property_i
	{
		virtual bool pg_visible (std::span<const object* const> objects) const = 0;
	};

	struct __declspec(novtable) pg_hidden : pg_visible_property_i
	{
		virtual bool pg_visible (std::span<const object* const> objects) const override final { return false; }
	};

	struct __declspec(novtable) pg_editable_property_i
	{
		virtual bool pg_editable (std::span<const object* const> objects) const = 0;
	};

	struct __declspec(novtable) pg_custom_item_i
	{
		virtual std::unique_ptr<pgitem> create_item (group_item* parent, const property* prop) const = 0;
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
		virtual RECT rectp() const = 0;
		virtual D2D1_RECT_F rectd() const = 0;
		virtual void set_rect (const RECT& rectp) = 0;
		virtual void set_border_width (float bw) = 0;
		virtual float border_width() const = 0;
		virtual void on_dpi_changed() = 0;
		virtual void clear() = 0;
		virtual void add_section (const char* heading, std::span<object* const> objects) = 0;
		void add_section (const char* heading, object* obj) { add_section(heading, { &obj, 1 }); }
		virtual std::span<const std::unique_ptr<root_item>> sections() const = 0;
		virtual bool read_only() const = 0;
		virtual void render (ID2D1DeviceContext* dc) const = 0;
		virtual handled on_mouse_down (mouse_button button, modifier_key mks, POINT pp, D2D1_POINT_2F pd) = 0;
		virtual handled on_mouse_up   (mouse_button button, modifier_key mks, POINT pp, D2D1_POINT_2F pd) = 0;
		virtual void    on_mouse_move (modifier_key mks, POINT pp, D2D1_POINT_2F pd) = 0;
		virtual handled on_key_down (uint32_t vkey, modifier_key mks) = 0;
		virtual handled on_key_up (uint32_t vkey, modifier_key mks) = 0;
		virtual handled on_char_key (uint32_t ch) = 0;
		virtual HCURSOR cursor_at (POINT pp, D2D1_POINT_2F pd) const = 0;
		virtual D2D1_POINT_2F input_of (value_item* vi) const = 0;
		virtual D2D1_POINT_2F output_of (value_item* vi) const = 0;
		virtual value_item* find_item (const value_property* prop) const = 0;
		virtual bool editing_text() const = 0;

		root_item* single_section() const
		{
			auto sections = this->sections();
			assert(sections.size() == 1);
			return sections.front().get();
		}

		enum class htcode { none, input, expand, name, value, output };

		struct htresult
		{
			pgitem* item;
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
		virtual IDWriteFactory* dwrite_factory() const = 0;
		virtual IDWriteTextFormat* text_format() const = 0;
		virtual IDWriteTextFormat* bold_text_format() const = 0;
		virtual void invalidate() = 0;
		virtual text_editor_i* show_text_editor (const D2D1_RECT_F& rect, bool bold, float lr_padding, std::string_view str) = 0;
		virtual int show_enum_editor (D2D1_POINT_2F dip, const nvp* nvps) = 0;
		virtual void change_property (const std::vector<object*>& objects, const value_property* prop, std::string_view new_value_str) = 0;
		virtual float line_thickness() const = 0;
		virtual float name_column_x (size_t indent) const = 0;
		virtual float value_column_x() const = 0;
		float width() const { auto r = rectd(); return r.right - r.left; }
		float height() const { auto r = rectd(); return r.bottom - r.top; }
	};

	std::unique_ptr<property_grid_i> property_grid_factory (d2d_window_i* window, const RECT& rectp);
}
