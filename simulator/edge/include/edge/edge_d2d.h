
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#pragma once
#include "edge.h"

inline D2D1_SIZE_F operator- (D2D1_POINT_2F p0, D2D1_POINT_2F p1) { return { p0.x - p1.x, p0.y - p1.y }; }
inline D2D1_POINT_2F operator- (D2D1_POINT_2F p, D2D1_SIZE_F s) {return { p.x - s.width, p.y - s.height }; }
inline D2D1_POINT_2F operator+ (D2D1_POINT_2F p, D2D1_SIZE_F s) {return { p.x + s.width, p.y + s.height }; }
inline D2D1_POINT_2F operator* (D2D1_POINT_2F a, float b) {return { a.x * b, a.y * b }; }
inline D2D1_POINT_2F operator/ (D2D1_POINT_2F a, float b) {return { a.x / b, a.y / b }; }
inline D2D1_SIZE_F operator* (float a, D2D1_SIZE_F b) { return { a * b.width, a * b.height }; }
inline D2D1_SIZE_F operator/ (D2D1_SIZE_F a, float b) { return { a.width / b, a.height / b }; }
inline void operator+= (D2D1_POINT_2F& a, D2D1_SIZE_F b) { a.x += b.width; a.y += b.height; }
inline bool operator== (D2D1_POINT_2F p0, D2D1_POINT_2F p1) { return (p0.x == p1.x) && (p0.y == p1.y); }
inline bool operator!= (D2D1_POINT_2F p0, D2D1_POINT_2F p1) { return (p0.x != p1.x) || (p0.y != p1.y); }
inline bool operator== (D2D1_SIZE_F p0, D2D1_SIZE_F p1) { return (p0.width == p1.width) && (p0.height == p1.height); }
inline bool operator!= (D2D1_SIZE_F p0, D2D1_SIZE_F p1) { return (p0.width != p1.width) || (p0.height != p1.height); }
bool operator== (const D2D1_RECT_F& a, const D2D1_RECT_F& b);
bool operator!= (const D2D1_RECT_F& a, const D2D1_RECT_F& b);
inline bool operator== (const D2D1_MATRIX_3X2_F& a, const D2D1_MATRIX_3X2_F& b) { return memcmp(&a, &b, sizeof(D2D1_MATRIX_3X2_F)) == 0; }
inline bool operator== (const D2D1_COLOR_F& a, const D2D1_COLOR_F& b) { return memcmp(&a, &b, sizeof(D2D1_COLOR_F)) == 0; }
inline bool operator!= (const D2D1_COLOR_F& a, const D2D1_COLOR_F& b) { return memcmp(&a, &b, sizeof(D2D1_COLOR_F)) != 0; }
inline void operator+= (D2D1_RECT_F& a, D2D1_SIZE_F b) { a.left += b.width; a.top += b.height; a.right += b.width; a.bottom += b.height; }

namespace edge
{
	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("F1BBC724-9E56-45FF-9267-479B19DC9252") ID2DThemeColorProvider : IThemeColorProvider
	{
		D2D_COLOR_F color_d2d (theme_color color) const;
		com_ptr<ID2D1SolidColorBrush> make_brush (ID2D1DeviceContext* dc, theme_color color) const;
		com_ptr<ID2D1SolidColorBrush> make_brush (ID2D1DeviceContext* dc, theme_color color, float opacity) const;
	};


	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("D2CC65E1-61A4-4503-8906-44AAA97DF7C2") ID2DRenderEventsSink : IUnknown
	{
		virtual HRESULT STDMETHODCALLTYPE OnD2DRender (HWND hWnd, ID2D1DeviceContext* dc) = 0;
		virtual HRESULT STDMETHODCALLTYPE OnBeforeD2DRender (HWND hWnd, ID2D1DeviceContext* dc) = 0;
		virtual HRESULT STDMETHODCALLTYPE OnAfterD2DRender (HWND hWnd, ID2D1DeviceContext* dc) = 0;
		virtual HRESULT STDMETHODCALLTYPE OnD2DDCReleasing (ID2D1DeviceContext* dc) = 0;
		virtual HRESULT STDMETHODCALLTYPE OnD2DDCRecreated (ID2D1DeviceContext* dc) = 0;
	};

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("78A81AD7-3663-41ED-8DE9-E07629A257B6") ID2DRenderer : IUnknown
	{
		enum debug_flag
		{
			render_frame_durations_and_fps = 1,
			render_update_rects = 2,
			full_clear = 4,
		};

		virtual HWND HWnd() const = 0;
		virtual ID2D1DeviceContext* dc() const = 0;
		virtual ID2D1Factory1* d2d_factory() const = 0;
		virtual ID3D11DeviceContext* d3d_dc() const = 0;
		virtual IDWriteFactory* dwrite_factory() const = 0;
		// This can be called repeatedly, first to show the caret, and then to move it.
		virtual void show_caret (const D2D1_RECT_F& bounds, const D2D1_COLOR_F& color, const D2D1_MATRIX_3X2_F* transform = nullptr) = 0;
		virtual void hide_caret() = 0;
		virtual float fps() const = 0;
		virtual float average_render_duration() const = 0;
		virtual void set_debug_flag (debug_flag flag) = 0;
		virtual void clear_debug_flag (debug_flag flag) = 0;
		virtual debug_flag debug_flags() const = 0;
	};

	HRESULT MakeD2DRenderer (HWND hWnd, ID3D11DeviceContext* d3d_dc, IDWriteFactory* dwrite_factory,
		ID2D1Factory1* d2d_factory, ID2DRenderer** ppRenderer);

	//struct zoom_transform_changed_e : event<zoom_transform_changed_e> { };

