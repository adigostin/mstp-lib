
#pragma once
#include "object.h"
#include "win32/com_ptr.h"
#include "win32/win32_lib.h"

struct DrawingObjects
{
	edge::com_ptr<IDWriteFactory> _dWriteFactory;
	edge::com_ptr<ID2D1SolidColorBrush> _poweredFillBrush;
	edge::com_ptr<ID2D1SolidColorBrush> _unpoweredBrush;
	edge::com_ptr<ID2D1SolidColorBrush> _brushWindowText;
	edge::com_ptr<ID2D1SolidColorBrush> _brushWindow;
	edge::com_ptr<ID2D1SolidColorBrush> _brushHighlight;
	edge::com_ptr<ID2D1SolidColorBrush> _brushDiscardingPort;
	edge::com_ptr<ID2D1SolidColorBrush> _brushLearningPort;
	edge::com_ptr<ID2D1SolidColorBrush> _brushForwarding;
	edge::com_ptr<ID2D1SolidColorBrush> _brushNoForwardingWire;
	edge::com_ptr<ID2D1SolidColorBrush> _brushLoop;
	edge::com_ptr<ID2D1SolidColorBrush> _brushTempWire;
	edge::com_ptr<ID2D1StrokeStyle> _strokeStyleForwardingWire;
	edge::com_ptr<ID2D1StrokeStyle> _strokeStyleNoForwardingWire;
	edge::com_ptr<IDWriteTextFormat> _regularTextFormat;
	edge::com_ptr<IDWriteTextFormat> _smallTextFormat;
	edge::com_ptr<ID2D1StrokeStyle> _strokeStyleSelectionRect;
};

class renderable_object : public edge::object
{
public:
	struct HTResult
	{
		renderable_object* object;
		int code;
		bool operator==(const HTResult& other) const { return (this->object == other.object) && (this->code == other.code); }
		bool operator!=(const HTResult& other) const { return (this->object != other.object) || (this->code != other.code); }
	};

	struct invalidate_e : public edge::event<invalidate_e, renderable_object*> { };
	invalidate_e::subscriber GetInvalidateEvent() { return invalidate_e::subscriber(this); }

	virtual void RenderSelection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const = 0;
	virtual HTResult HitTest (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) = 0;
};
