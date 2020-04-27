
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "bridge_tree.h"
#include "bridge.h"

using namespace edge;

bridge_tree::bridge_tree (size_t tree_index)
	: _tree_index(tree_index)
{
	::GetSystemTime(&_last_topology_change);
	_topology_change_count = 0;
}

bridge* bridge_tree::parent() const
{
	return static_cast<bridge*>(static_cast<bridge_tree_collection_i*>(base::parent()));
}

void bridge_tree::on_inserted_into_parent()
{
	base::on_inserted_into_parent();
	parent()->property_changing().add_handler<&bridge_tree::on_bridge_property_changing>(this);
	parent()->property_changed().add_handler<&bridge_tree::on_bridge_property_changed>(this);
}

void bridge_tree::on_removing_from_parent()
{
	parent()->property_changed().remove_handler<&bridge_tree::on_bridge_property_changed>(this);
	parent()->property_changing().remove_handler<&bridge_tree::on_bridge_property_changing>(this);
	base::on_removing_from_parent();
}

static const value_property* const properties_changed_on_stp_enable_disable[] = {
	&bridge_tree::root_id_property,
	&bridge_tree::external_root_path_cost_property,
	&bridge_tree::regional_root_id_property,
	&bridge_tree::internal_root_path_cost_property,
	&bridge_tree::designated_bridge_id_property,
	&bridge_tree::designated_port_id_property,
	&bridge_tree::receiving_port_id_property,
};

void bridge_tree::on_bridge_property_changing (object* obj, const property_change_args& args)
{
	if (args.property == &bridge::stp_enabled_property)
	{
		for (auto prop : properties_changed_on_stp_enable_disable)
			this->on_property_changing(prop);
	}
}

void bridge_tree::on_bridge_property_changed (object* obj, const property_change_args& args)
{
	if (args.property == &bridge::stp_enabled_property)
	{
		for (auto prop : properties_changed_on_stp_enable_disable)
			this->on_property_changed(prop);
	}
}

void bridge_tree::on_topology_change (unsigned int timestamp)
{
	this->on_property_changing(&topology_change_count_property);
	::GetSystemTime(&_last_topology_change);
	_topology_change_count++;
	this->on_property_changed(&topology_change_count_property);
}

uint32_t bridge_tree::bridge_priority() const
{
	return STP_GetBridgePriority (parent()->stp_bridge(), (unsigned int)_tree_index);
}

void bridge_tree::set_bridge_priority (uint32_t priority)
{
	if (bridge_priority() != priority)
	{
		this->on_property_changing(&bridge_priority_property);
		STP_SetBridgePriority (parent()->stp_bridge(), (unsigned int)_tree_index, (unsigned short) priority, GetMessageTime());
		this->on_property_changed(&bridge_priority_property);
	}
}

std::array<unsigned char, 36> bridge_tree::root_priorty_vector() const
{
	std::array<unsigned char, 36> prioVector;
	STP_GetRootPriorityVector(parent()->stp_bridge(), (unsigned int)_tree_index, prioVector.data());
	return prioVector;
}

std::string bridge_tree::root_bridge_id() const
{
	if (!STP_IsBridgeStarted (parent()->stp_bridge()))
		throw std::logic_error(stp_disabled_text);

	auto rpv = root_priorty_vector();
	std::stringstream ss;
	ss << std::uppercase << std::setfill('0') << std::hex
		<< std::setw(2) << (int)rpv[0] << std::setw(2) << (int)rpv[1] << "."
		<< std::setw(2) << (int)rpv[2] << std::setw(2) << (int)rpv[3] << std::setw(2) << (int)rpv[4]
		<< std::setw(2) << (int)rpv[5] << std::setw(2) << (int)rpv[6] << std::setw(2) << (int)rpv[7];
	return ss.str();
}

uint32_t bridge_tree::external_root_path_cost() const
{
	if (!STP_IsBridgeStarted(parent()->stp_bridge()))
		throw std::logic_error(stp_disabled_text);

	auto rpv = root_priorty_vector();
	auto cost = ((uint32_t) rpv[8] << 24) | ((uint32_t) rpv[9] << 16) | ((uint32_t) rpv[10] << 8) | rpv[11];
	return cost;
}

