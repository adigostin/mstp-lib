
#include "pch.h"
#include "wire.h"
#include "Bridge.h"
#include "Port.h"
#include "win32/utility_functions.h"
#include "simulator.h"

using namespace edge;

static constexpr float thickness = 2;

wire::wire (wire_end firstEnd, wire_end secondEnd)
	: _points({ firstEnd, secondEnd })
{ }

// Workaround for what seems like a library bug: std::variant's operators == and != not working.
static bool Same (const wire_end& a, const wire_end& b)
{
	if (a.index() != b.index())
		return false;

	if (std::holds_alternative<loose_wire_end>(a))
		return std::get<loose_wire_end>(a) == std::get<loose_wire_end>(b);
	else
		return std::get<connected_wire_end>(a) == std::get<connected_wire_end>(b);
}

void wire::SetPoint (size_t pointIndex, const wire_end& point)
{
	if (!Same(_points[pointIndex], point))
	{
		_points[pointIndex] = point;
		event_invoker<invalidate_e>()(this);
	}
}

D2D1_POINT_2F wire::GetPointCoords (size_t pointIndex) const
{
	if (std::holds_alternative<loose_wire_end>(_points[pointIndex]))
		return std::get<loose_wire_end>(_points[pointIndex]);
	else
		return std::get<connected_wire_end>(_points[pointIndex])->GetCPLocation();
}

void wire::Render (ID2D1RenderTarget* rt, const drawing_resources& dos, bool forwarding, bool hasLoop) const
{
	float width = thickness;
	ID2D1Brush* brush;

	if (!forwarding)
		brush = dos._brushNoForwardingWire;
	else if (!hasLoop)
		brush = dos._brushForwarding;
	else
	{
		brush = dos._brushLoop;
		width *= 2;
	}

	auto& ss = forwarding ? dos._strokeStyleForwardingWire : dos._strokeStyleNoForwardingWire;
	rt->DrawLine (GetPointCoords(0), GetPointCoords(1), brush, width, ss);
}

