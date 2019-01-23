
#include "pch.h"
#include "zoomable_window.h"

using namespace std;
using namespace D2D1;

namespace edge
{
	void zoomable_window::create_render_resources (ID2D1DeviceContext* dc)
	{
		base::create_render_resources(dc);

		if (_smoothZoomInfo != nullptr)
		{
			auto& szi = *_smoothZoomInfo;

			// zoom in progress
			LARGE_INTEGER timeNow;
			BOOL bRes = QueryPerformanceCounter(&timeNow); assert(bRes);

			LARGE_INTEGER frequency;
			bRes = QueryPerformanceFrequency(&frequency); assert(bRes);

			LONGLONG ellapsedMilliseconds = (timeNow.QuadPart - szi._zoomStartTime.QuadPart) * 1000 / frequency.QuadPart;
			if (ellapsedMilliseconds == 0)
			{
				// The WM_PAINT message came too fast, so the zoom hasn't changed yet.
				// Let's keep invalidating in EndRender() so that Windows will keep sending us WM_PAINT messages.
			}
			else
			{
				static constexpr float DurationMilliseconds = 150;

				float newZoom = szi._zoomStart + (float)ellapsedMilliseconds * (szi._zoomEnd - szi._zoomStart) / (float)DurationMilliseconds;
				float newWOX = szi._woXStart + (float)ellapsedMilliseconds * (szi._woXEnd - szi._woXStart) / (float)DurationMilliseconds;
				float newWOY = szi._woYStart + (float)ellapsedMilliseconds * (szi._woYEnd - szi._woYStart) / (float)DurationMilliseconds;

				if (szi._zoomEnd > szi._zoomStart)
				{
					// zoom increasing
					if (newZoom > szi._zoomEnd)
					{
						newZoom = szi._zoomEnd;
						newWOX = szi._woXEnd;
						newWOY = szi._woYEnd;
					}
				}
				else
				{
					// zoom decreasing
					if (newZoom < szi._zoomEnd)
					{
						newZoom = szi._zoomEnd;
						newWOX = szi._woXEnd;
						newWOY = szi._woYEnd;
					}
				}

				if (_zoom != newZoom)
				{
					_zoom = newZoom;
					_workspaceOrigin.x = newWOX;
					_workspaceOrigin.y = newWOY;
					this->OnZoomTransformChanged();
				}
			}
		}
	}

	void zoomable_window::release_render_resources (ID2D1DeviceContext* dc)
	{
		base::release_render_resources(dc);

		if (_smoothZoomInfo != nullptr)
		{
			if (_smoothZoomInfo->_zoomEnd != _zoom)
			{
				// zooming still in progress. paint again as soon as possible.
				::InvalidateRect(hwnd(), nullptr, TRUE);
			}
			else
			{
				// zoom finished
				_smoothZoomInfo.reset();
			}
		}
	}

	Matrix3x2F zoomable_window::GetZoomTransform() const
	{
		return Matrix3x2F(_zoom, 0, 0, _zoom, _workspaceOrigin.x, _workspaceOrigin.y);
	}

	void zoomable_window::ProcessWmSize(WPARAM wparam, LPARAM lparam)
	{
		if (_zoomedToRect != nullptr)
			ZoomToRectangle (_zoomedToRect->_rect, _zoomedToRect->_minMarginDips, _zoomedToRect->_maxZoomOrZero, false);
	}

	void zoomable_window::SetZoomAndOrigin(float zoom, float originX, float originY, bool smooth)
	{
		SetZoomAndOriginInternal(zoom, originX, originY, smooth);
		_zoomedToRect.reset();
	}

	void zoomable_window::OnZoomTransformChanged()
	{
		//this->event_invoker<zoom_transform_changed_event>()(this);
	}

	void zoomable_window::ZoomToRectangle (const D2D1_RECT_F& rect, float minMarginDips, float maxZoomOrZero, bool smooth)
	{
		assert((rect.right > rect.left) && (rect.bottom > rect.top));

		auto clientSizeDips = base::client_size();

		// Make below calculations with even sizes, to make sure things are always pixel-aligned when zoom is 1.
		clientSizeDips.width = std::floor(clientSizeDips.width / 2) * 2;
		clientSizeDips.height = std::floor(clientSizeDips.height / 2) * 2;
		minMarginDips = std::floor(minMarginDips);

		//float minMarginPixels = minMarginDips;
		float horzZoom = (clientSizeDips.width - 2 * minMarginDips) / (rect.right - rect.left);
		float vertZoom = (clientSizeDips.height - 2 * minMarginDips) / (rect.bottom - rect.top);
		float newZoom = horzZoom < vertZoom ? horzZoom : vertZoom;
		if (newZoom < 0.3f)
			newZoom = 0.3f;

		if ((maxZoomOrZero > 0.0f) && (newZoom > maxZoomOrZero))
			newZoom = maxZoomOrZero;

		float newWOX = (clientSizeDips.width - (rect.right - rect.left) * newZoom) / 2 - rect.left * newZoom;
		float newWOY = (clientSizeDips.height - (rect.bottom - rect.top) * newZoom) / 2 - rect.top * newZoom;

		SetZoomAndOriginInternal(newZoom, newWOX, newWOY, smooth);

		_zoomedToRect = std::make_unique<ZoomedToRect>();
		_zoomedToRect->_rect = rect;
		_zoomedToRect->_minMarginDips = minMarginDips;
		_zoomedToRect->_maxZoomOrZero = maxZoomOrZero;
	}

