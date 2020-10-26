
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "com_ptr.h"
#include "object.h"

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
		virtual void deserialize_to (IXMLDOMElement* element, object* o) = 0;
		virtual std::unique_ptr<object> deserialize (IXMLDOMElement* elem) = 0;
		virtual string_convert_context_i* context() const = 0;
	};

	std::unique_ptr<xml_deserializer_i> create_deserializer (std::span<const concrete_type* const> known_types, string_convert_context_i* context);

	// ========================================================================

	struct __declspec(novtable) custom_serialize_property_i
	{
		virtual void serialize (xml_serializer_i* serializer, const object* obj, const serialize_element_getter& element_getter) const = 0;
		virtual void deserialize (xml_deserializer_i* deserializer, IXMLDOMElement* element, object* obj) const = 0;
		virtual void deserialize (xml_deserializer_i* deserializer, std::string_view attr_value, object* obj) const = 0;
	};

	struct __declspec(novtable) custom_serialize_object_i
	{
		virtual void serialize_before_reflection (xml_serializer_i* serializer, const serialize_element_getter& element_getter) const { }
		virtual void serialize_after_reflection  (xml_serializer_i* serializer, const serialize_element_getter& element_getter) const { }

		virtual void deserialize_before_reflection (xml_deserializer_i* de, IXMLDOMElement* obj_element) { }
		virtual void deserialize_after_reflection (xml_deserializer_i* de, IXMLDOMElement* obj_element) { }

		virtual bool try_deserialize_xml_attribute (xml_deserializer_i* de, std::string_view name, std::string_view value) { return false; }
		virtual bool try_deserialize_xml_element (xml_deserializer_i* de, IXMLDOMElement* e) { return false; }
	};
}
