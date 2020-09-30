
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "xml_serializer.h"
#include "utility_functions.h"
#include "../collections.h"

#pragma comment (lib, "msxml6.lib")

namespace edge
{
	static const _bstr_t entry_elem_name = "Entry";
	static const _bstr_t index_attr_name = "index";
	static const _bstr_t value_attr_name = "Value";

	static com_ptr<IXMLDOMElement> serialize_internal (IXMLDOMDocument* doc, const object* obj, bool force_serialize_unchanged, size_t index_attribute);
	static void deserialize_to_internal (IXMLDOMElement* element, object* obj, bool ignore_index_attribute, std::span<const concrete_type* const> known_types);

	static com_ptr<IXMLDOMElement> serialize_object_collection (IXMLDOMDocument* doc, const object* obj, const object_collection_property* prop)
	{
		auto collection = prop->collection_cast(obj);

		size_t child_count = collection->child_count();
		if (child_count == 0)
			return nullptr;

		HRESULT hr;
		com_ptr<IXMLDOMElement> collection_element;
		auto ensure_collection_element_created = [doc, prop, &collection_element]()
		{
			if (collection_element == nullptr)
			{
				auto hr = doc->createElement(_bstr_t(prop->_name), &collection_element);
				assert(SUCCEEDED(hr));
			}
		};

		bool force_serialize_unchanged;
		bool add_index_attribute;
		if (!prop->preallocated)
		{
			// variable-size collection - always serialize all children
			force_serialize_unchanged = true;
			add_index_attribute = false;
		}
		else
		{
			// fixed-size collection allocated by the object in its constructor - serialize only changed children
			force_serialize_unchanged = false;
			add_index_attribute = true;
		}

		for (size_t i = 0; i < child_count; i++)
		{
			auto child = collection->child_at(i);
			auto child_elem = serialize_internal(doc, child, force_serialize_unchanged, add_index_attribute ? i : -1);
			if (child_elem != nullptr)
			{
				ensure_collection_element_created();
				hr = collection_element->appendChild (child_elem, nullptr);  assert(SUCCEEDED(hr));
			}
		}

		return collection_element;
	}

	static com_ptr<IXMLDOMElement> serialize_value_collection (IXMLDOMDocument* doc, const object* obj, const value_collection_property* prop)
	{
		size_t size = prop->size(obj);
		if (size == 0)
			return nullptr;

		if (!prop->changed(obj))
			return nullptr;

		com_ptr<IXMLDOMElement> collection_element;
		auto hr = doc->createElement (_bstr_t(prop->_name), &collection_element); assert(SUCCEEDED(hr));
		std::string value;
		for (size_t i = 0; i < size; i++)
		{
			prop->get_value(obj, i, value);
			com_ptr<IXMLDOMElement> entry_element;
			hr = doc->createElement (entry_elem_name, &entry_element); assert(SUCCEEDED(hr));
			hr = entry_element->setAttribute (index_attr_name, _variant_t(i)); assert(SUCCEEDED(hr));
			hr = entry_element->setAttribute (value_attr_name, _variant_t(value.c_str())); assert(SUCCEEDED(hr));
			hr = collection_element->appendChild (entry_element, nullptr); assert(SUCCEEDED(hr));
		}

		return collection_element;
	}

	static com_ptr<IXMLDOMElement> serialize_internal (IXMLDOMDocument* doc, const object* obj, bool force_serialize_unchanged, size_t index_attribute)
	{
		com_ptr<IXMLDOMElement> object_element;
		auto ensure_object_element_created = [doc, obj, index_attribute, &object_element]()
		{
			if (object_element == nullptr)
			{
				const char* name = obj->type()->name();
				auto hr = doc->createElement(_bstr_t(name), &object_element);
				assert(SUCCEEDED(hr));

				if (index_attribute != (size_t)-1)
				{
					hr = object_element->setAttribute (index_attr_name, _variant_t(index_attribute));
					assert(SUCCEEDED(hr));
				}
			}
		};

		if (force_serialize_unchanged)
			ensure_object_element_created();

		auto props = obj->type()->make_property_list();
		for (const property* prop : props)
		{
			if (auto cs = dynamic_cast<const custom_serialize_property_i*>(prop); cs && !cs->need_serialize(obj))
				continue;

			if (auto value_prop = dynamic_cast<const value_property*>(prop))
			{
				bool is_factory_prop = std::any_of (obj->type()->factory_props().begin(), obj->type()->factory_props().end(), [value_prop](auto p) { return p == value_prop; });
				if (is_factory_prop || (value_prop->can_set(obj) && value_prop->changed_from_default(obj)))
				{
					ensure_object_element_created();
					auto value = value_prop->get_to_string(obj);
					auto hr = object_element->setAttribute(_bstr_t(value_prop->_name), _variant_t(value.c_str())); assert(SUCCEEDED(hr));
				}
			}
		}

		for (const property* prop : props)
		{
			if (auto cs = dynamic_cast<const custom_serialize_property_i*>(prop); cs && !cs->need_serialize(obj))
				continue;

			if (dynamic_cast<const value_property*>(prop))
				continue;

			com_ptr<IXMLDOMElement> prop_element;
			if (auto oc_prop = dynamic_cast<const object_collection_property*>(prop))
			{
				prop_element = serialize_object_collection (doc, obj, oc_prop);
			}
			else if (auto vc_prop = dynamic_cast<const value_collection_property*>(prop))
			{
				prop_element = serialize_value_collection (doc, obj, vc_prop);
			}
			else if (auto obj_prop = dynamic_cast<const object_property*>(prop))
			{
				if (auto value = obj_prop->get(obj))
					prop_element = serialize_internal (doc, value, false, -1);
			}
			else
				assert(false); // not implemented

			if (prop_element)
			{
				ensure_object_element_created();
				auto hr = object_element->appendChild (prop_element, nullptr); assert(SUCCEEDED(hr));
			}
		}

		return object_element;
	}

