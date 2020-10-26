
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "xml_serializer.h"
#include "utility_functions.h"
#include "collections.h"

#pragma comment (lib, "msxml6.lib")

namespace edge
{
	static const _bstr_t entry_elem_name = "Entry";
	static const _bstr_t index_attr_name = "index";
	static const _bstr_t value_attr_name = "Value";

	class serializer : public xml_serializer_i
	{
		com_ptr<IXMLDOMDocument> const _doc;
		string_convert_context_i* const _context;

	public:
		serializer(IXMLDOMDocument* doc, string_convert_context_i* context)
			: _doc(doc), _context(context)
		{ }

		virtual IXMLDOMDocument* doc() const override final { return _doc; }

		virtual string_convert_context_i* context() const override final { return _context; }

		com_ptr<IXMLDOMElement> serialize_object_collection (const object* obj, const object_collection_property* prop)
		{
			auto collection = prop->collection_cast(obj);

			size_t child_count = collection->child_count();
			if (child_count == 0)
				return nullptr;

			HRESULT hr;
			com_ptr<IXMLDOMElement> collection_element;
			auto ensure_collection_element_created = [doc=_doc.get(), prop, &collection_element]()
			{
				if (collection_element == nullptr)
				{
					auto hr = doc->createElement(_bstr_t(prop->name), &collection_element);
					rassert(SUCCEEDED(hr));
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
				auto child_elem = serialize_object (child, force_serialize_unchanged);
				if (child_elem != nullptr)
				{
					if (add_index_attribute)
					{
						hr = child_elem->setAttribute (index_attr_name, _variant_t((uint32_t)i));
						throw_if_failed(hr);
					}

					ensure_collection_element_created();
					hr = collection_element->appendChild (child_elem, nullptr);  rassert(SUCCEEDED(hr));
				}
			}

			return collection_element;
		}

		com_ptr<IXMLDOMElement> serialize_value_collection (const object* obj, const value_collection_property* prop)
		{
			size_t size = prop->size(obj);
			if (size == 0)
				return nullptr;

			if (!prop->changed(obj))
				return nullptr;

			com_ptr<IXMLDOMElement> collection_element;
			auto hr = _doc->createElement (_bstr_t(prop->name), &collection_element); rassert(SUCCEEDED(hr));
			std::string value;
			for (size_t i = 0; i < size; i++)
			{
				value = prop->get_to_string(obj, i, _context);
				com_ptr<IXMLDOMElement> entry_element;
				hr = _doc->createElement (entry_elem_name, &entry_element); rassert(SUCCEEDED(hr));
				hr = entry_element->setAttribute (index_attr_name, _variant_t(i)); rassert(SUCCEEDED(hr));
				hr = entry_element->setAttribute (value_attr_name, _variant_t(value.c_str())); rassert(SUCCEEDED(hr));
				hr = collection_element->appendChild (entry_element, nullptr); rassert(SUCCEEDED(hr));
			}

			return collection_element;
		}

		virtual com_ptr<IXMLDOMElement> serialize_object (const object* obj, bool force_serialize_unchanged) override final
		{
			com_ptr<IXMLDOMElement> object_element;
			serialize_element_getter get_or_create_object_element = [doc=_doc.get(), obj, &object_element]()
			{
				if (object_element == nullptr)
				{
					const char* name = obj->type()->name;
					auto hr = doc->createElement(_bstr_t(name), &object_element);
					throw_if_failed(hr);
				}

				return object_element;
			};

			if (force_serialize_unchanged)
				get_or_create_object_element();

			auto cs = dynamic_cast<const custom_serialize_object_i*>(obj);
			if (cs)
				cs->serialize_before_reflection(this, get_or_create_object_element);

			for (const type* t = obj->type(); t != nullptr; t = t->base_type)
			{
				for (auto prop : t->props)
				{
					if (auto cs = dynamic_cast<const custom_serialize_property_i*>(prop))
						cs->serialize (this, obj, get_or_create_object_element);
					else
						this->serialize_property (obj, prop, get_or_create_object_element);
				}
			}

			if (cs)
				cs->serialize_after_reflection(this, get_or_create_object_element);

			return object_element;
		}

		virtual void serialize_property (const object* obj, const property* prop, const std::function<IXMLDOMElement*()>& object_element_getter) override final
		{
			if (auto value_prop = dynamic_cast<const value_property*>(prop))
			{
				auto type = static_cast<const concrete_type*>(obj->type());
				rassert(type == dynamic_cast<const concrete_type*>(obj->type()));
				bool is_factory_prop = std::any_of (type->factory_props().begin(), type->factory_props().end(), [value_prop](auto p) { return p == value_prop; });
				if (is_factory_prop || (value_prop->can_set(obj) && value_prop->changed_from_default(obj)))
				{
					auto object_element = object_element_getter();
					auto value = value_prop->get_to_string(obj, _context);
					auto hr = object_element->setAttribute(_bstr_t(value_prop->name), _variant_t(value.c_str()));
					throw_if_failed(hr);
				}
			}
			else
			{
				com_ptr<IXMLDOMElement> prop_element;
				if (auto oc_prop = dynamic_cast<const object_collection_property*>(prop))
				{
					prop_element = serialize_object_collection(obj, oc_prop);
				}
				else if (auto vc_prop = dynamic_cast<const value_collection_property*>(prop))
				{
					prop_element = serialize_value_collection(obj, vc_prop);
				}
				else if (auto obj_prop = dynamic_cast<const object_property*>(prop))
				{
					if (auto value = obj_prop->get(obj))
						prop_element = serialize_object(value, false);
				}
				else
					throw not_implemented_exception();

				if (prop_element)
				{
					auto object_element = object_element_getter();
					auto hr = object_element->appendChild (prop_element, nullptr);
					throw_if_failed(hr);
				}
			}
		}
	};

