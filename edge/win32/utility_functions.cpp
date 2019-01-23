
#include "pch.h"
#include "utility_functions.h"
#include "win32_lib.h"

using namespace D2D1;

bool operator== (const D2D1_RECT_F& a, const D2D1_RECT_F& b)
{
	return memcmp (&a, &b, sizeof(D2D1_RECT_F)) == 0;
}

bool operator!= (const D2D1_RECT_F& a, const D2D1_RECT_F& b)
{
	return memcmp (&a, &b, sizeof(D2D1_RECT_F)) != 0;
}

namespace edge
{
	bool HitTestLine (const zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance, D2D1_POINT_2F p0w, D2D1_POINT_2F p1w, float lineWidth)
	{
		auto fd = zoomable->pointw_to_pointd(p0w);
		auto td = zoomable->pointw_to_pointd(p1w);

		float halfw = zoomable->lengthw_to_lengthd(lineWidth) / 2.0f;
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

	bool point_in_rect(const D2D1_RECT_F& rect, D2D1_POINT_2F location)
	{
		return (location.x >= rect.left) && (location.x < rect.right) && (location.y >= rect.top) && (location.y < rect.bottom);
	}

	bool point_in_polygon(const std::array<D2D1_POINT_2F, 4>& vertices, D2D1_POINT_2F point)
	{
		// Taken from http://stackoverflow.com/a/2922778/451036
		bool c = false;
		size_t vertexCount = 4;
		for (size_t i = 0, j = vertexCount - 1; i < vertexCount; j = i++)
		{
			if (((vertices[i].y > point.y) != (vertices[j].y > point.y)) &&
				(point.x < (vertices[j].x - vertices[i].x) * (point.y - vertices[i].y) / (vertices[j].y - vertices[i].y) + vertices[i].x))
				c = !c;
		}

		return c;
	}

	D2D1_RECT_F InflateRect (const D2D1_RECT_F& rect, float distance)
	{
		auto result = rect;
		InflateRect (&result, distance);
		return result;
	}

	void InflateRect (D2D1_RECT_F* rect, float distance)
	{
		rect->left -= distance;
		rect->top -= distance;
		rect->right += distance;
		rect->bottom += distance;
	}

	D2D1_ROUNDED_RECT InflateRoundedRect (const D2D1_ROUNDED_RECT& rr, float distance)
	{
		D2D1_ROUNDED_RECT result = rr;
		InflateRoundedRect (&result, distance);
		return result;
	}

	void InflateRoundedRect (D2D1_ROUNDED_RECT* rr, float distance)
	{
		InflateRect (&rr->rect, distance);

		rr->radiusX += distance;
		if (rr->radiusX < 0)
			rr->radiusX = 0;

		rr->radiusY += distance;
		if (rr->radiusY < 0)
			rr->radiusY = 0;
	}

	ColorF GetD2DSystemColor (int sysColorIndex)
	{
		DWORD brg = GetSysColor (sysColorIndex);
		DWORD rgb = ((brg & 0xff0000) >> 16) | (brg & 0xff00) | ((brg & 0xff) << 16);
		return ColorF (rgb);
	}

	std::string get_window_text (HWND hwnd)
	{
		int char_count = ::GetWindowTextLength(hwnd);
		auto wstr = std::make_unique<wchar_t[]>(char_count + 1);
		::GetWindowTextW (hwnd, wstr.get(), char_count + 1);
		int size_bytes = WideCharToMultiByte (CP_UTF8, 0, wstr.get(), (int) char_count, nullptr, 0, nullptr, nullptr);
		auto str = std::string (size_bytes, 0);
		WideCharToMultiByte (CP_UTF8, 0, wstr.get(), (int) char_count, str.data(), size_bytes, nullptr, nullptr);
		return str;
	};

	std::array<D2D1_POINT_2F, 4> corners (const D2D1_RECT_F& rect)
	{
		return {
			D2D1_POINT_2F{ rect.left, rect.top },
			D2D1_POINT_2F{ rect.right, rect.top },
			D2D1_POINT_2F{ rect.right, rect.bottom },
			D2D1_POINT_2F{ rect.left, rect.bottom },
		};
	}

	D2D1_RECT_F polygon_bounds (const std::array<D2D1_POINT_2F, 4>& points)
	{
		D2D1_RECT_F r = { points[0].x, points[0].y, points[0].x, points[0].y };
		
		for (size_t i = 1; i < 4; i++)
		{
			r.left   = std::min (r.left  , points[i].x);
			r.top    = std::min (r.top   , points[i].y);
			r.right  = std::max (r.right , points[i].x);
			r.bottom = std::max (r.bottom, points[i].y);
		}

		return r;
	}
}
