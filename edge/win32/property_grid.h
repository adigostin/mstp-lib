
#pragma once
#include "../object.h"
#include "window.h"

namespace edge
{
	struct __declspec(novtable) property_grid_i : public virtual win32_window_i
	{
		virtual ~property_grid_i() { }
		virtual void set_title (std::string_view title) = 0;
		virtual void select_objects (object* const* objects, size_t size) = 0;

		struct property_changed_args
		{
			const std::vector<object*>& objects;
			std::vector<std::string> old_values;
			std::string new_value;
		};

		struct property_changed_e : event<property_changed_e, property_changed_args&&> { };
		virtual property_changed_e::subscriber property_changed() = 0;

		// TODO: make these internal to property_grid.cpp / property_grid_items.cpp
		virtual IDWriteFactory* dwrite_factory() const = 0;
		virtual IDWriteTextFormat* text_format() const = 0;
		virtual float value_text_width() const = 0;
		virtual void perform_layout() = 0;
		virtual void invalidate() = 0;
	};

	using property_grid_factory_t = std::unique_ptr<property_grid_i>(HINSTANCE hInstance, const RECT& rect, HWND hWndParent, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory);
	extern property_grid_factory_t* const property_grid_factory;
}