	std::unique_ptr<xml_serializer_i> create_serializer (IXMLDOMDocument* doc, string_convert_context_i* context)
	{
		return std::make_unique<serializer>(doc, context);
	}

	// ========================================================================

	class deserializer : public xml_deserializer_i
	{
		std::span<const concrete_type* const> const _known_types;
		string_convert_context_i* const _context;

	public:
		deserializer (std::span<const concrete_type* const> known_types, string_convert_context_i* context)
			: _known_types(known_types)
			, _context(context)
		{ }

		//static void deserialize_to_internal (IXMLDOMElement* element, object* obj, bool ignore_index_attribute, std::span<const concrete_type* const> known_types);

		static std::unique_ptr<object> create_object (IXMLDOMElement* elem, const concrete_type* type, string_convert_context_i* context)
		{
			HRESULT hr;

			std::vector<std::string> factory_param_strings;
			for (auto factory_prop : type->factory_props())
			{
				_bstr_t name = factory_prop->name;
				_variant_t value;
				hr = elem->getAttribute (name, value.GetAddress());
				throw_if_failed(hr);
				hr = elem->removeAttribute (name);
				throw_if_failed(hr);
				rassert (value.vt == VT_BSTR);
				factory_param_strings.push_back(bstr_to_utf8(value.bstrVal));
			}

			std::vector<std::string_view> factory_params;
			for (auto& str : factory_param_strings)
				factory_params.push_back(str);
			auto obj = type->create({ factory_params.data(), factory_params.data() + factory_params.size() }, context);
			return obj;
		}

		virtual std::unique_ptr<object> deserialize (IXMLDOMElement* elem) override final
		{
			_bstr_t namebstr;
			auto hr = elem->get_nodeName(namebstr.GetAddress()); rassert(SUCCEEDED(hr));
			auto name = bstr_to_utf8(namebstr);
			auto it = std::find_if(_known_types.begin(), _known_types.end(), [&name](const concrete_type* t) { return t && (strcmp(t->name, name.c_str()) == 0); });
			if (it == _known_types.end())
				rassert(false); // error handling for this not implemented
			auto obj = create_object(elem, *it, _context);
			deserialize_to_internal (elem, obj.get());
			return obj;
		}