std::string bridge_tree::regional_root_id() const
{
	if (!STP_IsBridgeStarted (parent()->stp_bridge()))
		throw std::logic_error(stp_disabled_text);

	auto rpv = root_priorty_vector();
	std::stringstream ss;
	ss << std::uppercase << std::setfill('0') << std::hex
		<< std::setw(2) << (int)rpv[12] << std::setw(2) << (int)rpv[13] << "."
		<< std::setw(2) << (int)rpv[14] << std::setw(2) << (int)rpv[15] << std::setw(2) << (int)rpv[16]
		<< std::setw(2) << (int)rpv[17] << std::setw(2) << (int)rpv[18] << std::setw(2) << (int)rpv[19];
	return ss.str();
}

uint32_t bridge_tree::internal_root_path_cost() const
{
	if (!STP_IsBridgeStarted(parent()->stp_bridge()))
		throw std::logic_error(stp_disabled_text);

	auto rpv = root_priorty_vector();
	auto cost = ((uint32_t) rpv[20] << 24) | ((uint32_t) rpv[21] << 16) | ((uint32_t) rpv[22] << 8) | rpv[23];
	return cost;
}

std::string bridge_tree::designated_bridge_id() const
{
	if (!STP_IsBridgeStarted(parent()->stp_bridge()))
		throw std::logic_error(stp_disabled_text);

	auto rpv = root_priorty_vector();
	std::stringstream ss;
	ss << std::uppercase << std::setfill('0') << std::hex
		<< std::setw(2) << (int)rpv[24] << std::setw(2) << (int)rpv[25] << "."
		<< std::setw(2) << (int)rpv[26] << std::setw(2) << (int)rpv[27] << std::setw(2) << (int)rpv[28]
		<< std::setw(2) << (int)rpv[29] << std::setw(2) << (int)rpv[30] << std::setw(2) << (int)rpv[31];
	return ss.str();
}

std::string bridge_tree::designated_port_id() const
{
	if (!STP_IsBridgeStarted(parent()->stp_bridge()))
		throw std::logic_error(stp_disabled_text);

	auto rpv = root_priorty_vector();
	std::stringstream ss;
	ss << std::uppercase << std::setfill('0') << std::hex << std::setw(2) << (int)rpv[32] << std::setw(2) << (int)rpv[33];
	return ss.str();
}

std::string bridge_tree::receiving_port_id() const
{
	if (!STP_IsBridgeStarted(parent()->stp_bridge()))
		throw std::logic_error(stp_disabled_text);

	auto rpv = root_priorty_vector();
	std::stringstream ss;
	ss << std::uppercase << std::setfill('0') << std::hex << std::setw(2) << (int)rpv[34] << std::setw(2) << (int)rpv[35];
	return ss.str();
}

uint32_t bridge_tree::hello_time() const
{
	unsigned short ht;
	STP_GetRootTimes(parent()->stp_bridge(), (unsigned int)_tree_index, nullptr, &ht, nullptr, nullptr, nullptr);
	return ht;
}

uint32_t bridge_tree::max_age() const
{
	unsigned short ma;
	STP_GetRootTimes(parent()->stp_bridge(), (unsigned int)_tree_index, nullptr, nullptr, &ma, nullptr, nullptr);
	return ma;
}

uint32_t bridge_tree::bridge_forward_delay() const
{
	unsigned short fd;
	STP_GetRootTimes(parent()->stp_bridge(), (unsigned int)_tree_index, &fd, nullptr, nullptr, nullptr, nullptr);
	return fd;
}

uint32_t bridge_tree::message_age() const
{
	unsigned short ma;
	STP_GetRootTimes(parent()->stp_bridge(), (unsigned int)_tree_index, nullptr, nullptr, nullptr, &ma, nullptr);
	return ma;
}

uint32_t bridge_tree::remaining_hops() const
{
	unsigned char rh;
	STP_GetRootTimes(parent()->stp_bridge(), (unsigned int)_tree_index, nullptr, nullptr, nullptr, nullptr, &rh);
	return rh;
}

