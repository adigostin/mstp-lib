
#include "pch.h"
#include "PortTree.h"
#include "Port.h"
#include "Bridge.h"
#include "stp.h"

using namespace edge;

static const _bstr_t PortTreeString = "PortTree";
static const _bstr_t TreeIndexString = "TreeIndex";
static const _bstr_t PortPriorityString = "PortPriority";

HRESULT PortTree::Serialize (IXMLDOMDocument3* doc, com_ptr<IXMLDOMElement>& elementOut) const
{
	com_ptr<IXMLDOMElement> portTreeElement;
	auto hr = doc->createElement (PortTreeString, &portTreeElement); assert(SUCCEEDED(hr));
	portTreeElement->setAttribute (TreeIndexString, _variant_t(_treeIndex));
	portTreeElement->setAttribute (PortPriorityString, _variant_t(priority()));
	elementOut = std::move(portTreeElement);
	return S_OK;
}

HRESULT PortTree::Deserialize (IXMLDOMElement* portTreeElement)
{
	_variant_t value;
	auto hr = portTreeElement->getAttribute (PortPriorityString, &value);
	if (FAILED(hr))
		return hr;
	if (value.vt == VT_BSTR)
		set_priority(wcstol (value.bstrVal, nullptr, 10));

	return S_OK;
}

uint32_t PortTree::priority() const
{
	return STP_GetPortPriority (_port->bridge()->stp_bridge(), _port->GetPortIndex(), _treeIndex);
}

void PortTree::set_priority (uint32_t priority)
{
	if (this->priority() != priority)
	{
		this->on_property_changing(&priority_property);
		STP_SetPortPriority (_port->bridge()->stp_bridge(), _port->GetPortIndex(), _treeIndex, (unsigned char) priority, GetMessageTime());
		this->on_property_changed(&priority_property);
	}
}

bool PortTree::learning() const
{
	return STP_GetPortLearning(_port->bridge()->stp_bridge(), _port->GetPortIndex(), _treeIndex);
}

bool PortTree::forwarding() const
{
	return STP_GetPortForwarding(_port->bridge()->stp_bridge(), _port->GetPortIndex(), _treeIndex);
}

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

const char port_priority_type_name[] = "PortPriority";

const port_priority_p PortTree::priority_property
(
	"PortPriority",
	nullptr,
	"The value of the priority field which is contained in the first (in network byte order) octet of the (2 octet long) Port ID. "
		"The other octet of the Port ID is given by the value of dot1dStpPort.",
	static_cast<port_priority_p::member_getter_t>(&priority),
	static_cast<port_priority_p::member_setter_t>(&set_priority),
	0x80
);

const edge::bool_p PortTree::learning_property (
	"learning",
	nullptr,
	"",
	static_cast<edge::bool_p::member_getter_t>(&learning),
	nullptr,
	std::nullopt);

const edge::bool_p PortTree::forwarding_property (
	"forwarding",
	nullptr,
	"",
	static_cast<edge::bool_p::member_getter_t>(&forwarding),
	nullptr,
	std::nullopt);

const edge::property* const PortTree::_properties[] = { &priority_property, &learning_property, &forwarding_property };

const edge::type_t PortTree::_type = { "PortTree", &base::_type, _properties };