		virtual string_convert_context_i* context() const override final
		{
			return _context;
		}

		void deserialize_to_new_object_collection (IXMLDOMElement* collection_elem, object* obj, const object_collection_property* prop)
		{
			auto collection = prop->collection_cast(obj);

			com_ptr<IXMLDOMNode> child_node;
			auto hr = collection_elem->get_firstChild(&child_node); rassert(SUCCEEDED(hr));
			while (child_node != nullptr)
			{
				com_ptr<IXMLDOMElement> child_elem = child_node;

				_bstr_t namebstr;
				auto hr = child_elem->get_nodeName(namebstr.GetAddress()); rassert(SUCCEEDED(hr));
				auto name = bstr_to_utf8(namebstr);
				auto it = std::find_if(_known_types.begin(), _known_types.end(), [&name](const concrete_type* t) { return strcmp(t->name, name.c_str()) == 0; });
				if (it == _known_types.end())
					rassert(false); // error handling for this not implemented
				auto child = create_object(child_elem, *it, _context);
				auto child_raw = child.get();
				collection->append(std::move(child));
				deserialize_to_internal (child_elem, child_raw);
				hr = child_node->get_nextSibling(&child_node); rassert(SUCCEEDED(hr));
			}
		}

		void deserialize_to_existing_object_collection (IXMLDOMElement* collection_elem, object* obj, const object_collection_property* prop)
		{
			auto collection = prop->collection_cast(obj);

			size_t child_node_index = 0;
			com_ptr<IXMLDOMNode> child_node;
			auto hr = collection_elem->get_firstChild(&child_node); rassert(SUCCEEDED(hr));
			while (child_node != nullptr)
			{
				com_ptr<IXMLDOMElement> child_elem = child_node;
				size_t index = child_node_index;
				_variant_t index_attr_value;
				hr = child_elem->getAttribute(index_attr_name, index_attr_value.GetAddress());
				if (hr == S_OK)
				{
					hr = ::VariantChangeType(&index_attr_value, &index_attr_value, VARIANT_NOUSEROVERRIDE, VT_UI4);
					throw_if_failed(hr);
					index = index_attr_value.uintVal;
					hr = child_elem->removeAttribute(index_attr_name);
					throw_if_failed(hr);
				}

				auto child = collection->child_at(index);
				deserialize_to_internal (child_elem, child);

				child_node_index++;
				hr = child_node->get_nextSibling(&child_node); rassert(SUCCEEDED(hr));
			}
		}

		void deserialize_value_collection (IXMLDOMElement* collection_elem, object* obj, const value_collection_property* prop)
		{
			com_ptr<IXMLDOMNode> entry_node;
			auto hr = collection_elem->get_firstChild(&entry_node); rassert(SUCCEEDED(hr));
			while (entry_node != nullptr)
			{
				com_ptr<IXMLDOMElement> entry_elem = entry_node;
				_variant_t index_attr_value;
				hr = entry_elem->getAttribute(index_attr_name, index_attr_value.GetAddress()); rassert(SUCCEEDED(hr));
				hr = ::VariantChangeType(&index_attr_value, &index_attr_value, VARIANT_NOUSEROVERRIDE, VT_UI4);
				throw_if_failed(hr);
				size_t index = index_attr_value.uintVal;

				_variant_t value_attr_value;
				hr = entry_elem->getAttribute(value_attr_name, value_attr_value.GetAddress()); rassert(SUCCEEDED(hr));

				auto value_utf8 = bstr_to_utf8(value_attr_value.bstrVal);
				if (prop->can_insert_remove())
					prop->insert_value (value_utf8, obj, index, _context);
				else
					prop->set_value (value_utf8, obj, index, _context);

				hr = entry_node->get_nextSibling(&entry_node); rassert(SUCCEEDED(hr));
			}
		}

