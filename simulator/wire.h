#pragma once
#include "renderable_object.h"
#include "win32/win32_lib.h"
#include "win32/utility_functions.h"

class port;
struct project_i;

struct connected_wire_end
{
	size_t bridge_index;
	size_t port_index;
	bool operator== (const connected_wire_end& other) const { return (bridge_index == other.bridge_index) && (port_index == other.port_index); }
	bool operator!= (const connected_wire_end& other) const { return (bridge_index != other.bridge_index) || (port_index != other.port_index); }
};
using loose_wire_end = D2D1_POINT_2F;
using wire_end = std::variant<loose_wire_end, connected_wire_end>;

using edge::xtype;
using edge::com_ptr;
using edge::type;
using edge::property;
using edge::typed_property;
using edge::uint32_p;
using edge::ui_visible;

struct wire_end_property_traits
{
	static constexpr char type_name[] = "wire_end";
	using value_t = wire_end;
	using param_t = wire_end;
	using return_t = wire_end;
	static std::string to_string (wire_end from);
	static bool from_string (std::string_view from, wire_end& to);
};
using wire_end_p = typed_property<wire_end_property_traits>;

class wire : public project_child
{
	using base = project_child;

	std::array<wire_end, 2> _points;

public:
	wire() = default;
	wire (wire_end firstEnd, wire_end secondEnd);

	const std::array<wire_end, 2>& points() const { return _points; }
	wire_end point (size_t i) const { return _points[i]; }
	void set_point (size_t i, wire_end point);
	void set_point (size_t i, port* point);

	wire_end p0() const { return _points[0]; }
	void set_p0 (wire_end p0) { set_point(0, p0); }
	void set_p0 (port* p0) { set_point(0, p0); }
	wire_end p1() const { return _points[1]; }
	void set_p1 (wire_end p1) { set_point(1, p1); }
	void set_p1 (port* p1) { set_point(1, p1); }

	D2D1_POINT_2F point_coords (size_t pointIndex) const;

	void render (ID2D1RenderTarget* rt, const drawing_resources& dos, bool forwarding, bool hasLoop) const;

	virtual void render_selection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const override final;
	virtual HTResult hit_test (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

	static const wire_end_p p0_property;
	static const wire_end_p p1_property;
	static const property* _properties[];
	static const xtype<wire> _type;
	virtual const struct type* type() const override { return &_type; }
};