// ============================================================================

static const property_group rpv_group = { 4, "Root Priority Vector" };
static const property_group root_times_group = { 5, "Root Times" };

const nvp bridge_priority_nvps[] =
{
	{ "1000 (4096 dec)", 0x1000 },
	{ "2000 (8192 dec)", 0x2000 },
	{ "3000 (12288 dec)", 0x3000 },
	{ "4000 (16384 dec)", 0x4000 },
	{ "5000 (20480 dec)", 0x5000 },
	{ "6000 (24576 dec)", 0x6000 },
	{ "7000 (28672 dec)", 0x7000 },
	{ "8000 (32768 dec)", 0x8000 },
	{ "9000 (36864 dec)", 0x9000 },
	{ "A000 (40960 dec)", 0xA000 },
	{ "B000 (45056 dec)", 0xB000 },
	{ "C000 (49152 dec)", 0xC000 },
	{ "D000 (53248 dec)", 0xD000 },
	{ "E000 (57344 dec)", 0xE000 },
	{ "F000 (61440 dec)", 0xF000 },
	{ nullptr, 0 },
};

const char bridge_priority_type_name[] = "bridge_priority";

const bridge_priority_p bridge_tree::bridge_priority_property
	{ "BridgePriority", nullptr, nullptr, &bridge_priority, &set_bridge_priority, 0x8000 };

const temp_string_p bridge_tree::root_id_property =
	{ "RootID", &rpv_group, nullptr, &root_bridge_id, nullptr, };

const uint32_p bridge_tree::external_root_path_cost_property =
	{ "ExternalRootPathCost", &rpv_group, nullptr, &external_root_path_cost, nullptr };

const temp_string_p bridge_tree::regional_root_id_property =
	{ "RegionalRootId", &rpv_group, nullptr, &regional_root_id, nullptr };

const uint32_p bridge_tree::internal_root_path_cost_property =
	{ "InternalRootPathCost", &rpv_group, nullptr, &internal_root_path_cost, nullptr };

const temp_string_p bridge_tree::designated_bridge_id_property =
	{ "DesignatedBridgeId", &rpv_group, nullptr, &designated_bridge_id, nullptr };

const temp_string_p bridge_tree::designated_port_id_property =
	{ "DesignatedPortId", &rpv_group, nullptr, &designated_port_id, nullptr };

const temp_string_p bridge_tree::receiving_port_id_property =
	{ "ReceivingPortId", &rpv_group, nullptr, &receiving_port_id, nullptr };

const edge::uint32_p bridge_tree::hello_time_property =
	{ "HelloTime", &root_times_group, nullptr, &hello_time, nullptr };

const edge::uint32_p bridge_tree::max_age_property =
	{ "MaxAge", &root_times_group, nullptr, &max_age, nullptr };

const edge::uint32_p bridge_tree::forward_delay_property =
	{ "ForwardDelay", &root_times_group, nullptr, &bridge_forward_delay, nullptr };

const edge::uint32_p bridge_tree::message_age_property =
	{ "MessageAge", &root_times_group, nullptr, &message_age, nullptr };

const edge::uint32_p bridge_tree::remaining_hops_property =
	{ "remainingHops", &root_times_group, nullptr, &remaining_hops, nullptr };

const edge::uint32_p bridge_tree::topology_change_count_property =
	{ "Topology Change Count", nullptr, nullptr, &topology_change_count, nullptr };

const edge::property* const bridge_tree::_properties[] =
{
	&bridge_priority_property,
	&root_id_property,
	&topology_change_count_property,
	&external_root_path_cost_property,
	&regional_root_id_property,
	&internal_root_path_cost_property,
	&designated_bridge_id_property,
	&designated_port_id_property,
	&receiving_port_id_property,
	&hello_time_property,
	&max_age_property,
	&forward_delay_property,
	&message_age_property,
	&remaining_hops_property
};

const edge::xtype<bridge_tree> bridge_tree::_type = { "BridgeTree", &base::_type, _properties, nullptr };
