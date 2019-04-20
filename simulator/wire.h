#pragma once
#include "renderable_object.h"
#include "win32/win32_lib.h"
#include "win32/utility_functions.h"

class port;
struct project_i;

using connected_wire_end = port*;
using loose_wire_end = D2D1_POINT_2F;
using wire_end = std::variant<loose_wire_end, connected_wire_end>;

using edge::xtype;
using edge::com_ptr;
using edge::type;
using edge::property;
using edge::uint32_p;

class wire : public project_child
{
	using base = project_child;

	std::array<wire_end, 2> _points;

public:
	wire() = default;
	wire (wire_end firstEnd, wire_end secondEnd);

	const std::array<wire_end, 2>& points() const { return _points; }
	void set_point (size_t pointIndex, const wire_end& point);

	const wire_end& p0() const { return _points[0]; }
	void set_p0 (const wire_end& p0) { set_point(0, p0); }
	const wire_end& p1() const { return _points[1]; }
	void set_p1 (const wire_end& p1) { set_point(1, p1); }

	D2D1_POINT_2F point_coords (size_t pointIndex) const;

	void render (ID2D1RenderTarget* rt, const drawing_resources& dos, bool forwarding, bool hasLoop) const;

	virtual void render_selection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const override final;
	virtual HTResult hit_test (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

	static const xtype<wire> _type;
	virtual const struct type* type() const override { return &_type; }
};
