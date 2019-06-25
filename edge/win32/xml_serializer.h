
#pragma once
#include "com_ptr.h"
#include "../object.h"

namespace edge
{
	com_ptr<IXMLDOMElement> serialize (IXMLDOMDocument* doc, const object* o, bool force_serialize_unchanged);
	void deserialize_to (IXMLDOMElement* element, object* o);

	struct deserialize_i
	{
		virtual void on_deserializing() = 0;
		virtual void on_deserialized() = 0;
	};
}
