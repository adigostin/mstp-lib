
#pragma once
#include "../object.h"
#include "window.h"
#include "text_editor.h"

namespace edge
{
	struct __declspec(novtable) property_grid_i : public win32_window_i, public property_editor_parent_i
	{
		virtual ~property_grid_i() { }
		virtual void clear() = 0;
		virtual void add_section (const char* heading, object* const* objects, size_t size) = 0;
		virtual void set_description_height (float height) = 0;

		struct property_changed_args
		{
			const std::vector<object*>& objects;
			std::vector<std::string> old_values;
			std::string new_value;
		};

		// TODO: rename to property_edited_e
		struct property_changed_e : event<property_changed_e, property_changed_args&&> { };
		virtual property_changed_e::subscriber property_changed() = 0;

		struct description_height_changed_e : event<description_height_changed_e, float> { };
		virtual description_height_changed_e::subscriber description_height_changed() = 0;

		// TODO: make these internal to property_grid.cpp / property_grid_items.cpp
		virtual IDWriteFactory* dwrite_factory() const = 0;
		virtual IDWriteTextFormat* text_format() const = 0;
		virtual void invalidate() = 0;
		virtual text_editor_i* show_text_editor (const D2D1_RECT_F& rect, float lr_padding, std::string_view str) = 0;
		virtual int show_enum_editor (D2D1_POINT_2F dip, const NVP* nvps) = 0;
		virtual bool try_change_property (const std::vector<object*>& objects, const value_property* prop, std::string_view new_value_str) = 0;
		virtual float line_thickness() const = 0;
	};

	using property_grid_factory_t = std::unique_ptr<property_grid_i>(HINSTANCE hInstance, DWORD exStyle, const RECT& rect, HWND hWndParent, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory);
	extern property_grid_factory_t* const property_grid_factory;
}
