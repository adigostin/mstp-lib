#pragma once
#include "renderable_object.h"
#include "win32/win32_lib.h"
#include "win32/utility_functions.h"

class Port;
struct project_i;

using ConnectedWireEnd = Port*;
using LooseWireEnd = D2D1_POINT_2F;
using WireEnd = std::variant<LooseWireEnd, ConnectedWireEnd>;

class Wire : public renderable_object
{
	std::array<WireEnd, 2> _points;

public:
	static constexpr float Thickness = 2;

	Wire() = default;
	Wire (WireEnd firstEnd, WireEnd secondEnd);

	const std::array<WireEnd, 2>& GetPoints() const { return _points; }
	void SetPoint (size_t pointIndex, const WireEnd& point);

	const WireEnd& GetP0() const { return _points[0]; }
	void SetP0 (const WireEnd& p0) { SetPoint(0, p0); }
	const WireEnd& GetP1() const { return _points[1]; }
	void SetP1 (const WireEnd& p1) { SetPoint(1, p1); }

	D2D1_POINT_2F GetPointCoords (size_t pointIndex) const;

	edge::com_ptr<IXMLDOMElement> Serialize (project_i* project, IXMLDOMDocument* doc) const;
	static std::unique_ptr<Wire> Deserialize (project_i* project, IXMLDOMElement* element);

	void Render (ID2D1RenderTarget* rt, const drawing_resources& dos, bool forwarding, bool hasLoop) const;

	virtual void RenderSelection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const override final;
	virtual HTResult HitTest (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

private:
	static edge::com_ptr<IXMLDOMElement> SerializeEnd (project_i* project, IXMLDOMDocument* doc, const WireEnd& end);
	static WireEnd DeserializeEnd (project_i* project, IXMLDOMElement* element);
};
