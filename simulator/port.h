
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "renderable_object.h"
#include "port_tree.h"
#include "stp.h"

using mac_address = std::array<uint8_t, 6>;

struct frame_t
{
	uint32_t timestamp;
	std::vector<uint8_t> data;;
	std::vector<mac_address> tx_path_taken;
};

struct link_pulse_t
{
	uint32_t timestamp;
	uint32_t sender_supported_speed;
};

using packet_t = std::variant<link_pulse_t, frame_t>;

extern const char admin_p2p_type_name[];
extern const nvp admin_p2p_nvps[];
using admin_p2p_p = edge::enum_property<STP_ADMIN_P2P, admin_p2p_type_name, admin_p2p_nvps>;

extern const char port_speed_type_name[];
extern const char port_speed_unknown_str[];
extern const nvp port_speed_nvps[];
using port_speed_p = edge::enum_property<uint32_t, port_speed_type_name, port_speed_nvps, false, port_speed_unknown_str>;

class port : public renderable_object, public typed_object_collection_i<port_tree>
{
	using base = renderable_object;

	friend class bridge;

	size_t  const _port_index;
	side _side = side_property.default_value.value();
	float _offset;
	uint32_t _supported_speed = supported_speed_property.default_value.value();
	uint32_t _actual_speed = 0;
	std::vector<std::unique_ptr<port_tree>> _trees;

	static constexpr uint32_t MissedLinkPulseCounterMax = 3;
	uint32_t _missedLinkPulseCounter = MissedLinkPulseCounterMax;

	virtual std::vector<std::unique_ptr<port_tree>>& children_store() override final { return _trees; }
	virtual const typed_object_collection_property<port_tree>* collection_property() const override final { return &trees_property; }
	virtual void call_property_changing (const property_change_args& args) override final { this->on_property_changing(args); }
	virtual void call_property_changed  (const property_change_args& args) override final { this->on_property_changed(args); }

	virtual void on_inserted_into_parent() override;
	virtual void on_removing_from_parent() override;

	void on_bridge_property_changing (object* obj, const property_change_args& args);
	void on_bridge_property_changed  (object* obj, const property_change_args& args);

public:
	port (size_t port_index, side side, float offset);

	static constexpr int HTCodeInnerOuter = 1;
	static constexpr int HTCodeCP = 2;

	static constexpr float InteriorWidth = 30;
	static constexpr float InteriorDepth = 16;
	static constexpr float PortToPortSpacing = 16;
	static constexpr float ExteriorWidth = 10;
	static constexpr float ExteriorHeight = 20;
	static constexpr float OutlineWidth = 2;

	::bridge* bridge() const;
	size_t port_index() const { return _port_index; }
	side side() const { return _side; }
	float offset() const { return _offset; }
	D2D1_POINT_2F GetCPLocation() const;
	bool mac_operational() const;
	D2D1::Matrix3x2F GetPortTransform() const;
	D2D1_RECT_F GetInnerOuterRect() const;
	bool IsForwarding (unsigned int vlanNumber) const;
	void SetSideAndOffset (edge::side side, float offset);
	const std::vector<std::unique_ptr<port_tree>>& trees() const { return _trees; }

	struct stp_enabled_changing_e : event<stp_enabled_changing_e, const property_change_args&> { };
	struct stp_enabled_changed_e  : event<stp_enabled_changed_e,  const property_change_args&> { };

	stp_enabled_changing_e::subscriber stp_enabled_changing() { return stp_enabled_changing_e::subscriber(this); }
	stp_enabled_changed_e ::subscriber stp_enabled_changed () { return stp_enabled_changed_e ::subscriber(this); }

	static void RenderExteriorNonStpPort (ID2D1RenderTarget* dc, const drawing_resources& dos, bool macOperational);
	static void RenderExteriorStpPort (ID2D1RenderTarget* dc, const drawing_resources& dos, STP_PORT_ROLE role, bool learning, bool forwarding, bool operEdge);

	void render (ID2D1RenderTarget* dc, const drawing_resources& dos, unsigned int vlanNumber) const;

	virtual void render_selection (const edge::zoomable_window_i* window, ID2D1RenderTarget* rt, const drawing_resources& dos) const override final;
	virtual ht_result hit_test (const edge::zoomable_window_i* window, D2D1_POINT_2F dLocation, float tolerance) override final;
	virtual D2D1_RECT_F extent() const override { assert(false); return { }; }

	void invalidate();

	bool HitTestInnerOuter (const edge::zoomable_window_i* window, D2D1_POINT_2F dLocation, float tolerance) const;
	bool HitTestCP (const edge::zoomable_window_i* window, D2D1_POINT_2F dLocation, float tolerance) const;

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

	uint32_t supported_speed() const { return _supported_speed; }
	void set_supported_speed (uint32_t value);

	uint32_t actual_speed() const { return _actual_speed; }

	void set_actual_speed (uint32_t value);
	void set_side (edge::side side) { _side = side; }
	void set_offset (float offset) { _offset = offset; }

	static const prop_wrapper<side_p, pg_hidden> side_property;
	static const prop_wrapper<float_p, pg_hidden> offset_property;
	static const port_speed_p supported_speed_property;
	static const port_speed_p actual_speed_property;
	static const bool_p auto_edge_property;
	static const bool_p admin_edge_property;
	static const bool_p mac_operational_property;
	static const uint32_p admin_external_port_path_cost_property;
	static const uint32_p detected_port_path_cost_property;
	static const uint32_p external_port_path_cost_property;
	static const admin_p2p_p admin_p2p_property;
	static const bool_p detected_p2p_property;
	static const bool_p oper_p2p_property;
	static const prop_wrapper<typed_object_collection_property<port_tree>, pg_hidden> trees_property;

	static const property* const _properties[];
	static const xtype<> _type;
	virtual const concrete_type* type() const { return &_type; }
};
