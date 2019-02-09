
#pragma once
#include "object.h"
#include "win32/com_ptr.h"

class Bridge;

extern const edge::NVP bridge_priority_nvps[];
extern const char bridge_priority_type_name[];
using bridge_priority_p = edge::enum_property<uint32_t, bridge_priority_type_name, bridge_priority_nvps>;

struct BridgeTree : edge::object
{
	using base = edge::object;

	Bridge* const _parent;
	unsigned int const _treeIndex;

	BridgeTree (Bridge* parent, unsigned int treeIndex)
		: _parent(parent), _treeIndex(treeIndex)
	{ }

	HRESULT Serialize (IXMLDOMDocument3* doc, edge::com_ptr<IXMLDOMElement>& elementOut) const;
	HRESULT Deserialize (IXMLDOMElement* bridgeTreeElement);

	uint32_t bridge_priority() const;
	void set_bridge_priority (uint32_t priority);

	std::array<unsigned char, 36> root_priorty_vector() const;
	std::string root_bridge_id() const;
	std::string GetExternalRootPathCost() const;
	std::string GetRegionalRootBridgeId() const;
	std::string GetInternalRootPathCost() const;
	std::string GetDesignatedBridgeId() const;
	std::string GetDesignatedPortId() const;
	std::string GetReceivingPortId() const;

	uint32_t hello_time() const;
	uint32_t max_age() const;
	uint32_t forward_delay() const;
	uint32_t message_age() const;
	uint32_t remaining_hops() const;

	static const bridge_priority_p   bridge_priority_property;
	static const edge::temp_string_p root_bridge_id_property;
	static const edge::uint32_p      hello_time_property;
	static const edge::uint32_p      max_age_property;
	static const edge::uint32_p      forward_delay_property;
	static const edge::uint32_p      message_age_property;
	static const edge::uint32_p      remaining_hops_property;
	static const edge::property* const _properties[];
	static const edge::type_t _type;
	const edge::type_t* type() const override { return &_type; }
};

