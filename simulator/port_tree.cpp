
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "port_tree.h"
#include "port.h"
#include "bridge.h"
#include "stp.h"
#include "xml_serializer.h"

using namespace edge;

const char port_priority_type_name[] = "PortPriority";
const edge::nvp port_priority_nvps[] {
	{ "10 (16 dec)",  0x10 },
	{ "20 (32 dec)",  0x20 },
	{ "30 (48 dec)",  0x30 },
	{ "40 (64 dec)",  0x40 },
	{ "50 (80 dec)",  0x50 },
	{ "60 (96 dec)",  0x60 },
	{ "70 (112 dec)", 0x70 },
	{ "80 (128 dec)", 0x80 },
	{ "90 (144 dec)", 0x90 },
	{ "A0 (160 dec)", 0xA0 },
	{ "B0 (176 dec)", 0xB0 },
	{ "C0 (192 dec)", 0xC0 },
	{ "D0 (208 dec)", 0xD0 },
	{ "E0 (224 dec)", 0xE0 },
	{ "F0 (240 dec)", 0xF0 },
	{ nullptr, 0 },
};

const char port_role_type_name[] = "port_role";
const edge::nvp port_role_nvps[] =
{
	{ STP_GetPortRoleString(STP_PORT_ROLE_DISABLED),   (int) STP_PORT_ROLE_DISABLED },
	{ STP_GetPortRoleString(STP_PORT_ROLE_ROOT),       (int) STP_PORT_ROLE_ROOT },
	{ STP_GetPortRoleString(STP_PORT_ROLE_DESIGNATED), (int) STP_PORT_ROLE_DESIGNATED },
	{ STP_GetPortRoleString(STP_PORT_ROLE_ALTERNATE),  (int) STP_PORT_ROLE_ALTERNATE },
	{ STP_GetPortRoleString(STP_PORT_ROLE_BACKUP),     (int) STP_PORT_ROLE_BACKUP },
	{ STP_GetPortRoleString(STP_PORT_ROLE_MASTER),     (int) STP_PORT_ROLE_MASTER },
	{ nullptr, 0 }
};

const char stp_disabled_text[] = "(STP disabled)";

UINT_PTR                       port_tree::_flush_timer;
std::unordered_set<port_tree*> port_tree::_trees;

port_tree::port_tree (size_t tree_index)
	: _tree_index(tree_index)
{ }

::port* port_tree::port() const
{
	return static_cast<::port*>(static_cast<typed_object_collection_i<port_tree>*>(base::parent()));
}

void port_tree::on_inserted_into_parent()
{
	base::on_inserted_into_parent();

	port()->stp_enabled_changing().add_handler<&port_tree::on_stp_enabled_changing>(this);
	port()->stp_enabled_changed().add_handler<&port_tree::on_stp_enabled_changed>(this);

	if (_trees.empty())
		_flush_timer = ::SetTimer (nullptr, 0, 100, flush_timer_proc);
	_trees.insert(this);
}

void port_tree::on_removing_from_parent()
{
	_trees.erase(this);
	if (_trees.empty())
		::KillTimer (nullptr, _flush_timer);

	port()->stp_enabled_changed().remove_handler<&port_tree::on_stp_enabled_changed>(this);
	port()->stp_enabled_changing().remove_handler<&port_tree::on_stp_enabled_changing>(this);

	base::on_removing_from_parent();
}

void port_tree::on_stp_enabled_changing (const property_change_args& args)
{
	on_property_changing(&learning_property);
	on_property_changing(&forwarding_property);
	on_property_changing(&role_property);
}

void port_tree::on_stp_enabled_changed (const property_change_args& args)
{
	on_property_changed(&role_property);
	on_property_changed(&forwarding_property);
	on_property_changed(&learning_property);
	port()->invalidate();
}

void port_tree::flush_timer_proc (HWND hwnd, UINT, UINT_PTR timer_id, DWORD)
{
	auto now = ::GetTickCount64();

	for (port_tree* tree : _trees)
	{
		auto time_since_last_flush = now - tree->_flush_tick_count;
		bool text_visible = time_since_last_flush < 2000;
		if (tree->_flush_text_visible != text_visible)
		{
			tree->_flush_text_visible = text_visible;
			tree->port()->invalidate();
		}
	}
}

void port_tree::flush_fdb (unsigned int timestamp)
{
	_flush_tick_count = ::GetTickCount64();
	if (!_flush_text_visible)
	{
		_flush_text_visible = true;
		port()->invalidate();
	}
}

