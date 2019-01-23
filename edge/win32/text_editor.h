
#pragma once
#include "com_ptr.h"
#include "d2d_window.h"

namespace edge
{ 
	struct __declspec(novtable) text_editor_i
	{
		virtual ~text_editor_i() { }

		virtual void render (ID2D1DeviceContext* dc) const = 0;
		virtual std::string u8str() const = 0;
		virtual std::wstring_view wstr() const = 0;
		virtual void select_all() = 0;
		virtual const D2D1_RECT_F& rect() const = 0;
		virtual void process_mouse_button_down (mouse_button button, UINT modifier_keys, POINT pixel, D2D1_POINT_2F dip) = 0;
		virtual void process_mouse_button_up   (mouse_button button, UINT modifier_keys, POINT pixel, D2D1_POINT_2F dip) = 0;
		virtual void process_mouse_move (UINT modifier_keys, POINT pixel, D2D1_POINT_2F dip) = 0;
		virtual handled process_virtual_key_down (uint32_t virtualKey, UINT modifier_keys) = 0;
		virtual handled process_virtual_key_up (uint32_t key, UINT modifier_keys) = 0;
		virtual handled process_character_key (uint32_t ch) = 0;
	};

	using text_editor_factory_t = std::unique_ptr<text_editor_i>(d2d_window* control, IDWriteFactory* dwrite_factory, IDWriteTextFormat* format, uint32_t fill_argb, uint32_t text_argb, const D2D1_RECT_F& rect, std::string_view text);
	extern text_editor_factory_t* const text_editor_factory;
}
