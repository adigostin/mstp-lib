
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "renderable_object.h"
#include "utility_functions.h"
#include "xml_serializer.h"

class port;
struct project_i;

using loose_wire_end = D2D1_POINT_2F;
using connected_wire_end = port*;
using wire_end = std::variant<loose_wire_end, connected_wire_end>;

using edge::xtype;
using edge::com_ptr;
using edge::type;
using edge::static_value_property;
using edge::uint32_p;
using edge::object;

struct wire_end_p : edge::property, edge::custom_serialize_property_i
{
	size_t const _index;
	_bstr_t const _name_bstr;

	wire_end_p (const char* name, size_t index);
	// custom_serialize_property_i
	virtual void serialize (edge::xml_serializer_i* serializer, const object* obj, const edge::serialize_element_getter& element_getter) const override;
	virtual void deserialize (edge::xml_deserializer_i* deserializer, IXMLDOMElement* element, object* obj) const override;
	virtual void deserialize (edge::xml_deserializer_i* deserializer, std::string_view attr_value, object* obj) const override;
};

class wire : public renderable_object
{
	using base = renderable_object;

	std::array<wire_end, 2> _points;

public:
	wire() = default;
	wire (wire_end firstEnd, wire_end secondEnd);

	project_i* project() const;

	const std::array<wire_end, 2>& points() const { return _points; }
	wire_end point (size_t i) const { return _points[i]; }
	void set_point (size_t i, wire_end point);

	wire_end p0() const { return _points[0]; }
	void set_p0 (wire_end p0) { set_point(0, p0); }
	void set_p0 (port* p0) { set_point(0, p0); }
	wire_end p1() const { return _points[1]; }
	void set_p1 (wire_end p1) { set_point(1, p1); }
	void set_p1 (port* p1) { set_point(1, p1); }

	D2D1_POINT_2F point_coords (size_t pointIndex) const;

	void render (ID2D1RenderTarget* rt, const drawing_resources& dos, bool forwarding, bool hasLoop) const;

	virtual void render_selection (const edge::zoomable_window_i* window, ID2D1RenderTarget* rt, const drawing_resources& dos) const override final;
	virtual ht_result hit_test (const edge::zoomable_window_i* window, D2D1_POINT_2F dLocation, float tolerance) override final;
	virtual D2D1_RECT_F extent() const override;

	static const wire_end_p p0_property;
	static const wire_end_p p1_property;
	static const property* const _properties[];
	static const xtype<wire> _type;
	virtual const edge::concrete_type* type() const override { return &_type; }
};