	void zoomable_window::SetZoomAndOriginInternal(float newZoom, float newOriginX, float newOriginY, bool smooth)
	{
		if (smooth)
		{
			_smoothZoomInfo = std::make_unique<SmoothZoomInfo>();
			_smoothZoomInfo->_zoomStart = _zoom;
			_smoothZoomInfo->_woXStart = _workspaceOrigin.x;
			_smoothZoomInfo->_woYStart = _workspaceOrigin.y;
			_smoothZoomInfo->_zoomEnd = newZoom;
			_smoothZoomInfo->_woXEnd = newOriginX;
			_smoothZoomInfo->_woYEnd = newOriginY;
			QueryPerformanceCounter(&_smoothZoomInfo->_zoomStartTime);
			// Zooming will continue in BeginDraw.
		}
		else
		{
			_zoom = newZoom;
			_workspaceOrigin.x = newOriginX;
			_workspaceOrigin.y = newOriginY;
			_smoothZoomInfo.reset();
		}

		this->OnZoomTransformChanged();
		::InvalidateRect(base::hwnd(), nullptr, FALSE);
	}

	bool zoomable_window::ProcessWmMButtonDown(WPARAM wparam, LPARAM lparam)
	{
		if (!_enableUserZoomingAndPanning)
			return false; // not handled

		POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		_panningLastMouseLocation = pointp_to_pointd(pt.x, pt.y);
		_panning = true;
		return true; // handled
	}

	void zoomable_window::ProcessWmMouseMove(WPARAM wparam, LPARAM lparam)
	{
		POINT point = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

		if (_panning)
		{
			auto dipLocation = pointp_to_pointd(point.x, point.y);

			_workspaceOrigin.x += (dipLocation.x - _panningLastMouseLocation.x);
			_workspaceOrigin.y += (dipLocation.y - _panningLastMouseLocation.y);

			_panningLastMouseLocation = dipLocation;

			_zoomedToRect.reset();
			this->OnZoomTransformChanged();
			//OnUserPanned();
			::InvalidateRect(base::hwnd(), nullptr, FALSE);
		}
	}

	bool zoomable_window::ProcessWmMButtonUp(WPARAM wparam, LPARAM lparam)
	{
		if (!_enableUserZoomingAndPanning)
			return false; // not handled

		_panning = false;
		return true; // handled
	}

	bool zoomable_window::ProcessWmMouseWheel(WPARAM wparam, LPARAM lparam)
	{
		if (!_enableUserZoomingAndPanning)
			return false; // not handled

		auto keyState = GET_KEYSTATE_WPARAM(wparam);
		auto zDelta = GET_WHEEL_DELTA_WPARAM(wparam);
		POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

		ScreenToClient(hwnd(), &pt);

		float factor = (zDelta > 0) ? ZoomFactor : (1 / ZoomFactor);

		if (keyState & MK_CONTROL)
			factor = ((factor - 1) / 10) + 1;

		auto dipLocation = this->pointp_to_pointd(pt.x, pt.y);

		float _zoomHitWX = (dipLocation.x - _workspaceOrigin.x) / _zoom;
		float _zoomHitWY = (dipLocation.y - _workspaceOrigin.y) / _zoom;

		// TODO: compose also for workspaceOrigin
		//_zoomEnd = _zoomEnd * factor; // if a zooming animation is currently running, compose its endZoom with the new factor
		float newZoom = ((_smoothZoomInfo != nullptr) ? _smoothZoomInfo->_zoomEnd : _zoom) * factor;
		if (newZoom < 0.1f)
			newZoom = 0.1f;
		float newWOX = dipLocation.x - _zoomHitWX * newZoom;
		float newWOY = dipLocation.y - _zoomHitWY * newZoom;

		SetZoomAndOriginInternal(newZoom, newWOX, newWOY, true);

		_zoomedToRect.reset();
		return true; // handled
	}

	std::optional<LRESULT> zoomable_window::window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (uMsg == WM_MOUSEWHEEL)
		{
			bool handled = ProcessWmMouseWheel(wParam, lParam);
			if (handled)
				return 0;
			else
				return base::window_proc(hwnd, uMsg, wParam, lParam);
		}

		if (uMsg == WM_MBUTTONDOWN)
		{
			bool handled = ProcessWmMButtonDown(wParam, lParam);
			if (handled)
				return 0;
			else
				return base::window_proc(hwnd, uMsg, wParam, lParam);
		}

		if (uMsg == WM_MBUTTONUP)
		{
			bool handled = ProcessWmMButtonUp(wParam, lParam);
			if (handled)
				return 0;
			else
				return base::window_proc(hwnd, uMsg, wParam, lParam);
		}

		if (uMsg == WM_MOUSEMOVE)
		{
			ProcessWmMouseMove(wParam, lParam);
			base::window_proc(hwnd, uMsg, wParam, lParam);
			return 0;
		}

		if (uMsg == WM_SIZE)
		{
			base::window_proc (hwnd, uMsg, wParam, lParam); // Pass it to the base class first, which stores the client size.
			ProcessWmSize(wParam, lParam);
			return 0;
		}

		return base::window_proc(hwnd, uMsg, wParam, lParam);
	}

	D2D1_POINT_2F zoomable_window::pointd_to_pointw (D2D1_POINT_2F dlocation) const
	{
		float x = (dlocation.x - _workspaceOrigin.x) / _zoom;
		float y = (dlocation.y - _workspaceOrigin.y) / _zoom;
		return { x, y };
	}

	D2D1_POINT_2F zoomable_window::pointw_to_pointd (D2D1_POINT_2F wlocation) const
	{
		float x = _workspaceOrigin.x + wlocation.x * _zoom;
		float y = _workspaceOrigin.y + wlocation.y * _zoom;
		return { x, y };
	}
}
