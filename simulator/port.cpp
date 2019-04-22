
#include "pch.h"
#include "port.h"
#include "bridge.h"
#include "win32/utility_functions.h"
#include "win32/d2d_window.h"

using namespace std;
using namespace D2D1;
using namespace edge;

const char side_type_name[] = "side";

const edge::NVP side_nvps[] =
{
	{ "Left",   (int) side::left },
	{ "Top",    (int) side::top },
	{ "Right",  (int) side::right },
	{ "Bottom", (int) side::bottom },
	{ 0, 0 },
};

const char admin_p2p_type_name[] = "admin_p2p";
const edge::NVP admin_p2p_nvps[] =
{
	{ "ForceTrue", (int)STP_ADMIN_P2P_FORCE_TRUE },
	{ "ForceFalse", (int)STP_ADMIN_P2P_FORCE_FALSE },
	{ "Auto", (int)STP_ADMIN_P2P_AUTO },
	{ 0, 0 },
};

port::port (class bridge* bridge, unsigned int portIndex, enum side side, float offset)
	: _bridge(bridge), _portIndex(portIndex), _side(side), _offset(offset)
{
	for (unsigned int treeIndex = 0; treeIndex < (unsigned int) bridge->trees().size(); treeIndex++)
	{
		auto tree = unique_ptr<port_tree>(new port_tree(this, treeIndex));
		_trees.push_back (move(tree));
	}
}

D2D1_POINT_2F port::GetCPLocation() const
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

Matrix3x2F port::GetPortTransform() const
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
void port::RenderExteriorNonStpPort (ID2D1RenderTarget* dc, const drawing_resources& dos, bool macOperational)
{
	auto& brush = macOperational ? dos._brushForwarding : dos._brushDiscardingPort;
	dc->DrawLine (Point2F (0, 0), Point2F (0, ExteriorHeight), brush, 2);
}

// static
void port::RenderExteriorStpPort (ID2D1RenderTarget* dc, const drawing_resources& dos, STP_PORT_ROLE role, bool learning, bool forwarding, bool operEdge)
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
	else
		assert(false); // not implemented

	dc->SetAntialiasMode(oldaa);
}

void port::Render (ID2D1RenderTarget* rt, const drawing_resources& dos, unsigned int vlanNumber) const
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
		auto role       = STP_GetPortRole (b, _portIndex, treeIndex);
		auto learning   = STP_GetPortLearning (b, _portIndex, treeIndex);
		auto forwarding = STP_GetPortForwarding (b, _portIndex, treeIndex);
		auto operEdge   = STP_GetPortOperEdge (b, _portIndex);
		RenderExteriorStpPort (rt, dos, role, learning, forwarding, operEdge);

		if (STP_GetTxCount(b, _portIndex) == STP_GetTxHoldCount(b))
		{
			auto layout = edge::text_layout::create (dos._dWriteFactory, dos._smallTextFormat, "txCount=TxHoldCount");
			D2D1_MATRIX_3X2_F old;
			rt->GetTransform(&old);
			rt->SetTransform (Matrix3x2F::Rotation(-90) * old);
			rt->DrawTextLayout ({ -layout.metrics.width - 3, 2 }, layout.layout, dos._brushWindowText);
			rt->SetTransform(&old);
		}

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
	ss << setfill(L'0') << setw(4) << hex << STP_GetPortIdentifier(b, _portIndex, treeIndex);
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

D2D1_RECT_F port::GetInnerOuterRect() const
{
	auto tl = D2D1_POINT_2F { -InteriorWidth / 2, -InteriorDepth };
	auto br = D2D1_POINT_2F { InteriorWidth / 2, ExteriorHeight };
	auto tr = GetPortTransform();
	tl = tr.TransformPoint(tl);
	br = tr.TransformPoint(br);
	return { min(tl.x, br.x), min (tl.y, br.y), max(tl.x, br.x), max(tl.y, br.y) };
}

void port::render_selection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const
{
	auto ir = GetInnerOuterRect();

	auto oldaa = rt->GetAntialiasMode();
	rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

	auto lt = zoomable->pointw_to_pointd ({ ir.left, ir.top });
	auto rb = zoomable->pointw_to_pointd ({ ir.right, ir.bottom });
	rt->DrawRectangle ({ lt.x - 10, lt.y - 10, rb.x + 10, rb.y + 10 }, dos._brushHighlight, 2, dos._strokeStyleSelectionRect);

	rt->SetAntialiasMode(oldaa);
}

