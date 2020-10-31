
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "win32_window_i.h"

namespace edge
{
	bool hwnd_i::visible() const
	{
		return (GetWindowLongPtr (hwnd(), GWL_STYLE) & WS_VISIBLE) != 0;
	}

	RECT hwnd_i::client_rect_pixels() const
	{
		RECT rect;
		BOOL bRes = ::GetClientRect (hwnd(), &rect); rassert(bRes);
		return rect;
	};

	SIZE hwnd_i::client_size_pixels() const
	{
		RECT rect = this->client_rect_pixels();
		return SIZE { rect.right, rect.bottom };
	}

	LONG hwnd_i::client_width_pixels() const
	{
		RECT rect = client_rect_pixels();
		return rect.right - rect.left;
	}

	LONG hwnd_i::client_height_pixels() const
	{
		RECT rect = client_rect_pixels();
		return rect.bottom - rect.top;
	}

	RECT hwnd_i::rect_pixels() const
	{
		auto parent = ::GetParent(hwnd()); rassert (parent != nullptr);
		RECT rect;
		BOOL bRes = ::GetWindowRect (hwnd(), &rect); rassert(bRes);
		MapWindowPoints (HWND_DESKTOP, parent, (LPPOINT) &rect, 2);
		return rect;
	}

	POINT hwnd_i::location_pixels() const
	{
		auto rect = this->rect_pixels();
		return { rect.left, rect.top };
	}

	LONG hwnd_i::x_pixels() const
	{
		return rect_pixels().left;
	}

	LONG hwnd_i::y_pixels() const
	{
		return rect_pixels().top;
	}

	LONG hwnd_i::width_pixels() const
	{
		RECT rect;
		::GetWindowRect (hwnd(), &rect);
		return rect.right - rect.left;
	}

	LONG hwnd_i::height_pixels() const
	{
		RECT rect;
		::GetWindowRect (hwnd(), &rect);
		return rect.bottom - rect.top;
	}

	SIZE hwnd_i::size_pixels() const
	{
		RECT rect;
		::GetWindowRect (hwnd(), &rect);
		return { rect.right - rect.left, rect.bottom - rect.top };
	}

	void hwnd_i::move_window (const RECT& rect)
	{
		BOOL bRes = ::MoveWindow (hwnd(), rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
		rassert(bRes);
	}

	void hwnd_i::invalidate()
	{
		::InvalidateRect (hwnd(), nullptr, FALSE);
	}

	void hwnd_i::invalidate (const RECT& rect)
	{
		::InvalidateRect (hwnd(), &rect, FALSE);
	}

	uint32_t hwnd_i::dpi() const
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

	D2D1::Matrix3x2F hwnd_i::dpi_transform() const
	{
		auto dpi = this->dpi();
		return { dpi / 96.0f, 0, 0, dpi / 96.0f, 0, 0 };
	}

	float hwnd_i::pixel_width() const
	{
		return 96.0f / this->dpi();
	}

	D2D1_RECT_F hwnd_i::client_rect() const
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

	D2D1_SIZE_F hwnd_i::client_size() const
	{
		SIZE cs = client_size_pixels();
		auto dpi = this->dpi();
		float width = cs.cx * 96.0f / dpi;
		float height = cs.cy * 96.0f / dpi;
		return { width, height };
	}

	float hwnd_i::client_width() const
	{
		return client_width_pixels() * 96.0f / this->dpi();
	}

	float hwnd_i::client_height() const
	{
		return client_height_pixels() * 96.0f / this->dpi();
	}

	float hwnd_i::lengthp_to_lengthd (LONG lengthp) const
	{
		return lengthp * 96.0f / this->dpi();
	}

	LONG hwnd_i::lengthd_to_lengthp (float lengthd, int round_style) const
	{
		auto dpi = this->dpi();

		if (round_style < 0)
			return (LONG) std::floorf(lengthd / 96.0f * dpi);

		if (round_style > 0)
			return (LONG) std::ceilf(lengthd / 96.0f * dpi);

		return (LONG) std::roundf(lengthd / 96.0f * dpi);
	}

	D2D1_POINT_2F hwnd_i::pointp_to_pointd (POINT p) const
	{
		auto dpi = this->dpi();
		return { p.x * 96.0f / dpi, p.y * 96.0f / dpi };
	}

	D2D1_POINT_2F hwnd_i::pointp_to_pointd (long xPixels, long yPixels) const
	{
		auto dpi = this->dpi();
		return { xPixels * 96.0f / dpi, yPixels * 96.0f / dpi };
	}

	POINT hwnd_i::pointd_to_pointp (float xDips, float yDips, int round_style) const
	{
		auto dpi = this->dpi();

		if (round_style < 0)
			return { (int)std::floor(xDips / 96.0f * dpi), (int)std::floor(yDips / 96.0f * dpi) };

		if (round_style > 0)
			return { (int)std::ceil(xDips / 96.0f * dpi), (int)std::ceil(yDips / 96.0f * dpi) };

		return { (int)std::round(xDips / 96.0f * dpi), (int)std::round(yDips / 96.0f * dpi) };
	}

	POINT hwnd_i::pointd_to_pointp (D2D1_POINT_2F locationDips, int round_style) const
	{
		return pointd_to_pointp(locationDips.x, locationDips.y, round_style);
	}

	D2D1_SIZE_F hwnd_i::sizep_to_sized(SIZE sizep) const
	{
		auto dpi = this->dpi();
		return D2D1_SIZE_F{ sizep.cx * 96.0f / dpi, sizep.cy * 96.0f / dpi };
	}

	SIZE hwnd_i::sized_to_sizep (float width, float height, int round_style) const
	{
		return { lengthd_to_lengthp(width, round_style), lengthd_to_lengthp(height, round_style) };
	}

	SIZE hwnd_i::sized_to_sizep (D2D1_SIZE_F size, int round_style) const
	{
		return { lengthd_to_lengthp(size.width, round_style), lengthd_to_lengthp(size.height, round_style) };
	}

	D2D1_RECT_F hwnd_i::rectp_to_rectd (const RECT& rp) const
	{
		auto tl = pointp_to_pointd(rp.left, rp.top);
		auto br = pointp_to_pointd(rp.right, rp.bottom);
		return { tl.x, tl.y, br.x, br.y };
	}

	RECT hwnd_i::rectd_to_rectp (const D2D1_RECT_F& rd, int round_style) const
	{
		auto tl = pointd_to_pointp(rd.left, rd.top, -round_style);
		auto br = pointd_to_pointp(rd.right, rd.bottom, round_style);
		return { tl.x, tl.y, br.x, br.y };
	}

	void hwnd_i::invalidate (const D2D1_RECT_F& rect)
	{
		auto tl = this->pointd_to_pointp (rect.left, rect.top, -1);
		auto br = this->pointd_to_pointp (rect.right, rect.bottom, 1);
		RECT rectp = { tl.x, tl.y, br.x, br.y };
		invalidate (rectp);
	}
}
