
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "edge/om/object.h"
#include "edge/om/value_property.h"
#include "edge/com_ptr.h"
#include "pg/item.h"
#include "simulator_props.h"
#include "bridge.h"

extern const edge::nvp bridge_priority_nvps[];
extern const char bridge_priority_type_name[];
using bridge_priority_traits = edge::enum_property_traits<uint32_t, bridge_priority_type_name, bridge_priority_nvps, true>;
using bridge_priority_p = static_ui_prop<bridge_priority_traits>;

class bridge_tree : public edge::object
{
	friend class bridge;

	edge::event_manager _em;
	bridge* const _parent;
	size_t const _tree_index;
	SYSTEMTIME _last_topology_change;
	uint32_t _topology_change_count;

	void on_topology_change (unsigned int timestamp);
	void on_bridge_property_changing (edge::object* obj, const property_change_args& args);
	void on_bridge_property_changed (edge::object* obj, const property_change_args& args);

public:
	bridge_tree (bridge* parent, size_t tree_index);
	~bridge_tree();

	virtual ::bridge* parent() const override { return _parent; }

	uint32_t bridge_priority() const;
	void set_bridge_priority (uint32_t priority);

	std::array<unsigned char, 36> root_priorty_vector() const;
	std::string root_bridge_id() const;
	uint32_t    external_root_path_cost() const;
	std::string regional_root_id() const;
	uint32_t    internal_root_path_cost() const;
	std::string designated_bridge_id() const;
	std::string designated_port_id() const;
	std::string receiving_port_id() const;

	uint32_t hello_time() const;
	uint32_t max_age() const;
	uint32_t bridge_forward_delay() const;
	uint32_t message_age() const;
	uint32_t remaining_hops() const;

	uint32_t topology_change_count() const { return _topology_change_count; }

	static const value_property* const properties_changed_on_stp_enable_disable[];

	static const bridge_priority_p bridge_priority_property;
	static const string_p      root_id_property;
	static const uint32_p      external_root_path_cost_property;
	static const string_p      regional_root_id_property;
	static const uint32_p      internal_root_path_cost_property;
	static const string_p      designated_bridge_id_property;
	static const string_p      designated_port_id_property;
	static const string_p      receiving_port_id_property;
	static const uint32_p      hello_time_property;
	static const uint32_p      max_age_property;
	static const uint32_p      forward_delay_property;
	static const uint32_p      message_age_property;
	static const uint32_p      remaining_hops_property;
	static const uint32_p      topology_change_count_property;
	static const edge::property* const _properties[];
	static const edge::xtype<bridge_tree> _type;
	virtual const edge::concrete_type* type() const override final { return &_type; }
};