bool port::HitTestCP (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) const
{
	auto cpWLocation = GetCPLocation();
	auto cpDLocation = zoomable->pointw_to_pointd(cpWLocation);

	return (abs (cpDLocation.x - dLocation.x) <= tolerance)
		&& (abs (cpDLocation.y - dLocation.y) <= tolerance);
}

bool port::HitTestInnerOuter (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) const
{
	auto ir = GetInnerOuterRect();
	auto lt = zoomable->pointw_to_pointd ({ ir.left, ir.top });
	auto rb = zoomable->pointw_to_pointd ({ ir.right, ir.bottom });
	return (dLocation.x >= lt.x) && (dLocation.y >= lt.y) && (dLocation.x < rb.x) && (dLocation.y < rb.y);
}

renderable_object::HTResult port::hit_test (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance)
{
	if (HitTestCP (zoomable, dLocation, tolerance))
		return { this, HTCodeCP };

	if (HitTestInnerOuter (zoomable, dLocation, tolerance))
		return { this, HTCodeInnerOuter };

	return { };
}

bool port::IsForwarding (unsigned int vlanNumber) const
{
	auto stpb = _bridge->stp_bridge();
	if (!STP_IsBridgeStarted(stpb))
		return true;

	auto treeIndex = STP_GetTreeIndexFromVlanNumber(stpb, vlanNumber);
	return STP_GetPortForwarding (stpb, (unsigned int) _portIndex, treeIndex);
}

void port::SetSideAndOffset (enum side side, float offset)
{
	if ((_side != side) || (_offset != offset))
	{
		_side = side;
		_offset = offset;
		event_invoker<invalidate_e>()(this);
	}
}

bool port::GetMacOperational() const
{
	return _missedLinkPulseCounter < MissedLinkPulseCounterMax;
}

bool port::auto_edge() const
{
	return STP_GetPortAutoEdge (_bridge->stp_bridge(), _portIndex);
}

void port::set_auto_edge (bool autoEdge)
{
	STP_SetPortAutoEdge (_bridge->stp_bridge(), _portIndex, autoEdge, (unsigned int) GetMessageTime());
}

bool port::admin_edge() const
{
	return STP_GetPortAdminEdge (_bridge->stp_bridge(), _portIndex);
}

void port::set_admin_edge (bool adminEdge)
{
	STP_SetPortAdminEdge (_bridge->stp_bridge(), _portIndex, adminEdge, (unsigned int) GetMessageTime());
}

unsigned int port::GetDetectedPortPathCost() const
{
	return STP_GetDetectedPortPathCost(_bridge->stp_bridge(), _portIndex);
}

unsigned int port::GetAdminExternalPortPathCost() const
{
	return STP_GetAdminExternalPortPathCost (_bridge->stp_bridge(), _portIndex);
}

void port::SetAdminExternalPortPathCost(unsigned int adminExternalPortPathCost)
{
	this->on_property_changing (&AdminExternalPortPathCost);
	this->on_property_changing (&ExternalPortPathCost);
	STP_SetAdminExternalPortPathCost (_bridge->stp_bridge(), _portIndex, adminExternalPortPathCost, GetMessageTime());
	this->on_property_changed (&ExternalPortPathCost);
	this->on_property_changed (&AdminExternalPortPathCost);
}

unsigned int port::GetExternalPortPathCost() const
{
	unsigned int treeIndex = 0; // CIST
	return STP_GetExternalPortPathCost (_bridge->stp_bridge(), _portIndex);
}

bool port::detected_p2p() const
{
	return STP_GetDetectedPointToPointMAC(_bridge->stp_bridge(), _portIndex);
}

STP_ADMIN_P2P port::admin_p2p() const
{
	return STP_GetAdminPointToPointMAC(_bridge->stp_bridge(), _portIndex);
}

void port::set_admin_p2p (STP_ADMIN_P2P admin_p2p)
{
	this->on_property_changing (&admin_p2p_property);
	this->on_property_changing (&oper_p2p_property);
	STP_SetAdminPointToPointMAC (_bridge->stp_bridge(), _portIndex, admin_p2p, ::GetMessageTime());
	this->on_property_changed (&oper_p2p_property);
	this->on_property_changed (&admin_p2p_property);
}

bool port::oper_p2p() const
{
	return STP_GetOperPointToPointMAC(_bridge->stp_bridge(), _portIndex);
}

const side_p port::side_property {
	"Side", nullptr, nullptr, ui_visible::no,
	static_cast<side_p::member_getter_t>(&side),
	static_cast<side_p::member_setter_t>(&set_side),
	side::bottom
};

