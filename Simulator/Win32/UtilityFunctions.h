#pragma once
#include "EventManager.h"

struct IZoomable;

//unsigned int GetTimestampMilliseconds();
bool HitTestLine (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance, D2D1_POINT_2F p0w, D2D1_POINT_2F p1w, float lineWidth);
bool PointInRect (const D2D1_RECT_F& rect, D2D1_POINT_2F location);
bool PointInPolygon (const D2D1_POINT_2F* vertices, size_t vertexCount, D2D1_POINT_2F point);
D2D1_RECT_F InflateRect (const D2D1_RECT_F& rect, float distance);
void InflateRect (D2D1_RECT_F* rect, float distance);
D2D1_ROUNDED_RECT InflateRoundedRect (const D2D1_ROUNDED_RECT& rr, float distance);
void InflateRoundedRect (D2D1_ROUNDED_RECT* rr, float distance);
inline D2D1_SIZE_F operator- (D2D1_POINT_2F p0, D2D1_POINT_2F p1) { return { p0.x - p1.x, p0.y - p1.y }; }
inline D2D1_POINT_2F operator- (D2D1_POINT_2F p, D2D1_SIZE_F s) {return { p.x - s.width, p.y - s.height }; }
inline D2D1_POINT_2F operator+ (D2D1_POINT_2F p, D2D1_SIZE_F s) {return { p.x + s.width, p.y + s.height }; }
inline bool operator== (D2D1_POINT_2F p0, D2D1_POINT_2F p1) { return (p0.x == p1.x) && (p0.y == p1.y); }
inline bool operator!= (D2D1_POINT_2F p0, D2D1_POINT_2F p1) { return (p0.x != p1.x) || (p0.y != p1.y); }
