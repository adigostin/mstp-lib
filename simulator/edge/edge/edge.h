
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "com_ptr.h"
#include "events.h"

namespace edge
{
	using handled = bool;

	enum class mouse_button { left, right, middle, };

	enum class modifier_key
	{
		none    = 0,
		shift   = 4,    // MK_SHIFT
		control = 8,    // MK_CONTROL
		alt     = 0x20, // MK_ALT
		lbutton = 1,    // MK_LBUTTON
		rbutton = 2,    // MK_RBUTTON
		mbutton = 0x10, // MK_MBUTTON
		shift_control_alt = shift | control | alt,
		shift_alt = shift | alt,
	};
	//DEFINE_ENUM_FLAG_OPERATORS(modifier_key);
	inline constexpr modifier_key operator& (modifier_key a, modifier_key b) noexcept { return (modifier_key) ((std::underlying_type_t<modifier_key>)a & (std::underlying_type_t<modifier_key>)b); }
	inline constexpr modifier_key operator| (modifier_key a, modifier_key b) noexcept { return (modifier_key) ((std::underlying_type_t<modifier_key>)a | (std::underlying_type_t<modifier_key>)b); }
	inline constexpr modifier_key& operator |= (modifier_key& a, modifier_key b) noexcept { return (modifier_key&) ((std::underlying_type_t<modifier_key>&)a |= (std::underlying_type_t<modifier_key>)b); }
	inline constexpr bool operator== (modifier_key a, std::underlying_type_t<modifier_key> b) noexcept { return (std::underlying_type_t<modifier_key>)a == b; }
	inline constexpr bool operator!= (modifier_key a, std::underlying_type_t<modifier_key> b) noexcept { return (std::underlying_type_t<modifier_key>)a != b; }

	modifier_key get_modifier_keys();

	struct mouse_ud_args
	{
		mouse_button button;
		modifier_key mks;
		POINT pp;
		D2D1_POINT_2F pd;
	};

	struct mouse_move_args
	{
		modifier_key mks;
		D2D1_POINT_2F pd;
	};

	struct gdi_object_deleter
	{
		void operator() (HGDIOBJ object) { ::DeleteObject(object); }
	};
	using HFONT_unique_ptr = std::unique_ptr<std::remove_pointer<HFONT>::type, gdi_object_deleter>;

	enum class theme_color
	{
		background,
		foreground,
		disabled_fore,
		selected_back_focused,
		selected_back_not_focused,
		selected_fore,
		tooltip_back,
		tooltip_fore,
		active_caption_back,
		active_caption_fore,
		inactive_caption_back,
		inactive_caption_fore,
		button_back,
		button_back_hot,
		button_back_pushed,
		button_fore,
		button_fore_hot,
		button_fore_pushed,
		text_editor_back,
		text_editor_fore,
		text_editor_selection_focused,
		text_editor_selection_not_focused,
	};

	struct __declspec(novtable) theme_color_provider_i
	{
		virtual uint32_t argb (theme_color color) const = 0;

		struct theme_colors_changed_e : event<theme_colors_changed_e> { };
		virtual theme_colors_changed_e::subscriber theme_colors_changed() = 0;

		COLORREF color_win32 (theme_color color) const;
		D2D_COLOR_F color_d2d (theme_color color) const;
		com_ptr<ID2D1SolidColorBrush> make_brush (ID2D1DeviceContext* dc, theme_color color) const;
		com_ptr<ID2D1SolidColorBrush> make_brush (ID2D1DeviceContext* dc, theme_color color, float opacity) const;
	};
}
