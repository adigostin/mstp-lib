
#pragma once
#include "com_ptr.h"
#include "../object.h"

namespace edge
{
	com_ptr<IXMLDOMElement> serialize(IXMLDOMDocument* doc, const object* o);
	std::unique_ptr<object> deserialize (IXMLDOMElement* elem, object* parent);
	void deserialize_to (IXMLDOMElement* element, object* parent, object* o);
}
