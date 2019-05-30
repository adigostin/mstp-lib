#pragma once
#include "object.h"
#include "win32/com_ptr.h"
#include "stp.h"

class bridge;
class port;

using edge::size_p;
using edge::uint32_p;
using edge::bool_p;
using edge::float_p;
using edge::backed_string_p;
using edge::temp_string_p;
using edge::property;
using edge::type;
using edge::xtype;
using edge::value_property;
using edge::typed_object_collection_property;
using edge::property_change_args;

extern const edge::NVP port_priority_nvps[];
extern const char port_priority_type_name[];
using port_priority_p = edge::enum_property<uint32_t, port_priority_type_name, port_priority_nvps, true>;

extern const edge::NVP port_role_nvps[];
extern const char port_role_type_name[];
using port_role_p = edge::enum_property<STP_PORT_ROLE, port_role_type_name, port_role_nvps>;

extern const char stp_disabled_text[];

class port_tree : public edge::object
{
	using base = edge::object;

	friend port;

	port* const _port;
	size_t const _tree_index;
	LONG fdb_flush_timestamp = 0;
	bool fdb_flush_text_visible = false;
	
	static void on_bridge_property_changing (void* arg, object* obj, const property_change_args& args);
	static void on_bridge_property_changed (void* arg, object* obj, const property_change_args& args);
	static void on_link_pulse (void* arg, bridge* b, size_t port_index, unsigned int timestamp);

public:
	port_tree (port* port, size_t tree_index);

	uint32_t priority() const;
	void set_priority (uint32_t priority);

	bool learning() const;
	bool forwarding() const;
	STP_PORT_ROLE role() const;

	void flush_fdb (unsigned int timestamp);

	size_t tree_index() const { return _tree_index; }

	static const size_p tree_index_property;
	static const port_priority_p priority_property;
	static const bool_p learning_property;
	static const bool_p forwarding_property;
	static const port_role_p role_property;
	static const property* const _properties[];
	static const xtype<port_tree> _type;
	const struct type* type() const;
};
