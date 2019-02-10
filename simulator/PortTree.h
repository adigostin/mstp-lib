#pragma once
#include "object.h"
#include "win32/com_ptr.h"

class Port;

extern const edge::NVP port_priority_nvps[];
extern const char port_priority_type_name[];
using port_priority_p = edge::enum_property<uint32_t, port_priority_type_name, port_priority_nvps>;

class PortTree : public edge::object
{
	using base = edge::object;

	Port* const _port;
	unsigned int const _treeIndex;
	
	SYSTEMTIME _last_topology_change;
	uint32_t _topology_change_count = 0;

	friend class Bridge;

	void on_topology_change (unsigned int timestamp);

public:
	PortTree (Port* port, unsigned int treeIndex)
		: _port(port), _treeIndex(treeIndex)
	{
		::GetSystemTime(&_last_topology_change);
	}

	HRESULT Serialize (IXMLDOMDocument3* doc, edge::com_ptr<IXMLDOMElement>& elementOut) const;
	HRESULT Deserialize (IXMLDOMElement* portTreeElement);

	uint32_t priority() const;
	void set_priority (uint32_t priority);

	bool learning() const;
	bool forwarding() const;
	uint32_t topology_change_count() const { return _topology_change_count; }

	static const port_priority_p priority_property;
	static const edge::bool_p learning_property;
	static const edge::bool_p forwarding_property;
	static const edge::uint32_p topology_change_count_property;
	static const edge::property* const _properties[];
	static const edge::type_t _type;
	const edge::type_t* type() const override { return &_type; }
};
