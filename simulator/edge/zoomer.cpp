
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "zoomer.h"
#include "utility_functions.h"

using namespace std;
using namespace D2D1;

namespace edge
{
	zoomer::zoomer (d2d_window_i* window)
		: _window(window)
	{
		_window->renderer().before_render().add_handler(&on_before_render, this);
		_window->renderer().after_render().add_handler(&on_after_render, this);
		_window->window_proc().add_handler<&zoomer::on_window_proc>(this);
	}

	zoomer::~zoomer()
	{
		_window->window_proc().remove_handler<&zoomer::on_window_proc>(this);
		_window->renderer().after_render().remove_handler(&on_after_render, this);
		_window->renderer().before_render().remove_handler(&on_before_render, this);
	}

	void zoomer::create_render_resources (ID2D1DeviceContext* dc)
	{
		if (_smooth_zoom_info)
		{
			auto& szi = _smooth_zoom_info.value();

			// zoom in progress
			LARGE_INTEGER timeNow;
			BOOL bRes = QueryPerformanceCounter(&timeNow); rassert(bRes);

			LARGE_INTEGER frequency;
			bRes = QueryPerformanceFrequency(&frequency); rassert(bRes);

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
					this->event_invoker<zoom_transform_changed_e>()();
				}
			}
		}
	}

	void zoomer::release_render_resources (ID2D1DeviceContext* dc)
	{
		if (_smooth_zoom_info)
		{
			if ((_smooth_zoom_info->end_zoom != _zoom) || (_smooth_zoom_info->end_aimpoint != _aimpoint))
			{
				// zooming still in progress. paint again as soon as possible.
				::InvalidateRect(_window->hwnd(), nullptr, TRUE);
			}
			else
			{
				// zoom finished
				_smooth_zoom_info.reset();
			}
		}
	}

	void zoomer::process_wm_size(WPARAM wparam, LPARAM lparam)
	{
		if (_zoomed_to_rect)
		{
			auto copy = _zoomed_to_rect->rect;
			zoom_to (copy, _zoomed_to_rect->min_margin, _zoomed_to_rect->min_zoom, _zoomed_to_rect->max_zoom, false);
		}
	}

	void zoomer::zoom_to (D2D1_POINT_2F aimpoint, float zoom, bool smooth)
	{
		set_zoom_and_aimpoint_internal (zoom, aimpoint, smooth);
		_zoomed_to_rect.reset();
	}

	void zoomer::zoom_to (const D2D1_RECT_F& rect, float min_margin, float min_zoom, float max_zoom, bool smooth)
	{
		rassert((rect.right > rect.left) && (rect.bottom > rect.top));

		auto clientSizeDips = _window->client_size();

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

	void zoomer::set_zoom_and_aimpoint_internal (float newZoom, D2D1_POINT_2F aimpoint, bool smooth)
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

		this->event_invoker<zoom_transform_changed_e>()();
		::InvalidateRect(_window->hwnd(), nullptr, FALSE);
	}

	void zoomer::process_wm_mbuttondown (WPARAM wparam, LPARAM lparam)
	{
		POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		_panningLastMouseLocation = _window->pointp_to_pointd(pt.x, pt.y);
		_panning = true;
	}

	void zoomer::process_wm_mousemove(WPARAM wparam, LPARAM lparam)
	{
		POINT point = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

		if (_panning)
		{
			auto dipLocation = _window->pointp_to_pointd(point.x, point.y);

			_aimpoint += (_panningLastMouseLocation - dipLocation) / _zoom;

			_panningLastMouseLocation = dipLocation;

			_zoomed_to_rect.reset();
			this->event_invoker<zoom_transform_changed_e>()();
			::InvalidateRect(_window->hwnd(), nullptr, FALSE);
		}
	}

	void zoomer::process_wm_mbuttonup(WPARAM wparam, LPARAM lparam)
	{
		_panning = false;
	}

	void zoomer::process_wm_mousewheel(WPARAM wparam, LPARAM lparam)
	{
		auto keyState = GET_KEYSTATE_WPARAM(wparam);
		auto zDelta = GET_WHEEL_DELTA_WPARAM(wparam);
		POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

		ScreenToClient(_window->hwnd(), &pt);

		static constexpr float zoom_factor = 1.5f;

		float factor = (zDelta > 0) ? zoom_factor : (1 / zoom_factor);

		if (keyState & MK_CONTROL)
			factor = ((factor - 1) / 10) + 1;

		auto dlocation = _window->pointp_to_pointd(pt.x, pt.y);

		auto wlocation = dynamic_cast<zoomable_window_i*>(_window)->pointd_to_pointw(dlocation);

		float newZoom = (_smooth_zoom_info ? _smooth_zoom_info->end_zoom : _zoom) * factor;
		if (newZoom < 0.1f)
			newZoom = 0.1f;
		auto o = dlocation - _window->client_size() / 2;
		float new_aimpoint_x = 2 * wlocation.x - o.x / _zoom - o.x / newZoom - _aimpoint.x;
		float new_aimpoint_y = 2 * wlocation.y - o.y / _zoom - o.y / newZoom - _aimpoint.y;

		set_zoom_and_aimpoint_internal (newZoom, { new_aimpoint_x, new_aimpoint_y }, true);

		_zoomed_to_rect.reset();
	}

	std::optional<LRESULT> zoomer::on_window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (_enableUserZoomingAndPanning)
		{
			if (msg == WM_MOUSEWHEEL)
			{
				process_wm_mousewheel(wparam, lparam);
				return 0;
			}

			if (msg == WM_MBUTTONDOWN)
			{
				::SetFocus(hwnd);
				process_wm_mbuttondown (wparam, lparam);
				return 0;
			}

			if (msg == WM_MBUTTONUP)
			{
				process_wm_mbuttonup (wparam, lparam);
				return 0;
			}

			if (msg == WM_MOUSEMOVE)
			{
				process_wm_mousemove(wparam, lparam);
				return std::nullopt;
			}
		}

		if (msg == WM_SIZE)
		{
			process_wm_size(wparam, lparam);
			return std::nullopt;
		}

		return std::nullopt;
	}
}
