
#include "pch.h"
#include "bridge_tree.h"
#include "bridge.h"

using namespace std;
using namespace edge;

bridge_tree::bridge_tree (bridge* parent, uint32_t treeIndex)
	: _parent(parent), _treeIndex(treeIndex)
{
	::GetSystemTime(&_last_topology_change);
	_topology_change_count = 0;
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
	return STP_GetBridgePriority (_parent->stp_bridge(), _treeIndex);
}

void bridge_tree::set_bridge_priority (uint32_t priority)
{
	if (bridge_priority() != priority)
	{
		this->on_property_changing(&bridge_priority_property);
		STP_SetBridgePriority (_parent->stp_bridge(), _treeIndex, (unsigned short) priority, GetMessageTime());
		this->on_property_changed(&bridge_priority_property);
	}
}

static constexpr char StpDisabledString[] = "(STP disabled)";

array<unsigned char, 36> bridge_tree::root_priorty_vector() const
{
	array<unsigned char, 36> prioVector;
	STP_GetRootPriorityVector(_parent->stp_bridge(), _treeIndex, prioVector.data());
	return prioVector;
}

std::string bridge_tree::root_bridge_id() const
{
	if (!STP_IsBridgeStarted (_parent->stp_bridge()))
		return StpDisabledString;

	auto rpv = root_priorty_vector();
	stringstream ss;
	ss << uppercase << setfill('0') << hex
		<< setw(2) << (int)rpv[0] << setw(2) << (int)rpv[1] << "."
		<< setw(2) << (int)rpv[2] << setw(2) << (int)rpv[3] << setw(2) << (int)rpv[4]
		<< setw(2) << (int)rpv[5] << setw(2) << (int)rpv[6] << setw(2) << (int)rpv[7];
	return ss.str();
}

std::string bridge_tree::GetExternalRootPathCost() const
{
	if (!STP_IsBridgeStarted(_parent->stp_bridge()))
		return StpDisabledString;

	auto rpv = root_priorty_vector();
	auto cost = ((uint32_t) rpv[8] << 24) | ((uint32_t) rpv[9] << 16) | ((uint32_t) rpv[10] << 8) | rpv[11];
	return to_string (cost);
}

std::string bridge_tree::GetRegionalRootBridgeId() const
{
	if (!STP_IsBridgeStarted (_parent->stp_bridge()))
		return StpDisabledString;

	auto rpv = root_priorty_vector();
	stringstream ss;
	ss << uppercase << setfill('0') << hex
		<< setw(2) << rpv[12] << setw(2) << rpv[13] << "."
		<< setw(2) << rpv[14] << setw(2) << rpv[15] << setw(2) << rpv[16]
		<< setw(2) << rpv[17] << setw(2) << rpv[18] << setw(2) << rpv[19];
	return ss.str();
}

std::string bridge_tree::GetInternalRootPathCost() const
{
	if (!STP_IsBridgeStarted(_parent->stp_bridge()))
		return StpDisabledString;

	auto rpv = root_priorty_vector();
	auto cost = ((uint32_t) rpv[20] << 24) | ((uint32_t) rpv[21] << 16) | ((uint32_t) rpv[22] << 8) | rpv[23];
	return to_string(cost);
}

std::string bridge_tree::GetDesignatedBridgeId() const
{
	if (!STP_IsBridgeStarted(_parent->stp_bridge()))
		return StpDisabledString;

	auto rpv = root_priorty_vector();
	stringstream ss;
	ss << uppercase << setfill('0') << hex
		<< setw(2) << rpv[24] << setw(2) << rpv[25] << "."
		<< setw(2) << rpv[26] << setw(2) << rpv[27] << setw(2) << rpv[28]
		<< setw(2) << rpv[29] << setw(2) << rpv[30] << setw(2) << rpv[31];
	return ss.str();
}

std::string bridge_tree::GetDesignatedPortId() const
{
	if (!STP_IsBridgeStarted(_parent->stp_bridge()))
		return StpDisabledString;

	auto rpv = root_priorty_vector();
	stringstream ss;
	ss << uppercase << setfill('0') << hex << setw(2) << rpv[32] << setw(2) << rpv[33];
	return ss.str();
}

std::string bridge_tree::GetReceivingPortId() const
{
	if (!STP_IsBridgeStarted(_parent->stp_bridge()))
		return StpDisabledString;

	auto rpv = root_priorty_vector();
	stringstream ss;
	ss << uppercase << setfill('0') << hex << setw(2) << rpv[34] << setw(2) << rpv[35];
	return ss.str();
}

uint32_t bridge_tree::hello_time() const
{
	unsigned short ht;
	STP_GetRootTimes(_parent->stp_bridge(), _treeIndex, nullptr, &ht, nullptr, nullptr, nullptr);
	return ht;
}

uint32_t bridge_tree::max_age() const
{
	unsigned short ma;
	STP_GetRootTimes(_parent->stp_bridge(), _treeIndex, nullptr, nullptr, &ma, nullptr, nullptr);
	return ma;
}

