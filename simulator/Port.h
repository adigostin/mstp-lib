
#pragma once
#include "renderable_object.h"
#include "PortTree.h"
#include "stp.h"

struct PacketInfo
{
	std::vector<uint8_t> data;
	unsigned int timestamp;
	std::vector<std::array<uint8_t, 6>> txPortPath;
};

enum class side { left, top, right, bottom };

static constexpr edge::NVP SideNVPs[] =
{
	{ "Left",   (int) side::left },
	{ "Top",    (int) side::top },
	{ "Right",  (int) side::right },
	{ "Bottom", (int) side::bottom },
	{ 0, 0 },
};

class Port : public renderable_object
{
	using base = renderable_object;

	friend class Bridge;

	Bridge* const _bridge;
	unsigned int const _portIndex;
	side _side;
	float _offset;
	std::vector<std::unique_ptr<PortTree>> _trees;

	static constexpr unsigned int MissedLinkPulseCounterMax = 5;
	unsigned int _missedLinkPulseCounter = MissedLinkPulseCounterMax; // _missedLinkPulseCounter equal to MissedLinkPulseCounterMax means macOperational=false

public:
	Port (Bridge* bridge, unsigned int portIndex, side side, float offset);
	~Port();

	edge::com_ptr<IXMLDOMElement> Serialize (IXMLDOMDocument3* doc) const;
	HRESULT Deserialize (IXMLDOMElement* portElement);

	static constexpr int HTCodeInnerOuter = 1;
	static constexpr int HTCodeCP = 2;

	static constexpr float InteriorWidth = 30;
	static constexpr float InteriorDepth = 16;
	static constexpr float PortToPortSpacing = 16;
	static constexpr float ExteriorWidth = 10;
	static constexpr float ExteriorHeight = 20;
	static constexpr float OutlineWidth = 2;

	Bridge* bridge() const { return _bridge; }
	unsigned int GetPortIndex() const { return _portIndex; }
	side GetSide() const { return _side; }
	float GetOffset() const { return _offset; }
	D2D1_POINT_2F GetCPLocation() const;
	bool GetMacOperational() const;
	D2D1::Matrix3x2F GetPortTransform() const;
	D2D1_RECT_F GetInnerOuterRect() const;
	bool IsForwarding (unsigned int vlanNumber) const;
	void SetSideAndOffset (side side, float offset);
	const std::vector<std::unique_ptr<PortTree>>& trees() const { return _trees; }

	static void RenderExteriorNonStpPort (ID2D1RenderTarget* dc, const drawing_resources& dos, bool macOperational);
	static void RenderExteriorStpPort (ID2D1RenderTarget* dc, const drawing_resources& dos, STP_PORT_ROLE role, bool learning, bool forwarding, bool operEdge);

	void Render (ID2D1RenderTarget* dc, const drawing_resources& dos, unsigned int vlanNumber) const;

	virtual void RenderSelection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const override final;
	virtual HTResult HitTest (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

	bool HitTestInnerOuter (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) const;
	bool HitTestCP (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) const;

	bool auto_edge() const;
	void set_auto_edge (bool autoEdge);

	bool admin_edge() const;
	void set_admin_edge (bool adminEdge);

	unsigned int GetDetectedPortPathCost() const;
	unsigned int GetAdminExternalPortPathCost() const;
	void SetAdminExternalPortPathCost(unsigned int adminExternalPortPathCost);
	unsigned int GetExternalPortPathCost() const;

private:
	static const edge::bool_property auto_edge_property;
	static const edge::bool_property admin_edge_property;
	static const edge::bool_property MacOperational;
	static const edge::uint32_property DetectedPortPathCost;
	static const edge::uint32_property AdminExternalPortPathCost;
	static const edge::uint32_property ExternalPortPathCost;
	static const edge::property* const Port::_properties[];
	static const edge::type_t _type;

public:
	virtual const edge::type_t* type() const { return &_type; }
};
