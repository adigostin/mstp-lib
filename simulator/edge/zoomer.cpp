
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "edge_d2d.h"

using namespace std;
using namespace D2D1;

namespace edge
{
	class ZoomerImpl : public IZoomer
	{
		HWND _hWnd;
		wil::unique_hwnd _helper;
		wil::unique_threadpool_timer _timerH;
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
		HRESULT InitInstance (HWND hWnd)
		{
			_hWnd = hWnd;

			static const WNDCLASS wc = {
				.lpfnWndProc = HelperWndProc,
				.hInstance = GetModuleHandle(nullptr),
				.lpszClassName = L"MyTimerHelperClass",
			};
			RegisterClass(&wc);

			_helper.reset(CreateWindow(wc.lpszClassName, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr));
			RETURN_LAST_ERROR_IF_NULL(_helper);
			_timerH.reset (CreateThreadpoolTimer (TimerCallback, reinterpret_cast<void*>(this), nullptr));

			SetWindowSubclass (hWnd, SubclassProcStatic, 0, reinterpret_cast<DWORD_PTR>(this));
			
			return S_OK;
		}

		~ZoomerImpl()
		{
			// Make sure our timer routine is not called on a threadpool thread while we're destroying ourselves.
			SetThreadpoolTimer (_timerH.get(), nullptr, 0, 0);
			WaitForThreadpoolTimerCallbacks (_timerH.get(), TRUE);
			_timerH.reset();

			RemoveWindowSubclass (_hWnd, SubclassProcStatic, 0);
		}

