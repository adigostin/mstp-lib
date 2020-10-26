
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "win32_window_i.h"
#include "com_ptr.h"

namespace edge
{
	class d2d_renderer
	{
		event_manager _em;
		win32_window_i* const _window;
		bool _painting = false;
		bool _forceFullPresentation;
		com_ptr<IDWriteFactory> const _dwrite_factory;
		com_ptr<ID3D11Device1> _d3d_device;
		com_ptr<ID3D11DeviceContext> _d3d_dc;
		com_ptr<IDXGIDevice2> _dxgi_device;
		com_ptr<IDXGIAdapter> _dxgi_adapter;
		com_ptr<IDXGIFactory2> _dxgi_factory;
		com_ptr<IDXGISwapChain1> _swap_chain;
		com_ptr<ID2D1DeviceContext> _d2d_dc;
		bool _caret_on = false;
		bool _caret_blink_on = false;
		std::pair<D2D1_RECT_F, D2D1_MATRIX_3X2_F> _caret_bounds;
		D2D1_COLOR_F _caret_color;

		struct render_perf_info
		{
			LARGE_INTEGER start_time;
			float duration;
		};

		LARGE_INTEGER _performance_counter_frequency;
		std::deque<render_perf_info> perf_info_queue;

		enum class debug_flag
		{
			render_frame_durations_and_fps = 1,
			render_update_rects = 2,
			full_clear = 4,
		};

		debug_flag _debug_flags = (debug_flag)0; //debug_flag::render_frame_durations_and_fps;
		com_ptr<IDWriteTextFormat> _debug_text_format;

	public:
		d2d_renderer (win32_window_i* window, ID3D11DeviceContext* d3d_dc, IDWriteFactory* dwrite_factory);
		d2d_renderer(const d2d_renderer&) = delete;
		d2d_renderer& operator=(const d2d_renderer&) = delete;
		~d2d_renderer();

		win32_window_i* window() const { return _window; }
		ID2D1DeviceContext* dc() const { return _d2d_dc; }
		ID3D11DeviceContext* d3d_dc() const { return _d3d_dc; }
		IDWriteFactory* dwrite_factory() const { return _dwrite_factory; }
		void show_caret (const D2D1_RECT_F& bounds, const D2D1_COLOR_F& color, const D2D1_MATRIX_3X2_F* transform = nullptr);
		void hide_caret();
		float fps();
		float average_render_duration();

		struct before_render_e : event<before_render_e, ID2D1DeviceContext*> { };
		struct        render_e : event<       render_e, ID2D1DeviceContext*> { };
		struct  after_render_e : event< after_render_e, ID2D1DeviceContext*> { };
		struct dc_releasing_e : event<dc_releasing_e, ID2D1DeviceContext*> { };
		struct dc_recreated_e : event<dc_recreated_e, ID2D1DeviceContext*> { };

		before_render_e::subscriber before_render() { return before_render_e::subscriber(_em); }
		render_e::subscriber render() { return render_e::subscriber(_em); }
		after_render_e::subscriber after_render() { return after_render_e::subscriber(_em); }
		dc_releasing_e::subscriber dc_releasing() { return dc_releasing_e::subscriber(_em); }
		dc_recreated_e::subscriber dc_recreated() { return dc_recreated_e::subscriber(_em); }

	private:
		void render_no_handlers (ID2D1DeviceContext* dc) const;
		void invalidate_caret();
		static void CALLBACK on_blink_timer (HWND Arg1, UINT Arg2, UINT_PTR Arg3, DWORD Arg4);
		UINT_PTR timer_id_from_window() const;
		static d2d_renderer* window_from_timer_id (UINT_PTR);
		void create_d2d_dc();
		void release_d2d_dc();
		void process_wm_paint();
		void process_wm_set_focus();
		void process_wm_kill_focus();
		void process_wm_size (SIZE client_size_pixels, D2D1_SIZE_F client_size_dips);
		std::optional<LRESULT> on_window_proc (HWND, UINT msg, WPARAM wparam, LPARAM lparam);
	};
}
