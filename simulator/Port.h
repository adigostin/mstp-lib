
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
extern const char side_type_name[];
extern const edge::NVP side_nvps[];
using side_p = edge::enum_property<enum side, side_type_name, side_nvps>;

extern const char admin_p2p_type_name[];
extern const edge::NVP admin_p2p_nvps[];
using admin_p2p_p = edge::enum_property<STP_ADMIN_P2P, admin_p2p_type_name, admin_p2p_nvps>;

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

	static constexpr int HTCodeInnerOuter = 1;
	static constexpr int HTCodeCP = 2;

	static constexpr float InteriorWidth = 30;
	static constexpr float InteriorDepth = 16;
	static constexpr float PortToPortSpacing = 16;
	static constexpr float ExteriorWidth = 10;
	static constexpr float ExteriorHeight = 20;
	static constexpr float OutlineWidth = 2;

	const Bridge* bridge() const { return _bridge; }
	Bridge* bridge() { return _bridge; }
	unsigned int port_index() const { return _portIndex; }
	enum side side() const { return _side; }
	float offset() const { return _offset; }
	D2D1_POINT_2F GetCPLocation() const;
	bool GetMacOperational() const;
	D2D1::Matrix3x2F GetPortTransform() const;
	D2D1_RECT_F GetInnerOuterRect() const;
	bool IsForwarding (unsigned int vlanNumber) const;
	void SetSideAndOffset (enum side side, float offset);
	const std::vector<std::unique_ptr<PortTree>>& trees() const { return _trees; }

	static void RenderExteriorNonStpPort (ID2D1RenderTarget* dc, const drawing_resources& dos, bool macOperational);
	static void RenderExteriorStpPort (ID2D1RenderTarget* dc, const drawing_resources& dos, STP_PORT_ROLE role, bool learning, bool forwarding, bool operEdge);

	void Render (ID2D1RenderTarget* dc, const drawing_resources& dos, unsigned int vlanNumber) const;

	virtual void render_selection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const override final;
	virtual HTResult hit_test (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance) override final;

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

	bool detected_p2p() const;
	STP_ADMIN_P2P admin_p2p() const;
	void set_admin_p2p (STP_ADMIN_P2P admin_p2p);
	bool oper_p2p() const;

private:
	void set_side (enum side side) { _side = side; }
	void set_offset (float offset) { _offset = offset; }

	static const side_p side_property;
	static const edge::float_p offset_property;
	static const edge::bool_p auto_edge_property;
	static const edge::bool_p admin_edge_property;
	static const edge::bool_p MacOperational;
	static const edge::uint32_p AdminExternalPortPathCost;
	static const edge::uint32_p DetectedPortPathCost;
	static const edge::uint32_p ExternalPortPathCost;
	static const admin_p2p_p admin_p2p_property;
	static const edge::bool_p detected_p2p_property;
	static const edge::bool_p oper_p2p_property;
	static const edge::property* const Port::_properties[];
	static const xtype<Port> _type;

public:
	virtual const struct type* type() const { return &_type; }
};
