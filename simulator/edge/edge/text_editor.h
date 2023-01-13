
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "com_ptr.h"
#include "d2d_renderer.h"
#include "edge.h"

namespace edge
{
	struct __declspec(novtable) text_editor_i
	{
		virtual ~text_editor_i() { }

		virtual std::wstring_view wstr() const = 0;
		virtual void select_all() = 0;
		virtual const D2D1_RECT_F& rect() const = 0;
		virtual handled on_mouse_down (const mouse_ud_args& ma) = 0;
		virtual handled on_mouse_up   (const mouse_ud_args& ma) = 0;
		virtual void    on_mouse_move (const mouse_move_args& ma) = 0;
		virtual handled on_key_down (uint32_t virtualKey, modifier_key mks) = 0;
		virtual handled on_key_up (uint32_t key, modifier_key mks) = 0;
		virtual handled on_char_key (uint32_t ch) = 0;
		virtual bool mouse_captured() const = 0;
	};

	using text_editor_factory_t = std::unique_ptr<text_editor_i>(d2d_renderer_i* renderer, IDWriteTextFormat* format, theme_color_provider_i* tcp, const D2D1_RECT_F& rect, float lr_padding, std::string_view text);
	extern text_editor_factory_t* const text_editor_factory;
}
