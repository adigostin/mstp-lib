
#include "pch.h"
#include "wire.h"
#include "bridge.h"
#include "port.h"
#include "win32/utility_functions.h"
#include "simulator.h"

static constexpr float thickness = 2;

std::string wire_end_property_traits::to_string (wire_end from)
{
	std::stringstream ss;
	if (std::holds_alternative<connected_wire_end>(from))
	{
		port* p = std::get<connected_wire_end>(from);
		for (size_t bi = 0; bi < p->bridge()->project()->bridges().size(); bi++)
		{
			auto b = p->bridge()->project()->bridges()[bi].get();
			for (size_t pi = 0; pi < b->ports().size(); pi++)
			{
				if (b->ports()[pi].get() == p)
				{
					ss << "Connected;" << bi << ";" << pi;
					return ss.str();
				}
			}
		}

		assert(false); return { };
	}
	else
	{
		auto location = std::get<loose_wire_end>(from);
		ss << "Loose;" << location.x << ";" << location.y;
		return ss.str();
	}
}

bool wire_end_property_traits::from_string (std::string_view from, wire_end& to, const object* obj)
{
	size_t s1 = from.find(';');
	size_t s2 = from.rfind(';');

	if (std::string_view(from.data(), s1) == "Connected")
	{
		size_t bridge_index, port_index;
		std::from_chars (from.data() + s1 + 1, from.data() + s2, bridge_index);
		std::from_chars (from.data() + s2 + 1, from.data() + from.size(), port_index);
		auto w = static_cast<const wire*>(obj);
		auto port = w->project()->bridges()[bridge_index]->ports()[port_index].get();
		to = port;
		return true;
	}

	if (std::string_view(from.data(), s1) == "Loose")
	{
		loose_wire_end end;
		std::from_chars (from.data() + s1 + 1, from.data() + s2, end.x);
		std::from_chars (from.data() + s2 + 1, from.data() + from.size(), end.y);
		to = end;
		return true;
	}

	return false;
}

wire::wire (wire_end firstEnd, wire_end secondEnd)
	: _points({ firstEnd, secondEnd })
{ }

void wire::set_point (size_t pointIndex, wire_end point)
{
	if (_points[pointIndex] != point)
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

renderable_object::ht_result wire::hit_test (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance)
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

const wire_end_p wire::p0_property = {
	"P0", nullptr, nullptr, ui_visible::no,
	static_cast<wire_end_p::member_getter_t>(&p0),
	static_cast<wire_end_p::member_setter_t>(&set_p0),
	std::nullopt
};

const wire_end_p wire::p1_property = {
	"P1", nullptr, nullptr, ui_visible::no,
	static_cast<wire_end_p::member_getter_t>(&p1),
	static_cast<wire_end_p::member_setter_t>(&set_p1),
	std::nullopt
};

const property* wire::_properties[] = { &p0_property, &p1_property };

const xtype<wire> wire::_type = { "Wire", &base::_type, _properties, [] { return new wire(); } };
