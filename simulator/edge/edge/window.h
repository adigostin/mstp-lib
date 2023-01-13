
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "win32_window_i.h"

namespace edge
{
	std::unique_ptr<win32_window_i> make_window (const WNDCLASSEX& wcex, DWORD ex_style, DWORD style, HWND parent, int x, int y, int width, int height);
	std::unique_ptr<win32_window_i> make_window (const WNDCLASSEX& wcex, DWORD ex_style, DWORD style, HWND parent, const RECT& rect);
}
