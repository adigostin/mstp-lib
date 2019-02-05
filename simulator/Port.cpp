
#include "pch.h"
#include "Port.h"
#include "Bridge.h"
#include "win32/utility_functions.h"

using namespace std;
using namespace D2D1;
using namespace edge;

Port::Port (Bridge* bridge, unsigned int portIndex, side side, float offset)
	: _bridge(bridge), _portIndex(portIndex), _side(side), _offset(offset)
{
	for (unsigned int treeIndex = 0; treeIndex < (unsigned int) bridge->trees().size(); treeIndex++)
	{
		auto tree = unique_ptr<PortTree>(new PortTree(this, treeIndex));
		_trees.push_back (move(tree));
	}
}

Port::~Port()
{
}

D2D1_POINT_2F Port::GetCPLocation() const
{
	auto bounds = _bridge->GetBounds();

	if (_side == side::left)
		return Point2F (bounds.left - ExteriorHeight, bounds.top + _offset);

	if (_side == side::right)
		return Point2F (bounds.right + ExteriorHeight, bounds.top + _offset);

	if (_side == side::top)
		return Point2F (bounds.left + _offset, bounds.top - ExteriorHeight);

	// _side == side::bottom
	return Point2F (bounds.left + _offset, bounds.bottom + ExteriorHeight);
}

Matrix3x2F Port::GetPortTransform() const
{
	if (_side == side::left)
	{
		//portTransform = Matrix3x2F::Rotation (90, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.left, bridgeRect.top + port->GetOffset ());
		// The above calculation is correct but slow. Let's assign the matrix members directly.
		return { 0, 1, -1, 0, _bridge->GetLeft(), _bridge->GetTop() + _offset};
	}
	else if (_side == side::right)
	{
		//portTransform = Matrix3x2F::Rotation (270, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.right, bridgeRect.top + port->GetOffset ());
		return { 0, -1, 1, 0, _bridge->GetRight(), _bridge->GetTop() + _offset };
	}
	else if (_side == side::top)
	{
		//portTransform = Matrix3x2F::Rotation (180, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.left + port->GetOffset (), bridgeRect.top);
		return { -1, 0, 0, -1, _bridge->GetLeft() + _offset, _bridge->GetTop() };
	}
	else //if (_side == side::bottom)
	{
		//portTransform = Matrix3x2F::Translation (bridgeRect.left + port->GetOffset (), bridgeRect.bottom);
		return { 1, 0, 0, 1, _bridge->GetLeft() + _offset, _bridge->GetBottom() };
	}
}

// static
void Port::RenderExteriorNonStpPort (ID2D1RenderTarget* dc, const drawing_resources& dos, bool macOperational)
{
	auto& brush = macOperational ? dos._brushForwarding : dos._brushDiscardingPort;
	dc->DrawLine (Point2F (0, 0), Point2F (0, ExteriorHeight), brush, 2);
}

