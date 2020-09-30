
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "window.h"

using namespace edge;

static HINSTANCE GetHInstance()
{
	HMODULE hm;
	BOOL bRes = ::GetModuleHandleEx (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&GetHInstance, &hm);
	assert(bRes);
	return hm;
}

void window::register_class (HINSTANCE hInstance, const wnd_class_params& class_params)
{
	WNDCLASSEX wcex;
	BOOL bRes = ::GetClassInfoEx (hInstance, class_params.lpszClassName, &wcex);
	if (!bRes)
	{
		wcex.cbSize = sizeof(wcex);
		wcex.style = class_params.style;
		wcex.lpfnWndProc = &window_proc_static;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = class_params.lpIconName ? ::LoadIcon(hInstance, class_params.lpIconName) : nullptr;
		wcex.hCursor = LoadCursor (nullptr, IDC_ARROW);
		wcex.hbrBackground = nullptr;
		wcex.lpszMenuName = class_params.lpszMenuName;
		wcex.lpszClassName = class_params.lpszClassName;
		wcex.hIconSm = class_params.lpIconSmName ? ::LoadIcon(hInstance, class_params.lpIconSmName) : nullptr;
		auto atom = RegisterClassEx (&wcex); assert (atom != 0);
	}
}

window::window (const wnd_class_params& class_params, DWORD exStyle, DWORD style, int x, int y, int width, int height, HWND hWndParent, HMENU hMenu)
{
	register_class (GetHInstance(), class_params);

	auto hwnd = ::CreateWindowEx (exStyle, class_params.lpszClassName, L"", style, x, y, width, height, hWndParent, hMenu, GetHInstance(), this); assert (hwnd != nullptr);
	assert (hwnd == _hwnd);
}

static const wnd_class_params child_wnd_class_params =
{
	L"window-{0F45B203-6AE9-49B0-968A-4006176EDA40}", // lpszClassName
	CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, // style
	nullptr, // lpszMenuName
	nullptr, // lpIconName
	nullptr, // lpIconSmName
};

window::window (DWORD exStyle, DWORD style, const RECT& rect, HWND hWndParent, int child_control_id)
	: window (child_wnd_class_params, exStyle, style,
			  rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
			  hWndParent, (HMENU)(size_t)child_control_id)
{ }

window::~window()
{
	if (_hwnd != nullptr)
		::DestroyWindow(_hwnd);
}

// From http://blogs.msdn.com/b/oldnewthing/archive/2005/04/22/410773.aspx
//static
LRESULT CALLBACK window::window_proc_static (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (assert_function_running)
	{
		// Let's try not to run application code while the assertion dialog is shown. We'll probably mess things up even more.
		return DefWindowProc (hwnd, uMsg, wParam, lParam);
	}

	window* wnd;
	if (uMsg == WM_NCCREATE)
	{
		LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
		wnd = reinterpret_cast<window*>(lpcs->lpCreateParams);
		wnd->_hwnd = hwnd;
		SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(wnd));
	}
	else
		wnd = reinterpret_cast<window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (wnd == nullptr)
	{
		// this must be one of those messages sent before WM_NCCREATE or after WM_NCDESTROY.
		return DefWindowProc (hwnd, uMsg, wParam, lParam);
	}

	auto result = wnd->window_proc (hwnd, uMsg, wParam, lParam);

	if (uMsg == WM_NCDESTROY)
	{
		wnd->_hwnd = nullptr;
		SetWindowLongPtr (hwnd, GWLP_USERDATA, 0);
	}

	if (result.has_value())
		return result.value();

	return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
}

