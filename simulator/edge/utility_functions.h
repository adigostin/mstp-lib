
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once

inline D2D1_SIZE_F operator- (D2D1_POINT_2F p0, D2D1_POINT_2F p1) { return { p0.x - p1.x, p0.y - p1.y }; }
inline D2D1_POINT_2F operator- (D2D1_POINT_2F p, D2D1_SIZE_F s) {return { p.x - s.width, p.y - s.height }; }
inline D2D1_POINT_2F operator+ (D2D1_POINT_2F p, D2D1_SIZE_F s) {return { p.x + s.width, p.y + s.height }; }
inline D2D1_SIZE_F operator* (float a, D2D1_SIZE_F b) { return { a * b.width, a * b.height }; }
inline D2D1_SIZE_F operator/ (D2D1_SIZE_F a, float b) { return { a.width / b, a.height / b }; }
inline void operator+= (D2D1_POINT_2F& a, D2D1_SIZE_F b) { a.x += b.width; a.y += b.height; }
inline bool operator== (D2D1_POINT_2F p0, D2D1_POINT_2F p1) { return (p0.x == p1.x) && (p0.y == p1.y); }
inline bool operator!= (D2D1_POINT_2F p0, D2D1_POINT_2F p1) { return (p0.x != p1.x) || (p0.y != p1.y); }
inline bool operator== (D2D1_SIZE_F p0, D2D1_SIZE_F p1) { return (p0.width == p1.width) && (p0.height == p1.height); }
inline bool operator!= (D2D1_SIZE_F p0, D2D1_SIZE_F p1) { return (p0.width != p1.width) || (p0.height != p1.height); }
bool operator== (const D2D1_RECT_F& a, const D2D1_RECT_F& b);
bool operator!= (const D2D1_RECT_F& a, const D2D1_RECT_F& b);
inline bool operator== (POINT a, POINT b) { return (a.x == b.x) && (a.y == b.y); }
inline bool operator!= (POINT a, POINT b) { return (a.x != b.x) || (a.y != b.y); }
inline bool operator== (SIZE a, SIZE b) { return (a.cx == b.cx) && (a.cy == b.cy); }
inline bool operator!= (SIZE a, SIZE b) { return (a.cx != b.cx) || (a.cy != b.cy); }
inline bool operator== (const D2D1_MATRIX_3X2_F& a, const D2D1_MATRIX_3X2_F& b) { return memcmp(&a, &b, sizeof(D2D1_MATRIX_3X2_F)) == 0; }
inline bool operator== (const D2D1_COLOR_F& a, const D2D1_COLOR_F& b) { return memcmp(&a, &b, sizeof(D2D1_COLOR_F)) == 0; }
inline bool operator!= (const D2D1_COLOR_F& a, const D2D1_COLOR_F& b) { return memcmp(&a, &b, sizeof(D2D1_COLOR_F)) != 0; }

namespace edge
{
	bool point_in_rect (const D2D1_RECT_F& rect, D2D1_POINT_2F location);
	bool point_in_rect (const RECT& rect, POINT pt);
	bool point_in_polygon(const std::array<D2D1_POINT_2F, 4>& vertices, D2D1_POINT_2F point);
	D2D1_RECT_F inflate (const D2D1_RECT_F& rect, float distance);
	void inflate (D2D1_RECT_F* rect, float distance);
	D2D1_RECT_F inflate (D2D1_POINT_2F p, float distance);
	D2D1_ROUNDED_RECT inflate (const D2D1_ROUNDED_RECT& rr, float distance);
	void inflate (D2D1_ROUNDED_RECT* rr, float distance);
	std::string get_window_text (HWND hwnd);
	std::array<D2D1_POINT_2F, 4> corners (const D2D1_RECT_F& rect);
	D2D1_RECT_F polygon_bounds (const std::array<D2D1_POINT_2F, 4>& points);
	D2D1_COLOR_F interpolate (const D2D1_COLOR_F& first, const D2D1_COLOR_F& second, uint32_t percent_first);
	uint32_t interpolate (uint32_t first, uint32_t second, uint32_t percent_first);
	D2D1_RECT_F align_to_pixel (const D2D1_RECT_F& rect, uint32_t dpi);
	std::wstring utf8_to_utf16 (std::string_view str_utf8);
	std::string utf16_to_utf8 (std::wstring_view str_utf16);
	std::string bstr_to_utf8 (BSTR bstr);
	D2D1_RECT_F make_positive (const D2D1_RECT_F& r);
	D2D1_RECT_F union_rects (const D2D1_RECT_F& a, const D2D1_RECT_F& b);
	bool hit_test_line (D2D1_POINT_2F dLocation, float tolerance, D2D1_POINT_2F p0w, D2D1_POINT_2F p1w, float lineWidth);

	class canceled_by_user_exception : std::exception
	{
		virtual const char* what() const override { return "Canceled by user"; }
	};

	std::wstring try_choose_open_path (HWND parent_hwnd, const wchar_t* initial_path,
		std::span<const COMDLG_FILTERSPEC> file_types, const wchar_t* file_extension_without_dot);

	std::wstring try_choose_save_path (HWND parent_hwnd, const wchar_t* initial_path,
		std::span<const COMDLG_FILTERSPEC> file_types, const wchar_t* file_extension_without_dot);

	bool ask_save_discard_cancel (HWND parent_hwnd, const wchar_t* ask_text, const wchar_t* title_text);
}