// static
void Port::RenderExteriorStpPort (ID2D1RenderTarget* dc, const drawing_resources& dos, STP_PORT_ROLE role, bool learning, bool forwarding, bool operEdge)
{
	static constexpr float circleDiameter = min (ExteriorHeight / 2, ExteriorWidth);

	static constexpr float edw = ExteriorWidth;
	static constexpr float edh = ExteriorHeight;

	static constexpr float discardingFirstHorizontalLineY = circleDiameter + (edh - circleDiameter) / 3;
	static constexpr float discardingSecondHorizontalLineY = circleDiameter + (edh - circleDiameter) * 2 / 3;
	static constexpr float learningHorizontalLineY = circleDiameter + (edh - circleDiameter) / 2;

	static constexpr float dfhly = discardingFirstHorizontalLineY;
	static constexpr float dshly = discardingSecondHorizontalLineY;

	static const D2D1_ELLIPSE ellipseFill = { Point2F (0, circleDiameter / 2), circleDiameter / 2 + 0.5f, circleDiameter / 2 + 0.5f };
	static const D2D1_ELLIPSE ellipseDraw = { Point2F (0, circleDiameter / 2), circleDiameter / 2 - 0.5f, circleDiameter / 2 - 0.5f};

	auto oldaa = dc->GetAntialiasMode();

	if (role == STP_PORT_ROLE_DISABLED)
	{
		// disabled
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->DrawLine (Point2F (-edw / 2, edh / 3), Point2F (edw / 2, edh * 2 / 3), dos._brushDiscardingPort);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && !learning && !forwarding)
	{
		// designated discarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->FillEllipse (&ellipseFill, dos._brushDiscardingPort);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && !forwarding)
	{
		// designated learning
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->FillEllipse (&ellipseFill, dos._brushLearningPort);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushLearningPort, 2);
		dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), dos._brushLearningPort);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && forwarding && !operEdge)
	{
		// designated forwarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->FillEllipse (&ellipseFill, dos._brushForwarding);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushForwarding, 2);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && forwarding && operEdge)
	{
		// designated forwarding operEdge
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->FillEllipse (&ellipseFill, dos._brushForwarding);
		static constexpr D2D1_POINT_2F points[] =
		{
			{ 0, circleDiameter },
			{ -edw / 2 + 1, circleDiameter + (edh - circleDiameter) / 2 },
			{ 0, edh },
			{ edw / 2 - 1, circleDiameter + (edh - circleDiameter) / 2 },
		};

		dc->DrawLine (points[0], points[1], dos._brushForwarding, 2);
		dc->DrawLine (points[1], points[2], dos._brushForwarding, 2);
		dc->DrawLine (points[2], points[3], dos._brushForwarding, 2);
		dc->DrawLine (points[3], points[0], dos._brushForwarding, 2);
	}
	else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && !learning && !forwarding)
	{
		// root or master discarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->DrawEllipse (&ellipseDraw, dos._brushDiscardingPort, 2);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && learning && !forwarding)
	{
		// root or master learning
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->DrawEllipse (&ellipseDraw, dos._brushLearningPort, 2);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushLearningPort, 2);
		dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), dos._brushLearningPort);
	}
	else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && learning && forwarding)
	{
		// root or master forwarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->DrawEllipse (&ellipseDraw, dos._brushForwarding, 2);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushForwarding, 2);
	}
	else if ((role == STP_PORT_ROLE_ALTERNATE) && !learning && !forwarding)
	{
		// Alternate discarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if ((role == STP_PORT_ROLE_ALTERNATE) && learning && !forwarding)
	{
		// Alternate learning
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushLearningPort, 2);
		dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), dos._brushLearningPort);
	}
	else if ((role == STP_PORT_ROLE_BACKUP) && !learning && !forwarding)
	{
		// Backup discarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->DrawLine (Point2F (-edw / 2, dfhly / 2), Point2F (edw / 2, dfhly / 2), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if (role == STP_PORT_ROLE_UNKNOWN)
	{
		// Undefined
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->DrawText (L"?", 1, dos._regularTextFormat, { 2, 0, 20, 20 }, dos._brushDiscardingPort, D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
	}
	else
		assert(false); // not implemented

	dc->SetAntialiasMode(oldaa);
}

void Port::Render (ID2D1RenderTarget* rt, const drawing_resources& dos, unsigned int vlanNumber) const
{
	D2D1_MATRIX_3X2_F oldtr;
	rt->GetTransform(&oldtr);
	rt->SetTransform (GetPortTransform() * oldtr);

	// Draw the exterior of the port.
	float interiorPortOutlineWidth = OutlineWidth;
	auto b = _bridge->stp_bridge();
	auto treeIndex  = STP_GetTreeIndexFromVlanNumber(b, vlanNumber);
	if (STP_IsBridgeStarted (b))
	{
		auto role       = STP_GetPortRole (b, (unsigned int) _portIndex, treeIndex);
		auto learning   = STP_GetPortLearning (b, (unsigned int) _portIndex, treeIndex);
		auto forwarding = STP_GetPortForwarding (b, (unsigned int) _portIndex, treeIndex);
		auto operEdge   = STP_GetPortOperEdge (b, (unsigned int) _portIndex);
		RenderExteriorStpPort (rt, dos, role, learning, forwarding, operEdge);

		if (role == STP_PORT_ROLE_ROOT)
			interiorPortOutlineWidth *= 2;
	}
	else
		RenderExteriorNonStpPort(rt, dos, GetMacOperational());

	// Draw the interior of the port.
	auto portRect = D2D1_RECT_F { -InteriorWidth / 2, -InteriorDepth, InteriorWidth / 2, 0 };
	InflateRect (&portRect, -interiorPortOutlineWidth / 2);
	rt->FillRectangle (&portRect, GetMacOperational() ? dos._poweredFillBrush : dos._unpoweredBrush);
	rt->DrawRectangle (&portRect, dos._brushWindowText, interiorPortOutlineWidth);

	com_ptr<IDWriteTextFormat> format;
	auto hr = dos._dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 9, L"en-US", &format); assert(SUCCEEDED(hr));
	com_ptr<IDWriteTextLayout> layout;
	wstringstream ss;
	ss << setfill(L'0') << setw(4) << hex << STP_GetPortIdentifier(b, (unsigned int) _portIndex, treeIndex);
	auto portIdText = ss.str();
	hr = dos._dWriteFactory->CreateTextLayout (portIdText.c_str(), (UINT32) portIdText.length(), format, 10000, 10000, &layout); assert(SUCCEEDED(hr));
	DWRITE_TEXT_METRICS metrics;
	hr = layout->GetMetrics(&metrics); assert(SUCCEEDED(hr));
	DWRITE_LINE_METRICS lineMetrics;
	UINT32 actualLineCount;
	hr = layout->GetLineMetrics(&lineMetrics, 1, &actualLineCount); assert(SUCCEEDED(hr));
	rt->DrawTextLayout ({ -metrics.width / 2, -lineMetrics.baseline - OutlineWidth * 2 - 1}, layout, dos._brushWindowText);

	rt->SetTransform (&oldtr);
}

