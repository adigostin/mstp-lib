
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "object.h"
#include "win32/com_ptr.h"
#include "stp.h"
#include "renderable_object.h"

class bridge;
class port;

extern const nvp port_priority_nvps[];
extern const char port_priority_type_name[];
using port_priority_p = edge::enum_property<uint32_t, port_priority_type_name, port_priority_nvps, true>;

extern const nvp port_role_nvps[];
extern const char port_role_type_name[];
using port_role_p = edge::enum_property<STP_PORT_ROLE, port_role_type_name, port_role_nvps>;

extern const char stp_disabled_text[];

class port_tree : public edge::object
{
	using base = edge::object;

	friend port;

	port* const _port;
	size_t const _tree_index;
	ULONGLONG _flush_tick_count = 0;
	bool _flush_text_visible = false;

	static UINT_PTR _flush_timer;
	static std::unordered_set<port_tree*> _trees;

	void on_bridge_property_changing (object* obj, const property_change_args& args);
	void on_bridge_property_changed (object* obj, const property_change_args& args);
	static void CALLBACK flush_timer_proc (HWND hwnd, UINT, UINT_PTR, DWORD);

public:
	port_tree (port* port, size_t tree_index);
	~port_tree();

	uint32_t priority() const;
	void set_priority (uint32_t priority);

	uint32_t internal_port_path_cost() const;
	uint32_t admin_internal_port_path_cost() const;
	void set_admin_internal_port_path_cost (uint32_t value);

	bool learning() const;
	bool forwarding() const;
	STP_PORT_ROLE role() const;

	void flush_fdb (unsigned int timestamp);

	bool fdb_flush_text_visible() const { return _flush_text_visible; }

	size_t tree_index() const { return _tree_index; }

	static const prop_wrapper<size_t_p, edge::pg_hidden> tree_index_property;
	static const port_priority_p priority_property;
	static const bool_p learning_property;
	static const bool_p forwarding_property;
	static const port_role_p role_property;
	static const uint32_p admin_internal_port_path_cost_property;
	static const uint32_p internal_port_path_cost_property;
	static const property* const _properties[];
	static const xtype<port_tree> _type;
	const concrete_type* type() const override final { return &_type; }
};
