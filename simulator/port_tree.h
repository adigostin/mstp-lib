#pragma once
#include "object.h"
#include "win32/com_ptr.h"
#include "stp.h"

class port;

using edge::uint32_p;
using edge::bool_p;
using edge::float_p;
using edge::backed_string_p;
using edge::temp_string_p;
using edge::property;
using edge::type;
using edge::xtype;
using edge::typed_object_collection_property;

extern const edge::NVP port_priority_nvps[];
extern const char port_priority_type_name[];
using port_priority_p = edge::enum_property<uint32_t, port_priority_type_name, port_priority_nvps, true>;

extern const edge::NVP port_role_nvps[];
extern const char port_role_type_name[];
using port_role_p = edge::enum_property<STP_PORT_ROLE, port_role_type_name, port_role_nvps>;

class port_tree : public edge::object
{
	using base = edge::object;

	port* const _port;
	unsigned int const _treeIndex;
	
public:
	port_tree (port* port, unsigned int treeIndex)
		: _port(port), _treeIndex(treeIndex)
	{ }

	HRESULT Serialize (IXMLDOMDocument3* doc, edge::com_ptr<IXMLDOMElement>& elementOut) const;
	HRESULT Deserialize (IXMLDOMElement* portTreeElement);

	uint32_t priority() const;
	void set_priority (uint32_t priority);

	bool learning() const;
	bool forwarding() const;
	STP_PORT_ROLE role() const;

	uint32_t tree_index() const { return _treeIndex; }

	static const uint32_p tree_index_property;
	static const port_priority_p priority_property;
	static const bool_p learning_property;
	static const bool_p forwarding_property;
	static const port_role_p role_property;
	static const property* const _properties[];
	static const xtype<port_tree> _type;
	const struct type* type() const;
};
