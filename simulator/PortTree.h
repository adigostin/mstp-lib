#pragma once
#include "object.h"
#include "win32/com_ptr.h"
#include "stp.h"

class Port;

using edge::uint32_p;
using edge::bool_p;
using edge::float_p;
using edge::backed_string_p;
using edge::temp_string_p;
using edge::property;
using edge::type;
using edge::xtype;

extern const edge::NVP port_priority_nvps[];
extern const char port_priority_type_name[];
using port_priority_p = edge::enum_property<uint32_t, port_priority_type_name, port_priority_nvps>;

extern const edge::NVP port_role_nvps[];
extern const char port_role_type_name[];
using port_role_p = edge::enum_property<STP_PORT_ROLE, port_role_type_name, port_role_nvps>;

inline bool compat_bool_from_string (std::string_view from, bool& to)
{
	if (from == "-1" || from == "1")
	{
		to = true;
		return true;
	}
	else if (from == "0")
	{
		to = false;
		return true;
	}
	else
		return edge::bool_from_string(from, to);
}
static constexpr char compat_bool_type_name[] = "compat_bool";
using compat_bool_p = edge::typed_property<bool, bool, bool, compat_bool_type_name, edge::bool_to_string, compat_bool_from_string>;

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
	STP_PORT_ROLE role() const;

	uint32_t tree_index() const { return _treeIndex; }

	static const uint32_p tree_index_property;
	static const port_priority_p priority_property;
	static const bool_p learning_property;
	static const bool_p forwarding_property;
	static const port_role_p role_property;
	static const property* const _properties[];
	static const xtype<PortTree> _type;
	const struct type* type() const;
};
