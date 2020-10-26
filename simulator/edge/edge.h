
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "events.h"
#include "com_ptr.h"
#include "win32_window_i.h"
#include "d2d_renderer.h"

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

	struct __declspec(novtable) d2d_window_i : win32_window_i
	{
		virtual d2d_renderer& renderer() = 0;
		const d2d_renderer& renderer() const { return const_cast<d2d_window_i*>(this)->renderer(); }

		virtual void show_caret (const D2D1_RECT_F& bounds, const D2D1_COLOR_F& color, const D2D1_MATRIX_3X2_F* transform = nullptr) = 0;
		virtual void hide_caret() = 0;
	};

	struct zoom_transform_changed_e : event<zoom_transform_changed_e> { };

	struct __declspec(novtable) zoomable_window_i : d2d_window_i
	{
		virtual D2D1_POINT_2F aimpoint() const = 0;
		virtual float zoom() const = 0;
		virtual zoom_transform_changed_e::subscriber zoom_transform_changed() = 0;
		virtual void zoom_to (const D2D1_RECT_F& rect, float min_margin, float min_zoom, float max_zoom, bool smooth) = 0;

		D2D1_POINT_2F pointd_to_pointw (D2D1_POINT_2F dlocation) const;
		void pointw_to_pointd (std::span<D2D1_POINT_2F> locations) const;
		float lengthw_to_lengthd (float lengthw) const { return lengthw * zoom(); }
		D2D1_SIZE_F pixel_aligned_window_center() const;
		D2D1_POINT_2F pointw_to_pointd (D2D1_POINT_2F location) const;
		D2D1_RECT_F rectw_to_rectd (const D2D1_RECT_F& r) const;
		D2D1::Matrix3x2F zoom_transform() const;
	};

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
	};

	struct __declspec(novtable) theme_color_provider_i
	{
		virtual uint32_t argb (theme_color color) const = 0;

		COLORREF color_win32 (theme_color color) const;
		D2D_COLOR_F color_d2d (theme_color color) const;
		com_ptr<ID2D1SolidColorBrush> make_brush (ID2D1DeviceContext* dc, theme_color color) const;
		com_ptr<ID2D1SolidColorBrush> make_brush (ID2D1DeviceContext* dc, theme_color color, float opacity) const;
	};
}
