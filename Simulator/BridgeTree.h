
#pragma once
#include "Object.h"

class Bridge;

struct BridgeTree : Object
{
	Bridge* const _parent;
	unsigned int const _treeIndex;

	BridgeTree (Bridge* parent, unsigned int treeIndex)
		: _parent(parent), _treeIndex(treeIndex)
	{ }

	HRESULT Serialize (IXMLDOMDocument3* doc, IXMLDOMElementPtr& elementOut) const;
	HRESULT Deserialize (IXMLDOMElement* bridgeTreeElement);

	//std::wstring GetPriorityLabel () const;
	int GetPriority() const;
	void SetPriority (int priority);

	std::array<unsigned char, 36> GetRootPV() const;
	std::wstring GetRootBridgeId() const;
	std::wstring GetExternalRootPathCost() const;
	std::wstring GetRegionalRootBridgeId() const;
	std::wstring GetInternalRootPathCost() const;
	std::wstring GetDesignatedBridgeId() const;
	std::wstring GetDesignatedPortId() const;
	std::wstring GetReceivingPortId() const;

	static const PropertyGroup Common;
	static const EnumProperty Priority;
	static const PropertyOrGroup* const Properties[];
	virtual const PropertyOrGroup* const* GetProperties() const override final { return Properties; }
};

