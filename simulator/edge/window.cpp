
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "window.h"

namespace edge
{
	window::window (DWORD ex_style, DWORD style, HWND parent, int x, int y, int width, int height)
	{
		HMODULE hm;
		DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
		BOOL bRes = ::GetModuleHandleEx (flags, (LPCWSTR)&window_proc_static, &hm); rassert(bRes);

		static const wchar_t class_name[] = L"window-{6DF4008F-06D8-4FD3-A511-F346411EFFCE}";

		WNDCLASSEX wcex;
		if (!::GetClassInfoExW(hm, class_name, &wcex))
		{
			wcex.cbSize = sizeof(wcex);
			wcex.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
			wcex.lpfnWndProc = window_proc_static;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hm;
			wcex.hIcon = nullptr;
			wcex.hCursor = LoadCursor (nullptr, IDC_ARROW);
			wcex.hbrBackground = nullptr;
			wcex.lpszMenuName = nullptr;
			wcex.lpszClassName = class_name;
			wcex.hIconSm = nullptr;
			auto atom = RegisterClassEx (&wcex); rassert (atom != 0);
		}

		_hwnd = ::CreateWindowExW (ex_style, class_name, L"", style,
			x, y, width, height, parent, nullptr, hm, nullptr); rassert(_hwnd);

		SetWindowLongPtr (_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	}

	window::window (DWORD ex_style, DWORD style, HWND parent, const RECT& rect)
		: window(ex_style, style, parent, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top)
	{ }

	window::~window()
	{
		rassert(_hwnd);
		::SetWindowLongPtr (_hwnd, GWLP_USERDATA, 0);
		::DestroyWindow(_hwnd);
	}

	LRESULT CALLBACK window::window_proc_static (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (!assert_function_running)
		{
			if (auto w = reinterpret_cast<window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA)))
			{
				rassert(hwnd == w->_hwnd);
				std::optional<LRESULT> result = w->_em.event_invoker<window_proc_e>()(hwnd, msg, wparam, lparam);
				if (result)
					return result.value();
			}
		}

		return DefWindowProc (hwnd, msg, wparam, lparam);
	};
}