D2D1_RECT_F Port::GetInnerOuterRect() const
{
	auto tl = D2D1_POINT_2F { -InteriorWidth / 2, -InteriorDepth };
	auto br = D2D1_POINT_2F { InteriorWidth / 2, ExteriorHeight };
	auto tr = GetPortTransform();
	tl = tr.TransformPoint(tl);
	br = tr.TransformPoint(br);
	return { min(tl.x, br.x), min (tl.y, br.y), max(tl.x, br.x), max(tl.y, br.y) };
}

void Port::render_selection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const
{
	auto ir = GetInnerOuterRect();

	auto oldaa = rt->GetAntialiasMode();
	rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

	auto lt = zoomable->pointw_to_pointd ({ ir.left, ir.top });
	auto rb = zoomable->pointw_to_pointd ({ ir.right, ir.bottom });
	rt->DrawRectangle ({ lt.x - 10, lt.y - 10, rb.x + 10, rb.y + 10 }, dos._brushHighlight, 2, dos._strokeStyleSelectionRect);

	rt->SetAntialiasMode(oldaa);
}

bool Port::HitTestCP (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) const
{
	auto cpWLocation = GetCPLocation();
	auto cpDLocation = zoomable->pointw_to_pointd(cpWLocation);

	return (abs (cpDLocation.x - dLocation.x) <= tolerance)
		&& (abs (cpDLocation.y - dLocation.y) <= tolerance);
}

bool Port::HitTestInnerOuter (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) const
{
	auto ir = GetInnerOuterRect();
	auto lt = zoomable->pointw_to_pointd ({ ir.left, ir.top });
	auto rb = zoomable->pointw_to_pointd ({ ir.right, ir.bottom });
	return (dLocation.x >= lt.x) && (dLocation.y >= lt.y) && (dLocation.x < rb.x) && (dLocation.y < rb.y);
}

renderable_object::HTResult Port::hit_test (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance)
{
	if (HitTestCP (zoomable, dLocation, tolerance))
		return { this, HTCodeCP };

	if (HitTestInnerOuter (zoomable, dLocation, tolerance))
		return { this, HTCodeInnerOuter };

	return { };
}

bool Port::IsForwarding (unsigned int vlanNumber) const
{
	auto stpb = _bridge->stp_bridge();
	if (!STP_IsBridgeStarted(stpb))
		return true;

	auto treeIndex = STP_GetTreeIndexFromVlanNumber(stpb, vlanNumber);
	return STP_GetPortForwarding (stpb, (unsigned int) _portIndex, treeIndex);
}

void Port::SetSideAndOffset (side side, float offset)
{
	if ((_side != side) || (_offset != offset))
	{
		_side = side;
		_offset = offset;
		event_invoker<invalidate_e>()(this);
	}
}

