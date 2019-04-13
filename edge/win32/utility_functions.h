#pragma once

inline D2D1_POINT_2F location (const D2D1_RECT_F& r) { return { r.left, r.top }; }
inline D2D1_SIZE_F size (const D2D1_RECT_F& r) { return { r.right - r.left, r.bottom - r.top }; }
inline D2D1_SIZE_F operator- (D2D1_POINT_2F p0, D2D1_POINT_2F p1) { return { p0.x - p1.x, p0.y - p1.y }; }
inline D2D1_POINT_2F operator- (D2D1_POINT_2F p, D2D1_SIZE_F s) {return { p.x - s.width, p.y - s.height }; }
inline D2D1_POINT_2F operator+ (D2D1_POINT_2F p, D2D1_SIZE_F s) {return { p.x + s.width, p.y + s.height }; }
inline bool operator== (D2D1_POINT_2F p0, D2D1_POINT_2F p1) { return (p0.x == p1.x) && (p0.y == p1.y); }
inline bool operator!= (D2D1_POINT_2F p0, D2D1_POINT_2F p1) { return (p0.x != p1.x) || (p0.y != p1.y); }
bool operator== (const D2D1_RECT_F& a, const D2D1_RECT_F& b);
bool operator!= (const D2D1_RECT_F& a, const D2D1_RECT_F& b);
inline POINT location (const RECT& r) { return { r.left, r.top }; }
inline SIZE size (const RECT& r) { return { r.right - r.left, r.bottom - r.top }; }
inline bool operator== (POINT a, POINT b) { return (a.x == b.x) && (a.y == b.y); }
inline bool operator!= (POINT a, POINT b) { return (a.x != b.x) || (a.y != b.y); }
inline bool operator== (SIZE a, SIZE b) { return (a.cx == b.cx) && (a.cy == b.cy); }
inline bool operator!= (SIZE a, SIZE b) { return (a.cx != b.cx) || (a.cy != b.cy); }

namespace edge
{
	struct zoomable_i;

	bool HitTestLine (const zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance, D2D1_POINT_2F p0w, D2D1_POINT_2F p1w, float lineWidth);
	bool point_in_rect(const D2D1_RECT_F& rect, D2D1_POINT_2F location);
	bool point_in_polygon(const std::array<D2D1_POINT_2F, 4>& vertices, D2D1_POINT_2F point);
	D2D1_RECT_F InflateRect (const D2D1_RECT_F& rect, float distance);
	void InflateRect (D2D1_RECT_F* rect, float distance);
	D2D1_ROUNDED_RECT InflateRoundedRect (const D2D1_ROUNDED_RECT& rr, float distance);
	void InflateRoundedRect (D2D1_ROUNDED_RECT* rr, float distance);
	D2D1::ColorF GetD2DSystemColor (int sysColorIndex);
	std::string get_window_text (HWND hwnd);
	std::array<D2D1_POINT_2F, 4> corners (const D2D1_RECT_F& rect);
	D2D1_RECT_F polygon_bounds (const std::array<D2D1_POINT_2F, 4>& points);
	D2D1_COLOR_F interpolate (const D2D1_COLOR_F& first, const D2D1_COLOR_F& second, uint32_t percent_first);
	D2D1_RECT_F align_to_pixel (const D2D1_RECT_F& rect, uint32_t dpi);
	std::string bstr_to_utf8 (BSTR bstr);
}
