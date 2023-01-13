
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "window.h"
#include "com_ptr.h"

namespace edge
{
	struct __declspec(novtable) d2d_renderer_i
	{
		virtual ~d2d_renderer_i() = default;

		enum debug_flag
		{
			render_frame_durations_and_fps = 1,
			render_update_rects = 2,
			full_clear = 4,
		};

		virtual win32_window_i& window() const = 0;
		virtual ID2D1DeviceContext* dc() const = 0;
		virtual ID2D1Factory1* d2d_factory() const = 0;
		virtual ID3D11DeviceContext* d3d_dc() const = 0;
		virtual IDWriteFactory* dwrite_factory() const = 0;
		// This can be called repeatedly, first to show the caret, and then to move it.
		virtual void show_caret (const D2D1_RECT_F& bounds, const D2D1_COLOR_F& color, const D2D1_MATRIX_3X2_F* transform = nullptr) = 0;
		virtual void hide_caret() = 0;
		virtual float fps() const = 0;
		virtual float average_render_duration() const = 0;
		virtual void set_debug_flag (debug_flag flag) = 0;
		virtual void clear_debug_flag (debug_flag flag) = 0;
		virtual debug_flag debug_flags() const = 0;

		struct before_render_e : event<before_render_e, ID2D1DeviceContext*> { };
		struct        render_e : event<       render_e, HWND, ID2D1DeviceContext*> { };
		struct  after_render_e : event< after_render_e, ID2D1DeviceContext*> { };
		struct dc_releasing_e : event<dc_releasing_e, ID2D1DeviceContext*> { };
		struct dc_recreated_e : event<dc_recreated_e, ID2D1DeviceContext*> { };

		virtual before_render_e::subscriber before_render() = 0;
		virtual render_e::subscriber render() = 0;
		virtual after_render_e::subscriber after_render() = 0;
		virtual dc_releasing_e::subscriber dc_releasing() = 0;
		virtual dc_recreated_e::subscriber dc_recreated() = 0;
	};

	std::unique_ptr<d2d_renderer_i> make_d2d_renderer (win32_window_i& window, ID3D11DeviceContext* d3d_dc, IDWriteFactory* dwrite_factory, ID2D1Factory1* d2d_factory);
}
