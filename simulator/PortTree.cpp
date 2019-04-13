
#include "pch.h"
#include "PortTree.h"
#include "Port.h"
#include "Bridge.h"
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

uint32_t PortTree::priority() const
{
	return STP_GetPortPriority (_port->bridge()->stp_bridge(), _port->port_index(), _treeIndex);
}

void PortTree::set_priority (uint32_t priority)
{
	if (this->priority() != priority)
	{
		this->on_property_changing(&priority_property);
		STP_SetPortPriority (_port->bridge()->stp_bridge(), _port->port_index(), _treeIndex, (unsigned char) priority, GetMessageTime());
		this->on_property_changed(&priority_property);
	}
}

bool PortTree::learning() const
{
	return STP_GetPortLearning(_port->bridge()->stp_bridge(), _port->port_index(), _treeIndex);
}

bool PortTree::forwarding() const
{
	return STP_GetPortForwarding(_port->bridge()->stp_bridge(), _port->port_index(), _treeIndex);
}

STP_PORT_ROLE PortTree::role() const
{
	return STP_GetPortRole (_port->bridge()->stp_bridge(), _port->port_index(), _treeIndex);
}

const edge::uint32_p PortTree::tree_index_property {
	"TreeIndex", nullptr, nullptr, edge::ui_visible::no,
	static_cast<const edge::uint32_p::member_getter_t>(&tree_index),
	nullptr,
	std::nullopt,
};

const port_priority_p PortTree::priority_property {
	"PortPriority",
	nullptr,
	"The value of the priority field which is contained in the first (in network byte order) octet of the (2 octet long) Port ID. "
		"The other octet of the Port ID is given by the value of dot1dStpPort.",
	ui_visible::yes,
	static_cast<port_priority_p::member_getter_t>(&priority),
	static_cast<port_priority_p::member_setter_t>(&set_priority),
	0x80,
};

const edge::bool_p PortTree::learning_property {
	"learning",
	nullptr,
	nullptr,
	ui_visible::yes,
	static_cast<edge::bool_p::member_getter_t>(&learning),
	nullptr,
	std::nullopt,
};

const edge::bool_p PortTree::forwarding_property {
	"forwarding",
	nullptr,
	nullptr,
	ui_visible::yes,
	static_cast<edge::bool_p::member_getter_t>(&forwarding),
	nullptr,
	std::nullopt,
};

const port_role_p PortTree::role_property {
	"role",
	nullptr,
	nullptr,
	ui_visible::yes,
	static_cast<port_role_p::member_getter_t>(&role),
	nullptr,
	std::nullopt,
};

const edge::property* const PortTree::_properties[] = { &tree_index_property, &priority_property, &learning_property, &forwarding_property, &role_property };

const xtype<PortTree> PortTree::_type = {
	"PortTree",
	&base::_type,
	_properties, 
	nullptr
};

const edge::type* PortTree::type() const { return &_type; }