void wire::RenderSelection (const zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const
{
	auto fd = zoomable->pointw_to_pointd(GetPointCoords(0));
	auto td = zoomable->pointw_to_pointd(GetPointCoords(1));

	float halfw = 10;
	float angle = atan2(td.y - fd.y, td.x - fd.x);
	float s = sin(angle);
	float c = cos(angle);

	D2D1_POINT_2F vertices[4] =
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

renderable_object::HTResult wire::HitTest (const zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance)
{
	for (size_t i = 0; i < _points.size(); i++)
	{
		auto pointWLocation = this->GetPointCoords(i);
		auto pointDLocation = zoomable->pointw_to_pointd(pointWLocation);
		D2D1_RECT_F rect = { pointDLocation.x, pointDLocation.y, pointDLocation.x, pointDLocation.y };
		InflateRect(&rect, tolerance);
		if (point_in_rect (rect, dLocation))
			return { this, (int) i };
	}

	if (HitTestLine (zoomable, dLocation, tolerance, GetPointCoords(0), GetPointCoords(1), thickness))
		return { this, -1 };

	return { };
}

static const _bstr_t WireElementName = "Wire";

com_ptr<IXMLDOMElement> wire::Serialize (project_i* project, IXMLDOMDocument* doc) const
{
	com_ptr<IXMLDOMElement> wireElement;
	HRESULT hr = doc->createElement (WireElementName, &wireElement); assert(SUCCEEDED(hr));

	hr = wireElement->appendChild(SerializeEnd(project, doc, _points[0]), nullptr); assert(SUCCEEDED(hr));
	hr = wireElement->appendChild(SerializeEnd(project, doc, _points[1]), nullptr); assert(SUCCEEDED(hr));

	return wireElement;
}

// static
std::unique_ptr<wire> wire::Deserialize (project_i* project, IXMLDOMElement* wireElement)
{
	com_ptr<IXMLDOMNode> firstChild;
	auto hr = wireElement->get_firstChild(&firstChild); assert(SUCCEEDED(hr));
	auto firstEnd = DeserializeEnd (project, (com_ptr<IXMLDOMElement>) firstChild);

	com_ptr<IXMLDOMNode> secondChild;
	hr = firstChild->get_nextSibling(&secondChild); assert(SUCCEEDED(hr));
	auto secondEnd = DeserializeEnd (project, (com_ptr<IXMLDOMElement>) secondChild);

	auto w = std::make_unique<wire>(firstEnd, secondEnd);
	return w;
}

static const _bstr_t ConnectedEndString = "ConnectedEnd";
static const _bstr_t BridgeIndexString  = "BridgeIndex";
static const _bstr_t PortIndexString    = "PortIndex";
static const _bstr_t LooseEndString     = "LooseEnd";
static const _bstr_t XString            = "X";
static const _bstr_t YString            = "Y";

//static
com_ptr<IXMLDOMElement> wire::SerializeEnd (project_i* project, IXMLDOMDocument* doc, const wire_end& end)
{
	HRESULT hr;
	com_ptr<IXMLDOMElement> element;

	if (std::holds_alternative<connected_wire_end>(end))
	{
		hr = doc->createElement (ConnectedEndString, &element); assert(SUCCEEDED(hr));
		auto port = std::get<connected_wire_end>(end);
		auto& bridges = project->bridges();
		auto it = find_if (bridges.begin(), bridges.end(), [port](auto& up) { return up.get() == port->bridge(); });
		auto bridgeIndex = it - bridges.begin();
		hr = element->setAttribute (BridgeIndexString, _variant_t(std::to_string(bridgeIndex).c_str())); assert(SUCCEEDED(hr));
		hr = element->setAttribute (PortIndexString, _variant_t(std::to_string(port->GetPortIndex()).c_str())); assert(SUCCEEDED(hr));
	}
	else //if (std::holds_alternative<loose_wire_end>(end))
	{
		auto& location = std::get<loose_wire_end>(end);
		hr = doc->createElement (LooseEndString, &element); assert(SUCCEEDED(hr));
		hr = element->setAttribute (XString, _variant_t(location.x)); assert(SUCCEEDED(hr));
		hr = element->setAttribute (YString, _variant_t(location.y)); assert(SUCCEEDED(hr));
	}

	return element;
}

//static
wire_end wire::DeserializeEnd (project_i* project, IXMLDOMElement* element)
{
	HRESULT hr;

	_bstr_t name;
	hr = element->get_nodeName(name.GetAddress()); assert(SUCCEEDED(hr));

	if (wcscmp(name, ConnectedEndString) == 0)
	{
		_variant_t value;
		hr = element->getAttribute (BridgeIndexString, &value); assert (SUCCEEDED(hr) && (value.vt == VT_BSTR));
		size_t bridgeIndex = wcstoul (value.bstrVal, nullptr, 10);
		hr = element->getAttribute (PortIndexString, &value); assert (SUCCEEDED(hr) && (value.vt == VT_BSTR));
		size_t portIndex = wcstoul (value.bstrVal, nullptr, 10);
		return connected_wire_end { project->bridges()[bridgeIndex]->GetPorts()[portIndex].get() };
	}
	else if (wcscmp (name, LooseEndString) == 0)
	{
		_variant_t value;
		hr = element->getAttribute (XString, &value); assert (SUCCEEDED(hr) && (value.vt == VT_BSTR));
		float x = wcstof (value.bstrVal, nullptr);
		hr = element->getAttribute (YString, &value); assert (SUCCEEDED(hr) && (value.vt == VT_BSTR));
		float y = wcstof (value.bstrVal, nullptr);
		return loose_wire_end { x, y };
	}
	else
	{
		assert(false); // not implemented
		return nullptr;
	}
}