		void deserialize_to_internal (IXMLDOMElement* element, object* obj)
		{
			auto cs = dynamic_cast<custom_serialize_object_i*>(obj);
			if (cs)
				cs->deserialize_before_reflection(this, element);

			com_ptr<IXMLDOMNamedNodeMap> attrs;
			auto hr = element->get_attributes (&attrs); rassert(SUCCEEDED(hr));
			long attr_count;
			hr = attrs->get_length(&attr_count); rassert(SUCCEEDED(hr));
			for (long i = 0; i < attr_count; i++)
			{
				com_ptr<IXMLDOMNode> attr_node;
				hr = attrs->get_item(i, &attr_node); rassert(SUCCEEDED(hr));
				com_ptr<IXMLDOMAttribute> attr = attr_node;

				_bstr_t namebstr;
				hr = attr->get_name(namebstr.GetAddress()); rassert(SUCCEEDED(hr));
				auto name = bstr_to_utf8(namebstr);

				_bstr_t valuebstr;
				hr = attr->get_text(valuebstr.GetAddress()); rassert(SUCCEEDED(hr));
				auto value = bstr_to_utf8(valuebstr);

				auto prop = obj->type()->find_property(name.c_str());
				if (prop)
				{
					if (auto cs = dynamic_cast<const custom_serialize_property_i*>(prop))
					{
						cs->deserialize (this, value, obj);
					}
					else
					{
						auto type = static_cast<const concrete_type*>(obj->type());
						rassert (type == dynamic_cast<const concrete_type*>(obj->type()));
						//bool is_factory_property = std::any_of (type->factory_props().begin(), type->factory_props().end(),
						//	[&name](const value_property* fp) { return strcmp(name.c_str(), fp->name) == 0; });
						//if (is_factory_property)
						//	continue;

						auto value_prop = dynamic_cast<const value_property*>(prop);
						if (value_prop == nullptr)
							rassert(false); // error handling for this not implemented
						value_prop->set_from_string(value, obj, _context);
					}
				}
				else if (cs && cs->try_deserialize_xml_attribute(this, name, value))
				{
				}
				else
					rassert(false); // unknown xml attribute
			}

			com_ptr<IXMLDOMNode> child_node;
			hr = element->get_firstChild(&child_node); rassert(SUCCEEDED(hr));
			while (child_node != nullptr)
			{
				com_ptr<IXMLDOMElement> child_elem = child_node;
				_bstr_t namebstr;
				hr = child_elem->get_nodeName(namebstr.GetAddress()); rassert(SUCCEEDED(hr));
				auto name = bstr_to_utf8(namebstr);

				auto prop = obj->type()->find_property(name.c_str());
				if (prop)
				{
					if (auto cs = dynamic_cast<const custom_serialize_property_i*>(prop))
					{
						cs->deserialize (this, child_elem, obj);
					}
					else if (auto vc_prop = dynamic_cast<const value_collection_property*>(prop))
					{
						deserialize_value_collection (child_elem, obj, vc_prop);
					}
					else if (auto oc_prop = dynamic_cast<const object_collection_property*>(prop))
					{
						if (!oc_prop->preallocated)
							deserialize_to_new_object_collection (child_elem, obj, oc_prop);
						else
							deserialize_to_existing_object_collection (child_elem, obj, oc_prop);
					}
					else if (auto obj_prop = dynamic_cast<const object_property*>(prop))
					{
						auto child = deserialize (child_elem);
						obj_prop->set (obj, std::move(child));
					}
					else
						rassert(false); // error handling for this not implemented
				}
				else if (cs && cs->try_deserialize_xml_element(this, child_elem))
				{
				}
				else
					rassert(false); // unknown xml element

				hr = child_node->get_nextSibling(&child_node); rassert(SUCCEEDED(hr));
			}

			if (cs)
				cs->deserialize_after_reflection(this, element);
		}

		virtual void deserialize_to (IXMLDOMElement* element, object* obj) override final
		{
			return deserialize_to_internal (element, obj);
		}
	};

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

		rassert (!::PathIsRelative(file_path));
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

	std::unique_ptr<xml_deserializer_i> create_deserializer (std::span<const concrete_type* const> known_types, string_convert_context_i* context)
	{
		return std::make_unique<deserializer>(known_types, context);
	}
}
