
#include "pch.h"
#include "BridgeTree.h"
#include "Bridge.h"

using namespace std;

static const _bstr_t BridgeTreeString = "BridgeTree";
static const _bstr_t TreeIndexString = "TreeIndex";
static const _bstr_t BridgePriorityString = "BridgePriority";

IXMLDOMElementPtr BridgeTree::Serialize (IXMLDOMDocument3* doc) const
{
	IXMLDOMElementPtr bridgeTreeElement;
	auto hr = doc->createElement (BridgeTreeString, &bridgeTreeElement); assert(SUCCEEDED(hr));
	bridgeTreeElement->setAttribute (TreeIndexString, _variant_t(_treeIndex));
	bridgeTreeElement->setAttribute (BridgePriorityString, _variant_t(GetPriority()));
	return bridgeTreeElement;
}

HRESULT BridgeTree::Deserialize (IXMLDOMElement* portElement)
{
	_variant_t value;
	auto hr = portElement->getAttribute (BridgePriorityString, &value);
	if (FAILED(hr))
		return hr;
	if (value.vt == VT_BSTR)
		SetPriority (wcstol (value.bstrVal, nullptr, 10));
	
	return S_OK;
}

//std::wstring BridgeTree::GetPriorityLabel() const
//{
//	auto treeIndex = dynamic_cast<BridgeTree*>(objs[0])->_treeIndex;
//	bool allSameTree = all_of (objs.begin(), objs.end(), [treeIndex](Object* o) { return dynamic_cast<BridgeTree*>(o)->_treeIndex == treeIndex; });
//	if (!allSameTree)
//		return wstring(L"Bridge Priority");
//
//	wstringstream label;
//	label << L"Bridge Priority (";
//	if (treeIndex == 0)
//		label << L"CIST)";
//	else
//		label << L"MSTI " << treeIndex << L")";
//	return label.str();
//}

int BridgeTree::GetPriority() const
{
	return STP_GetBridgePriority (_parent->GetStpBridge(), _treeIndex);
}

void BridgeTree::SetPriority (int priority)
{
	STP_SetBridgePriority (_parent->GetStpBridge(), _treeIndex, (unsigned short) priority, GetMessageTime());
}

static constexpr wchar_t StpDisabledString[] = L"(STP disabled)";

array<unsigned char, 36> BridgeTree::GetRootPV() const
{
	array<unsigned char, 36> prioVector;
	STP_GetRootPriorityVector(_parent->GetStpBridge(), _treeIndex, prioVector.data());
	return prioVector;
}

std::wstring BridgeTree::GetRootBridgeId() const
{
	if (!STP_IsBridgeStarted (_parent->GetStpBridge()))
		return StpDisabledString;

	auto rpv = GetRootPV();
	wstringstream ss;
	ss << uppercase << setfill(L'0') << hex
		<< setw(2) << rpv[0] << setw(2) << rpv[1] << "."
		<< setw(2) << rpv[2] << setw(2) << rpv[3] << setw(2) << rpv[4]
		<< setw(2) << rpv[5] << setw(2) << rpv[6] << setw(2) << rpv[7];
	return ss.str();
}

std::wstring BridgeTree::GetExternalRootPathCost() const
{
	if (!STP_IsBridgeStarted(_parent->GetStpBridge()))
		return StpDisabledString;

	auto rpv = GetRootPV();
	auto cost = ((uint32_t) rpv[8] << 24) | ((uint32_t) rpv[9] << 16) | ((uint32_t) rpv[10] << 8) | rpv[11];
	return to_wstring (cost);
}

std::wstring BridgeTree::GetRegionalRootBridgeId() const
{
	if (!STP_IsBridgeStarted (_parent->GetStpBridge()))
		return StpDisabledString;

	auto rpv = GetRootPV();
	wstringstream ss;
	ss << uppercase << setfill(L'0') << hex
		<< setw(2) << rpv[12] << setw(2) << rpv[13] << "."
		<< setw(2) << rpv[14] << setw(2) << rpv[15] << setw(2) << rpv[16]
		<< setw(2) << rpv[17] << setw(2) << rpv[18] << setw(2) << rpv[19];
	return ss.str();
}

std::wstring BridgeTree::GetInternalRootPathCost() const
{
	if (!STP_IsBridgeStarted(_parent->GetStpBridge()))
		return StpDisabledString;

	auto rpv = GetRootPV();
	auto cost = ((uint32_t) rpv[20] << 24) | ((uint32_t) rpv[21] << 16) | ((uint32_t) rpv[22] << 8) | rpv[23];
	return to_wstring(cost);
}