	com_ptr<IXMLDOMElement> serialize (IXMLDOMDocument* doc, const object* obj, bool force_serialize_unchanged)
	{
		return serialize_internal (doc, obj, force_serialize_unchanged, -1);
	}

	// ========================================================================

	static std::unique_ptr<object> create_object (IXMLDOMElement* elem, const concrete_type* type)
	{
		std::vector<std::string> factory_param_strings;
		for (auto factory_prop : type->factory_props())
		{
			_variant_t value;
			elem->getAttribute(_bstr_t(factory_prop->_name), value.GetAddress());
			assert (value.vt == VT_BSTR);
			factory_param_strings.push_back(bstr_to_utf8(value.bstrVal));
		}

		std::vector<std::string_view> factory_params;
		for (auto& str : factory_param_strings)
			factory_params.push_back(str);
		auto obj = type->create({ factory_params.data(), factory_params.data() + factory_params.size() });
		return obj;
	}

	static std::unique_ptr<object> deserialize (IXMLDOMElement* elem, std::span<const concrete_type* const> known_types)
	{
		_bstr_t namebstr;
		auto hr = elem->get_nodeName(namebstr.GetAddress()); assert(SUCCEEDED(hr));
		auto name = bstr_to_utf8(namebstr);
		auto it = std::find_if(known_types.begin(), known_types.end(), [&name](const type* t) { return (t->name() == name) || strcmp(t->name(), name.c_str()) == 0; });
		if (it == known_types.end())
			assert(false); // error handling for this not implemented
		auto obj = create_object(elem, *it);
		deserialize_to_internal (elem, obj.get(), false, known_types);
		return obj;
	}

	static void deserialize_to_new_object_collection (IXMLDOMElement* collection_elem, object* obj, const object_collection_property* prop, std::span<const concrete_type* const> known_types)
	{
		auto collection = prop->collection_cast(obj);

		com_ptr<IXMLDOMNode> child_node;
		auto hr = collection_elem->get_firstChild(&child_node); assert(SUCCEEDED(hr));
		while (child_node != nullptr)
		{
			com_ptr<IXMLDOMElement> child_elem = child_node;

			_bstr_t namebstr;
			auto hr = child_elem->get_nodeName(namebstr.GetAddress()); assert(SUCCEEDED(hr));
			auto name = bstr_to_utf8(namebstr);
			auto it = std::find_if(known_types.begin(), known_types.end(), [&name](const type* t) { return (t->name() == name) || strcmp(t->name(), name.c_str()) == 0; });
			if (it == known_types.end())
				assert(false); // error handling for this not implemented
			auto child = create_object(child_elem, *it);
			auto child_raw = child.get();
			collection->append(std::move(child));
			deserialize_to_internal (child_elem, child_raw, false, known_types);
			hr = child_node->get_nextSibling(&child_node); assert(SUCCEEDED(hr));
		}
	}

	static void deserialize_to_existing_object_collection (IXMLDOMElement* collection_elem, object* obj, const object_collection_property* prop, std::span<const concrete_type* const> known_types)
	{
		auto collection = prop->collection_cast(obj);

		size_t child_node_index = 0;
		com_ptr<IXMLDOMNode> child_node;
		auto hr = collection_elem->get_firstChild(&child_node); assert(SUCCEEDED(hr));
		while (child_node != nullptr)
		{
			com_ptr<IXMLDOMElement> child_elem = child_node;
			size_t index = child_node_index;
			_variant_t index_attr_value;
			hr = child_elem->getAttribute(index_attr_name, index_attr_value.GetAddress());
			if (hr == S_OK)
				size_t_property_traits::from_string(bstr_to_utf8(index_attr_value.bstrVal), index);

			auto child = collection->child_at(index);
			deserialize_to_internal (child_elem, child, true, known_types);

			child_node_index++;
			hr = child_node->get_nextSibling(&child_node); assert(SUCCEEDED(hr));
		}
	}

