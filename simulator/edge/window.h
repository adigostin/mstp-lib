
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "win32_window_i.h"

namespace edge
{
	class window
	{
	public:
		window (const WNDCLASSEX& wcex, DWORD ex_style, DWORD style, HWND parent, int x, int y, int width, int height);
		window (const WNDCLASSEX& wcex, DWORD ex_style, DWORD style, HWND parent, const RECT& rect);

		window (const window&) = delete;
		window& operator= (const window&) = delete;

		~window();

		HWND hwnd() const { return _hwnd; }

		window_proc_e::subscriber window_proc() { return window_proc_e::subscriber(_em); }

	private:
		event_manager _em;
		HWND _hwnd;

		static LRESULT CALLBACK window_proc_static (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	};
}
