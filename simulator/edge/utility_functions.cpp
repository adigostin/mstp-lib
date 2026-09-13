
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "com.h"
#include "edge_d2d.h"

#pragma comment (lib, "Shlwapi")
#pragma comment (lib, "Shell32")

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
	bool point_in_rect(const D2D1_RECT_F& rect, D2D1_POINT_2F pt)
	{
		return (pt.x >= rect.left) && (pt.x < rect.right) && (pt.y >= rect.top) && (pt.y < rect.bottom);
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

	D2D1_RECT_F inflate (const D2D1_RECT_F& rect, float distance)
	{
		auto result = rect;
		inflate (&result, distance);
		return result;
	}

	void inflate (D2D1_RECT_F* rect, float distance)
	{
		rect->left -= distance;
		rect->top -= distance;
		rect->right += distance;
		rect->bottom += distance;
	}

	D2D1_RECT_F inflate (D2D1_POINT_2F p, float distance)
	{
		return { p.x - distance, p.y - distance, p.x + distance, p.y + distance };
	}

	D2D1_ROUNDED_RECT inflate (const D2D1_ROUNDED_RECT& rr, float distance)
	{
		D2D1_ROUNDED_RECT result = rr;
		inflate (&result, distance);
		return result;
	}

	void inflate (D2D1_ROUNDED_RECT* rr, float distance)
	{
		inflate (&rr->rect, distance);

		rr->radiusX += distance;
		if (rr->radiusX < 0)
			rr->radiusX = 0;

		rr->radiusY += distance;
		if (rr->radiusY < 0)
			rr->radiusY = 0;
	}

	std::array<D2D1_POINT_2F, 4> corners (const D2D1_RECT_F& rect)
	{
		return {
			D2D1_POINT_2F{ rect.left, rect.top },
			D2D1_POINT_2F{ rect.right, rect.top },
			D2D1_POINT_2F{ rect.right, rect.bottom },
			D2D1_POINT_2F{ rect.left, rect.bottom },
		};
	}

	D2D1_POINT_2F center (const D2D1_RECT_F& r)
	{
		return { (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
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

	D2D1_COLOR_F interpolate (const D2D1_COLOR_F& first, const D2D1_COLOR_F& second, uint32_t percent_first)
	{
		_ASSERT (percent_first <= 100);
		float r = (first.r * percent_first + second.r * (100 - percent_first)) / 100;
		float g = (first.g * percent_first + second.g * (100 - percent_first)) / 100;
		float b = (first.b * percent_first + second.b * (100 - percent_first)) / 100;
		float a = (first.a * percent_first + second.a * (100 - percent_first)) / 100;
		return { r, g, b, a };
	}

	uint32_t interpolate (uint32_t first, uint32_t second, uint32_t percent_first)
	{
		_ASSERT (percent_first <= 100);
		static constexpr auto a = [](uint32_t argb) -> uint32_t { return (argb >> 24) & 0xff; };
		static constexpr auto r = [](uint32_t argb) -> uint32_t { return (argb >> 16) & 0xff; };
		static constexpr auto g = [](uint32_t argb) -> uint32_t { return (argb >> 8) & 0xff; };
		static constexpr auto b = [](uint32_t argb) -> uint32_t { return argb & 0xff; };
		uint32_t aa = (a(first) * percent_first + a(second) * (100 - percent_first)) / 100;
		uint32_t rr = (r(first) * percent_first + r(second) * (100 - percent_first)) / 100;
		uint32_t gg = (g(first) * percent_first + g(second) * (100 - percent_first)) / 100;
		uint32_t bb = (b(first) * percent_first + b(second) * (100 - percent_first)) / 100;
		return (aa << 24) | (rr << 16) | (gg << 8) | bb;
	}

	D2D1_RECT_F align_to_pixel (const D2D1_RECT_F& rect, uint32_t dpi)
	{
		float pixel_width = 96.0f / dpi;
		D2D1_RECT_F result;
		result.left   = round(rect.left   / pixel_width) * pixel_width;
		result.top    = round(rect.top    / pixel_width) * pixel_width;
		result.right  = round(rect.right  / pixel_width) * pixel_width;
		result.bottom = round(rect.bottom / pixel_width) * pixel_width;
		return result;
	}

	D2D1_RECT_F make_positive (const D2D1_RECT_F& rect)
	{
		float l = std::min (rect.left, rect.right);
		float t = std::min (rect.top,  rect.bottom);
		float r = std::max (rect.left, rect.right);
		float b = std::max (rect.top,  rect.bottom);
		return { l, t, r, b };
	}

	D2D1_RECT_F union_rects (const D2D1_RECT_F& one, const D2D1_RECT_F& other)
	{
		auto aa = make_positive(one);
		auto bb = make_positive(other);
		float l = std::min (aa.left, bb.left);
		float t = std::min (aa.top, bb.top);
		float r = std::max (aa.right, bb.right);
		float b = std::max (aa.bottom, bb.bottom);
		return { l, t, r, b };
	}

	bool hit_test_line (D2D1_POINT_2F dLocation, float tolerance, D2D1_POINT_2F p0, D2D1_POINT_2F p1, float line_width)
	{
		auto fd = p0;
		auto td = p1;

		float halfw = line_width / 2.0f;
		if (halfw < tolerance)
			halfw = tolerance;

		float angle = atan2(td - fd);
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

	bool dragged_threshold (POINT from, POINT to, uint32_t dpi)
	{
		LONG cxdrag, cydrag;
		if (auto pa = ::GetProcAddress(::GetModuleHandle(L"user32.dll"), "GetSystemMetricsForDpi"))
		{
			auto gsm = reinterpret_cast<int(WINAPI*)(int, UINT)>(pa);
			cxdrag = gsm(SM_CXDRAG, dpi);
			cydrag = gsm(SM_CYDRAG, dpi);
		}
		else
		{
			cxdrag = ::GetSystemMetrics(SM_CXDRAG);
			cydrag = ::GetSystemMetrics(SM_CYDRAG);
		}

		RECT rect = { from.x, from.y, from.x, from.y };
		::InflateRect (&rect, cxdrag, cydrag);

		return !::PtInRect(&rect, to);
	}

	// ========================================================================

	enum class OpenOrSave { Open, Save };

	static HRESULT PickFilePath (OpenOrSave which, HWND hWndParent, PCWSTR initialPath,
								 std::span<const COMDLG_FILTERSPEC> fileTypes,
								 PCWSTR fileExtNoDot, BSTR* pbstrPath)
	{
		com_ptr<IFileDialog> dialog;
		HRESULT hr = CoCreateInstance ((which == OpenOrSave::Save) ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,
									   nullptr, CLSCTX_ALL, __uuidof(dialog), (void**) &dialog); RETURN_IF_FAILED(hr);

		//DWORD options;
		//hr = dialog->GetOptions (&options); _ASSERT(SUCCEEDED(hr));
		//hr = dialog->SetOptions (options | FOS_FORCEFILESYSTEM); _ASSERT(SUCCEEDED(hr));
		hr = dialog->SetFileTypes ((UINT)fileTypes.size(), fileTypes.data()); RETURN_IF_FAILED(hr);

		hr = dialog->SetDefaultExtension (fileExtNoDot); RETURN_IF_FAILED(hr);

		if (initialPath && initialPath[0])
		{
			auto filePtr = PathFindFileName(initialPath);
			hr = dialog->SetFileName(filePtr); RETURN_IF_FAILED(hr);
			auto dir = wil::make_process_heap_string_nothrow(initialPath, filePtr - initialPath); RETURN_IF_NULL_ALLOC(dir);
			com_ptr<IShellItem> si;
			hr = SHCreateItemFromParsingName (dir.get(), nullptr, IID_PPV_ARGS(&si));
			if (SUCCEEDED(hr))
				dialog->SetFolder(si);
		}

		hr = dialog->Show(hWndParent);
		if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
			return hr;
		RETURN_IF_FAILED(hr);

		com_ptr<IShellItem> item;
		hr = dialog->GetResult (&item); RETURN_IF_FAILED(hr);

		wil::unique_cotaskmem_string filePath;
		hr = item->GetDisplayName (SIGDN_FILESYSPATH, &filePath); RETURN_IF_FAILED(hr);

		SHAddToRecentDocs(SHARD_PATHW, filePath.get());

		*pbstrPath = SysAllocString(filePath.get()); RETURN_IF_NULL_ALLOC(*pbstrPath);
		return S_OK;
	}

	HRESULT PickOpenPath (HWND hWndParent, PCWSTR initialPath,
						  std::span<const COMDLG_FILTERSPEC> fileTypes,
						  PCWSTR fileExtNoDot, BSTR* pbstrPath)
	{
		return PickFilePath (OpenOrSave::Open, hWndParent, initialPath, fileTypes, fileExtNoDot, pbstrPath);
	}

	HRESULT PickSavePath (HWND hWndParent, PCWSTR initialPath,
						  std::span<const COMDLG_FILTERSPEC> fileTypes,
						  PCWSTR fileExtNoDot, BSTR* pbstrPath)
	{
		return PickFilePath (OpenOrSave::Save, hWndParent, initialPath, fileTypes, fileExtNoDot, pbstrPath);
	}

	// ========================================================================

	uint32_t dpi (HWND hwnd)
	{
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

	D2D1::Matrix3x2F dpi_transform(uint32_t dpi)
	{
		return { dpi / 96.0f, 0, 0, dpi / 96.0f, 0, 0 };
	}

	D2D1::Matrix3x2F dpi_transform (HWND hwnd)
	{
		auto dpi = edge::dpi(hwnd);
		return dpi_transform(dpi);
	}

	float pixel_width (uint32_t dpi)
	{
		return 96.0f / dpi;
	}

	float pixel_width(HWND hwnd)
	{
		return pixel_width(dpi(hwnd));
	}

	LONG lengthd_to_lengthp (float lengthd, uint32_t dpi, int round_style)
	{
		if (round_style < 0)
			return (LONG) std::floorf(lengthd / 96.0f * dpi);

		if (round_style > 0)
			return (LONG) std::ceilf(lengthd / 96.0f * dpi);

		return (LONG) std::roundf(lengthd / 96.0f * dpi);
	}

	float lengthp_to_lengthd (LONG lengthp, uint32_t dpi)
	{
		return lengthp * 96.0f / dpi;
	}

	D2D1_POINT_2F pointp_to_pointd (POINT p, uint32_t dpi)
	{
		return { p.x * 96.0f / dpi, p.y * 96.0f / dpi };
	}

	D2D1_POINT_2F pointp_to_pointd (LONG xPixels, LONG yPixels, uint32_t dpi)
	{
		return { xPixels * 96.0f / dpi, yPixels * 96.0f / dpi };
	}

	POINT pointd_to_pointp (float xDips, float yDips, uint32_t dpi, int round_style)
	{
		if (round_style < 0)
			return { (int)std::floor(xDips / 96.0f * dpi), (int)std::floor(yDips / 96.0f * dpi) };

		if (round_style > 0)
			return { (int)std::ceil(xDips / 96.0f * dpi), (int)std::ceil(yDips / 96.0f * dpi) };

		return { (int)std::round(xDips / 96.0f * dpi), (int)std::round(yDips / 96.0f * dpi) };
	}

	POINT pointd_to_pointp (D2D1_POINT_2F locationDips, uint32_t dpi, int round_style)
	{
		return pointd_to_pointp(locationDips.x, locationDips.y, dpi, round_style);
	}

	SIZE client_size_pixels (HWND hwnd)
	{
		RECT r;
		BOOL bres = ::GetClientRect(hwnd, &r); _ASSERT(bres);
		return { r.right, r.bottom };
	}

	RECT client_rect_pixels (HWND hwnd)
	{
		RECT r;
		BOOL bres = ::GetClientRect(hwnd, &r); _ASSERT(bres);
		return r;
	}

	D2D1_RECT_F client_rect (HWND hwnd)
	{
		RECT rect = client_rect_pixels(hwnd);
		auto dpi = edge::dpi(hwnd);
		D2D1_RECT_F res;
		res.left   = rect.left   * 96.0f / dpi;
		res.top    = rect.top    * 96.0f / dpi;
		res.right  = rect.right  * 96.0f / dpi;
		res.bottom = rect.bottom * 96.0f / dpi;
		return res;
	}

	D2D1_SIZE_F client_size (HWND hwnd)
	{
		SIZE cs = client_size_pixels(hwnd);
		auto dpi = edge::dpi(hwnd);
		float width = cs.cx * 96.0f / dpi;
		float height = cs.cy * 96.0f / dpi;
		return { width, height };
	}

	RECT rectd_to_rectp (const D2D1_RECT_F& rd, uint32_t dpi, int round_style)
	{
		auto tl = pointd_to_pointp(rd.left, rd.top, dpi, -round_style);
		auto br = pointd_to_pointp(rd.right, rd.bottom, dpi, round_style);
		return { tl.x, tl.y, br.x, br.y };
	}

	void invalidate (const D2D1_RECT_F& rect, HWND hwnd)
	{
		uint32_t dpi = edge::dpi(hwnd);
		auto tl = pointd_to_pointp (rect.left, rect.top, dpi, -1);
		auto br = pointd_to_pointp (rect.right, rect.bottom, dpi, 1);
		RECT rectp = { tl.x, tl.y, br.x, br.y };
		::InvalidateRect (hwnd, &rectp, TRUE);
	}
}
