
#pragma once
#include "Simulator.h"
#include "mstp-lib/stp.h"

class Port : public Object
{
	friend class Bridge;

	Bridge* const _bridge;
	unsigned int const _portIndex;
	Side _side;
	float _offset;
	bool _macOperational = false;

public:
	Port (Bridge* bridge, unsigned int portIndex, Side side, float offset)
		: _bridge(bridge), _portIndex(portIndex), _side(side), _offset(offset)
	{ }

protected:
	virtual ~Port() = default;

public:
	static constexpr int HTCodeInner = 1;
	static constexpr int HTCodeCP = 2;

	static constexpr float InteriorWidth = 30;
	static constexpr float InteriorDepth = 16;
	static constexpr float PortToPortSpacing = 16;
	static constexpr float ExteriorWidth = 10;
	static constexpr float ExteriorHeight = 20;
	static constexpr float OutlineWidth = 2;

	Bridge* GetBridge() const { return _bridge; }
	unsigned int GetPortIndex() const { return _portIndex; }
	Side GetSide() const { return _side; }
	float GetOffset() const { return _offset; }
	D2D1_POINT_2F GetCPLocation() const;
	bool GetMacOperational() const { return _macOperational; }
	D2D1::Matrix3x2F GetPortTransform() const;
	D2D1_RECT_F GetInnerRect() const;

	static void RenderExteriorNonStpPort (ID2D1RenderTarget* dc, const DrawingObjects& dos, bool macOperational);
	static void RenderExteriorStpPort (ID2D1RenderTarget* dc, const DrawingObjects& dos, STP_PORT_ROLE role, bool learning, bool forwarding, bool operEdge);

	virtual void Render (ID2D1RenderTarget* dc, const DrawingObjects& dos, uint16_t vlanNumber) const override final;
	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const override final;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

	bool HitTestInner (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) const;
	bool HitTestCP (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) const;
};