uint32_t bridge_tree::bridge_forward_delay() const
{
	unsigned short fd;
	STP_GetRootTimes(_parent->stp_bridge(), _treeIndex, &fd, nullptr, nullptr, nullptr, nullptr);
	return fd;
}

uint32_t bridge_tree::message_age() const
{
	unsigned short ma;
	STP_GetRootTimes(_parent->stp_bridge(), _treeIndex, nullptr, nullptr, nullptr, &ma, nullptr);
	return ma;
}

uint32_t bridge_tree::remaining_hops() const
{
	unsigned char rh;
	STP_GetRootTimes(_parent->stp_bridge(), _treeIndex, nullptr, nullptr, nullptr, nullptr, &rh);
	return rh;
}

// ============================================================================

static const edge::property_group root_times_group = { 5, "Root Times" };

const NVP bridge_priority_nvps[] =
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

const bridge_priority_p bridge_tree::bridge_priority_property {
	"BridgePriority", nullptr, nullptr, ui_visible::yes,
	static_cast<bridge_priority_p::member_getter_t>(&bridge_priority),
	static_cast<bridge_priority_p::member_setter_t>(&set_bridge_priority),
	0x8000,
};

const temp_string_p bridge_tree::root_bridge_id_property {
	"Root Bridge ID", nullptr, nullptr, ui_visible::yes,
	static_cast<temp_string_p::member_getter_t>(&root_bridge_id),
	nullptr,
	std::nullopt,
};

const edge::uint32_p bridge_tree::topology_change_count_property {
	"Topology Change Count", nullptr, nullptr, ui_visible::yes,
	static_cast<edge::uint32_p::member_getter_t>(&topology_change_count),
	nullptr,
	std::nullopt,
};
/*
static const TypedProperty<wstring> ExternalRootPathCost
(
	L"External Root Path Cost",
	static_cast<TypedProperty<wstring>::Getter>(&bridge_tree::GetExternalRootPathCost),
	nullptr
);

static const TypedProperty<wstring> RegionalRootBridgeId
(
	L"Regional Root Bridge Id",
	static_cast<TypedProperty<wstring>::Getter>(&bridge_tree::GetRegionalRootBridgeId),
	nullptr
);

static const TypedProperty<wstring> InternalRootPathCost
(
	L"Internal Root Path Cost",
	static_cast<TypedProperty<wstring>::Getter>(&bridge_tree::GetInternalRootPathCost),
	nullptr
);

static const TypedProperty<wstring> DesignatedBridgeId
(
	L"Designated Bridge Id",
	static_cast<TypedProperty<wstring>::Getter>(&bridge_tree::GetDesignatedBridgeId),
	nullptr
);

static const TypedProperty<wstring> DesignatedPortId
(
	L"Designated Port Id",
	static_cast<TypedProperty<wstring>::Getter>(&bridge_tree::GetDesignatedPortId),
	nullptr
);

static const TypedProperty<wstring> ReceivingPortId
(
	L"Receiving Port Id",
	static_cast<TypedProperty<wstring>::Getter>(&bridge_tree::GetReceivingPortId),
	nullptr
);
*/

const edge::uint32_p bridge_tree::hello_time_property {
	"HelloTime",
	&root_times_group,
	nullptr,
	ui_visible::yes,
	static_cast<edge::uint32_p::member_getter_t>(&bridge_tree::hello_time),
	nullptr,
	std::nullopt,
};

const edge::uint32_p bridge_tree::max_age_property {
	"MaxAge",
	&root_times_group,
	nullptr,
	ui_visible::yes,
	static_cast<edge::uint32_p::member_getter_t>(&bridge_tree::max_age),
	nullptr,
	std::nullopt,
};

const edge::uint32_p bridge_tree::forward_delay_property {
	"ForwardDelay",
	&root_times_group,
	nullptr,
	ui_visible::yes,
	static_cast<edge::uint32_p::member_getter_t>(&bridge_tree::bridge_forward_delay),
	nullptr,
	std::nullopt,
};

const edge::uint32_p bridge_tree::message_age_property {
	"MessageAge",
	&root_times_group,
	nullptr,
	ui_visible::yes,
	static_cast<edge::uint32_p::member_getter_t>(&bridge_tree::message_age),
	nullptr,
	std::nullopt,
};

const edge::uint32_p bridge_tree::remaining_hops_property {
	"remainingHops",
	&root_times_group,
	nullptr,
	ui_visible::yes,
	static_cast<edge::uint32_p::member_getter_t>(&remaining_hops),
	nullptr,
	std::nullopt,
};

const edge::property* const bridge_tree::_properties[] =
{
	&bridge_priority_property,
	&root_bridge_id_property,
	&topology_change_count_property,
/*	&ExternalRootPathCost,
	&RegionalRootBridgeId,
	&InternalRootPathCost,
	&DesignatedBridgeId,
	&DesignatedPortId,
	&ReceivingPortId,
*/
	&hello_time_property,
	&max_age_property,
	&forward_delay_property,
	&message_age_property,
	&remaining_hops_property
};

const edge::xtype<bridge_tree> bridge_tree::_type = {
	"BridgeTree",
	&base::_type,
	_properties,
	nullptr,
};

const edge::type* bridge_tree::type() const { return &_type; }