std::wstring BridgeTree::GetDesignatedBridgeId() const
{
	if (!STP_IsBridgeStarted(_parent->GetStpBridge()))
		return StpDisabledString;

	auto rpv = GetRootPV();
	wstringstream ss;
	ss << uppercase << setfill(L'0') << hex
		<< setw(2) << rpv[24] << setw(2) << rpv[25] << "."
		<< setw(2) << rpv[26] << setw(2) << rpv[27] << setw(2) << rpv[28]
		<< setw(2) << rpv[29] << setw(2) << rpv[30] << setw(2) << rpv[31];
	return ss.str();
}

std::wstring BridgeTree::GetDesignatedPortId() const
{
	if (!STP_IsBridgeStarted(_parent->GetStpBridge()))
		return StpDisabledString;

	auto rpv = GetRootPV();
	wstringstream ss;
	ss << uppercase << setfill(L'0') << hex << setw(2) << rpv[32] << setw(2) << rpv[33];
	return ss.str();
}

std::wstring BridgeTree::GetReceivingPortId() const
{
	if (!STP_IsBridgeStarted(_parent->GetStpBridge()))
		return StpDisabledString;

	auto rpv = GetRootPV();
	wstringstream ss;
	ss << uppercase << setfill(L'0') << hex << setw(2) << rpv[34] << setw(2) << rpv[35];
	return ss.str();
}

// ============================================================================

static const PropertyGroup CommonPropGroup
{
	L"Common",
	nullptr,
};

static const NVP BridgePrioNVPs[] =
{
	{ L"1000 (4096 dec)", 0x1000 },
	{ L"2000 (8192 dec)", 0x2000 },
	{ L"3000 (12288 dec)", 0x3000 },
	{ L"4000 (16384 dec)", 0x4000 },
	{ L"5000 (20480 dec)", 0x5000 },
	{ L"6000 (24576 dec)", 0x6000 },
	{ L"7000 (28672 dec)", 0x7000 },
	{ L"8000 (32768 dec)", 0x8000 },
	{ L"9000 (36864 dec)", 0x9000 },
	{ L"A000 (40960 dec)", 0xA000 },
	{ L"B000 (45056 dec)", 0xB000 },
	{ L"C000 (49152 dec)", 0xC000 },
	{ L"D000 (53248 dec)", 0xD000 },
	{ L"E000 (57344 dec)", 0xE000 },
	{ L"F000 (61440 dec)", 0xF000 },
	{ nullptr, 0 },
};

static const EnumProperty PriorityProperty
(
	L"Bridge Priority",
	nullptr,//[](const std::vector<Object*>& objs) -> wstring
	static_cast<EnumProperty::Getter>(&BridgeTree::GetPriority),
	static_cast<EnumProperty::Setter>(&BridgeTree::SetPriority),
	BridgePrioNVPs
);

static const PropertyGroup RootPriorityVectorPropGroup
{
	L"Root Priority Vector",
	nullptr,
};

static const TypedProperty<wstring> RootBridgeId
(
	L"Root Bridge ID",
	nullptr,
	static_cast<TypedProperty<wstring>::Getter>(&BridgeTree::GetRootBridgeId),
	nullptr
);

static const TypedProperty<wstring> ExternalRootPathCost
(
	L"External Root Path Cost",
	nullptr,
	static_cast<TypedProperty<wstring>::Getter>(&BridgeTree::GetExternalRootPathCost),
	nullptr
);

static const TypedProperty<wstring> RegionalRootBridgeId
(
	L"Regional Root Bridge Id",
	nullptr,
	static_cast<TypedProperty<wstring>::Getter>(&BridgeTree::GetRegionalRootBridgeId),
	nullptr
);

static const TypedProperty<wstring> InternalRootPathCost
(
	L"Internal Root Path Cost",
	nullptr,
	static_cast<TypedProperty<wstring>::Getter>(&BridgeTree::GetInternalRootPathCost),
	nullptr
);

static const TypedProperty<wstring> DesignatedBridgeId
(
	L"Designated Bridge Id",
	nullptr,
	static_cast<TypedProperty<wstring>::Getter>(&BridgeTree::GetDesignatedBridgeId),
	nullptr
);

static const TypedProperty<wstring> DesignatedPortId
(
	L"Designated Port Id",
	nullptr,
	static_cast<TypedProperty<wstring>::Getter>(&BridgeTree::GetDesignatedPortId),
	nullptr
);

static const TypedProperty<wstring> ReceivingPortId
(
	L"Receiving Port Id",
	nullptr,
	static_cast<TypedProperty<wstring>::Getter>(&BridgeTree::GetReceivingPortId),
	nullptr
);

static const PropertyOrGroup* const BridgeTreeProperties[] =
{
	&CommonPropGroup,
	&PriorityProperty,
	&RootPriorityVectorPropGroup,
	&RootBridgeId,
	&ExternalRootPathCost,
	&RegionalRootBridgeId,
	&InternalRootPathCost,
	&DesignatedBridgeId,
	&DesignatedPortId,
	&ReceivingPortId,
	nullptr,
};

const PropertyOrGroup* const* BridgeTree::GetProperties() const
{
	return BridgeTreeProperties;
}

