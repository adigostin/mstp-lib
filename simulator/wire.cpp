
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"

static constexpr float thickness = 2;

// ============================================================================
// wire_end_p

wire_end_p::wire_end_p (const char* name, size_t index)
	: property(name, nullptr, nullptr, false)
	, _index(index)
	, _name_bstr(name)
{ }

void wire_end_p::serialize (edge::xml_serializer_i* serializer, const object* obj, const edge::serialize_element_getter& element_getter) const
{
	auto w = static_cast<const wire*>(obj);
	auto& from = w->point(_index);

	std::stringstream ss;
	if (std::holds_alternative<connected_wire_end>(from))
	{
		//auto p = std::get<connected_wire_end>(we);
		//auto b = p->bridge();
		//auto pi = static_cast<const edge::typed_object_collection_i<port>*>(p->bridge())->index_of(p);
		//auto bi = static_cast<const edge::typed_object_collection_i<bridge>*>(b->project())->index_of(b);
		//auto value = std::string("Connected;") + std::to_string(bi) + ";" + std::to_string(pi);
		//element_getter()->setAttribute(_name_bstr, _variant_t(value.c_str()));
		port* p = std::get<connected_wire_end>(from);
		for (size_t bi = 0; bi < p->bridge()->project()->bridges().size(); bi++)
		{
			auto b = p->bridge()->project()->bridges()[bi].get();
			for (size_t pi = 0; pi < b->ports().size(); pi++)
			{
				if (b->ports()[pi].get() == p)
				{
					ss << "Connected;" << bi << ";" << pi;
					element_getter()->setAttribute(_name_bstr, _variant_t(ss.str().c_str()));
					return;
				}
			}
		}

		rassert(false);
	}
	else
	{
		//auto p = std::get<loose_wire_end>(we);
		//auto value = std::string("Loose;") + std::to_string(p.x) + ";" + std::to_string(p.y);
		//element_getter()->setAttribute(_name_bstr, _variant_t(value.c_str()));
		auto location = std::get<loose_wire_end>(from);
		ss << "Loose;" << location.x << ";" << location.y;
		element_getter()->setAttribute(_name_bstr, _variant_t(ss.str().c_str()));
		return;
	}
}

void wire_end_p::deserialize (edge::xml_deserializer_i* deserializer, IXMLDOMElement* element, object* obj) const
{
	rassert(false);
}

void wire_end_p::deserialize (edge::xml_deserializer_i* deserializer, std::string_view attr_value, object* obj) const
{
	auto w = static_cast<wire*>(obj);
	auto project = static_cast<project_i*>(deserializer->context());
	auto from = attr_value;
	size_t s1 = from.find(';');
	size_t s2 = from.rfind(';');

	if (std::string_view(from.data(), s1) == "Connected")
	{
		size_t bridge_index, port_index;
		std::from_chars (from.data() + s1 + 1, from.data() + s2, bridge_index);
		std::from_chars (from.data() + s2 + 1, from.data() + from.size(), port_index);
		auto port = project->bridges()[bridge_index]->ports()[port_index].get();
		w->set_point(_index, port);
		return;
	}

	if (std::string_view(from.data(), s1) == "Loose")
	{
		loose_wire_end end;
		std::from_chars (from.data() + s1 + 1, from.data() + s2, end.x);
		std::from_chars (from.data() + s2 + 1, from.data() + from.size(), end.y);
		w->set_point(_index, end);
		return;
	}

	rassert(false);
}

// ============================================================================

wire::wire (wire_end firstEnd, wire_end secondEnd)
	: _points({ firstEnd, secondEnd })
{ }

project_i* wire::project() const
{
	auto bc = static_cast<edge::typed_object_collection_i<wire>*>(base::parent());
	return static_cast<project_i*>(bc);
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

const wire_end_p wire::p0_property("P0", 0);
const wire_end_p wire::p1_property("P1", 1);
const property* const wire::_properties[] = { &p0_property, &p1_property };

const xtype<> wire::_type = { "Wire", &base::_type, _properties, [] { return std::unique_ptr<object>(new wire()); } };
