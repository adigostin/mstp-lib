
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "edge_win32.h"

namespace edge
{
	struct wnd_class_params
	{
		LPCWSTR lpszClassName;
		UINT    style;
		LPCWSTR lpszMenuName;
		LPCWSTR lpIconName;
		LPCWSTR lpIconSmName;
	};

	class window : public event_manager, public virtual win32_window_i
	{
		HWND _hwnd = nullptr;

	public:
		window (DWORD exStyle, DWORD style, const RECT& rect, HWND hWndParent, int child_control_id);
		window (const wnd_class_params& class_params, DWORD exStyle, DWORD style, int x, int y, int width, int height, HWND hWndParent, HMENU hMenu);

		window (const window&) = delete;
		window& operator= (const window&) = delete;

	protected:
		virtual ~window();

		virtual std::optional<LRESULT> window_proc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		static constexpr UINT WM_NEXT = WM_APP;

	public:
		// win32_window_i
		virtual HWND hwnd() const override { return _hwnd; }

	private:
		static void register_class (HINSTANCE hInstance, const wnd_class_params& class_params);

		static LRESULT CALLBACK window_proc_static (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	};
}
