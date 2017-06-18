
#pragma once
#include "Reflection/Object.h"

class Bridge;

struct BridgeTree : Object
{
	Bridge* const _parent;
	unsigned int const _treeIndex;

	BridgeTree (Bridge* parent, unsigned int treeIndex)
		: _parent(parent), _treeIndex(treeIndex)
	{ }

	//std::wstring GetPriorityLabel () const;
	int GetPriority() const;
	void SetPriority (int priority, unsigned int timestamp);

	std::array<unsigned char, 36> GetRootPV() const;
	std::wstring GetRootBridgeId() const;
	std::wstring GetExternalRootPathCost() const;
	std::wstring GetRegionalRootBridgeId() const;
	std::wstring GetInternalRootPathCost() const;
	std::wstring GetDesignatedBridgeId() const;
	std::wstring GetDesignatedPortId() const;
	std::wstring GetReceivingPortId() const;

	virtual const PropertyOrGroup* const* GetProperties() const override final;
};

