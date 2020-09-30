
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "com_ptr.h"
#include "../object.h"

namespace edge
{
	com_ptr<IXMLDOMElement> serialize (IXMLDOMDocument* doc, const object* o, bool force_serialize_unchanged);
	void deserialize_to (IXMLDOMElement* element, object* o, std::span<const concrete_type* const> known_types);

	struct custom_serialize_property_i
	{
		virtual bool need_serialize (const object* obj) const = 0;
	};

	struct deserialize_i
	{
		virtual void on_deserializing() = 0;
		virtual void on_deserialized() = 0;
	};

	HRESULT format_and_save_to_file (IXMLDOMDocument3* doc, const wchar_t* file_path);
}
