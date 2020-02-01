
#include "pch.h"
#include "port_tree.h"
#include "port.h"
#include "bridge.h"
#include "stp.h"
#include "win32/xml_serializer.h"

using namespace edge;

const char port_priority_type_name[] = "PortPriority";
const edge::NVP port_priority_nvps[] {
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
const edge::NVP port_role_nvps[] =
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

port_tree::port_tree (port* port, size_t tree_index)
	: _port(port), _tree_index(tree_index)
{
	// No need to call remove_handler since a bridge and its bridge_trees are deleted at the same time.
	_port->bridge()->property_changing().add_handler(&on_bridge_property_changing, this);
	_port->bridge()->property_changed().add_handler(&on_bridge_property_changed, this);

	if (_trees.empty())
		_flush_timer = ::SetTimer (nullptr, 0, 100, flush_timer_proc);
	_trees.insert(this);
}

port_tree::~port_tree()
{
	_trees.erase(this);
	if (_trees.empty())
		::KillTimer (nullptr, _flush_timer);
}

void port_tree::on_bridge_property_changing (void* arg, object* obj, const property_change_args& args)
{
	auto this_ = static_cast<port_tree*>(arg);

	if (args.property == &bridge::stp_enabled_property)
	{
		this_->on_property_changing(&learning_property);
		this_->on_property_changing(&forwarding_property);
		this_->on_property_changing(&role_property);
	}
}

void port_tree::on_bridge_property_changed (void* arg, object* obj, const property_change_args& args)
{
	auto this_ = static_cast<port_tree*>(arg);

	if (args.property == &bridge::stp_enabled_property)
	{
		this_->on_property_changed(&role_property);
		this_->on_property_changed(&forwarding_property);
		this_->on_property_changed(&learning_property);
		this_->_port->invalidate();
	}
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
			tree->_port->invalidate();
		}
	}
}

void port_tree::flush_fdb (unsigned int timestamp)
{
	_flush_tick_count = ::GetTickCount64();
	if (!_flush_text_visible)
	{
		_flush_text_visible = true;
		_port->invalidate();
	}
}

uint32_t port_tree::priority() const
{
	return STP_GetPortPriority (_port->bridge()->stp_bridge(), (unsigned int)_port->port_index(), (unsigned int)_tree_index);
}

void port_tree::set_priority (uint32_t priority)
{
	if (this->priority() != priority)
	{
		this->on_property_changing(&priority_property);
		STP_SetPortPriority (_port->bridge()->stp_bridge(), (unsigned int)_port->port_index(), (unsigned int)_tree_index, (unsigned char) priority, GetMessageTime());
		this->on_property_changed(&priority_property);
	}
}

uint32_t port_tree::internal_port_path_cost() const
{
	return STP_GetInternalPortPathCost(_port->bridge()->stp_bridge(), (unsigned int)_port->port_index(), (unsigned int)_tree_index);
}

uint32_t port_tree::admin_internal_port_path_cost() const
{
	return STP_GetAdminInternalPortPathCost(_port->bridge()->stp_bridge(), (unsigned int)_port->port_index(), (unsigned int)_tree_index);
}

void port_tree::set_admin_internal_port_path_cost (uint32_t value)
{
	if (STP_GetAdminInternalPortPathCost (_port->bridge()->stp_bridge(), (unsigned int)_port->port_index(), (unsigned int)_tree_index) != value)
	{
		this->on_property_changing (&admin_internal_port_path_cost_property);
		this->on_property_changing (&internal_port_path_cost_property);
		STP_SetAdminInternalPortPathCost (_port->bridge()->stp_bridge(), (unsigned int)_port->port_index(), (unsigned int)_tree_index, value, ::GetMessageTime());
		this->on_property_changed (&internal_port_path_cost_property);
		this->on_property_changed (&admin_internal_port_path_cost_property);
	}
}

bool port_tree::learning() const
{
	if (!STP_IsBridgeStarted(_port->bridge()->stp_bridge()))
		throw std::logic_error(stp_disabled_text);
	return STP_GetPortLearning(_port->bridge()->stp_bridge(), (unsigned int)_port->port_index(), (unsigned int)_tree_index);
}

bool port_tree::forwarding() const
{
	if (!STP_IsBridgeStarted(_port->bridge()->stp_bridge()))
		throw std::logic_error(stp_disabled_text);
	return STP_GetPortForwarding(_port->bridge()->stp_bridge(), (unsigned int)_port->port_index(), (unsigned int)_tree_index);
}

STP_PORT_ROLE port_tree::role() const
{
	if (!STP_IsBridgeStarted(_port->bridge()->stp_bridge()))
		throw std::logic_error(stp_disabled_text);
	return STP_GetPortRole (_port->bridge()->stp_bridge(), (unsigned int)_port->port_index(), (unsigned int)_tree_index);
}

const edge::size_t_p port_tree::tree_index_property {
	"TreeIndex", nullptr, nullptr, edge::ui_visible::no,
	static_cast<size_t_p::member_getter_t>(&tree_index),
	nullptr,
};

const port_priority_p port_tree::priority_property {
	"PortPriority",
	nullptr,
	"The value of the priority field which is contained in the first (in network byte order) octet of the (2 octet long) Port ID. "
		"The other octet of the Port ID is given by the value of dot1dStpPort.",
	ui_visible::yes,
	static_cast<port_priority_p::member_getter_t>(&priority),
	static_cast<port_priority_p::member_setter_t>(&set_priority),
	0x80,
};

const edge::bool_p port_tree::learning_property {
	"learning",
	nullptr,
	nullptr,
	ui_visible::yes,
	static_cast<edge::bool_p::member_getter_t>(&learning),
	nullptr,
};

const edge::bool_p port_tree::forwarding_property {
	"forwarding",
	nullptr,
	nullptr,
	ui_visible::yes,
	static_cast<edge::bool_p::member_getter_t>(&forwarding),
	nullptr,
};

const port_role_p port_tree::role_property {
	"role",
	nullptr,
	nullptr,
	ui_visible::yes,
	static_cast<port_role_p::member_getter_t>(&role),
	nullptr,
};

static const edge::property_group port_path_cost_group = { 5, "Port Path Cost" };

const uint32_p port_tree::admin_internal_port_path_cost_property {
	"AdminInternalPortPathCost",
	&port_path_cost_group,
	nullptr,
	ui_visible::yes,
	static_cast<uint32_p::member_getter_t>(&admin_internal_port_path_cost),
	static_cast<uint32_p::member_setter_t>(&set_admin_internal_port_path_cost),
	0,
};

const uint32_p port_tree::internal_port_path_cost_property {
	"InternalPortPathCost",
	&port_path_cost_group,
	nullptr,
	ui_visible::yes,
	static_cast<uint32_p::member_getter_t>(&internal_port_path_cost),
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

const xtype<port_tree> port_tree::_type = { "PortTree", &base::_type, _properties, nullptr };

const edge::type* port_tree::type() const { return &_type; }