	struct IZoomer
	{
		virtual ~IZoomer() = default;
		virtual D2D1_POINT_2F pointd_to_pointw (D2D1_POINT_2F dlocation) const = 0;
		virtual D2D1_POINT_2F pointw_to_pointd (D2D1_POINT_2F location) const = 0;
		virtual D2D1::Matrix3x2F zoom_transform() const = 0;
		//virtual zoom_transform_changed_e::subscriber zoom_transform_changed() = 0;
		virtual void zoom_to (D2D1_POINT_2F aimpoint, float zoom, bool smooth) = 0;
		virtual void zoom_to (const D2D1_RECT_F& rect, float min_margin, float min_zoom, float max_zoom, bool smooth) = 0;
	};

	HRESULT MakeZoomer (HWND hWnd, wistd::unique_ptr<IZoomer>& zoomerOut);

	struct TextLayoutWithMetrics
	{
		wil::com_ptr_nothrow<IDWriteTextLayout> layout;
		DWRITE_TEXT_METRICS metrics;
		operator IDWriteTextLayout*() const { return layout.get(); }
		IDWriteTextLayout* operator->() const { return layout.get(); }
	};

	HRESULT CreateTextLayoutWithMetrics (IDWriteFactory* dwf, IDWriteTextFormat* format,
		const wchar_t* text, int textLen, float maxWidth, TextLayoutWithMetrics& tl);

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("82743515-45F3-43A3-8BC4-973FD7F2C9FD") ITextEditor : IUnknown
	{
		virtual const wchar_t* GetTextPtr (uint32_t* size) = 0;
		virtual void SelectAll() = 0;
		virtual const RECT& rect() const = 0;
	};

	HRESULT MakeTextEditor (ID2DRenderer* renderer,
		IDWriteTextFormat* format,
		IThemeColorProvider* tcp,
		const RECT& rect,
		LONG lr_padding,
		const wchar_t* text,
		ITextEditor** ppTextEditor);

	inline float length_squared (D2D1_SIZE_F size) { return (size.width * size.width) + (size.height * size.height); }
	inline float length (D2D1_SIZE_F size) { return sqrtf(length_squared(size)); }
	inline float atan2 (D2D1_SIZE_F s) { return std::atan2(s.height, s.width); }
	bool point_in_rect (const D2D1_RECT_F& rect, D2D1_POINT_2F location);
	bool point_in_polygon(const std::array<D2D1_POINT_2F, 4>& vertices, D2D1_POINT_2F point);
	D2D1_RECT_F inflate (const D2D1_RECT_F& rect, float distance);
	void inflate (D2D1_RECT_F* rect, float distance);
	D2D1_RECT_F inflate (D2D1_POINT_2F p, float distance);
	D2D1_ROUNDED_RECT inflate (const D2D1_ROUNDED_RECT& rr, float distance);
	void inflate (D2D1_ROUNDED_RECT* rr, float distance);
	std::array<D2D1_POINT_2F, 4> corners (const D2D1_RECT_F& rect);
	D2D1_POINT_2F center (const D2D1_RECT_F& r);
	D2D1_RECT_F polygon_bounds (const std::array<D2D1_POINT_2F, 4>& points);
	D2D1_COLOR_F interpolate (const D2D1_COLOR_F& first, const D2D1_COLOR_F& second, uint32_t percent_first);
	uint32_t interpolate (uint32_t first, uint32_t second, uint32_t percent_first);
	D2D1_RECT_F align_to_pixel (const D2D1_RECT_F& rect, uint32_t dpi);
	D2D1_RECT_F make_positive (const D2D1_RECT_F& r);
	D2D1_RECT_F union_rects (const D2D1_RECT_F& a, const D2D1_RECT_F& b);
	bool hit_test_line (D2D1_POINT_2F dLocation, float tolerance, D2D1_POINT_2F p0w, D2D1_POINT_2F p1w, float lineWidth);
	bool dragged_threshold (POINT from, POINT to, uint32_t dpi);
	inline D2D1_POINT_2F middle (D2D1_POINT_2F a, D2D1_POINT_2F b) { return { (a.x + b.x) / 2, (a.y + b.y) / 2 }; }
	inline bool empty_rect (const D2D1_RECT_F& r) { return (r.left == r.right) && (r.top == r.bottom); }

	D2D1::Matrix3x2F dpi_transform(uint32_t dpi);
	D2D1::Matrix3x2F dpi_transform(HWND hwnd);
	float pixel_width(uint32_t dpi);
	float pixel_width(HWND hwnd);
	LONG lengthd_to_lengthp (float lengthd, uint32_t dpi, int round_style); // round_style: <0--align by shrinking; =0--align by rounding; >0--align by extending
	float lengthp_to_lengthd (LONG lengthp, uint32_t dpi);
	D2D1_POINT_2F pointp_to_pointd (POINT p, uint32_t dpi);
	D2D1_POINT_2F pointp_to_pointd (LONG xPixels, LONG yPixels, uint32_t dpi);
	POINT pointd_to_pointp (float xDips, float yDips, uint32_t dpi, int round_style);
	POINT pointd_to_pointp (D2D1_POINT_2F locationDips, uint32_t dpi, int round_style);
	D2D1_RECT_F client_rect (HWND hwnd);
	D2D1_SIZE_F client_size (HWND hwnd);
	// round_style: <0--align by shrinking; =0--align by rounding; >0--align by extending
	RECT rectd_to_rectp (const D2D1_RECT_F& rd, uint32_t dpi, int round_style = 0);
	void invalidate (const D2D1_RECT_F& rect, HWND hwnd);

}

namespace std
{
	inline D2D1_POINT_2F round (D2D1_POINT_2F p)
	{
		return { std::round(p.x), std::round(p.y) };
	}
}
