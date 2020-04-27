
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "wire.h"
#include "bridge.h"
#include "port.h"
#include "win32/utility_functions.h"
#include "simulator.h"

static constexpr float thickness = 2;

void wire_end_property_traits::to_string (value_t from, std::string& to)
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
					to = ss.str();
					return;
				}
			}
		}

		assert(false);
	}
	else
	{
		auto location = std::get<loose_wire_end>(from);
		ss << "Loose;" << location.x << ";" << location.y;
		to = ss.str();
	}
}

void wire_end_property_traits::from_string (std::string_view from, value_t& to)
{
	size_t s1 = from.find(';');
	size_t s2 = from.rfind(';');

	if (std::string_view(from.data(), s1) == "Connected")
	{
		size_t bridge_index, port_index;
		std::from_chars (from.data() + s1 + 1, from.data() + s2, bridge_index);
		std::from_chars (from.data() + s2 + 1, from.data() + from.size(), port_index);
		to = serialized_connected_end(bridge_index, port_index);
		return;
	}

	if (std::string_view(from.data(), s1) == "Loose")
	{
		loose_wire_end end;
		std::from_chars (from.data() + s1 + 1, from.data() + s2, end.x);
		std::from_chars (from.data() + s2 + 1, from.data() + from.size(), end.y);
		to = end;
		return;
	}

	throw edge::string_convert_exception(from, type_name);
}

wire::wire (wire_end firstEnd, wire_end secondEnd)
	: _points({ firstEnd, secondEnd })
{ }

project_i* wire::project() const
{
	return static_cast<project_i*>(static_cast<wire_collection_i*>(base::parent()));
}

// deserialize_i
void wire::on_deserializing()
{ }

void wire::on_deserialized()
{
	for (auto& p : _points)
	{
		if (std::holds_alternative<serialized_connected_end>(p))
		{
			auto bridge_index = std::get<serialized_connected_end>(p).first;
			auto port_index = std::get<serialized_connected_end>(p).second;
			auto port = project()->bridges()[bridge_index]->ports()[port_index].get();
			p = port;
		}
	}
}

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

void wire::render_selection (const edge::zoomable_window_i* window, ID2D1RenderTarget* rt, const drawing_resources& dos) const
{
	auto fd = window->pointw_to_pointd(point_coords(0));
	auto td = window->pointw_to_pointd(point_coords(1));

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

renderable_object::ht_result wire::hit_test (const edge::zoomable_window_i* window, D2D1_POINT_2F dLocation, float tolerance)
{
	for (size_t i = 0; i < _points.size(); i++)
	{
		auto pointWLocation = this->point_coords(i);
		auto pointDLocation = window->pointw_to_pointd(pointWLocation);
		D2D1_RECT_F rect = { pointDLocation.x, pointDLocation.y, pointDLocation.x, pointDLocation.y };
		edge::inflate(&rect, tolerance);
		if (edge::point_in_rect (rect, dLocation))
			return { this, (int) i };
	}

	if (window->hit_test_line (dLocation, tolerance, point_coords(0), point_coords(1), thickness))
		return { this, -1 };

	return { };
}

D2D1_RECT_F wire::extent() const
{
	auto tl = point_coords(0);
	auto br = point_coords(0);
	for (size_t i = 1; i < _points.size(); i++)
	{
		auto c = point_coords(i);
		tl.x = std::min (tl.x, c.x);
		tl.y = std::min (tl.y, c.y);
		br.x = std::max (br.x, c.x);
		br.y = std::max (br.y, c.y);
	}

	return { tl.x, tl.y, br.x, br.y };
}

const prop_wrapper<wire_end_p, pg_hidden> wire::p0_property = { "P0", nullptr, nullptr, &p0, &set_p0, };

const prop_wrapper<wire_end_p, pg_hidden> wire::p1_property = { "P1", nullptr, nullptr, &p1, &set_p1 };

const property* const wire::_properties[] = { &p0_property, &p1_property };

const xtype<wire> wire::_type = { "Wire", &base::_type, _properties, std::make_unique<wire> };
