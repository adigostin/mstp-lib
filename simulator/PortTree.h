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
	
public:
	PortTree (Port* port, unsigned int treeIndex)
		: _port(port), _treeIndex(treeIndex)
	{ }

	HRESULT Serialize (IXMLDOMDocument3* doc, edge::com_ptr<IXMLDOMElement>& elementOut) const;
	HRESULT Deserialize (IXMLDOMElement* portTreeElement);

	uint32_t priority() const;
	void set_priority (uint32_t priority);

	bool learning() const;
	bool forwarding() const;

	static const port_priority_p priority_property;
	static const edge::bool_p learning_property;
	static const edge::bool_p forwarding_property;
	static const edge::property* const _properties[];
	static const edge::type_t _type;
	const edge::type_t* type() const override { return &_type; }
};
