
#pragma once
#include "com_ptr.h"
#include "../object.h"

namespace edge
{
	com_ptr<IXMLDOMElement> serialize (IXMLDOMDocument* doc, const object* o, bool force_serialize_unchanged);
	std::unique_ptr<object> deserialize (IXMLDOMElement* elem);
	void deserialize_to (IXMLDOMElement* element, object* o);
}
