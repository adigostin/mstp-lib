
#include "pch.h"
#include "Wire.h"
#include "Bridge.h"
#include "Port.h"

using namespace std;

static constexpr float WireThickness = 2;

// Workaround for what seems like a library bug: std::variant's operators == and != not working.
static bool Same (const WireEnd& a, const WireEnd& b)
{
	if (a.index() != b.index())
		return false;

	if (holds_alternative<LooseWireEnd>(a))
	{
		auto aa = get<LooseWireEnd>(a);
		auto bb = get<LooseWireEnd>(b);
		return (aa.x == bb.x) && (aa.y == bb.y);
	}
	else
		return get<ConnectedWireEnd>(a) == get<ConnectedWireEnd>(b);
}

void Wire::SetPoint (size_t pointIndex, const WireEnd& point)
{
	if (!Same(_points[pointIndex], point))
	{
		/*
		if (holds_alternative<ConnectedWireEnd>(_points[pointIndex]) && holds_alternative<ConnectedWireEnd>(_points[1 - pointIndex]))
		{
			// disconnecting two ports
			auto portA = get<ConnectedWireEnd>(_points[pointIndex]);
			auto portB = get<ConnectedWireEnd>(_points[1 - pointIndex]);
		}
		*/
		_points[pointIndex] = point;
		/*
		if (holds_alternative<ConnectedWireEnd>(_points[pointIndex]) && holds_alternative<ConnectedWireEnd>(_points[1 - pointIndex]))
		{
			// connected two ports
			auto portA = get<ConnectedWireEnd>(_points[pointIndex]);
			auto portB = get<ConnectedWireEnd>(_points[1 - pointIndex]);
		}
		*/
		WireInvalidateEvent::InvokeHandlers(_em, this);
	}
}

D2D1_POINT_2F Wire::GetPointCoords (size_t pointIndex) const
{
	if (holds_alternative<LooseWireEnd>(_points[pointIndex]))
		return get<LooseWireEnd>(_points[pointIndex]);
	else
		return get<ConnectedWireEnd>(_points[pointIndex])->GetCPLocation();
}

void Wire::Render (ID2D1RenderTarget* rt, const DrawingObjects& dos, IDWriteFactory* dWriteFactory, uint16_t vlanNumber) const
{
	bool forwarding = false;
	if (holds_alternative<ConnectedWireEnd>(_points[0]) && holds_alternative<ConnectedWireEnd>(_points[1]))
	{
		auto portA = get<ConnectedWireEnd>(_points[0]);
		auto portB = get<ConnectedWireEnd>(_points[1]);
		bool portAFw = portA->GetBridge()->IsPortForwardingOnVlan(portA->GetPortIndex(), vlanNumber);
		bool portBFw = portB->GetBridge()->IsPortForwardingOnVlan(portB->GetPortIndex(), vlanNumber);
		forwarding = portAFw && portBFw;
	}

	auto brush = forwarding ? dos._brushForwarding.Get() : dos._brushNoForwardingWire.Get();
	rt->DrawLine (GetP0Coords(), GetP1Coords(), brush, WireThickness);
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


