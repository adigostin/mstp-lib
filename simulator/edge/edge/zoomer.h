
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "d2d_renderer.h"

namespace edge
{
	struct zoom_transform_changed_e : event<zoom_transform_changed_e> { };

	// TODO: make this independent of d2d_renderer. All the zoomer needs is hooking WM_PAINT.
	class zoomer : event_manager
	{
		d2d_renderer_i* const _renderer;

		D2D1_POINT_2F _aimpoint = { 0, 0 }; // workspace coordinate shown at the center of the client area
		float _zoom = 1;
		float _minDistanceBetweenGridPoints = 15;
		float _minDistanceBetweenGridLines = 40;
		bool _enableUserZoomingAndPanning = true;
		bool _panning = false;
		D2D1_POINT_2F _panningLastMouseLocation;

		struct smooth_zoom_info
		{
			LARGE_INTEGER begin_time;
			float         begin_zoom;
			D2D1_POINT_2F begin_aimpoint;
			float         end_zoom;
			D2D1_POINT_2F end_aimpoint;
		};
		std::optional<smooth_zoom_info> _smooth_zoom_info;

		struct zoomed_to_rect
		{
			D2D1_RECT_F rect;
			float min_margin;
			float min_zoom;
			float max_zoom;
		};
		std::optional<zoomed_to_rect> _zoomed_to_rect;

	public:
		zoomer(d2d_renderer_i* renderer);
		zoomer(const zoomer&) = delete;
		zoomer& operator=(const zoomer&) = delete;
		~zoomer();

		d2d_renderer_i* renderer() const { return _renderer; }
		void zoom_to (D2D1_POINT_2F aimpoint, float zoom, bool smooth);
		D2D1_POINT_2F aimpoint() const { return _aimpoint; }
		float zoom() const { return _zoom; }
		zoom_transform_changed_e::subscriber zoom_transform_changed() { return zoom_transform_changed_e::subscriber(this); }
		void zoom_to (const D2D1_RECT_F& rect, float min_margin, float min_zoom, float max_zoom, bool smooth);

	private:
		std::optional<LRESULT> on_window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
		void create_render_resources (ID2D1DeviceContext* dc);
		void release_render_resources (ID2D1DeviceContext* dc);
		static void on_before_render (void* arg, ID2D1DeviceContext* dc) { static_cast<zoomer*>(arg)->create_render_resources(dc); }
		static void on_after_render (void* arg, ID2D1DeviceContext* dc) { static_cast<zoomer*>(arg)->release_render_resources(dc); }
		void set_zoom_and_aimpoint_internal (float zoom, D2D1_POINT_2F aimpoint, bool smooth);
		void process_wm_size        (HWND hwnd, WPARAM wparam, LPARAM lparam);
		void process_wm_mbuttondown (HWND hwnd, WPARAM wparam, LPARAM lparam);
		void process_wm_mbuttonup   (HWND hwnd, WPARAM wparam, LPARAM lparam);
		void process_wm_mousewheel  (HWND hwnd, WPARAM wparam, LPARAM lparam);
		void process_wm_mousemove   (HWND hwnd, WPARAM wparam, LPARAM lparam);

	public:
		D2D1_POINT_2F pointd_to_pointw (D2D1_POINT_2F dlocation) const;
		void pointw_to_pointd (std::span<D2D1_POINT_2F> locations) const;
		float lengthw_to_lengthd (float lengthw) const { return lengthw * _zoom; }
		D2D1_SIZE_F pixel_aligned_window_center() const;
		D2D1_POINT_2F pointw_to_pointd (float x, float y) const;
		D2D1_POINT_2F pointw_to_pointd (D2D1_POINT_2F location) const;
		D2D1_RECT_F rectw_to_rectd (const D2D1_RECT_F& r) const;
		D2D1::Matrix3x2F zoom_transform() const;
	};
}
