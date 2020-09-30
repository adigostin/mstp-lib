
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "window.h"
#include "com_ptr.h"
#include "utility_functions.h"

namespace edge
{
	#pragma warning(disable: 4250) // disable "inherits via dominance" warning

	class d2d_window abstract : public window, public virtual d2d_window_i
	{
		using base = window;
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
		com_ptr<ID2D1Factory1> _d2d_factory;
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
		//virtual bool GetDebugFlag (debug_flag flag) const = 0;
		//virtual void SetDebugFlag (debug_flag flag, bool value) = 0;

		debug_flag _debug_flags = (debug_flag)0; //debug_flag::render_frame_durations_and_fps;
		com_ptr<IDWriteTextFormat> _debug_text_format;

	public:
		d2d_window (DWORD exStyle, DWORD style,
				   const RECT& rect, HWND hWndParent, int child_control_id,
				   ID3D11DeviceContext* d3d_dc, IDWriteFactory* dwrite_factory);

		ID3D11DeviceContext* d3d_dc() const { return _d3d_dc; }
		ID2D1Factory1* d2d_factory() const { return _d2d_factory; }

		// d2d_window_i
		virtual ID2D1DeviceContext* dc() const { return _d2d_dc; }
		virtual IDWriteFactory* dwrite_factory() const override { return _dwrite_factory; }
		virtual void show_caret (const D2D1_RECT_F& bounds, D2D1_COLOR_F color, const D2D1_MATRIX_3X2_F* transform = nullptr) override;
		virtual void hide_caret() override;

		float fps();
		float average_render_duration();

	protected:
		virtual std::optional<LRESULT> window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
		virtual void create_render_resources (ID2D1DeviceContext* dc) { }
		virtual void render (ID2D1DeviceContext* dc) const;
		virtual void release_render_resources (ID2D1DeviceContext* dc) { }

		virtual void d2d_dc_releasing() { }
		virtual void d2d_dc_recreated() { }

		virtual void on_size_changed (SIZE client_size_pixels, D2D1_SIZE_F client_size_dips) override;

	private:
		void invalidate_caret();
		static void CALLBACK on_blink_timer (HWND Arg1, UINT Arg2, UINT_PTR Arg3, DWORD Arg4);
		UINT_PTR timer_id_from_window() const;
		static d2d_window* window_from_timer_id (UINT_PTR);
		void create_d2d_dc();
		void release_d2d_dc();
		void process_wm_paint();
		void process_wm_set_focus();
		void process_wm_kill_focus();
	};
}
