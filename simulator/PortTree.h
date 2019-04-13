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

struct compat_bool_property_traits
{
	static inline const char type_name[] = "compat_bool";
	using value_t = bool;
	using param_t = bool;
	using return_t = bool;
	static std::string to_string (bool from) { return edge::bool_property_traits::to_string(from); }
	static bool from_string (std::string_view from, bool& to)
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
			return edge::bool_property_traits::from_string(from, to);
	}
};
using compat_bool_p = edge::typed_property<compat_bool_property_traits>;

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