std::optional<LRESULT> window::window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_NCCREATE)
	{
		if (auto proc_addr = GetProcAddress(GetModuleHandleA("User32.dll"), "GetDpiForWindow"))
		{
			auto proc = reinterpret_cast<UINT(WINAPI*)(HWND)>(proc_addr);
			_dpi = (uint32_t)proc(hwnd);
		}
		else
		{
			HDC tempDC = GetDC(hwnd);
			_dpi = (uint32_t)GetDeviceCaps (tempDC, LOGPIXELSX);
			ReleaseDC (hwnd, tempDC);
		}
	}

	if (msg == WM_SIZE)
	{
		SIZE client_size_pixels = { LOWORD(lParam), HIWORD(lParam) };
		D2D1_SIZE_F client_size_dips = sizep_to_sized(client_size_pixels);
		this->on_size_changed(client_size_pixels, client_size_dips);
		return 0;
	}

	if (msg == 0x02E3) // WM_DPICHANGED_AFTERPARENT
	{
		auto proc_addr = GetProcAddress(GetModuleHandleA("User32.dll"), "GetDpiForWindow");
		auto proc = reinterpret_cast<UINT(WINAPI*)(HWND)>(proc_addr);
		_dpi = (uint32_t)proc(hwnd);
		this->on_dpi_changed(_dpi);
		::InvalidateRect (hwnd, nullptr, FALSE);
		return 0;
	}

	if (msg == WM_SETCURSOR)
	{
		if (((HWND)wParam == hwnd) && (LOWORD(lParam) == HTCLIENT))
		{
			POINT pt;
			if (::GetCursorPos(&pt) && ::ScreenToClient (hwnd, &pt))
			{
				if (auto cursor = this->cursor_at(pt, pointp_to_pointd(pt)))
				{
					::SetCursor(cursor);
					return TRUE;
				}
			}
		}

		return std::nullopt;
	}

	if ((msg == WM_LBUTTONDOWN) || (msg == WM_RBUTTONDOWN) || (msg == WM_MBUTTONDOWN))
	{
		::SetFocus(hwnd);
		if (::GetFocus() != hwnd)
			// Another window probably stole the focus back right away.
			return 0;

		if (::GetCapture() != hwnd)
			::SetCapture(hwnd);

		auto button = (msg == WM_LBUTTONDOWN) ? mouse_button::left : ((msg == WM_RBUTTONDOWN) ? mouse_button::right : mouse_button::middle);
		auto mks = (modifier_key)(UINT)wParam | ((::GetKeyState(VK_MENU) < 0) ? modifier_key::alt : modifier_key::none);
		auto pt = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		auto dip = pointp_to_pointd(pt);
		bool handled = this->on_mouse_down(button, mks, pt, dip);
		if (handled)
			return 0;
		return std::nullopt;
	}

	if ((msg == WM_LBUTTONUP) || (msg == WM_RBUTTONUP) || (msg == WM_MBUTTONUP))
	{
		auto mks = (modifier_key)(UINT)wParam | ((::GetKeyState(VK_MENU) < 0) ? modifier_key::alt : modifier_key::none);

		if ((::GetCapture() == hwnd) &&((mks & (modifier_key::lbutton | modifier_key::rbutton | modifier_key::mbutton)) == 0))
			::ReleaseCapture();

		auto button = (msg == WM_LBUTTONUP) ? mouse_button::left : ((msg == WM_RBUTTONUP) ? mouse_button::right : mouse_button::middle);
		auto pt = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		auto dip = pointp_to_pointd(pt);
		bool handled = this->on_mouse_up(button, mks, pt, dip);
		if (handled)
			return 0;
		return std::nullopt;
	}

	if (msg == WM_MOUSEMOVE)
	{
		auto mks = (modifier_key)(UINT)wParam | ((::GetKeyState(VK_MENU) < 0) ? modifier_key::alt : modifier_key::none);
		auto pt = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		auto dip = pointp_to_pointd(pt);
		this->on_mouse_move(mks, pt, dip);
		return 0;
	}

	if ((msg == WM_KEYDOWN) || (msg == WM_SYSKEYDOWN))
	{
		auto mks = GetModifierKeys();
		bool handled = this->on_key_down ((UINT) wParam, mks);
		if (handled)
			return 0;
		return std::nullopt;
	}

	if ((msg == WM_KEYUP) || (msg == WM_SYSKEYUP))
	{
		auto mks = GetModifierKeys();
		bool handled = this->on_key_up ((UINT) wParam, mks);
		if (handled)
			return 0;
		return std::nullopt;
	}

	if (msg == WM_CHAR)
	{
		bool handled = this->on_char_key((uint32_t)wParam);
		if (handled)
			return 0;
		return std::nullopt;
	}

	if (msg == WM_CONTEXTMENU)
	{
		POINT pt_screen = { GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam) };
		POINT pp = pt_screen;
		::ScreenToClient (hwnd, &pp);
		auto pd = pointp_to_pointd(pp);
		this->show_context_menu (pt_screen, pp, pd);
		return 0;
	}

	return std::nullopt;
}

// static
modifier_key window::GetModifierKeys()
{
	modifier_key keys = modifier_key::none;

	if (GetKeyState (VK_SHIFT) < 0)
		keys |= modifier_key::shift;

	if (GetKeyState (VK_CONTROL) < 0)
		keys |= modifier_key::control;

	if (GetKeyState (VK_MENU) < 0)
		keys |= modifier_key::alt;

	return keys;
}
