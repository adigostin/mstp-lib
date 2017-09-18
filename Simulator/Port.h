
#pragma once
#include "Object.h"
#include "PortTree.h"
#include "Win32/Win32Defs.h"
#include "stp.h"

struct PacketInfo
{
	std::vector<uint8_t> data;
	unsigned int timestamp;
	std::vector<std::array<uint8_t, 6>> txPortPath;
};

enum class Side { Left, Top, Right, Bottom };

static constexpr NVP SideNVPs[] =
{
	{ L"Left", (int) Side::Left },
	{ L"Top", (int) Side::Top },
	{ L"Right", (int) Side::Right },
	{ L"Bottom", (int) Side::Bottom },
	{ 0, 0 },
};

class Port : public RenderableObject
{
	friend class Bridge;

	Bridge* const _bridge;
	unsigned int const _portIndex;
	Side _side;
	float _offset;
	std::vector<std::unique_ptr<PortTree>> _trees;

	static constexpr unsigned int MissedLinkPulseCounterMax = 5;
	unsigned int _missedLinkPulseCounter = MissedLinkPulseCounterMax; // _missedLinkPulseCounter equal to MissedLinkPulseCounterMax means macOperational=false
	static void OnTreePropertyChanged (void* callbackArg, Object* o, const Property* property);

public:
	Port (Bridge* bridge, unsigned int portIndex, Side side, float offset);
	~Port();

	IXMLDOMElementPtr Serialize (IXMLDOMDocument3* doc) const;
	HRESULT Deserialize (IXMLDOMElement* portElement);

	static constexpr int HTCodeInnerOuter = 1;
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
	bool GetMacOperational() const;
	D2D1::Matrix3x2F GetPortTransform() const;
	D2D1_RECT_F GetInnerOuterRect() const;
	bool IsForwarding (unsigned int vlanNumber) const;
	void SetSideAndOffset (Side side, float offset);
	const std::vector<std::unique_ptr<PortTree>>& GetTrees() const { return _trees; }

	static void RenderExteriorNonStpPort (ID2D1RenderTarget* dc, const DrawingObjects& dos, bool macOperational);
	static void RenderExteriorStpPort (ID2D1RenderTarget* dc, const DrawingObjects& dos, STP_PORT_ROLE role, bool learning, bool forwarding, bool operEdge);

	void Render (ID2D1RenderTarget* dc, const DrawingObjects& dos, unsigned int vlanNumber) const;

	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const override final;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

	bool HitTestInnerOuter (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) const;
	bool HitTestCP (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) const;

	bool GetAutoEdge() const;
	void SetAutoEdge (bool autoEdge);

	bool GetAdminEdge() const;
	void SetAdminEdge (bool adminEdge);

	unsigned int GetDetectedPortPathCost() const;
	unsigned int GetAdminExternalPortPathCost() const;
	void SetAdminExternalPortPathCost(unsigned int adminExternalPortPathCost);
	unsigned int GetExternalPortPathCost() const;

	static const TypedProperty<bool> AutoEdge;
	static const TypedProperty<bool> AdminEdge;
	static const TypedProperty<bool> MacOperational;
	static const TypedProperty<unsigned int> DetectedPortPathCost;
	static const TypedProperty<unsigned int> AdminExternalPortPathCost;
	static const TypedProperty<unsigned int> ExternalPortPathCost;
	static const PropertyOrGroup* const Properties[];
	virtual const PropertyOrGroup* const* GetProperties() const override final { return Properties; }
};