const edge::float_p port::offset_property {
	"Offset", nullptr, nullptr, ui_visible::no,
	static_cast<float_p::member_getter_t>(&offset),
	static_cast<float_p::member_setter_t>(&set_offset),
	std::nullopt,
};

const bool_p port::auto_edge_property {
	"AutoEdge",
	nullptr,
	"The administrative value of the Auto Edge Port parameter. "
		"A value of true(1) indicates if the Bridge detection state machine (BDM, 13.31) "
		"is to detect other Bridges attached to the LAN, and set ieee8021SpanningTreeRstpPortOperEdgePort automatically. "
		"The default value is true(1) This is optional and provided only by implementations that support the automatic "
		"identification of edge ports. The value of this object MUST be retained across reinitializations of the management system.",
	ui_visible::yes,
	static_cast<bool_p::member_getter_t>(&auto_edge),
	static_cast<bool_p::member_setter_t>(&set_auto_edge),
	true,
};

const bool_p port::admin_edge_property {
	"AdminEdge",
	nullptr,
	"The administrative value of the Edge Port parameter. "
		"A value of true(1) indicates that this port should be assumed as an edge-port, and a value of false(2) indicates "
		"that this port should be assumed as a non-edge-port. Setting this object will also cause the corresponding instance "
		"of dot1dStpPortOperEdgePort to change to the same value. Note that even when this object's value is true, "
		"the value of the corresponding instance of dot1dStpPortOperEdgePort can be false if a BPDU has been received. "
		"The value of this object MUST be retained across reinitializations of the management system",
	ui_visible::yes,
	static_cast<bool_p::member_getter_t>(&admin_edge),
	static_cast<bool_p::member_setter_t>(&set_admin_edge),
	false,
};

const bool_p port::MacOperational {
	"MAC_Operational",
	nullptr,
	nullptr,
	ui_visible::yes,
	static_cast<bool_p::member_getter_t>(&GetMacOperational),
	nullptr,
	false,
};

static const edge::property_group port_path_cost_group = { 5, "Port Path Costs" };

const uint32_p port::DetectedPortPathCost {
	"DetectedPortPathCost",
	&port_path_cost_group,
	nullptr,
	ui_visible::yes,
	static_cast<uint32_p::member_getter_t>(&GetDetectedPortPathCost),
	nullptr,
	0,
};

const uint32_p port::AdminExternalPortPathCost {
	"AdminExternalPortPathCost",
	&port_path_cost_group,
	nullptr,
	ui_visible::yes,
	static_cast<uint32_p::member_getter_t>(&GetAdminExternalPortPathCost),
	static_cast<uint32_p::member_setter_t>(&SetAdminExternalPortPathCost),
	0,
};

const uint32_p port::ExternalPortPathCost {
	"ExternalPortPathCost",
	&port_path_cost_group,
	nullptr,
	ui_visible::yes,
	static_cast<uint32_p::member_getter_t>(&GetExternalPortPathCost),
	nullptr,
	0,
};

static const edge::property_group p2p_group = { 4, "Point to Point" };

const bool_p port::detected_p2p_property {
	"detectedPointToPointMAC",
	&p2p_group,
	nullptr,
	ui_visible::yes,
	static_cast<bool_p::member_getter_t>(&detected_p2p),
	nullptr,
	std::nullopt,
};

const admin_p2p_p port::admin_p2p_property {
	"adminPointToPointMAC",
	&p2p_group,
	nullptr,
	ui_visible::yes,
	static_cast<admin_p2p_p::member_getter_t>(&admin_p2p),
	static_cast<admin_p2p_p::member_setter_t>(&set_admin_p2p),
	STP_ADMIN_P2P_AUTO,
};

const edge::bool_p port::oper_p2p_property {
	"operPointToPointMAC",
	&p2p_group,
	nullptr,
	ui_visible::yes,
	static_cast<bool_p::member_getter_t>(&oper_p2p),
	nullptr,
	std::nullopt,
};

const typed_object_collection_property<port, port_tree> port::trees_property {
	"PortTrees", nullptr, nullptr, ui_visible::no,
	&tree_count, &tree
};

const edge::property* const port::_properties[] =
{
	&side_property,
	&offset_property,
	&auto_edge_property,
	&admin_edge_property,
	&MacOperational,
	&DetectedPortPathCost,
	&AdminExternalPortPathCost,
	&ExternalPortPathCost,
	&detected_p2p_property,
	&admin_p2p_property,
	&oper_p2p_property,
	&trees_property,
};

const edge::xtype<port> port::_type = { "Port", &base::_type, _properties, nullptr };
