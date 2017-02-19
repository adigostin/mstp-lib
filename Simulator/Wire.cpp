
#include "pch.h"
#include "Wire.h"
#include "Bridge.h"
#include "Port.h"

using namespace std;

static constexpr float WireThickness = 2;

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

D2D1_POINT_2F Wire::GetP0Coords() const
{
	return holds_alternative<LooseWireEnd>(_p0) ? get<LooseWireEnd>(_p0) : get<ConnectedWireEnd>(_p0)->GetCPLocation();
}

D2D1_POINT_2F Wire::GetP1Coords() const
{
	return holds_alternative<LooseWireEnd>(_p1) ? get<LooseWireEnd>(_p1) : get<ConnectedWireEnd>(_p1)->GetCPLocation();
}

void Wire::Render (ID2D1RenderTarget* rt, const DrawingObjects& dos, IDWriteFactory* dWriteFactory, uint16_t vlanNumber) const
{
	rt->DrawLine (GetP0Coords(), GetP1Coords(), dos._brushNoForwardingWire, WireThickness);
}

void Wire::RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const
{
	auto fd = zoomable->GetDLocationFromWLocation(GetP0Coords());
	auto td = zoomable->GetDLocationFromWLocation(GetP1Coords());

	float halfw = 10;
	float angle = atan2(td.y - fd.y, td.x - fd.x);
	float s = sin(angle);
	float c = cos(angle);

	array<D2D1_POINT_2F, 4> vertices = 
	{
		D2D1_POINT_2F { fd.x + s * halfw, fd.y - c * halfw },
		D2D1_POINT_2F { fd.x - s * halfw, fd.y + c * halfw },
		D2D1_POINT_2F { td.x - s * halfw, td.y + c * halfw },
		D2D1_POINT_2F { td.x + s * halfw, td.y - c * halfw }
	};

	rt->DrawLine (vertices[0], vertices[1], dos._brushHighlight, 2, dos._strokeStyleSelectionRect);
	rt->DrawLine (vertices[1], vertices[2], dos._brushHighlight, 2, dos._strokeStyleSelectionRect);
	rt->DrawLine (vertices[2], vertices[3], dos._brushHighlight, 2, dos._strokeStyleSelectionRect);
	rt->DrawLine (vertices[3], vertices[0], dos._brushHighlight, 2, dos._strokeStyleSelectionRect);
}

HTResult Wire::HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance)
{
	if (HitTestLine (zoomable, dLocation, tolerance, GetP0Coords(), GetP1Coords(), WireThickness))
		return { this, 1 };

	return { };
}


