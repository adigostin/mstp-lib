
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "edge.h"

namespace edge
{
	// TODO: move to utility functions
	modifier_key get_modifier_keys()
	{
		modifier_key keys = modifier_key::none;

		if (GetKeyState (VK_SHIFT) < 0)
			keys |= modifier_key::shift;

		if (GetKeyState (VK_CONTROL) < 0)
			keys |= modifier_key::control;

		if (GetKeyState (VK_MENU) < 0)
			keys |= modifier_key::alt;

		return keys;
	}

	#pragma region win32_window_i interface
	bool win32_window_i::visible() const
	{
		return (GetWindowLongPtr (hwnd(), GWL_STYLE) & WS_VISIBLE) != 0;
	}

	RECT win32_window_i::client_rect_pixels() const
	{
		RECT rect;
		BOOL bRes = ::GetClientRect (hwnd(), &rect); rassert(bRes);
		return rect;
	};

	SIZE win32_window_i::client_size_pixels() const
	{
		RECT rect = this->client_rect_pixels();
		return SIZE { rect.right, rect.bottom };
	}

	LONG win32_window_i::client_width_pixels() const
	{
		RECT rect = client_rect_pixels();
		return rect.right - rect.left;
	}

	LONG win32_window_i::client_height_pixels() const
	{
		RECT rect = client_rect_pixels();
		return rect.bottom - rect.top;
	}

	RECT win32_window_i::rect_pixels() const
	{
		auto parent = ::GetParent(hwnd()); rassert (parent != nullptr);
		RECT rect;
		BOOL bRes = ::GetWindowRect (hwnd(), &rect); rassert(bRes);
		MapWindowPoints (HWND_DESKTOP, parent, (LPPOINT) &rect, 2);
		return rect;
	}

	POINT win32_window_i::location_pixels() const
	{
		auto rect = this->rect_pixels();
		return { rect.left, rect.top };
	}

	LONG win32_window_i::x_pixels() const
	{
		return rect_pixels().left;
	}

	LONG win32_window_i::y_pixels() const
	{
		return rect_pixels().top;
	}

	LONG win32_window_i::width_pixels() const
	{
		RECT rect;
		::GetWindowRect (hwnd(), &rect);
		return rect.right - rect.left;
	}

	LONG win32_window_i::height_pixels() const
	{
		RECT rect;
		::GetWindowRect (hwnd(), &rect);
		return rect.bottom - rect.top;
	}

	SIZE win32_window_i::size_pixels() const
	{
		RECT rect;
		::GetWindowRect (hwnd(), &rect);
		return { rect.right - rect.left, rect.bottom - rect.top };
	}

