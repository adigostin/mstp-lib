
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "utility_functions.h"
#include "com_ptr.h"

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

	bool point_in_rect (const RECT& rect, POINT pt)
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

	D2D1_COLOR_F interpolate (const D2D1_COLOR_F& first, const D2D1_COLOR_F& second, uint32_t percent_first)
	{
		rassert (percent_first <= 100);
		float r = (first.r * percent_first + second.r * (100 - percent_first)) / 100;
		float g = (first.g * percent_first + second.g * (100 - percent_first)) / 100;
		float b = (first.b * percent_first + second.b * (100 - percent_first)) / 100;
		float a = (first.a * percent_first + second.a * (100 - percent_first)) / 100;
		return { r, g, b, a };
	}

	uint32_t interpolate (uint32_t first, uint32_t second, uint32_t percent_first)
	{
		rassert (percent_first <= 100);
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

	std::wstring utf8_to_utf16 (std::string_view str_utf8)
	{
		if (str_utf8.empty())
			return { };

		int char_count = MultiByteToWideChar (CP_UTF8, 0, str_utf8.data(), (int)str_utf8.size(), nullptr, 0);
		std::wstring wide (char_count, 0);
		MultiByteToWideChar (CP_UTF8, 0, str_utf8.data(), (int)str_utf8.size(), wide.data(), char_count);
		return wide;
	}

	std::string utf16_to_utf8 (std::wstring_view str_utf16)
	{
		if (str_utf16.empty())
			return { };

		int char_count = WideCharToMultiByte (CP_UTF8, 0, str_utf16.data(), (int) str_utf16.size(), nullptr, 0, nullptr, nullptr);
		std::string str (char_count, 0);
		WideCharToMultiByte (CP_UTF8, 0, str_utf16.data(), (int) str_utf16.size(), str.data(), char_count, nullptr, nullptr);
		return str;
	}

	std::string bstr_to_utf8 (BSTR bstr)
	{
		size_t char_count = SysStringLen(bstr);
		return utf16_to_utf8 ({ bstr, char_count });
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

	// ========================================================================

	enum class open_or_save { open, save };

	static std::wstring try_choose_file_path (open_or_save which, HWND parent_hwnd, const wchar_t* initial_path,
		std::span<const COMDLG_FILTERSPEC> file_types, const wchar_t* file_extension_without_dot)
	{
		com_ptr<IFileDialog> dialog;
		HRESULT hr = CoCreateInstance ((which == open_or_save::save) ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, __uuidof(dialog), (void**) &dialog);
		throw_if_failed(hr);

		//DWORD options;
		//hr = dialog->GetOptions (&options); rassert_hr(hr);
		//hr = dialog->SetOptions (options | FOS_FORCEFILESYSTEM); rassert_hr(hr);
		hr = dialog->SetFileTypes ((UINT)file_types.size(), file_types.data());
		throw_if_failed(hr);

		hr = dialog->SetDefaultExtension (file_extension_without_dot);
		throw_if_failed(hr);

		if ((initial_path != nullptr) && (initial_path[0] != 0))
		{
			auto filePtr = PathFindFileName(initial_path);
			hr = dialog->SetFileName(filePtr);
			throw_if_failed(hr);

			std::wstring dir (initial_path, filePtr - initial_path);
			com_ptr<IShellItem> si;
			hr = SHCreateItemFromParsingName (dir.c_str(), nullptr, IID_PPV_ARGS(&si));
			if (SUCCEEDED(hr))
				dialog->SetFolder(si);
		}

		hr = dialog->Show(parent_hwnd);
		if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
			throw canceled_by_user_exception();
		throw_if_failed(hr);

		com_ptr<IShellItem> item;
		hr = dialog->GetResult (&item);
		throw_if_failed(hr);

		co_task_mem_ptr<wchar_t> filePath;
		hr = item->GetDisplayName (SIGDN_FILESYSPATH, &filePath);
		throw_if_failed(hr);

		SHAddToRecentDocs(SHARD_PATHW, filePath.get());
		return std::wstring(filePath.get());
	}

	std::wstring try_choose_open_path (HWND parent_hwnd, const wchar_t* initial_path,
		std::span<const COMDLG_FILTERSPEC> file_types, const wchar_t* file_extension_without_dot)
	{
		return try_choose_file_path (open_or_save::open, parent_hwnd, initial_path, file_types, file_extension_without_dot);
	}

	std::wstring try_choose_save_path (HWND parent_hwnd, const wchar_t* initial_path,
		std::span<const COMDLG_FILTERSPEC> file_types, const wchar_t* file_extension_without_dot)
	{
		return try_choose_file_path (open_or_save::save, parent_hwnd, initial_path, file_types, file_extension_without_dot);
	}

	// ========================================================================

	bool ask_save_discard_cancel (HWND parent_hwnd, const wchar_t* ask_text, const wchar_t* title_text)
	{
		static const TASKDIALOG_BUTTON buttons[] =
		{
			{ IDYES, L"Save Changes" },
			{ IDNO, L"Discard Changes" },
			{ IDCANCEL, L"Cancel" },
		};

		TASKDIALOGCONFIG tdc = { sizeof (tdc) };
		tdc.hwndParent = parent_hwnd;
		tdc.pszWindowTitle = title_text;
		tdc.pszMainIcon = TD_WARNING_ICON;
		tdc.pszMainInstruction = L"File was changed";
		tdc.pszContent = ask_text;
		tdc.cButtons = _countof(buttons);
		tdc.pButtons = buttons;
		tdc.nDefaultButton = IDOK;

		int pressedButton;
		auto hr = TaskDialogIndirect (&tdc, &pressedButton, nullptr, nullptr);
		throw_if_failed(hr);

		if (pressedButton == IDCANCEL)
			throw canceled_by_user_exception();

		return pressedButton == IDYES;
	}
}