	static void deserialize_value_collection (IXMLDOMElement* collection_elem, object* obj, const value_collection_property* prop)
	{
		com_ptr<IXMLDOMNode> entry_node;
		auto hr = collection_elem->get_firstChild(&entry_node); assert(SUCCEEDED(hr));
		while (entry_node != nullptr)
		{
			com_ptr<IXMLDOMElement> entry_elem = entry_node;
			_variant_t index_attr_value;
			hr = entry_elem->getAttribute(index_attr_name, index_attr_value.GetAddress()); assert(SUCCEEDED(hr));
			size_t index;
			size_t_property_traits::from_string(bstr_to_utf8(index_attr_value.bstrVal), index);

			_variant_t value_attr_value;
			hr = entry_elem->getAttribute(value_attr_name, value_attr_value.GetAddress()); assert(SUCCEEDED(hr));

			auto value_utf8 = bstr_to_utf8(value_attr_value.bstrVal);
			if (prop->can_insert_remove())
				prop->insert_value (value_utf8, obj, index);
			else
				prop->set_value (value_utf8, obj, index);

			hr = entry_node->get_nextSibling(&entry_node); assert(SUCCEEDED(hr));
		}
	}

	static void deserialize_to_internal (IXMLDOMElement* element, object* obj, bool ignore_index_attribute, std::span<const concrete_type* const> known_types)
	{
		auto deserializable = dynamic_cast<deserialize_i*>(obj);
		if (deserializable != nullptr)
			deserializable->on_deserializing();

		com_ptr<IXMLDOMNamedNodeMap> attrs;
		auto hr = element->get_attributes (&attrs); assert(SUCCEEDED(hr));
		long attr_count;
		hr = attrs->get_length(&attr_count); assert(SUCCEEDED(hr));
		for (long i = 0; i < attr_count; i++)
		{
			com_ptr<IXMLDOMNode> attr_node;
			hr = attrs->get_item(i, &attr_node); assert(SUCCEEDED(hr));
			com_ptr<IXMLDOMAttribute> attr = attr_node;
			_bstr_t namebstr;
			hr = attr->get_name(namebstr.GetAddress()); assert(SUCCEEDED(hr));

			if (ignore_index_attribute && (namebstr == index_attr_name))
				continue;

			auto name = bstr_to_utf8(namebstr);

			bool is_factory_property = std::any_of (obj->type()->factory_props().begin(),
				obj->type()->factory_props().end(),
				[&name](const value_property* fp) { return strcmp(name.c_str(), fp->_name) == 0; });
			if (is_factory_property)
				continue;

			_bstr_t valuebstr;
			hr = attr->get_text(valuebstr.GetAddress()); assert(SUCCEEDED(hr));
			auto value = bstr_to_utf8(valuebstr);

			auto prop = obj->type()->find_property(name.c_str());
			if (prop == nullptr)
				assert(false); // error handling for this not implemented
			auto value_prop = dynamic_cast<const value_property*>(prop);
			if (value_prop == nullptr)
				assert(false); // error handling for this not implemented
			value_prop->set_from_string(value, obj);
		}

		com_ptr<IXMLDOMNode> child_node;
		hr = element->get_firstChild(&child_node); assert(SUCCEEDED(hr));
		while (child_node != nullptr)
		{
			com_ptr<IXMLDOMElement> child_elem = child_node;
			_bstr_t namebstr;
			hr = child_elem->get_nodeName(namebstr.GetAddress()); assert(SUCCEEDED(hr));
			auto name = bstr_to_utf8(namebstr);

			auto prop = obj->type()->find_property(name.c_str());
			if (prop == nullptr)
				assert(false); // error handling for this not implemented

			if (auto vc_prop = dynamic_cast<const value_collection_property*>(prop))
			{
				deserialize_value_collection (child_elem, obj, vc_prop);
			}
			else if (auto oc_prop = dynamic_cast<const object_collection_property*>(prop))
			{
				if (!oc_prop->preallocated)
					deserialize_to_new_object_collection (child_elem, obj, oc_prop, known_types);
				else
					deserialize_to_existing_object_collection (child_elem, obj, oc_prop, known_types);
			}
			else
				assert(false); // error handling for this not implemented

			hr = child_node->get_nextSibling(&child_node); assert(SUCCEEDED(hr));
		}

		if (deserializable != nullptr)
			deserializable->on_deserialized();
	}

