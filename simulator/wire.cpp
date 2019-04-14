
#include "pch.h"
#include "wire.h"
#include "Bridge.h"
#include "Port.h"
#include "win32/utility_functions.h"
#include "simulator.h"

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

void wire::set_point (size_t pointIndex, const wire_end& point)
{
	if (!Same(_points[pointIndex], point))
	{
		_points[pointIndex] = point;
		event_invoker<invalidate_e>()(this);
	}
}

D2D1_POINT_2F wire::point_coords (size_t pointIndex) const
{
	if (std::holds_alternative<loose_wire_end>(_points[pointIndex]))
		return std::get<loose_wire_end>(_points[pointIndex]);
	else
		return std::get<connected_wire_end>(_points[pointIndex])->GetCPLocation();
}

void wire::render (ID2D1RenderTarget* rt, const drawing_resources& dos, bool forwarding, bool hasLoop) const
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
	rt->DrawLine (point_coords(0), point_coords(1), brush, width, ss);
}

void wire::render_selection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const
{
	auto fd = zoomable->pointw_to_pointd(point_coords(0));
	auto td = zoomable->pointw_to_pointd(point_coords(1));

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

renderable_object::HTResult wire::hit_test (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance)
{
	for (size_t i = 0; i < _points.size(); i++)
	{
		auto pointWLocation = this->point_coords(i);
		auto pointDLocation = zoomable->pointw_to_pointd(pointWLocation);
		D2D1_RECT_F rect = { pointDLocation.x, pointDLocation.y, pointDLocation.x, pointDLocation.y };
		edge::InflateRect(&rect, tolerance);
		if (edge::point_in_rect (rect, dLocation))
			return { this, (int) i };
	}

	if (HitTestLine (zoomable, dLocation, tolerance, point_coords(0), point_coords(1), thickness))
		return { this, -1 };

	return { };
}

const xtype<wire> wire::_type = { "Wire", &base::_type, { }, [] { return new wire(); } };

struct connected_end_wrapper : public object
{
	uint32_t bridge_index;
	uint32_t port_index;

	static const uint32_p bridge_index_property;
	static const uint32_p port_index_property;
	static inline const property* const props[] = { &bridge_index_property, &port_index_property };
	static const xtype<connected_end_wrapper> _type;
	virtual const struct type* type() const override { return &_type; }
};

const uint32_p connected_end_wrapper::bridge_index_property = {
	"BridgeIndex", nullptr, nullptr, edge::ui_visible::no,
	static_cast<uint32_p::member_var_t>(&connected_end_wrapper::bridge_index), std::nullopt
};

const uint32_p connected_end_wrapper::port_index_property = {
	"PortIndex", nullptr, nullptr, edge::ui_visible::no,
	static_cast<uint32_p::member_var_t>(&connected_end_wrapper::port_index), std::nullopt
};

const xtype<connected_end_wrapper> connected_end_wrapper::_type = {
	"ConnectedEnd", &object::_type, props, [] { return new connected_end_wrapper(); }
};

