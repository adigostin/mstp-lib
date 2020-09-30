
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "zoomable_window.h"

using namespace std;
using namespace D2D1;

namespace edge
{
	void zoomable_window::create_render_resources (ID2D1DeviceContext* dc)
	{
		base::create_render_resources(dc);

		if (_smooth_zoom_info)
		{
			auto& szi = _smooth_zoom_info.value();

			// zoom in progress
			LARGE_INTEGER timeNow;
			BOOL bRes = QueryPerformanceCounter(&timeNow); assert(bRes);

			LARGE_INTEGER frequency;
			bRes = QueryPerformanceFrequency(&frequency); assert(bRes);

			float ellapsedMilliseconds = (float)(((double)timeNow.QuadPart - (double)szi.begin_time.QuadPart) * 1000 / frequency.QuadPart);
			if (ellapsedMilliseconds == 0)
			{
				// The WM_PAINT message came too fast, so the zoom hasn't changed yet.
				// Let's keep invalidating in EndRender() so that Windows will keep sending us WM_PAINT messages.
			}
			else
			{
				static constexpr float duration_milliseconds = 150;

				float new_zoom;
				D2D1_POINT_2F new_aimpoint;
				if (ellapsedMilliseconds < duration_milliseconds)
				{
					new_zoom = 1 / (1 / szi.begin_zoom + ellapsedMilliseconds * (1 / szi.end_zoom - 1 / szi.begin_zoom) / duration_milliseconds);
					new_aimpoint.x = szi.begin_aimpoint.x + ellapsedMilliseconds * (szi.end_aimpoint.x - szi.begin_aimpoint.x) / duration_milliseconds;
					new_aimpoint.y = szi.begin_aimpoint.y + ellapsedMilliseconds * (szi.end_aimpoint.y - szi.begin_aimpoint.y) / duration_milliseconds;
				}
				else
				{
					new_zoom     = szi.end_zoom;
					new_aimpoint = szi.end_aimpoint;
				}

				if ((_zoom != new_zoom) || (_aimpoint != new_aimpoint))
				{
					_zoom = new_zoom;
					_aimpoint = new_aimpoint;
					this->on_zoom_transform_changed();
				}
			}
		}
	}

	void zoomable_window::release_render_resources (ID2D1DeviceContext* dc)
	{
		base::release_render_resources(dc);

		if (_smooth_zoom_info)
		{
			if ((_smooth_zoom_info->end_zoom != _zoom) || (_smooth_zoom_info->end_aimpoint != _aimpoint))
			{
				// zooming still in progress. paint again as soon as possible.
				::InvalidateRect(hwnd(), nullptr, TRUE);
			}
			else
			{
				// zoom finished
				_smooth_zoom_info.reset();
			}
		}
	}

	void zoomable_window::process_wm_size(WPARAM wparam, LPARAM lparam)
	{
		if (_zoomed_to_rect)
		{
			auto copy = _zoomed_to_rect->rect;
			zoom_to (copy, _zoomed_to_rect->min_margin, _zoomed_to_rect->min_zoom, _zoomed_to_rect->max_zoom, false);
		}
	}

	void zoomable_window::zoom_to (D2D1_POINT_2F aimpoint, float zoom, bool smooth)
	{
		set_zoom_and_aimpoint_internal (zoom, aimpoint, smooth);
		_zoomed_to_rect.reset();
	}

	void zoomable_window::on_zoom_transform_changed()
	{
		this->event_invoker<zoom_transform_changed_e>()(this);
	}

