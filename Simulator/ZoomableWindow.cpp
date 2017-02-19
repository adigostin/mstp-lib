#include "pch.h"
#include "ZoomableWindow.h"

using namespace std;
using namespace D2D1;

void ZoomableWindow::OnBeforeRender()
{
	base::OnBeforeRender();

	if (_smoothZoomInfo.has_value())
	{
		auto& szi = _smoothZoomInfo.value();

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

void ZoomableWindow::OnAfterRender()
{
	base::OnAfterRender();

	if (_smoothZoomInfo.has_value())
	{
		if (_smoothZoomInfo.value()._zoomEnd != _zoom)
		{
			// zooming still in progress. paint again as soon as possible.
			::InvalidateRect(GetHWnd(), nullptr, TRUE);
		}
		else
		{
			// zoom finished
			_smoothZoomInfo.reset();
		}
	}
}

Matrix3x2F ZoomableWindow::GetZoomTransform() const
{
	return Matrix3x2F(_zoom, 0, 0, _zoom, _workspaceOrigin.x, _workspaceOrigin.y);
}

void ZoomableWindow::ProcessWmSize(WPARAM wparam, LPARAM lparam)
{
	if (_zoomedToRect.has_value())
		ZoomToRectangle (_zoomedToRect.value()._rect, _zoomedToRect.value()._minMarginDips, _zoomedToRect.value()._maxZoomOrZero, false);
}

void ZoomableWindow::SetZoomAndOrigin(float zoom, float originX, float originY, bool smooth)
{
	SetZoomAndOriginInternal(zoom, originX, originY, smooth);
	_zoomedToRect.reset();
}

void ZoomableWindow::OnZoomTransformChanged()
{
	ZoomTransformChangedEvent::InvokeHandlers(_em, this);
}

void ZoomableWindow::ZoomToRectangle (const D2D1_RECT_F& rect, float minMarginDips, float maxZoomOrZero, bool smooth)
{
	assert((rect.right > rect.left) && (rect.bottom > rect.top));

	auto clientSizeDips = base::GetClientSizeDips();

	// Make below calculations with even sizes, to make sure things are always pixel-aligned when zoom is 1.
	clientSizeDips.width = floor(clientSizeDips.width / 2.0f) * 2.0f;
	clientSizeDips.height = floor(clientSizeDips.height / 2.0f) * 2.0f;
	minMarginDips = floor(minMarginDips);

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

	_zoomedToRect = ZoomedToRect { rect, minMarginDips, maxZoomOrZero };
}


void ZoomableWindow::SetZoomAndOriginInternal(float newZoom, float newOriginX, float newOriginY, bool smooth)
{
	if (smooth)
	{
		_smoothZoomInfo = SmoothZoomInfo();
		_smoothZoomInfo.value()._zoomStart = _zoom;
		_smoothZoomInfo.value()._woXStart = _workspaceOrigin.x;
		_smoothZoomInfo.value()._woYStart = _workspaceOrigin.y;
		_smoothZoomInfo.value()._zoomEnd = newZoom;
		_smoothZoomInfo.value()._woXEnd = newOriginX;
		_smoothZoomInfo.value()._woYEnd = newOriginY;
		QueryPerformanceCounter(&_smoothZoomInfo.value()._zoomStartTime);
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
	::InvalidateRect(base::GetHWnd(), nullptr, FALSE);
}


std::optional<LRESULT> ZoomableWindow::ProcessWmMButtonDown(WPARAM wparam, LPARAM lparam)
{
	if (!_enableUserZoomingAndPanning)
		return nullopt;

	POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
	_panningLastMouseLocation = GetDipLocationFromPixelLocation(pt);
	return 0;
}

void ZoomableWindow::ProcessWmMouseMove(WPARAM wparam, LPARAM lparam)
{
	POINT point = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

	if (_panningLastMouseLocation.has_value())
	{
		D2D1_POINT_2F dipLocation = GetDipLocationFromPixelLocation(point);

		_workspaceOrigin.x += (dipLocation.x - _panningLastMouseLocation.value().x);
		_workspaceOrigin.y += (dipLocation.y - _panningLastMouseLocation.value().y);

		_panningLastMouseLocation.value() = dipLocation;

		_zoomedToRect.reset();
		this->OnZoomTransformChanged();
		//OnUserPanned();
		::InvalidateRect(base::GetHWnd(), nullptr, FALSE);
	}
}

std::optional<LRESULT> ZoomableWindow::ProcessWmMButtonUp(WPARAM wparam, LPARAM lparam)
{
	if (!_enableUserZoomingAndPanning)
		return nullopt;

	_panningLastMouseLocation.reset();
	return 0;
}

std::optional<LRESULT> ZoomableWindow::ProcessWmMouseWheel(WPARAM wparam, LPARAM lparam)
{
	if (!_enableUserZoomingAndPanning)
		return nullopt;

	auto keyState = GET_KEYSTATE_WPARAM(wparam);
	auto zDelta = GET_WHEEL_DELTA_WPARAM(wparam);
	POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

	ScreenToClient(GetHWnd(), &pt);

	float factor = (zDelta > 0) ? ZoomFactor : (1 / ZoomFactor);

	if (keyState & MK_CONTROL)
		factor = ((factor - 1) / 10) + 1;

	auto dipLocation = this->D2DWindow::GetDipLocationFromPixelLocation(pt);

	float _zoomHitWX = (dipLocation.x - _workspaceOrigin.x) / _zoom;
	float _zoomHitWY = (dipLocation.y - _workspaceOrigin.y) / _zoom;

	// TODO: compose also for workspaceOrigin
	//_zoomEnd = _zoomEnd * factor; // if a zooming animation is currently running, compose its endZoom with the new factor
	float newZoom = (_smoothZoomInfo.has_value() ? _smoothZoomInfo.value()._zoomEnd : _zoom) * factor;
	if (newZoom < 0.1f)
		newZoom = 0.1f;
	float newWOX = dipLocation.x - _zoomHitWX * newZoom;
	float newWOY = dipLocation.y - _zoomHitWY * newZoom;

	SetZoomAndOriginInternal(newZoom, newWOX, newWOY, true);

	_zoomedToRect.reset();
	return 0;
}

std::optional<LRESULT> ZoomableWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_MOUSEWHEEL)
	{
		auto result = ProcessWmMouseWheel(wParam, lParam);
		if (result.has_value())
			return result;
		else
			return base::WindowProc (hwnd, uMsg, wParam, lParam);
	}

	if (uMsg == WM_MBUTTONDOWN)
	{
		auto result = ProcessWmMButtonDown(wParam, lParam);
		if (result.has_value())
			return result;
		else
			return base::WindowProc(hwnd, uMsg, wParam, lParam);
	}

	if (uMsg == WM_MBUTTONUP)
	{
		auto result = ProcessWmMButtonUp(wParam, lParam);
		if (result.has_value())
			return result;
		else
			return base::WindowProc(hwnd, uMsg, wParam, lParam);
	}

	if (uMsg == WM_MOUSEMOVE)
	{
		ProcessWmMouseMove(wParam, lParam);
		base::WindowProc(hwnd, uMsg, wParam, lParam);
		return 0;
	}

	if (uMsg == WM_SIZE)
	{
		base::WindowProc (hwnd, uMsg, wParam, lParam); // Pass it to the base class first, which stores the client size.
		ProcessWmSize(wParam, lParam);
		return 0;
	}

	return base::WindowProc(hwnd, uMsg, wParam, lParam);
}

D2D1_POINT_2F ZoomableWindow::GetWLocationFromDLocation(D2D1_POINT_2F dLocation) const
{
	float x = (dLocation.x - _workspaceOrigin.x) / _zoom;
	float y = (dLocation.y - _workspaceOrigin.y) / _zoom;
	return { x, y };
}

D2D1_POINT_2F ZoomableWindow::GetDLocationFromWLocation(D2D1_POINT_2F wLocation) const
{
	float x = _workspaceOrigin.x + wLocation.x * _zoom;
	float y = _workspaceOrigin.y + wLocation.y * _zoom;
	return { x, y };
}
