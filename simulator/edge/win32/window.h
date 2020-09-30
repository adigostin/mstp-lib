
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

	class window : public event_manager, public virtual dpi_aware_window_i
	{
		HWND _hwnd = nullptr;
		uint32_t _dpi;

	public:
		window (DWORD exStyle, DWORD style, const RECT& rect, HWND hWndParent, int child_control_id);
		window (const wnd_class_params& class_params, DWORD exStyle, DWORD style, int x, int y, int width, int height, HWND hWndParent, HMENU hMenu);

		window (const window&) = delete;
		window& operator= (const window&) = delete;

	protected:
		virtual ~window();

		virtual std::optional<LRESULT> window_proc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		static modifier_key GetModifierKeys();

		static constexpr UINT WM_NEXT = WM_APP;

		virtual void on_size_changed (SIZE client_size_pixels, D2D1_SIZE_F client_size_dips) { }
		virtual void on_dpi_changed (UINT dpi) { }
		virtual handled on_mouse_down (mouse_button button, modifier_key mks, POINT pp, D2D1_POINT_2F pd) { return false; }
		virtual handled on_mouse_up   (mouse_button button, modifier_key mks, POINT pp, D2D1_POINT_2F pd) { return false; }
		virtual void on_mouse_move (modifier_key mks, POINT pp, D2D1_POINT_2F pd) { }
		virtual handled on_key_down (uint32_t vkey, modifier_key mks) { return false; }
		virtual handled on_key_up   (uint32_t virtual_key, modifier_key mks) { return false; }
		virtual handled on_char_key (uint32_t key) { return false; }
		virtual HCURSOR cursor_at (POINT pp, D2D1_POINT_2F pd) const { return nullptr; }
		virtual void show_context_menu (POINT pt_screen, POINT pp, D2D1_POINT_2F pd) { }

	public:
		// win32_window_i
		virtual HWND hwnd() const override { return _hwnd; }

		// dpi_aware_window_i
		virtual uint32_t dpi() const override { return _dpi; }

	private:
		static void register_class (HINSTANCE hInstance, const wnd_class_params& class_params);

		static LRESULT CALLBACK window_proc_static (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	};
}
