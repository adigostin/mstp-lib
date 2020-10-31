
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "edge.h"

namespace edge
{
	// TODO: move to utility functions
	modifier_key get_modifier_keys()
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

	COLORREF theme_color_provider_i::color_win32 (theme_color color) const
	{
		uint32_t argb = this->argb(color);
		return argb & 0xFFFFFF;
	}

	D2D_COLOR_F theme_color_provider_i::color_d2d (theme_color color) const
	{
		uint32_t argb = this->argb(color);
		D2D_COLOR_F res = {
			((argb >> 16) & 0xff) / 255.0f,
			((argb >> 8) & 0xff) / 255.0f,
			(argb & 0xff) / 255.0f,
			((argb >> 24) & 0xff) / 255.0f
		};
		return res;
	}

	com_ptr<ID2D1SolidColorBrush> theme_color_provider_i::make_brush (ID2D1DeviceContext* dc, theme_color color) const
	{
		com_ptr<ID2D1SolidColorBrush> brush;
		auto hr = dc->CreateSolidColorBrush (color_d2d(color), &brush);
		rassert(SUCCEEDED(hr));
		return brush;
	}

	com_ptr<ID2D1SolidColorBrush> theme_color_provider_i::make_brush (ID2D1DeviceContext* dc, theme_color color, float opacity) const
	{
		auto b = make_brush(dc, color);
		b->SetOpacity(opacity);
		return b;
	}
}
