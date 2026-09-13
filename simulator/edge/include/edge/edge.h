
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#pragma once
#include "com.h"

namespace edge
{
	using handled = bool;

	extern const LCID InvariantLCID;

	enum class mouse_button { left, right, middle, };

	UINT get_modifier_keys();

	struct mouse_ud_args
	{
		mouse_button button;
		UINT mks;
		POINT pt;
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

	struct __declspec(novtable) string_convert_context_i
	{
		virtual ~string_convert_context_i() = default;
	};

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("5DE4CF20-CC17-4FCA-8BD8-85D3738FFB61") IThemeChangedEvents : IUnknown
	{
		virtual HRESULT STDMETHODCALLTYPE OnThemeChanged() = 0;
	};

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("1A941635-180C-4193-B2FA-2714EE276EAE") IThemeColorProvider : IUnknown
	{
		virtual uint32_t argb (theme_color color) const = 0;
		COLORREF color_win32 (theme_color color) const;
	};

	HRESULT PropertyHasDefaultValue (IDispatch* obj, DISPID prop, WORD getterFuncIndex);

	HRESULT PickOpenPath (HWND hWndParent, PCWSTR initialPath,
		std::span<const COMDLG_FILTERSPEC> fileTypes,
		PCWSTR fileExtNoDot, BSTR* pbstrPath);

	HRESULT PickSavePath (HWND hWndParent, PCWSTR initialPath,
		std::span<const COMDLG_FILTERSPEC> fileTypes,
		PCWSTR fileExtNoDot, BSTR* pbstrPath);

	uint32_t dpi (HWND hwnd);
	SIZE client_size_pixels (HWND hwnd);
	RECT client_rect_pixels (HWND hwnd);

}

inline bool operator== (POINT a, POINT b) { return (a.x == b.x) && (a.y == b.y); }
inline bool operator!= (POINT a, POINT b) { return (a.x != b.x) || (a.y != b.y); }
inline bool operator== (SIZE a, SIZE b) { return (a.cx == b.cx) && (a.cy == b.cy); }
inline bool operator!= (SIZE a, SIZE b) { return (a.cx != b.cx) || (a.cy != b.cy); }
inline SIZE operator- (POINT a, POINT b) { return { a.x - b.x, a.y - b.y }; }
inline POINT operator- (POINT a, SIZE b) { return { a.x - b.cx, a.y - b.cy }; }
inline POINT operator+ (POINT a, SIZE b) { return { a.x + b.cx, a.y + b.cy }; }
inline bool operator== (RECT a, RECT b) { return (a.left == b.left) && (a.top == b.top) && (a.right == b.right) && (a.bottom == b.bottom); }