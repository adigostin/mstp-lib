
#include "pch.h"
#include "PortTree.h"
#include "Port.h"
#include "Bridge.h"
#include "stp.h"

static const _bstr_t PortTreeString = "PortTree";
static const _bstr_t TreeIndexString = "TreeIndex";
static const _bstr_t PortPriorityString = "PortPriority";

HRESULT PortTree::Serialize (IXMLDOMDocument3* doc, IXMLDOMElementPtr& elementOut) const
{
	IXMLDOMElementPtr portTreeElement;
	auto hr = doc->createElement (PortTreeString, &portTreeElement); assert(SUCCEEDED(hr));
	portTreeElement->setAttribute (TreeIndexString, _variant_t(_treeIndex));
	portTreeElement->setAttribute (PortPriorityString, _variant_t(GetPriority()));
	elementOut = portTreeElement;
	return S_OK;
}

HRESULT PortTree::Deserialize (IXMLDOMElement* portTreeElement)
{
	_variant_t value;
	auto hr = portTreeElement->getAttribute (PortPriorityString, &value);
	if (FAILED(hr))
		return hr;
	if (value.vt == VT_BSTR)
		SetPriority (wcstol (value.bstrVal, nullptr, 10));

	return S_OK;
}

int PortTree::GetPriority() const
{
	return STP_GetPortPriority (_parent->GetBridge()->GetStpBridge(), _parent->GetPortIndex(), _treeIndex);
}

void PortTree::SetPriority (int priority)
{
	STP_SetPortPriority (_parent->GetBridge()->GetStpBridge(), _parent->GetPortIndex(), _treeIndex, (unsigned char) priority, GetMessageTime());
}

static const NVP PortPrioNVPs[] =
{
	{ L"10 (16 dec)",  0x10 },
	{ L"20 (32 dec)",  0x20 },
	{ L"30 (48 dec)",  0x30 },
	{ L"40 (64 dec)",  0x40 },
	{ L"50 (80 dec)",  0x50 },
	{ L"60 (96 dec)",  0x60 },
	{ L"70 (112 dec)", 0x70 },
	{ L"80 (128 dec)", 0x80 },
	{ L"90 (144 dec)", 0x90 },
	{ L"A0 (160 dec)", 0xA0 },
	{ L"B0 (176 dec)", 0xB0 },
	{ L"C0 (192 dec)", 0xC0 },
	{ L"D0 (208 dec)", 0xD0 },
	{ L"E0 (224 dec)", 0xE0 },
	{ L"F0 (240 dec)", 0xF0 },
	{ nullptr, 0 },
};

const EnumProperty PortTree::Priority
(
	L"Bridge Priority",
	nullptr,//[](const std::vector<Object*>& objs) -> wstring
	static_cast<EnumProperty::Getter>(&GetPriority),
	static_cast<EnumProperty::Setter>(&SetPriority),
	PortPrioNVPs
);

const PropertyOrGroup* const PortTree::Properties[] =
{
	&Priority,
	nullptr
};