	void deserialize_to (IXMLDOMElement* element, object* obj, std::span<const concrete_type* const> known_types)
	{
		return deserialize_to_internal (element, obj, false, known_types);
	}

	HRESULT format_and_save_to_file (IXMLDOMDocument3* doc, const wchar_t* file_path)
	{
		/*
		static const char StylesheetText[] =
			"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
			"<xsl:stylesheet xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\" version=\"1.0\">\n"
			"  <xsl:output method=\"xml\" indent=\"yes\" omit-xml-declaration=\"no\" />\n"
			"  <xsl:template match=\"@* | node()\">\n"
			"    <xsl:copy>\n"
			"      <xsl:apply-templates select=\"@* | node()\"/>\n"
			"    </xsl:copy>\n"
			"  </xsl:template>\n"
			"</xsl:stylesheet>\n"
			"";

		edge::com_ptr<IXMLDOMDocument3> loadXML;
		HRESULT hr = CoCreateInstance (CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, __uuidof(loadXML), (void**) &loadXML);
		if (FAILED(hr))
			return hr;
		VARIANT_BOOL successful;
		hr = loadXML->loadXML (_bstr_t(StylesheetText), &successful);
		if (FAILED(hr))
			return hr;

		// Create the final document which will be indented properly.
		edge::com_ptr<IXMLDOMDocument3> pXMLFormattedDoc;
		hr = CoCreateInstance(CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, __uuidof(pXMLFormattedDoc), (void**) &pXMLFormattedDoc);
		if (FAILED(hr))
			return hr;

		edge::com_ptr<IDispatch> pDispatch;
		hr = pXMLFormattedDoc->QueryInterface(IID_IDispatch, (void**)&pDispatch);
		if (FAILED(hr))
			return hr;

		_variant_t vtOutObject;
		vtOutObject.vt = VT_DISPATCH;
		vtOutObject.pdispVal = pDispatch;
		vtOutObject.pdispVal->AddRef();

		// Apply the transformation to format the final document.
		hr = doc->transformNodeToObject(loadXML,vtOutObject);
		if (FAILED(hr))
			return hr;

		// By default it writes the encoding UTF-16; let's change it to UTF-8.
		edge::com_ptr<IXMLDOMNode> firstChild;
		hr = pXMLFormattedDoc->get_firstChild(&firstChild);
		if (FAILED(hr))
			return hr;
		edge::com_ptr<IXMLDOMNamedNodeMap> pXMLAttributeMap;
		hr = firstChild->get_attributes(&pXMLAttributeMap);
		if (FAILED(hr))
			return hr;
		edge::com_ptr<IXMLDOMNode> encodingNode;
		hr = pXMLAttributeMap->getNamedItem(_bstr_t("encoding"), &encodingNode);
		if (FAILED(hr))
			return hr;
		encodingNode->put_nodeValue (_variant_t("UTF-8"));

		assert (!::PathIsRelative(file_path));
		const wchar_t* file_name = ::PathFindFileName(file_path);
		std::wstring dir (file_path, file_name);
		if (!::PathFileExists(dir.c_str()))
		{
			if (!CreateDirectory(dir.c_str(), nullptr))
				return HRESULT_FROM_WIN32(GetLastError());
		}
		hr = pXMLFormattedDoc->save(_variant_t(file_path));
		return hr;
		*/
		com_ptr<IStream> stream;
		auto hr = SHCreateStreamOnFileEx (file_path, STGM_WRITE | STGM_SHARE_DENY_WRITE | STGM_CREATE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream);
		if (FAILED(hr))
			return hr;

		com_ptr<IMXWriter> writer;
		hr = CoCreateInstance (CLSID_MXXMLWriter60, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&writer));
		if (FAILED(hr))
			return hr;

		hr = writer->put_encoding (_bstr_t("utf-8"));
		if (FAILED(hr))
			return hr;

		hr = writer->put_indent (_variant_t(true));
		if (FAILED(hr))
			return hr;

		hr = writer->put_standalone (_variant_t(true));
		if (FAILED(hr))
			return hr;

		hr = writer->put_output (_variant_t(stream));
		if (FAILED(hr))
			return hr;

		com_ptr<ISAXXMLReader> saxReader;
		hr = CoCreateInstance (CLSID_SAXXMLReader60, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&saxReader));
		if (FAILED(hr))
			return hr;

		hr = saxReader->putContentHandler(com_ptr<ISAXContentHandler>(writer));
		if (FAILED(hr))
			return hr;

		hr = saxReader->putProperty(L"http://xml.org/sax/properties/lexical-handler", _variant_t(writer));
		if (FAILED(hr))
			return hr;

		hr = saxReader->parse(_variant_t(doc));
		if (FAILED(hr))
			return hr;

		hr = stream->Commit(STGC_DEFAULT);
		return hr;
	}
}
