#pragma once
#include "EventManager.h"

class Object;

struct HTResult
{
	Object* object;
	int code;
};

struct DrawingObjects
{
	IDWriteFactoryPtr _dWriteFactory;
	ID2D1SolidColorBrushPtr _poweredFillBrush;
	ID2D1SolidColorBrushPtr _unpoweredBrush;
	ID2D1SolidColorBrushPtr _brushWindowText;
	ID2D1SolidColorBrushPtr _brushWindow;
	ID2D1SolidColorBrushPtr _brushHighlight;
	ID2D1SolidColorBrushPtr _brushDiscardingPort;
	ID2D1SolidColorBrushPtr _brushLearningPort;
	ID2D1SolidColorBrushPtr _brushForwarding;
	ID2D1SolidColorBrushPtr _brushNoForwardingWire;
	ID2D1SolidColorBrushPtr _brushLoop;
	ID2D1SolidColorBrushPtr _brushTempWire;
	ID2D1StrokeStylePtr _strokeStyleForwardingWire;
	ID2D1StrokeStylePtr _strokeStyleNoForwardingWire;
	IDWriteTextFormatPtr _regularTextFormat;
	ID2D1StrokeStylePtr _strokeStyleSelectionRect;
};

struct IZoomable;

class Object : public EventManager
{
public:
	virtual ~Object() = default;

	struct InvalidateEvent : public Event<InvalidateEvent, void(Object*)> { };
	InvalidateEvent::Subscriber GetInvalidateEvent() { return InvalidateEvent::Subscriber(*this); }

	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const = 0;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) = 0;

	template<typename T>
	bool Is() const { return dynamic_cast<const T*>(this) != nullptr; }
};

enum class Side { Left, Top, Right, Bottom };

struct TextLayout
{
	IDWriteTextLayoutPtr layout;
	DWRITE_TEXT_METRICS metrics;

	static TextLayout Create (IDWriteFactory* dWriteFactory, IDWriteTextFormat* format, const wchar_t* str);
};

unsigned int GetTimestampMilliseconds();
D2D1::ColorF GetD2DSystemColor (int sysColorIndex);
bool HitTestLine (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance, D2D1_POINT_2F p0w, D2D1_POINT_2F p1w, float lineWidth);
bool PointInPolygon (const D2D1_POINT_2F* vertices, size_t vertexCount, D2D1_POINT_2F point);
D2D1_RECT_F InflateRect (const D2D1_RECT_F& rect, float distance);
void InflateRect (D2D1_RECT_F* rect, float distance);
D2D1_ROUNDED_RECT InflateRoundedRect (const D2D1_ROUNDED_RECT& rr, float distance);
void InflateRoundedRect (D2D1_ROUNDED_RECT* rr, float distance);