		void OnBeforeWmPaint(HWND hwnd)
		{
			if (_smooth_zoom_info)
			{
				auto& szi = _smooth_zoom_info.value();

				// zoom in progress
				LARGE_INTEGER timeNow;
				BOOL bRes = QueryPerformanceCounter(&timeNow); _ASSERT(bRes);

				LARGE_INTEGER frequency;
				bRes = QueryPerformanceFrequency(&frequency); _ASSERT(bRes);

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
//						zoom_transform_changed_e::invoker(_em).invoke();
					}
				}
			}
		}

		void process_wm_size (HWND hwnd, WPARAM wparam, LPARAM lparam)
		{
			if (_zoomed_to_rect)
			{
				auto copy = _zoomed_to_rect->rect;
				zoom_to (copy, _zoomed_to_rect->min_margin, _zoomed_to_rect->min_zoom, _zoomed_to_rect->max_zoom, false);
			}
		}

		//virtual zoom_transform_changed_e::subscriber zoom_transform_changed() override
		//{
		//	return zoom_transform_changed_e::subscriber(_em);
		//}

		void zoom_to (D2D1_POINT_2F aimpoint, float zoom, bool smooth)
		{
			set_zoom_and_aimpoint_internal (zoom, aimpoint, smooth);
			_zoomed_to_rect.reset();
		}

		void zoom_to (const D2D1_RECT_F& rect, float min_margin, float min_zoom, float max_zoom, bool smooth)
		{
			_ASSERT((rect.right > rect.left) && (rect.bottom > rect.top));

			auto clientSizeDips = edge::client_size(_hWnd);

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

		void set_zoom_and_aimpoint_internal (float newZoom, D2D1_POINT_2F aimpoint, bool smooth)
		{
			if (_smooth_zoom_info)
			{
				SetThreadpoolTimer (_timerH.get(), nullptr, 0, 0);
				WaitForThreadpoolTimerCallbacks (_timerH.get(), TRUE);
				_smooth_zoom_info.reset();
			}

			if (smooth)
			{
				_smooth_zoom_info = smooth_zoom_info{ };
				_smooth_zoom_info->begin_zoom = _zoom;
				_smooth_zoom_info->begin_aimpoint = _aimpoint;
				_smooth_zoom_info->end_zoom = newZoom;
				_smooth_zoom_info->end_aimpoint = aimpoint;
				FILETIME dueTime = { .dwLowDateTime = 0xFFFF'FFFF, .dwHighDateTime = 0xFFFF'FFFF };
				SetThreadpoolTimer (_timerH.get(), &dueTime, 10, 5);
				QueryPerformanceCounter(&_smooth_zoom_info->begin_time);
			}
			else
			{
				_zoom = newZoom;
				_aimpoint = aimpoint;
			}

//			zoom_transform_changed_e::invoker(_em).invoke();
			::InvalidateRect(_hWnd, nullptr, FALSE);
		}

		static VOID NTAPI TimerCallback (PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer)
		{
			auto inst = reinterpret_cast<ZoomerImpl*>(Context);
			PostMessage(inst->_helper.get(), WM_APP, 0, reinterpret_cast<LPARAM>(inst));
		}

		static LRESULT CALLBACK HelperWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			if (uMsg == WM_APP)
			{
				auto inst = reinterpret_cast<ZoomerImpl*>(lParam);
				inst->OnTimer();
				return 0;
			}
			
			return DefWindowProc (hWnd, uMsg, wParam, lParam);
		}
		
		void OnTimer()
		{
			if (_smooth_zoom_info)
			{
				if ((_smooth_zoom_info->end_zoom != _zoom) || (_smooth_zoom_info->end_aimpoint != _aimpoint))
				{
					// Zooming still in progress. Paint again.
					::InvalidateRect (_hWnd, nullptr, TRUE);
				}
				else
				{
					// Zooming finished.
					SetThreadpoolTimer (_timerH.get(), nullptr, 0, 0);
					WaitForThreadpoolTimerCallbacks (_timerH.get(), TRUE);
					_smooth_zoom_info.reset();
				}
			}
		}

		void process_wm_mbuttondown (HWND hwnd, WPARAM wparam, LPARAM lparam)
		{
			POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			uint32_t dpi = edge::dpi(hwnd);
			_panningLastMouseLocation = edge::pointp_to_pointd(pt.x, pt.y, dpi);
			_panning = true;
		}

		void process_wm_mousemove (HWND hwnd, WPARAM wparam, LPARAM lparam)
		{
			POINT point = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			uint32_t dpi = edge::dpi(hwnd);
			auto dipLocation = pointp_to_pointd(point.x, point.y, dpi);

			_aimpoint += (_panningLastMouseLocation - dipLocation) / _zoom;

			_panningLastMouseLocation = dipLocation;

			_zoomed_to_rect.reset();
//			zoom_transform_changed_e::invoker(_em).invoke();
			::InvalidateRect(hwnd, nullptr, FALSE);
		}

		void process_wm_mbuttonup (HWND hwnd, WPARAM wparam, LPARAM lparam)
		{
			_panning = false;
		}

		void process_wm_mousewheel (HWND hwnd, WPARAM wparam, LPARAM lparam)
		{
			auto keyState = GET_KEYSTATE_WPARAM(wparam);
			auto zDelta = GET_WHEEL_DELTA_WPARAM(wparam);
			POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

			::ScreenToClient(hwnd, &pt);

			static constexpr float zoom_factor = 1.5f;

			float factor = (zDelta > 0) ? zoom_factor : (1 / zoom_factor);

			if (keyState & MK_CONTROL)
				factor = ((factor - 1) / 10) + 1;

			uint32_t dpi = edge::dpi(hwnd);
			auto dlocation = edge::pointp_to_pointd(pt.x, pt.y, dpi);
			auto wlocation = pointd_to_pointw(dlocation);

			float newZoom = (_smooth_zoom_info ? _smooth_zoom_info->end_zoom : _zoom) * factor;
			if (newZoom < 0.1f)
				newZoom = 0.1f;
			auto o = dlocation - edge::client_size(hwnd) / 2;
			float new_aimpoint_x = 2 * wlocation.x - o.x / _zoom - o.x / newZoom - _aimpoint.x;
			float new_aimpoint_y = 2 * wlocation.y - o.y / _zoom - o.y / newZoom - _aimpoint.y;

			set_zoom_and_aimpoint_internal (newZoom, { new_aimpoint_x, new_aimpoint_y }, true);

			_zoomed_to_rect.reset();
		}

		static LRESULT CALLBACK SubclassProcStatic (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
		{
			ZoomerImpl* inst = reinterpret_cast<ZoomerImpl*>(dwRefData);
			return inst->SubclassProc (hWnd, uMsg, wParam, lParam);
		}

		LRESULT SubclassProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			if (_enableUserZoomingAndPanning)
			{
				if (uMsg == WM_MOUSEWHEEL)
				{
					process_wm_mousewheel (hWnd, wParam, lParam);
					return 0;
				}

				if (uMsg == WM_MBUTTONDOWN)
				{
					::SetFocus(hWnd);
					process_wm_mbuttondown (hWnd, wParam, lParam);
					return 0;
				}

				if (uMsg == WM_MBUTTONUP)
				{
					process_wm_mbuttonup (hWnd, wParam, lParam);
					return 0;
				}

				if (uMsg == WM_MOUSEMOVE)
				{
					if (_panning)
					{
						process_wm_mousemove(hWnd, wParam, lParam);
						return 0;
					}

					return DefSubclassProc (hWnd, uMsg, wParam, lParam);
				}
			}

			if (uMsg == WM_SIZE)
			{
				process_wm_size (hWnd, wParam, lParam);
				return DefSubclassProc (hWnd, uMsg, wParam, lParam);
			}

			if (uMsg == WM_PAINT)
			{
				// Since we subclassed the window, we'll receive the messages before the window.
				// If we're subclassing after someone else subclassed it, we'll receive messages first.
				OnBeforeWmPaint(hWnd);
				return DefSubclassProc (hWnd, uMsg, wParam, lParam);
			}

			return DefSubclassProc (hWnd, uMsg, wParam, lParam);
		}
	
		D2D1_POINT_2F pointd_to_pointw (D2D1_POINT_2F dlocation) const
		{
			auto center = pixel_aligned_window_center();
			float x = (dlocation.x - center.width) / _zoom + _aimpoint.x;
			float y = (dlocation.y - center.height) / _zoom + _aimpoint.y;
			return { x, y };
		}

		void pointw_to_pointd (std::span<D2D1_POINT_2F> locations) const
		{
			auto center = pixel_aligned_window_center();
			for (auto& l : locations)
			{
				l.x = (l.x - _aimpoint.x) * _zoom + center.width;
				l.y = (l.y - _aimpoint.y) * _zoom + center.height;
			}
		}

		D2D1_RECT_F rectw_to_rectd (const D2D1_RECT_F& r) const
		{
			D2D1_POINT_2F tl = pointw_to_pointd({ r.left, r.top });
			D2D1_POINT_2F br = pointw_to_pointd({ r.right, r.bottom });
			return { tl.x, tl.y, br.x, br.y };
		}

		// The implementor should align the aimpoint to a pixel center so that graphics will look crisp at integer zoom factors.
		D2D1_SIZE_F pixel_aligned_window_center() const
		{
			float pw = edge::pixel_width(_hWnd);

			D2D1_SIZE_F center = edge::client_size(_hWnd) / 2;
			float center_x = roundf(center.width  / pw) * pw;
			float center_y = roundf(center.height / pw) * pw;
			return { center_x, center_y };
		}

		D2D1_POINT_2F pointw_to_pointd (float x, float y) const
		{
			D2D1_POINT_2F p = { x, y };
			pointw_to_pointd({ &p, 1 });
			return p;
		}

		D2D1_POINT_2F pointw_to_pointd (D2D1_POINT_2F location) const
		{
			pointw_to_pointd ({ &location, 1 });
			return location;
		}

		D2D1::Matrix3x2F zoom_transform() const
		{
			return D2D1::Matrix3x2F::Translation(-_aimpoint.x, -_aimpoint.y)
				* D2D1::Matrix3x2F::Scale(_zoom, _zoom)
				* D2D1::Matrix3x2F::Translation(pixel_aligned_window_center());
		}
	};

	HRESULT MakeZoomer (HWND hWnd, wistd::unique_ptr<IZoomer>& zoomerOut)
	{
		auto p = wistd::unique_ptr<ZoomerImpl>(new (std::nothrow) ZoomerImpl()); RETURN_IF_NULL_ALLOC(p);
		auto hr = p->InitInstance (hWnd); RETURN_IF_FAILED(hr);
		zoomerOut = std::move(p);
		return S_OK;
	}
}
