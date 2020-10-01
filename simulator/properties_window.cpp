
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "d2d_window.h"
#include "property_grid.h"

using namespace edge;

class properties_window : public d2d_window, public virtual properties_window_i
{
	using base = d2d_window;

	std::unique_ptr<edge::property_grid_i> const _pg;

public:
	properties_window (const properties_window_create_params& cps)
		: base(WS_EX_CLIENTEDGE, WS_CHILD | WS_VISIBLE, cps.rect, cps.hwnd_parent, 0, cps.d3d_dc, cps.dwrite_factory)
		, _pg(property_grid_factory(this, this->client_rect()))
	{ }

	virtual HWND hwnd() const override { return this->window::hwnd(); }

	virtual property_grid_i* pg() const override { return _pg.get(); }

	virtual std::optional<LRESULT> window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override
	{
		auto lr = base::window_proc(hwnd, msg, wparam, lparam);
		if (lr)
			return lr;

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
			auto pd = this->pointp_to_pointd(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
			edge::mouse_ud_args ma = { button, mks, pd };
			bool handled = _pg->on_mouse_down(ma);
			if (handled)
				return 0;
			return std::nullopt;
		}

		if ((msg == WM_LBUTTONUP) || (msg == WM_RBUTTONUP))
		{
			mouse_button button = (msg == WM_LBUTTONUP) ? edge::mouse_button::left : edge::mouse_button::right;
			modifier_key mks = get_modifier_keys();
			auto pd = this->pointp_to_pointd(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
			edge::mouse_ud_args ma = { button, mks, pd };
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

	virtual void render (const d2d_render_args& ra) const override
	{
		_pg->render(ra);
	}

	// TODO: handle scrollbars
};

std::unique_ptr<properties_window_i> properties_window_factory (const properties_window_create_params& cps)
{
	return std::make_unique<properties_window>(cps);
}
