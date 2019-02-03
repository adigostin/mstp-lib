#pragma once
#include "renderable_object.h"
#include "win32/win32_lib.h"
#include "win32/utility_functions.h"

class Port;
struct project_i;

using connected_wire_end = Port*;
using loose_wire_end = D2D1_POINT_2F;
using wire_end = std::variant<loose_wire_end, connected_wire_end>;

class wire : public renderable_object
{
	std::array<wire_end, 2> _points;

public:
	wire() = default;
	wire (wire_end firstEnd, wire_end secondEnd);

	const std::array<wire_end, 2>& GetPoints() const { return _points; }
	void SetPoint (size_t pointIndex, const wire_end& point);

	const wire_end& GetP0() const { return _points[0]; }
	void SetP0 (const wire_end& p0) { SetPoint(0, p0); }
	const wire_end& GetP1() const { return _points[1]; }
	void SetP1 (const wire_end& p1) { SetPoint(1, p1); }

	D2D1_POINT_2F GetPointCoords (size_t pointIndex) const;

	edge::com_ptr<IXMLDOMElement> Serialize (project_i* project, IXMLDOMDocument* doc) const;
	static std::unique_ptr<wire> Deserialize (project_i* project, IXMLDOMElement* element);

	void Render (ID2D1RenderTarget* rt, const drawing_resources& dos, bool forwarding, bool hasLoop) const;

	virtual void RenderSelection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const override final;
	virtual HTResult HitTest (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

private:
	static edge::com_ptr<IXMLDOMElement> SerializeEnd (project_i* project, IXMLDOMDocument* doc, const wire_end& end);
	static wire_end DeserializeEnd (project_i* project, IXMLDOMElement* element);
};
