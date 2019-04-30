
#pragma once
#include "renderable_object.h"
#include "port_tree.h"
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

class port : public renderable_object
{
	using base = renderable_object;

	friend class bridge;

	bridge* const _bridge;
	unsigned int const _portIndex;
	side _side = side_property._default_value.value();
	float _offset;
	std::vector<std::unique_ptr<port_tree>> _trees;

	static constexpr unsigned int MissedLinkPulseCounterMax = 5;
	unsigned int _missedLinkPulseCounter = MissedLinkPulseCounterMax; // _missedLinkPulseCounter equal to MissedLinkPulseCounterMax means macOperational=false

	static void on_bridge_property_changing (void* arg, object* obj, const property_change_args& args);
	static void on_bridge_property_changed (void* arg, object* obj, const property_change_args& args);

public:
	port (class bridge* bridge, unsigned int portIndex, side side, float offset);

	static constexpr int HTCodeInnerOuter = 1;
	static constexpr int HTCodeCP = 2;

	static constexpr float InteriorWidth = 30;
	static constexpr float InteriorDepth = 16;
	static constexpr float PortToPortSpacing = 16;
	static constexpr float ExteriorWidth = 10;
	static constexpr float ExteriorHeight = 20;
	static constexpr float OutlineWidth = 2;

	const bridge* bridge() const { return _bridge; }
	class bridge* bridge() { return _bridge; }
	unsigned int port_index() const { return _portIndex; }
	enum side side() const { return _side; }
	float offset() const { return _offset; }
	D2D1_POINT_2F GetCPLocation() const;
	bool GetMacOperational() const;
	D2D1::Matrix3x2F GetPortTransform() const;
	D2D1_RECT_F GetInnerOuterRect() const;
	bool IsForwarding (unsigned int vlanNumber) const;
	void SetSideAndOffset (enum side side, float offset);
	const std::vector<std::unique_ptr<port_tree>>& trees() const { return _trees; }

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
	size_t tree_count() const { return _trees.size(); }
	port_tree* tree (size_t index) const { return _trees[index].get(); }

	static const side_p side_property;
	static const float_p offset_property;
	static const bool_p auto_edge_property;
	static const bool_p admin_edge_property;
	static const bool_p MacOperational;
	static const uint32_p AdminExternalPortPathCost;
	static const uint32_p DetectedPortPathCost;
	static const uint32_p ExternalPortPathCost;
	static const admin_p2p_p admin_p2p_property;
	static const bool_p detected_p2p_property;
	static const bool_p oper_p2p_property;
	static const typed_object_collection_property<port, port_tree> trees_property;

	static const property* const port::_properties[];
	static const xtype<port> _type;
	virtual const struct type* type() const { return &_type; }
};
