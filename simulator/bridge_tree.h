
#pragma once
#include "object.h"
#include "win32/com_ptr.h"

class bridge;

using edge::object;
using edge::uint32_p;
using edge::temp_string_p;
using edge::property_change_args;
using edge::nvp;
using edge::concrete_type;

extern const nvp bridge_priority_nvps[];
extern const char bridge_priority_type_name[];
using bridge_priority_p = edge::enum_property<uint32_t, bridge_priority_type_name, bridge_priority_nvps, true>;

class bridge_tree : public object
{
	using base = object;

	bridge* const _parent;
	size_t const _tree_index;

	SYSTEMTIME _last_topology_change;
	uint32_t _topology_change_count;

	friend class bridge;

	void on_topology_change (unsigned int timestamp);
	static void on_bridge_property_changing (void* arg, object* obj, const property_change_args& args);
	static void on_bridge_property_changed (void* arg, object* obj, const property_change_args& args);

public:
	bridge_tree (bridge* parent, size_t tree_index);

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

	static const bridge_priority_p bridge_priority_property;
	static const temp_string_p root_id_property;
	static const uint32_p      external_root_path_cost_property;
	static const temp_string_p regional_root_id_property;
	static const uint32_p      internal_root_path_cost_property;
	static const temp_string_p designated_bridge_id_property;
	static const temp_string_p designated_port_id_property;
	static const temp_string_p receiving_port_id_property;
	static const uint32_p      hello_time_property;
	static const uint32_p      max_age_property;
	static const uint32_p      forward_delay_property;
	static const uint32_p      message_age_property;
	static const uint32_p      remaining_hops_property;
	static const uint32_p      topology_change_count_property;
	static const edge::property* const _properties[];
	static const edge::xtype<bridge_tree> _type;
	const concrete_type* type() const override final { return &_type; }
};

