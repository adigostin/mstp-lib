
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "com_ptr.h"
#include "om/object.h"

namespace edge
{
	using serialize_element_getter = std::function<IXMLDOMElement*()>;

	struct __declspec(novtable) xml_serializer_i
	{
		virtual ~xml_serializer_i() = default;
		virtual IXMLDOMDocument* doc() const = 0;
		virtual string_convert_context_i* context() const = 0;
		virtual com_ptr<IXMLDOMElement> serialize_object (const object* obj, bool force_serialize_unchanged) = 0;
		virtual void serialize_property (const object* obj, const property* prop, const serialize_element_getter& object_element_getter) = 0;
	};

	std::unique_ptr<xml_serializer_i> create_serializer (IXMLDOMDocument* doc, string_convert_context_i* context);

	HRESULT format_and_save_to_file (IXMLDOMDocument3* doc, const wchar_t* file_path);

	// ========================================================================

	struct __declspec(novtable) xml_deserializer_i
	{
		virtual ~xml_deserializer_i() = default;
		virtual void deserialize_object (IXMLDOMElement* from, object* to) = 0;
		virtual std::unique_ptr<object> deserialize_object (IXMLDOMElement* from) = 0;
		virtual void deserialize_property (std::string_view attr_value, object* obj, const property* prop) = 0;
		virtual void deserialize_property (IXMLDOMElement* elem, object* obj, const property* prop) = 0;
		virtual string_convert_context_i* context() const = 0;
		virtual std::unique_ptr<object> deserialize_file (const wchar_t* file_path, const concrete_type* expected_root_element_type) = 0;
	};

	std::unique_ptr<xml_deserializer_i> create_deserializer (std::span<const concrete_type* const> known_types, string_convert_context_i* context);

	// ========================================================================

	struct __declspec(novtable) custom_serialize_property_i
	{
		virtual void serialize (xml_serializer_i* serializer, const object* obj, const property* prop, const serialize_element_getter& element_getter) const = 0;
		virtual void deserialize (xml_deserializer_i* deserializer, IXMLDOMElement* element, object* obj, const property* prop) const = 0;
		virtual void deserialize (xml_deserializer_i* deserializer, std::string_view attr_value, object* obj, const property* prop) const = 0;
	};

	struct __declspec(novtable) custom_serialize_object_i
	{
		virtual void sort_xml_properties (std::vector<const property*>& props) const = 0;
		virtual void on_deserializing (xml_deserializer_i* de) = 0;
		virtual void on_deserialized (xml_deserializer_i* de) = 0;
	};
}