	void zoomable_window::zoom_to (const D2D1_RECT_F& rect, float min_margin, float min_zoom, float max_zoom, bool smooth)
	{
		assert((rect.right > rect.left) && (rect.bottom > rect.top));

		auto clientSizeDips = client_size();

		float horzZoom = (clientSizeDips.width - 2 * min_margin) / (rect.right - rect.left);
		float vertZoom = (clientSizeDips.height - 2 * min_margin) / (rect.bottom - rect.top);
		float newZoom = horzZoom < vertZoom ? horzZoom : vertZoom;
		if (newZoom < 0.3f)
			newZoom = 0.3f;

		if ((max_zoom > 0) && (newZoom > max_zoom))
			newZoom = max_zoom;

		if ((min_zoom > 0) && (newZoom < min_zoom))
			newZoom = min_zoom;

		D2D1_POINT_2F center = { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
		set_zoom_and_aimpoint_internal (newZoom, center, smooth);

		_zoomed_to_rect = zoomed_to_rect{ };
		_zoomed_to_rect->rect = rect;
		_zoomed_to_rect->min_margin = min_margin;
		_zoomed_to_rect->min_zoom   = min_zoom;
		_zoomed_to_rect->max_zoom   = max_zoom;
	}

	void zoomable_window::set_zoom_and_aimpoint_internal (float newZoom, D2D1_POINT_2F aimpoint, bool smooth)
	{
		if (smooth)
		{
			_smooth_zoom_info = smooth_zoom_info{ };
			_smooth_zoom_info->begin_zoom = _zoom;
			_smooth_zoom_info->begin_aimpoint = _aimpoint;
			_smooth_zoom_info->end_zoom = newZoom;
			_smooth_zoom_info->end_aimpoint = aimpoint;
			QueryPerformanceCounter(&_smooth_zoom_info->begin_time);
			// Zooming will continue in BeginDraw.
		}
		else
		{
			_zoom = newZoom;
			_aimpoint = aimpoint;
			_smooth_zoom_info.reset();
		}

		this->on_zoom_transform_changed();
		::InvalidateRect(base::hwnd(), nullptr, FALSE);
	}

	void zoomable_window::process_wm_mbuttondown (WPARAM wparam, LPARAM lparam)
	{
		POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		_panningLastMouseLocation = pointp_to_pointd(pt.x, pt.y);
		_panning = true;
	}

	void zoomable_window::process_wm_mousemove(WPARAM wparam, LPARAM lparam)
	{
		POINT point = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

		if (_panning)
		{
			auto dipLocation = pointp_to_pointd(point.x, point.y);

			_aimpoint += (_panningLastMouseLocation - dipLocation) / _zoom;

			_panningLastMouseLocation = dipLocation;

			_zoomed_to_rect.reset();
			this->on_zoom_transform_changed();
			::InvalidateRect(base::hwnd(), nullptr, FALSE);
		}
	}

	void zoomable_window::process_wm_mbuttonup(WPARAM wparam, LPARAM lparam)
	{
		_panning = false;
	}

	void zoomable_window::process_wm_mousewheel(WPARAM wparam, LPARAM lparam)
	{
		auto keyState = GET_KEYSTATE_WPARAM(wparam);
		auto zDelta = GET_WHEEL_DELTA_WPARAM(wparam);
		POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

		ScreenToClient(hwnd(), &pt);

		static constexpr float zoom_factor = 1.5f;

		float factor = (zDelta > 0) ? zoom_factor : (1 / zoom_factor);

		if (keyState & MK_CONTROL)
			factor = ((factor - 1) / 10) + 1;

		auto dlocation = pointp_to_pointd(pt.x, pt.y);

		auto wlocation = pointd_to_pointw(dlocation);

		float newZoom = (_smooth_zoom_info ? _smooth_zoom_info->end_zoom : _zoom) * factor;
		if (newZoom < 0.1f)
			newZoom = 0.1f;
		auto o = dlocation - client_size() / 2;
		float new_aimpoint_x = 2 * wlocation.x - o.x / _zoom - o.x / newZoom - _aimpoint.x;
		float new_aimpoint_y = 2 * wlocation.y - o.y / _zoom - o.y / newZoom - _aimpoint.y;

		set_zoom_and_aimpoint_internal (newZoom, { new_aimpoint_x, new_aimpoint_y }, true);

		_zoomed_to_rect.reset();
	}

	std::optional<LRESULT> zoomable_window::window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (_enableUserZoomingAndPanning)
		{
			if (uMsg == WM_MOUSEWHEEL)
			{
				process_wm_mousewheel(wParam, lParam);
				return 0;
			}

			if (uMsg == WM_MBUTTONDOWN)
			{
				::SetFocus(hwnd);
				process_wm_mbuttondown (wParam, lParam);
				return 0;
			}

			if (uMsg == WM_MBUTTONUP)
			{
				process_wm_mbuttonup (wParam, lParam);
				return 0;
			}

			if (uMsg == WM_MOUSEMOVE)
			{
				process_wm_mousemove(wParam, lParam);
				base::window_proc(hwnd, uMsg, wParam, lParam);
				return 0;
			}
		}

		if (uMsg == WM_SIZE)
		{
			base::window_proc (hwnd, uMsg, wParam, lParam); // Pass it to the base class first, which stores the client size.
			process_wm_size(wParam, lParam);
			return 0;
		}

		return base::window_proc(hwnd, uMsg, wParam, lParam);
	}
}
