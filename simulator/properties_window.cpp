
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "window.h"
#include "property_grid.h"

using namespace edge;

class properties_window : event_manager, public properties_window_i
{
	window _window;
	d2d_renderer _renderer;
	std::unique_ptr<edge::property_grid_i> const _pg;

public:
	properties_window (const properties_window_create_params& cps)
		: _window(WS_EX_CLIENTEDGE, WS_CHILD | WS_VISIBLE, cps.hwnd_parent, cps.rect)
		, _renderer(this, cps.d3d_dc, cps.dwrite_factory)
		, _pg(property_grid_factory(this, this->client_rect(), cps.tcp))
	{
		_window.window_proc().add_handler<&properties_window::on_window_proc>(this);
		_renderer.render().add_handler<&properties_window::on_render>(this);
	}

	~properties_window()
	{
		_renderer.render().remove_handler<&properties_window::on_render>(this);
		_window.window_proc().remove_handler<&properties_window::on_window_proc>(this);
	}

	// win32_window_i
	virtual HWND hwnd() const override { return _window.hwnd(); }
	virtual window_proc_e::subscriber window_proc() override { return _window.window_proc(); }

	// d2d_window_i
	virtual d2d_renderer& renderer() override final { return _renderer; }
	virtual void show_caret (const D2D1_RECT_F& bounds, const D2D1_COLOR_F& color, const D2D1_MATRIX_3X2_F* transform = nullptr) override { _renderer.show_caret(bounds, color, transform); }
	virtual void hide_caret() override { _renderer.hide_caret(); }

	// properties_window_i
	virtual property_grid_i* pg() const override { return _pg.get(); }

	std::optional<LRESULT> on_window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (msg == WM_SIZE)
		{
			_pg->set_bounds(client_rect());
			::UpdateWindow(hwnd);
			return std::nullopt;
		}

		if (msg == 0x02E3) // WM_DPICHANGED_AFTERPARENT
		{
			_pg->set_bounds(this->client_rect());
			_pg->on_dpi_changed();
			return std::nullopt;
		}

		if ((msg == WM_LBUTTONDOWN) || (msg == WM_RBUTTONDOWN))
		{
			::SetFocus(hwnd);
			if (::GetFocus() != hwnd)
				return std::nullopt;

			mouse_button button = (msg == WM_LBUTTONDOWN) ? edge::mouse_button::left : edge::mouse_button::right;
			modifier_key mks = get_modifier_keys();
			auto pp = POINT{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			auto pd = this->pointp_to_pointd(pp);
			edge::mouse_ud_args ma = { button, mks, pp, pd };
			bool handled = _pg->on_mouse_down(ma);
			if (handled)
				return 0;
			return std::nullopt;
		}

		if ((msg == WM_LBUTTONUP) || (msg == WM_RBUTTONUP))
		{
			mouse_button button = (msg == WM_LBUTTONUP) ? edge::mouse_button::left : edge::mouse_button::right;
			modifier_key mks = get_modifier_keys();
			auto pp = POINT{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			auto pd = this->pointp_to_pointd(pp);
			edge::mouse_ud_args ma = { button, mks, pp, pd };
			bool handled = _pg->on_mouse_up(ma);
			if (handled)
				return 0;
			return std::nullopt;
		}

		if (msg == WM_MOUSEMOVE)
		{
			modifier_key mks = (modifier_key)wparam;
			auto pd = this->pointp_to_pointd(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
			edge::mouse_move_args ma = { mks, pd };
			_pg->on_mouse_move(ma);
			return std::nullopt;
		}

		if (msg == WM_KEYDOWN)
		{
			auto handled = _pg->on_key_down ((uint32_t)wparam, get_modifier_keys());
			return handled ? std::optional<LRESULT>(0) : std::nullopt;
		}

		if (msg == WM_KEYUP)
		{
			auto handled = _pg->on_key_up ((uint32_t) wparam, get_modifier_keys());
			return handled ? std::optional<LRESULT>(0) : std::nullopt;
		}

		if (msg == WM_CHAR)
		{
			auto handled = _pg->on_char_key ((uint32_t)wparam);
			return handled ? std::optional<LRESULT>(0) : std::nullopt;
		}

		if (msg == WM_SETCURSOR)
		{
			if (((HWND) wparam == hwnd) && (LOWORD (lparam) == HTCLIENT))
			{
				POINT pt;
				if (::GetCursorPos (&pt))
				{
					if (::ScreenToClient (hwnd, &pt))
					{
						auto pd = pointp_to_pointd(pt);
						auto cursor = _pg->cursor_at(pt, pd);
						::SetCursor (cursor);
						return TRUE;
					}
				}
			}

			return std::nullopt;
		}

		return std::nullopt;
	}

	void on_render (ID2D1DeviceContext* dc)
	{
		_pg->render(dc);
	}
};

std::unique_ptr<properties_window_i> properties_window_factory (const properties_window_create_params& cps)
{
	return std::make_unique<properties_window>(cps);
}
