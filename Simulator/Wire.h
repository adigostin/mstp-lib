#pragma once
#include "Simulator.h"

using ConnectedWireEnd = Port*;
using LooseWireEnd = D2D1_POINT_2F;
using WireEnd = std::variant<LooseWireEnd, ConnectedWireEnd>;

class Wire;
struct WireInvalidateEvent : public Event<WireInvalidateEvent, void(Wire*)> { };

class Wire : public Object
{
	WireEnd _p0;
	WireEnd _p1;
	EventManager _em;
	
protected:
	virtual ~Wire() = default;

public:
	const WireEnd& GetP0() const { return _p0; }
	void SetP0 (const WireEnd& p0);
	const WireEnd& GetP1() const { return _p1; }
	void SetP1 (const WireEnd& p1);
	D2D1_POINT_2F GetP0Coords() const;
	D2D1_POINT_2F GetP1Coords() const;
	WireInvalidateEvent::Subscriber GetWireInvalidateEvent() { return WireInvalidateEvent::Subscriber(_em); }

	virtual void Render (ID2D1RenderTarget* rt, const DrawingObjects& dos, IDWriteFactory* dWriteFactory, uint16_t vlanNumber) const override final;
	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const override final;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;
};
