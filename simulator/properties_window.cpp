
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "edge/window.h"
#include "pg/property_grid.h"
#include "edge/utility_functions.h"

using namespace edge;

class properties_window : event_manager, public properties_window_i
{
	std::unique_ptr<edge::win32_window_i> const _window;
	std::unique_ptr<d2d_renderer_i> const _renderer;
	std::unique_ptr<pg::property_grid_i> const _pg;

	static const inline WNDCLASSEX wnd_class = {
		.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
		.hCursor = ::LoadCursor(nullptr, IDC_ARROW),
		.lpszClassName = L"properties_window",
	};

public:
	properties_window (const properties_window_create_params& cps)
		: _window(edge::make_window(wnd_class, WS_EX_CLIENTEDGE, WS_CHILD | WS_VISIBLE, cps.hwnd_parent, cps.rect))
		, _renderer(edge::make_d2d_renderer(*_window, cps.d3d_dc, cps.dwrite_factory, cps.d2d_factory))
		, _pg(pg::property_grid_factory(_renderer.get(), edge::client_rect(_window->hwnd()), cps.tcp))
	{
		_window->window_proc().add_handler<&properties_window::on_window_proc>(this);
	}

	~properties_window()
	{
		_window->window_proc().remove_handler<&properties_window::on_window_proc>(this);
	}

	// win32_window_i
	virtual HWND hwnd() const override { return _window->hwnd(); }
	virtual window_proc_e::subscriber window_proc() override { return _window->window_proc(); }

	// properties_window_i
	virtual pg::property_grid_i* pg() const override { return _pg.get(); }

	std::optional<LRESULT> on_window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (msg == WM_SIZE)
		{
			_pg->set_bounds(edge::client_rect(hwnd));
			::UpdateWindow(hwnd);
			return std::nullopt;
		}

		return std::nullopt;
	}
};

std::unique_ptr<properties_window_i> properties_window_factory (const properties_window_create_params& cps)
{
	return std::make_unique<properties_window>(cps);
}