bool Port::GetMacOperational() const
{
	return _missedLinkPulseCounter < MissedLinkPulseCounterMax;
}

bool Port::auto_edge() const
{
	return STP_GetPortAutoEdgeToSnmpTruthValue (_bridge->stp_bridge(), _portIndex) == 1;
}

void Port::set_auto_edge (bool autoEdge)
{
	unsigned int snmp_truth_value = autoEdge ? 1 : 2;
	STP_SetPortAutoEdgeFromSnmpTruthValue (_bridge->stp_bridge(), _portIndex, snmp_truth_value, (unsigned int) GetMessageTime());
}

bool Port::admin_edge() const
{
	return STP_GetPortAdminEdgeToSnmpTruthValue (_bridge->stp_bridge(), _portIndex) == 1;
}

void Port::set_admin_edge (bool adminEdge)
{
	unsigned int snmp_truth_value = adminEdge ? 1 : 2;
	STP_SetPortAdminEdgeFromSnmpTruthValue (_bridge->stp_bridge(), _portIndex, snmp_truth_value, (unsigned int) GetMessageTime());
}

static const _bstr_t PortString = "Port";
static const _bstr_t SideString = L"Side";
static const _bstr_t OffsetString = L"Offset";
static const _bstr_t AutoEdgeString = L"AutoEdge";
static const _bstr_t AdminEdgeString = L"AdminEdge";
static const _bstr_t PortTreesString = "PortTrees";

com_ptr<IXMLDOMElement> Port::Serialize (IXMLDOMDocument3* doc) const
{
	com_ptr<IXMLDOMElement> portElement;
	auto hr = doc->createElement (PortString, &portElement); assert(SUCCEEDED(hr));
	portElement->setAttribute (SideString, _variant_t (enum_converters<side, SideNVPs>::enum_to_string(_side).c_str()));
	portElement->setAttribute (OffsetString, _variant_t (_offset));
	portElement->setAttribute (AutoEdgeString, _variant_t (auto_edge() ? L"True" : L"False"));
	portElement->setAttribute (AdminEdgeString, _variant_t (admin_edge() ? L"True" : L"False"));

	com_ptr<IXMLDOMElement> portTreesElement;
	hr = doc->createElement (PortTreesString, &portTreesElement); assert(SUCCEEDED(hr));
	hr = portElement->appendChild(portTreesElement, nullptr); assert(SUCCEEDED(hr));
	for (size_t treeIndex = 0; treeIndex < _trees.size(); treeIndex++)
	{
		com_ptr<IXMLDOMElement> portTreeElement;
		hr = _trees.at(treeIndex)->Serialize (doc, portTreeElement); assert(SUCCEEDED(hr));
		hr = portTreesElement->appendChild (portTreeElement, nullptr); assert(SUCCEEDED(hr));
	}

	return portElement;
}

HRESULT Port::Deserialize (IXMLDOMElement* portElement)
{
	_variant_t value;
	auto hr = portElement->getAttribute (SideString, &value);
	if (FAILED(hr))
		return hr;
	if (value.vt == VT_BSTR)
	{
		bool ok = enum_converters<side, SideNVPs>::enum_from_string(wstring_view(value.bstrVal), _side);
		assert(ok);
	}

	hr = portElement->getAttribute (OffsetString, &value);
	if (FAILED(hr))
		return hr;
	if (value.vt == VT_BSTR)
		_offset = wcstof (value.bstrVal, nullptr);

	hr = portElement->getAttribute (AutoEdgeString, &value);
	if (FAILED(hr))
		return hr;
	if (value.vt == VT_BSTR)
		set_auto_edge (_wcsicmp (value.bstrVal, L"True") == 0);

	hr = portElement->getAttribute (AdminEdgeString, &value);
	if (FAILED(hr))
		return hr;
	if (value.vt == VT_BSTR)
		set_admin_edge (_wcsicmp (value.bstrVal, L"True") == 0);

	com_ptr<IXMLDOMNode> portTreesNode;
	hr = portElement->selectSingleNode(PortTreesString, &portTreesNode);
	if (SUCCEEDED(hr) && (portTreesNode != nullptr))
	{
		com_ptr<IXMLDOMNodeList> portTreeNodes;
		hr = portTreesNode->get_childNodes(&portTreeNodes); assert(SUCCEEDED(hr));

		for (size_t treeIndex = 0; treeIndex < _trees.size(); treeIndex++)
		{
			com_ptr<IXMLDOMNode> portTreeNode;
			hr = portTreeNodes->get_item((long) treeIndex, &portTreeNode); assert(SUCCEEDED(hr));
			com_ptr<IXMLDOMElement> portTreeElement = portTreeNode;
			hr = _trees.at(treeIndex)->Deserialize(portTreeElement); assert(SUCCEEDED(hr));
		}
	}

	return S_OK;
}