uint32_t port_tree::priority() const
{
	return STP_GetPortPriority (port()->bridge()->stp_bridge(), (unsigned int)port()->port_index(), (unsigned int)_tree_index);
}

void port_tree::set_priority (uint32_t priority)
{
	if (this->priority() != priority)
	{
		this->on_property_changing(&priority_property);
		STP_SetPortPriority (port()->bridge()->stp_bridge(), (unsigned int)port()->port_index(), (unsigned int)_tree_index, (unsigned char) priority, GetMessageTime());
		this->on_property_changed(&priority_property);
	}
}

uint32_t port_tree::internal_port_path_cost() const
{
	return STP_GetInternalPortPathCost(port()->bridge()->stp_bridge(), (unsigned int)port()->port_index(), (unsigned int)_tree_index);
}

uint32_t port_tree::admin_internal_port_path_cost() const
{
	return STP_GetAdminInternalPortPathCost(port()->bridge()->stp_bridge(), (unsigned int)port()->port_index(), (unsigned int)_tree_index);
}

void port_tree::set_admin_internal_port_path_cost (uint32_t value)
{
	if (STP_GetAdminInternalPortPathCost (port()->bridge()->stp_bridge(), (unsigned int)port()->port_index(), (unsigned int)_tree_index) != value)
	{
		this->on_property_changing (&admin_internal_port_path_cost_property);
		this->on_property_changing (&internal_port_path_cost_property);
		STP_SetAdminInternalPortPathCost (port()->bridge()->stp_bridge(), (unsigned int)port()->port_index(), (unsigned int)_tree_index, value, ::GetMessageTime());
		this->on_property_changed (&internal_port_path_cost_property);
		this->on_property_changed (&admin_internal_port_path_cost_property);
	}
}

bool port_tree::learning() const
{
	if (!STP_IsBridgeStarted(port()->bridge()->stp_bridge()))
		throw std::logic_error(stp_disabled_text);
	return STP_GetPortLearning(port()->bridge()->stp_bridge(), (unsigned int)port()->port_index(), (unsigned int)_tree_index);
}

bool port_tree::forwarding() const
{
	if (!STP_IsBridgeStarted(port()->bridge()->stp_bridge()))
		throw std::logic_error(stp_disabled_text);
	return STP_GetPortForwarding(port()->bridge()->stp_bridge(), (unsigned int)port()->port_index(), (unsigned int)_tree_index);
}

STP_PORT_ROLE port_tree::role() const
{
	if (!STP_IsBridgeStarted(port()->bridge()->stp_bridge()))
		throw std::logic_error(stp_disabled_text);
	return STP_GetPortRole (port()->bridge()->stp_bridge(), (unsigned int)port()->port_index(), (unsigned int)_tree_index);
}

const edge::size_p port_tree::tree_index_property {
	"TreeIndex", nullptr, nullptr, false,
	&tree_index,
	nullptr,
};

const port_priority_p port_tree::priority_property {
	"PortPriority",
	nullptr,
	"The value of the priority field which is contained in the first (in network byte order) octet of the (2 octet long) Port ID. "
		"The other octet of the Port ID is given by the value of dot1dStpPort.",
	true,
	&priority,
	&set_priority,
	0x80,
};

const edge::bool_p port_tree::learning_property {
	"learning",
	nullptr,
	nullptr,
	true,
	&learning,
	nullptr,
};

const edge::bool_p port_tree::forwarding_property {
	"forwarding",
	nullptr,
	nullptr,
	true,
	&forwarding,
	nullptr,
};

const port_role_p port_tree::role_property {
	"role",
	nullptr,
	nullptr,
	true,
	&role,
	nullptr,
};

static const edge::property_group port_path_cost_group = { 5, "Port Path Cost" };

const uint32_p port_tree::admin_internal_port_path_cost_property {
	"AdminInternalPortPathCost",
	&port_path_cost_group,
	nullptr,
	true,
	&admin_internal_port_path_cost,
	&set_admin_internal_port_path_cost,
	0,
};

const uint32_p port_tree::internal_port_path_cost_property {
	"InternalPortPathCost",
	&port_path_cost_group,
	nullptr,
	true,
	&internal_port_path_cost,
	nullptr,
	0,
};

const edge::property* const port_tree::_properties[] = {
	&tree_index_property,
	&priority_property,
	&learning_property,
	&forwarding_property,
	&role_property,
	&admin_internal_port_path_cost_property,
	&internal_port_path_cost_property,
};

const xtype<port_tree> port_tree::_type = { "PortTree", &base::_type, _properties };

const concrete_type* port_tree::type() const { return &_type; }