
#pragma once
#include "Reflection/Object.h"
#include "Win32/Win32Defs.h"
#include "stp.h"

struct PacketInfo
{
	std::vector<uint8_t> data;
	unsigned int timestamp;
	std::vector<std::array<uint8_t, 6>> txPortPath;
};

enum class Side { Left, Top, Right, Bottom };

class Port : public RenderableObject
{
	friend class Bridge;

	Bridge* const _bridge;
	size_t const _portIndex;
	Side _side;
	float _offset;

	static constexpr unsigned int MissedLinkPulseCounterMax = 5;
	unsigned int _missedLinkPulseCounter = MissedLinkPulseCounterMax; // _missedLinkPulseCounter equal to MissedLinkPulseCounterMax means macOperational=false

public:
	Port (Bridge* bridge, unsigned int portIndex, Side side, float offset);

	virtual const PropertyOrGroup* const* GetProperties() const override final;

	static constexpr int HTCodeInnerOuter = 1;
	static constexpr int HTCodeCP = 2;

	static constexpr float InteriorWidth = 30;
	static constexpr float InteriorDepth = 16;
	static constexpr float PortToPortSpacing = 16;
	static constexpr float ExteriorWidth = 10;
	static constexpr float ExteriorHeight = 20;
	static constexpr float OutlineWidth = 2;

	Bridge* GetBridge() const { return _bridge; }
	size_t GetPortIndex() const { return _portIndex; }
	Side GetSide() const { return _side; }
	float GetOffset() const { return _offset; }
	D2D1_POINT_2F GetCPLocation() const;
	bool GetMacOperational() const;
	D2D1::Matrix3x2F GetPortTransform() const;
	D2D1_RECT_F GetInnerOuterRect() const;
	bool IsForwarding (unsigned int vlanNumber) const;
	void SetSideAndOffset (Side side, float offset);

	static void RenderExteriorNonStpPort (ID2D1RenderTarget* dc, const DrawingObjects& dos, bool macOperational);
	static void RenderExteriorStpPort (ID2D1RenderTarget* dc, const DrawingObjects& dos, STP_PORT_ROLE role, bool learning, bool forwarding, bool operEdge);

	void Render (ID2D1RenderTarget* dc, const DrawingObjects& dos, unsigned int vlanNumber) const;

	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const override final;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

	bool HitTestInnerOuter (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) const;
	bool HitTestCP (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) const;

	bool GetAutoEdge() const;
	bool GetAdminEdge() const;
};
