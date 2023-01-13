
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "win32_window_i.h"
#include "rassert.h"
#include "utility_functions.h"

namespace edge
{
	using hwnd_i = win32_window_i;

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

	float hwnd_i::client_width() const
	{
		return client_width_pixels() * 96.0f / edge::dpi(hwnd());
	}

	float hwnd_i::client_height() const
	{
		return client_height_pixels() * 96.0f / edge::dpi(hwnd());
	}

	D2D1_SIZE_F hwnd_i::sizep_to_sized(SIZE sizep) const
	{
		auto dpi = edge::dpi(hwnd());
		return D2D1_SIZE_F{ sizep.cx * 96.0f / dpi, sizep.cy * 96.0f / dpi };
	}

	SIZE hwnd_i::sized_to_sizep (float width, float height, int round_style) const
	{
		uint32_t dpi = edge::dpi(hwnd());
		return { lengthd_to_lengthp(width, dpi, round_style), lengthd_to_lengthp(height, dpi, round_style) };
	}

	SIZE hwnd_i::sized_to_sizep (D2D1_SIZE_F size, int round_style) const
	{
		uint32_t dpi = edge::dpi(hwnd());
		return { lengthd_to_lengthp(size.width, dpi, round_style), lengthd_to_lengthp(size.height, dpi, round_style) };
	}

	D2D1_RECT_F hwnd_i::rectp_to_rectd (const RECT& rp) const
	{
		uint32_t dpi = edge::dpi(hwnd());
		auto tl = pointp_to_pointd(rp.left, rp.top, dpi);
		auto br = pointp_to_pointd(rp.right, rp.bottom, dpi);
		return { tl.x, tl.y, br.x, br.y };
	}
}
