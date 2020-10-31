
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "window.h"

namespace edge
{
	window::window (const WNDCLASSEX& wcex, DWORD ex_style, DWORD style, HWND parent, int x, int y, int width, int height)
	{
		auto hm = (HINSTANCE)&__ImageBase;

		WNDCLASSEX copy;
		if (!::GetClassInfoExW(hm, wcex.lpszClassName, &copy))
		{
			copy = wcex;
			copy.cbSize = sizeof(copy);
			copy.lpfnWndProc = window_proc_static;
			copy.hInstance = hm;
			auto atom = RegisterClassEx (&copy);
			rassert (atom != 0);
		}

		_hwnd = ::CreateWindowEx (ex_style, wcex.lpszClassName, L"", style,
			x, y, width, height, parent, nullptr, hm, nullptr); rassert(_hwnd);

		SetWindowLongPtr (_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	}

	window::window (const WNDCLASSEX& wcex, DWORD ex_style, DWORD style, HWND parent, const RECT& rect)
		: window(wcex, ex_style, style, parent, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top)
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

