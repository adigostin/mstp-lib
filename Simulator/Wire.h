#pragma once
#include "Simulator.h"

using ConnectedWireEnd = Port*;
using LooseWireEnd = D2D1_POINT_2F;
using WireEnd = std::variant<LooseWireEnd, ConnectedWireEnd>;

class Wire;

class Wire : public Object
{
	std::array<WireEnd, 2> _points;
	std::string _debugName;

public:
	const std::array<WireEnd, 2>& GetPoints() const { return _points; }
	void SetPoint (size_t pointIndex, const WireEnd& point);

	const WireEnd& GetP0() const { return _points[0]; }
	void SetP0 (const WireEnd& p0) { SetPoint(0, p0); }
	const WireEnd& GetP1() const { return _points[1]; }
	void SetP1 (const WireEnd& p1) { SetPoint(1, p1); }

	void SetDebugName (const char* debugName) { _debugName = debugName; }

	D2D1_POINT_2F GetPointCoords (size_t pointIndex) const;
	D2D1_POINT_2F GetP0Coords() const { return GetPointCoords(0); }
	D2D1_POINT_2F GetP1Coords() const { return GetPointCoords(1); }

	IXMLDOMElementPtr Serialize (IXMLDOMDocument* doc) const;
	static std::unique_ptr<Wire> Deserialize (IXMLDOMElement* element);

	void Render (ID2D1RenderTarget* rt, const DrawingObjects& dos, unsigned int vlanNumber) const;

	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const override final;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

private:
	static IXMLDOMElementPtr SerializeEnd (IXMLDOMDocument* doc, const WireEnd& end);
	bool IsForwarding (unsigned int vlanNumber, _Out_opt_ bool* hasLoop) const;
};