	void win32_window_i::move_window (const RECT& rect)
	{
		BOOL bRes = ::MoveWindow (hwnd(), rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
		rassert(bRes);
	}

	void win32_window_i::invalidate()
	{
		::InvalidateRect (hwnd(), nullptr, FALSE);
	}

	void win32_window_i::invalidate (const RECT& rect)
	{
		::InvalidateRect (hwnd(), &rect, FALSE);
	}

	uint32_t win32_window_i::dpi() const
	{
		auto hwnd = this->hwnd();

		UINT dpi;
		if (auto proc_addr = GetProcAddress(GetModuleHandleA("User32.dll"), "GetDpiForWindow"))
		{
			auto proc = reinterpret_cast<UINT(WINAPI*)(HWND)>(proc_addr);
			dpi = proc(hwnd);
		}
		else
		{
			HDC tempDC = GetDC(hwnd);
			dpi = GetDeviceCaps (tempDC, LOGPIXELSX);
			ReleaseDC (hwnd, tempDC);
		}
		return dpi;
	}

	D2D1::Matrix3x2F win32_window_i::dpi_transform() const
	{
		auto dpi = this->dpi();
		return { dpi / 96.0f, 0, 0, dpi / 96.0f, 0, 0 };
	}

	float win32_window_i::pixel_width() const
	{
		return 96.0f / this->dpi();
	}

	D2D1_RECT_F win32_window_i::client_rect() const
	{
		RECT rect = this->client_rect_pixels();
		auto dpi = this->dpi();
		D2D1_RECT_F res;
		res.left   = rect.left   * 96.0f / dpi;
		res.top    = rect.top    * 96.0f / dpi;
		res.right  = rect.right  * 96.0f / dpi;
		res.bottom = rect.bottom * 96.0f / dpi;
		return res;
	}

	D2D1_SIZE_F win32_window_i::client_size() const
	{
		SIZE cs = client_size_pixels();
		auto dpi = this->dpi();
		float width = cs.cx * 96.0f / dpi;
		float height = cs.cy * 96.0f / dpi;
		return { width, height };
	}

	float win32_window_i::client_width() const
	{
		return client_width_pixels() * 96.0f / this->dpi();
	}

	float win32_window_i::client_height() const
	{
		return client_height_pixels() * 96.0f / this->dpi();
	}

	float win32_window_i::lengthp_to_lengthd (LONG lengthp) const
	{
		return lengthp * 96.0f / this->dpi();
	}

	LONG win32_window_i::lengthd_to_lengthp (float lengthd, int round_style) const
	{
		auto dpi = this->dpi();

		if (round_style < 0)
			return (LONG) std::floorf(lengthd / 96.0f * dpi);

		if (round_style > 0)
			return (LONG) std::ceilf(lengthd / 96.0f * dpi);

		return (LONG) std::roundf(lengthd / 96.0f * dpi);
	}

	D2D1_POINT_2F win32_window_i::pointp_to_pointd (POINT p) const
	{
		auto dpi = this->dpi();
		return { p.x * 96.0f / dpi, p.y * 96.0f / dpi };
	}

	D2D1_POINT_2F win32_window_i::pointp_to_pointd (long xPixels, long yPixels) const
	{
		auto dpi = this->dpi();
		return { xPixels * 96.0f / dpi, yPixels * 96.0f / dpi };
	}

	POINT win32_window_i::pointd_to_pointp (float xDips, float yDips, int round_style) const
	{
		auto dpi = this->dpi();

		if (round_style < 0)
			return { (int)std::floor(xDips / 96.0f * dpi), (int)std::floor(yDips / 96.0f * dpi) };

		if (round_style > 0)
			return { (int)std::ceil(xDips / 96.0f * dpi), (int)std::ceil(yDips / 96.0f * dpi) };

		return { (int)std::round(xDips / 96.0f * dpi), (int)std::round(yDips / 96.0f * dpi) };
	}

	POINT win32_window_i::pointd_to_pointp (D2D1_POINT_2F locationDips, int round_style) const
	{
		return pointd_to_pointp(locationDips.x, locationDips.y, round_style);
	}

	D2D1_SIZE_F win32_window_i::sizep_to_sized(SIZE sizep) const
	{
		auto dpi = this->dpi();
		return D2D1_SIZE_F{ sizep.cx * 96.0f / dpi, sizep.cy * 96.0f / dpi };
	}

	SIZE win32_window_i::sized_to_sizep (float width, float height, int round_style) const
	{
		return { lengthd_to_lengthp(width, round_style), lengthd_to_lengthp(height, round_style) };
	}

	SIZE win32_window_i::sized_to_sizep (D2D1_SIZE_F size, int round_style) const
	{
		return { lengthd_to_lengthp(size.width, round_style), lengthd_to_lengthp(size.height, round_style) };
	}

	D2D1_RECT_F win32_window_i::rectp_to_rectd (const RECT& rp) const
	{
		auto tl = pointp_to_pointd(rp.left, rp.top);
		auto br = pointp_to_pointd(rp.right, rp.bottom);
		return { tl.x, tl.y, br.x, br.y };
	}

	RECT win32_window_i::rectd_to_rectp (const D2D1_RECT_F& rd, int round_style) const
	{
		auto tl = pointd_to_pointp(rd.left, rd.top, -round_style);
		auto br = pointd_to_pointp(rd.right, rd.bottom, round_style);
		return { tl.x, tl.y, br.x, br.y };
	}

	void win32_window_i::invalidate (const D2D1_RECT_F& rect)
	{
		auto tl = this->pointd_to_pointp (rect.left, rect.top, -1);
		auto br = this->pointd_to_pointp (rect.right, rect.bottom, 1);
		RECT rectp = { tl.x, tl.y, br.x, br.y };
		invalidate (rectp);
	}
	#pragma endregion

	#pragma region zoomable_window_i interface
	D2D1_POINT_2F zoomable_window_i::pointd_to_pointw (D2D1_POINT_2F dlocation) const
	{
		auto center = pixel_aligned_window_center();
		auto aimpoint = this->aimpoint();
		auto zoom = this->zoom();
		float x = (dlocation.x - center.width) / zoom + aimpoint.x;
		float y = (dlocation.y - center.height) / zoom + aimpoint.y;
		return { x, y };
	}

	void zoomable_window_i::pointw_to_pointd (std::span<D2D1_POINT_2F> locations) const
	{
		auto center = pixel_aligned_window_center();
		auto aimpoint = this->aimpoint();
		auto zoom = this->zoom();
		for (auto& l : locations)
		{
			l.x = (l.x - aimpoint.x) * zoom + center.width;
			l.y = (l.y - aimpoint.y) * zoom + center.height;
		}
	}

	D2D1_RECT_F zoomable_window_i::rectw_to_rectd (const D2D1_RECT_F& r) const
	{
		D2D1_POINT_2F tl = pointw_to_pointd({ r.left, r.top });
		D2D1_POINT_2F br = pointw_to_pointd({ r.right, r.bottom });
		return { tl.x, tl.y, br.x, br.y };
	}

	// The implementor should align the aimpoint to a pixel center so that graphics will look crisp at integer zoom factors.
	D2D1_SIZE_F zoomable_window_i::pixel_aligned_window_center() const
	{
		float pw = pixel_width();

		float center_x = client_width() / 2;
		center_x = roundf(center_x / pw) * pw;

		float center_y = client_height() / 2;
		center_y = roundf(center_y / pw) * pw;

		return { center_x, center_y };
	}

	D2D1_POINT_2F zoomable_window_i::pointw_to_pointd (D2D1_POINT_2F location) const
	{
		pointw_to_pointd ({ &location, 1 });
		return location;
	}

	D2D1::Matrix3x2F zoomable_window_i::zoom_transform() const
	{
		auto aimpoint = this->aimpoint();
		auto zoom = this->zoom();

		return D2D1::Matrix3x2F::Translation(-aimpoint.x, -aimpoint.y)
			* D2D1::Matrix3x2F::Scale(zoom, zoom)
			* D2D1::Matrix3x2F::Translation(pixel_aligned_window_center());
	}
	#pragma endregion

	COLORREF theme_color_provider_i::color_win32 (theme_color color) const
	{
		uint32_t argb = this->argb(color);
		return argb & 0xFFFFFF;
	}

	D2D_COLOR_F theme_color_provider_i::color_d2d (theme_color color) const
	{
		uint32_t argb = this->argb(color);
		D2D_COLOR_F res = {
			((argb >> 16) & 0xff) / 255.0f,
			((argb >> 8) & 0xff) / 255.0f,
			(argb & 0xff) / 255.0f,
			((argb >> 24) & 0xff) / 255.0f
		};
		return res;
	}

	com_ptr<ID2D1SolidColorBrush> theme_color_provider_i::make_brush (ID2D1DeviceContext* dc, theme_color color) const
	{
		com_ptr<ID2D1SolidColorBrush> brush;
		auto hr = dc->CreateSolidColorBrush (color_d2d(color), &brush);
		rassert(SUCCEEDED(hr));
		return brush;
	}

	com_ptr<ID2D1SolidColorBrush> theme_color_provider_i::make_brush (ID2D1DeviceContext* dc, theme_color color, float opacity) const
	{
		auto b = make_brush(dc, color);
		b->SetOpacity(opacity);
		return b;
	}
}
