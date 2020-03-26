
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"

using namespace edge;

class properties_window : public d2d_window, public virtual properties_window_i
{
	using base = d2d_window;

	std::unique_ptr<edge::property_grid_i> const _pg;

	HWND _tooltip = nullptr;
	POINT _last_tt_location = { -1, -1 };

public:
	properties_window (HWND parent, const RECT& rect, ID3D11DeviceContext1* d3d_dc, IDWriteFactory* dwrite_factory)
		: base (WS_EX_CLIENTEDGE, WS_CHILD | WS_VISIBLE, rect, parent, 0, d3d_dc, dwrite_factory)
		, _pg(edge::property_grid_factory(this, client_rect_pixels()))
	{
		auto hinstance = (HINSTANCE)::GetWindowLongPtr (hwnd(), GWLP_HINSTANCE);

		_tooltip = CreateWindowEx (WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr,
			WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			hwnd(), nullptr, hinstance, nullptr);

		TOOLINFO ti = { sizeof(TOOLINFO) };
		ti.uFlags   = TTF_SUBCLASS;
		ti.hwnd     = hwnd();
		ti.lpszText = L"";
		ti.rect     = client_rect_pixels();
		SendMessage(_tooltip, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);

		SendMessage (_tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 1500);
		SendMessage (_tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, (LPARAM)(LONG)MAXSHORT);
		SendMessage (_tooltip, TTM_SETMAXTIPWIDTH, 0, client_width_pixels());
	}

	~properties_window()
	{
		::DestroyWindow(_tooltip);
	}

	virtual property_grid_i* pg() const override { return _pg.get(); }

	virtual HCURSOR cursor_at (POINT pp, D2D1_POINT_2F pd) const override
	{
		return _pg->cursor_at(pp, pd);
	}

	virtual void render(ID2D1DeviceContext* dc) const override
	{
		_pg->render(dc);
	}

	virtual handled on_mouse_down (mouse_button button, modifier_key mks, POINT pp, D2D1_POINT_2F pd) override
	{
		return base::on_mouse_down (button, mks, pp, pd)
			|| _pg->on_mouse_down (button, mks, pp, pd);
	}

	virtual handled on_mouse_up (mouse_button button, modifier_key mks, POINT pp, D2D1_POINT_2F pd) override
	{
		return base::on_mouse_up (button, mks, pp, pd)
			|| _pg->on_mouse_up (button, mks, pp, pd);
	}

	virtual void on_mouse_move (modifier_key mks, POINT pp, D2D1_POINT_2F pd) override
	{
		base::on_mouse_move (mks, pp, pd);
		_pg->on_mouse_move (mks, pp, pd);

		if (_last_tt_location != pp)
		{
			_last_tt_location = pp;

			::SendMessage (_tooltip, TTM_POP, 0, 0);

			std::wstring text;
			std::wstring title;

			auto i = _pg->item_at(pd);
			if (i.first)
			{
				title = utf8_to_utf16(i.first->description_title());
				text  = utf8_to_utf16(i.first->description_text());

				if (!title.empty() && text.empty())
					text = L"--";
			}

			TOOLINFO ti = { sizeof(TOOLINFO) };
			ti.hwnd     = hwnd();
			ti.lpszText = text.data();
			SendMessage(_tooltip, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);

			SendMessage(_tooltip, TTM_SETTITLE, TTI_INFO, (LPARAM)title.data());
		}
	}

	virtual handled on_key_down (uint32_t vkey, modifier_key mks) override
	{
		return base::on_key_down (vkey, mks)
			|| _pg->on_key_down(vkey, mks);
	}

	virtual handled on_key_up (uint32_t vkey, modifier_key mks) override
	{
		return base::on_key_up (vkey, mks)
			|| _pg->on_key_up(vkey, mks);
	}

	virtual handled on_char_key (uint32_t key) override
	{
		return base::on_char_key(key)
			|| _pg->on_char_key(key);
	}

	virtual void on_client_size_changed (SIZE client_size_pixels, D2D1_SIZE_F client_size_dips) override
	{
		base::on_client_size_changed(client_size_pixels, client_size_dips);
		_pg->set_rect(client_rect_pixels());
		::UpdateWindow(hwnd());

		TOOLINFO ti = { sizeof(TOOLINFO) };
		ti.uFlags   = TTF_SUBCLASS;
		ti.hwnd     = hwnd();
		ti.lpszText = L"";
		ti.rect     = client_rect_pixels();
		SendMessage(_tooltip, TTM_SETTOOLINFO, 0, (LPARAM) (LPTOOLINFO) &ti);
	}

	virtual void on_dpi_changed (UINT dpi) override
	{
		base::on_dpi_changed(dpi);
		_pg->set_rect(client_rect_pixels());
		_pg->on_dpi_changed();
	}

	// TODO: handle scrollbars
};

extern std::unique_ptr<properties_window_i> properties_window_factory (
	HWND parent,
	const RECT& rect,
	ID3D11DeviceContext1* d3d_dc,
	IDWriteFactory* dwrite_factory)
{
	return std::make_unique<properties_window>(parent, rect, d3d_dc, dwrite_factory);
};