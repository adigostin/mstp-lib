#pragma once
#include "Object.h"

class Port;

class PortTree : public Object
{
	using base = Object;

	Port* const _port;
	unsigned int const _treeIndex;

public:
	PortTree (Port* port, unsigned int treeIndex)
		: _port(port), _treeIndex(treeIndex)
	{ }

	HRESULT Serialize (IXMLDOMDocument3* doc, IXMLDOMElementPtr& elementOut) const;
	HRESULT Deserialize (IXMLDOMElement* portTreeElement);

	//std::wstring GetPriorityLabel () const;
	int GetPriority() const;
	void SetPriority (int priority);

	static const EnumProperty Priority;
	static const PropertyOrGroup* const Properties[];
	virtual const PropertyOrGroup* const* GetProperties() const override final { return Properties; }
};