unsigned int Port::GetDetectedPortPathCost() const
{
	return STP_GetDetectedPortPathCost(_bridge->stp_bridge(), _portIndex);
}

unsigned int Port::GetAdminExternalPortPathCost() const
{
	return STP_GetAdminPortPathCost (_bridge->stp_bridge(), _portIndex, 0);
}

void Port::SetAdminExternalPortPathCost(unsigned int adminExternalPortPathCost)
{
	STP_SetAdminPortPathCost (_bridge->stp_bridge(), _portIndex, 0, adminExternalPortPathCost, GetMessageTime());
	// TODO: invoke PropertyChangedEvent
}

unsigned int Port::GetExternalPortPathCost() const
{
	unsigned int treeIndex = 0; // CIST
	return STP_GetPortPathCost (_bridge->stp_bridge(), _portIndex, treeIndex);
}

const bool_property Port::auto_edge_property
(
	"AutoEdge",
	edge::misc_group_name,
	"The administrative value of the Auto Edge Port parameter. "
		"A value of true(1) indicates if the Bridge detection state machine (BDM, 13.31) "
		"is to detect other Bridges attached to the LAN, and set ieee8021SpanningTreeRstpPortOperEdgePort automatically. "
		"The default value is true(1) This is optional and provided only by implementations that support the automatic "
		"identification of edge ports. The value of this object MUST be retained across reinitializations of the management system.",
	static_cast<bool_property::getter_t>(&auto_edge),
	static_cast<bool_property::setter_t>(&set_auto_edge),
	true
);

const bool_property Port::admin_edge_property
(
	"AdminEdge",
	edge::misc_group_name,
	"The administrative value of the Edge Port parameter. "
		"A value of true(1) indicates that this port should be assumed as an edge-port, and a value of false(2) indicates "
		"that this port should be assumed as a non-edge-port. Setting this object will also cause the corresponding instance "
		"of dot1dStpPortOperEdgePort to change to the same value. Note that even when this object's value is true, "
		"the value of the corresponding instance of dot1dStpPortOperEdgePort can be false if a BPDU has been received. "
		"The value of this object MUST be retained across reinitializations of the management system",
	static_cast<bool_property::getter_t>(&admin_edge),
	static_cast<bool_property::setter_t>(&set_admin_edge),
	false
);

const bool_property Port::MacOperational (
	"MAC_Operational",
	edge::misc_group_name,
	nullptr,
	static_cast<bool_property::getter_t>(&GetMacOperational),
	nullptr,
	false);

const uint32_property Port::DetectedPortPathCost (
	"DetectedPortPathCost",
	edge::misc_group_name,
	nullptr,
	static_cast<uint32_property::getter_t>(&GetDetectedPortPathCost),
	nullptr,
	0);

const uint32_property Port::AdminExternalPortPathCost (
	"AdminExternalPortPathCost",
	edge::misc_group_name,
	nullptr,
	static_cast<uint32_property::getter_t>(&GetAdminExternalPortPathCost),
	static_cast<uint32_property::setter_t>(&SetAdminExternalPortPathCost),
	0);

const uint32_property Port::ExternalPortPathCost (
	"ExternalPortPathCost",
	edge::misc_group_name,
	nullptr,
  	static_cast<uint32_property::getter_t>(&GetExternalPortPathCost),
	nullptr,
	0);

const edge::property* const Port::_properties[] =
{
	&auto_edge_property,
	&admin_edge_property,
	&MacOperational,
	&DetectedPortPathCost,
	&AdminExternalPortPathCost,
	&ExternalPortPathCost,
};

const edge::type_t Port::_type = { "port", &base::_type, _properties };

