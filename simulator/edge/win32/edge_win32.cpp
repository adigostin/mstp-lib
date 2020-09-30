
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "edge_win32.h"
#include "utility_functions.h"

namespace edge
{
	#pragma region win32_window_i interface
	bool win32_window_i::visible() const
	{
		return (GetWindowLongPtr (hwnd(), GWL_STYLE) & WS_VISIBLE) != 0;
	}

	RECT win32_window_i::client_rect_pixels() const
	{
		RECT rect;
		BOOL bRes = ::GetClientRect (hwnd(), &rect); assert(bRes);
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

	RECT win32_window_i::GetRect() const
	{
		auto hwnd = this->hwnd();
		auto parent = ::GetParent(hwnd); assert (parent != nullptr);
		RECT rect;
		BOOL bRes = ::GetWindowRect (hwnd, &rect); assert(bRes);
		MapWindowPoints (HWND_DESKTOP, parent, (LPPOINT) &rect, 2);
		return rect;
	}

	POINT win32_window_i::GetLocation() const
	{
		auto rect = GetRect();
		return { rect.left, rect.top };
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
		assert(bRes);
	}

	void win32_window_i::invalidate()
	{
		::InvalidateRect (hwnd(), nullptr, FALSE);
	}

	void win32_window_i::invalidate (const RECT& rect)
	{
		::InvalidateRect (hwnd(), &rect, FALSE);
	}
	#pragma endregion

	#pragma region dpi_aware_i
	D2D1::Matrix3x2F dpi_aware_window_i::dpi_transform() const
	{
		auto dpi = this->dpi();
		return { dpi / 96.0f, 0, 0, dpi / 96.0f, 0, 0 };
	}

	float dpi_aware_window_i::pixel_width() const
	{
		return 96.0f / dpi();
	}

	D2D1_RECT_F dpi_aware_window_i::client_rect() const
	{
		RECT rect = this->client_rect_pixels();
		float dpi = (float)this->dpi();
		D2D1_RECT_F res;
		res.left   = rect.left   * 96.0f / dpi;
		res.top    = rect.top    * 96.0f / dpi;
		res.right  = rect.right  * 96.0f / dpi;
		res.bottom = rect.bottom * 96.0f / dpi;
		return res;
	}

	D2D1_SIZE_F dpi_aware_window_i::client_size() const
	{
		SIZE cs = client_size_pixels();
		float width = cs.cx * 96.0f / dpi();
		float height = cs.cy * 96.0f / dpi();
		return { width, height };
	}

	float dpi_aware_window_i::client_width() const
	{
		return client_width_pixels() * 96.0f / dpi();
	}

	float dpi_aware_window_i::client_height() const
	{
		return client_height_pixels() * 96.0f / dpi();
	}

	float dpi_aware_window_i::lengthp_to_lengthd (LONG lengthp) const
	{
		return lengthp * 96.0f / dpi();
	}

	LONG dpi_aware_window_i::lengthd_to_lengthp (float lengthd, int round_style) const
	{
		if (round_style < 0)
			return (LONG) std::floorf(lengthd / 96.0f * dpi());

		if (round_style > 0)
			return (LONG) std::ceilf(lengthd / 96.0f * dpi());

		return (LONG) std::roundf(lengthd / 96.0f * dpi());
	}

	D2D1_POINT_2F dpi_aware_window_i::pointp_to_pointd (POINT p) const
	{
		return { p.x * 96.0f / dpi(), p.y * 96.0f / dpi() };
	}

	D2D1_POINT_2F dpi_aware_window_i::pointp_to_pointd (long xPixels, long yPixels) const
	{
		return { xPixels * 96.0f / dpi(), yPixels * 96.0f / dpi() };
	}

	POINT dpi_aware_window_i::pointd_to_pointp (float xDips, float yDips, int round_style) const
	{
		if (round_style < 0)
			return { (int)std::floor(xDips / 96.0f * dpi()), (int)std::floor(yDips / 96.0f * dpi()) };

		if (round_style > 0)
			return { (int)std::ceil(xDips / 96.0f * dpi()), (int)std::ceil(yDips / 96.0f * dpi()) };

		return { (int)std::round(xDips / 96.0f * dpi()), (int)std::round(yDips / 96.0f * dpi()) };
	}

	POINT dpi_aware_window_i::pointd_to_pointp (D2D1_POINT_2F locationDips, int round_style) const
	{
		return pointd_to_pointp(locationDips.x, locationDips.y, round_style);
	}

	D2D1_SIZE_F dpi_aware_window_i::sizep_to_sized(SIZE sizep) const
	{
		return D2D1_SIZE_F{ sizep.cx * 96.0f / dpi(), sizep.cy * 96.0f / dpi() };
	}

	D2D1_RECT_F dpi_aware_window_i::rectp_to_rectd (RECT rp) const
	{
		auto tl = pointp_to_pointd(rp.left, rp.top);
		auto br = pointp_to_pointd(rp.right, rp.bottom);
		return { tl.x, tl.y, br.x, br.y };
	}

	void dpi_aware_window_i::invalidate (const D2D1_RECT_F& rect)
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

	bool zoomable_window_i::hit_test_line (D2D1_POINT_2F dLocation, float tolerance, D2D1_POINT_2F p0w, D2D1_POINT_2F p1w, float lineWidth) const
	{
		auto fd = this->pointw_to_pointd(p0w);
		auto td = this->pointw_to_pointd(p1w);

		float halfw = this->lengthw_to_lengthd(lineWidth) / 2.0f;
		if (halfw < tolerance)
			halfw = tolerance;

		float angle = atan2(td.y - fd.y, td.x - fd.x);
		float s = sin(angle);
		float c = cos(angle);

		std::array<D2D1_POINT_2F, 4> vertices =
		{
			D2D1_POINT_2F { fd.x + s * halfw, fd.y - c * halfw },
			D2D1_POINT_2F { fd.x - s * halfw, fd.y + c * halfw },
			D2D1_POINT_2F { td.x - s * halfw, td.y + c * halfw },
			D2D1_POINT_2F { td.x + s * halfw, td.y - c * halfw }
		};

		return point_in_polygon (vertices, dLocation);
	}
	#pragma endregion
}
