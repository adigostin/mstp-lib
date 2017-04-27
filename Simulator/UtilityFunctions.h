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
	ComPtr<IDWriteFactory> _dWriteFactory;
	ComPtr<ID2D1SolidColorBrush> _poweredFillBrush;
	ComPtr<ID2D1SolidColorBrush> _unpoweredBrush;
	ComPtr<ID2D1SolidColorBrush> _brushWindowText;
	ComPtr<ID2D1SolidColorBrush> _brushWindow;
	ComPtr<ID2D1SolidColorBrush> _brushHighlight;
	ComPtr<ID2D1SolidColorBrush> _brushDiscardingPort;
	ComPtr<ID2D1SolidColorBrush> _brushLearningPort;
	ComPtr<ID2D1SolidColorBrush> _brushForwarding;
	ComPtr<ID2D1SolidColorBrush> _brushNoForwardingWire;
	ComPtr<ID2D1SolidColorBrush> _brushLoop;
	ComPtr<ID2D1SolidColorBrush> _brushTempWire;
	ComPtr<ID2D1StrokeStyle> _strokeStyleForwardingWire;
	ComPtr<ID2D1StrokeStyle> _strokeStyleNoForwardingWire;
	ComPtr<IDWriteTextFormat> _regularTextFormat;
	ComPtr<ID2D1StrokeStyle> _strokeStyleSelectionRect;
};

class Object : public IUnknown
{
	ULONG _refCount = 1;
protected:
	EventManager _em;
	virtual ~Object() { assert(_refCount == 0); }

public:
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override { return E_NOTIMPL; }

	virtual ULONG STDMETHODCALLTYPE AddRef() override final;
	virtual ULONG STDMETHODCALLTYPE Release() override final;

	struct InvalidateEvent : public Event<InvalidateEvent, void(Object*)> { };
	InvalidateEvent::Subscriber GetInvalidateEvent() { return InvalidateEvent::Subscriber(_em); }

	virtual void Render (ID2D1RenderTarget* rt, const DrawingObjects& dos, uint16_t vlanNumber) const = 0;
	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const = 0;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) = 0;
};

enum class Side { Left, Top, Right, Bottom };

struct TextLayout
{
	ComPtr<IDWriteTextLayout> layout;
	DWRITE_TEXT_METRICS metrics;

	static TextLayout Make (IDWriteFactory* dWriteFactory, IDWriteTextFormat* format, const wchar_t* str);
};

unsigned int GetTimestampMilliseconds();
D2D1::ColorF GetD2DSystemColor (int sysColorIndex);
bool HitTestLine (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance, D2D1_POINT_2F p0w, D2D1_POINT_2F p1w, float lineWidth);
bool PointInPolygon (const D2D1_POINT_2F* vertices, size_t vertexCount, D2D1_POINT_2F point);
D2D1_RECT_F InflateRect (const D2D1_RECT_F& rect, float distance);
void InflateRect (D2D1_RECT_F* rect, float distance);
D2D1_ROUNDED_RECT InflateRoundedRect (const D2D1_ROUNDED_RECT& rr, float distance);
void InflateRoundedRect (D2D1_ROUNDED_RECT* rr, float distance);
