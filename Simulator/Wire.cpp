
#include "pch.h"
#include "Wire.h"
#include "Bridge.h"

using namespace std;

void Wire::SetP0 (const WireEnd& p0)
{
	//if (_p0 != p0)
	//{
		_p0 = p0;
		WireInvalidateEvent::InvokeHandlers(_em, this);
	//}
}

void Wire::SetP1 (const WireEnd& p1)
{
	//if (_p1 != p1)
	//{
		_p1 = p1;
		WireInvalidateEvent::InvokeHandlers(_em, this);
	//}
}

void Wire::Render (ID2D1RenderTarget* rt, const DrawingObjects& dos) const
{
	auto p0Coords = holds_alternative<LooseWireEnd>(_p0) ? get<LooseWireEnd>(_p0) : get<ConnectedWireEnd>(_p0)->GetConnectionPointLocation();
	auto p1Coords = holds_alternative<LooseWireEnd>(_p1) ? get<LooseWireEnd>(_p1) : get<ConnectedWireEnd>(_p1)->GetConnectionPointLocation();
	rt->DrawLine (p0Coords, p1Coords, dos._brushNoForwardingWire, 2);
}

