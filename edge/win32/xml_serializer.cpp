
#include "pch.h"
#include "xml_serializer.h"
#include "utility_functions.h"

namespace edge
{
	com_ptr<IXMLDOMElement> serialize (IXMLDOMDocument* doc, const object* o)
	{
		com_ptr<IXMLDOMElement> element;
		auto hr = doc->createElement(_bstr_t(o->type()->name), &element); assert(SUCCEEDED(hr));
		
		auto props = o->type()->make_property_list();
		for (const property* prop : props)
		{
			if (auto value_prop = dynamic_cast<const value_property*>(prop))
			{
				if (value_prop->has_setter() && value_prop->changed_from_default(o))
				{
					auto value = value_prop->get_to_string(o);
					hr = element->setAttribute(_bstr_t(value_prop->_name), _variant_t(value.c_str())); assert(SUCCEEDED(hr));
				}
			}
		}

		for (const property* prop : props)
		{
			if (dynamic_cast<const value_property*>(prop))
			{ }
			else if (auto oc_prop = dynamic_cast<const object_collection_property*>(prop))
			{
				com_ptr<IXMLDOMElement> collection_element;
				hr = doc->createElement (_bstr_t(oc_prop->_name), &collection_element); assert(SUCCEEDED(hr));
				for (size_t i = 0; i < oc_prop->child_count(o); i++)
				{
					auto child = oc_prop->child_at(o, i);
					auto child_element = serialize (doc, child);
					hr = collection_element->appendChild(child_element, nullptr); assert(SUCCEEDED(hr));
				}
				
				hr = element->appendChild (collection_element, nullptr); assert(SUCCEEDED(hr));
			}
			else
				assert(false); // not implemented
		}

		return element;
	}

	std::unique_ptr<object> deserialize (IXMLDOMElement* elem, object* parent)
	{
		_bstr_t namebstr;
		auto hr = elem->get_nodeName(namebstr.GetAddress()); assert(SUCCEEDED(hr));
		auto name = bstr_to_utf8(namebstr);
		auto type = type::find_type(name.c_str());
		if (type == nullptr)
			assert(false); // error handling for this not implemented
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
		auto obj = std::unique_ptr<object>(type->create(parent, { factory_params.data(), factory_params.data() + factory_params.size() }));
		deserialize_to (elem, obj.get());
		return obj;
	}

	static void deserialize_object_collection (IXMLDOMElement* collection_elem, object* o, const object_collection_property* prop)
	{
		size_t index = 0;
		com_ptr<IXMLDOMNode> child_node;
		auto hr = collection_elem->get_firstChild(&child_node); assert(SUCCEEDED(hr));
		while (child_node != nullptr)
		{
			com_ptr<IXMLDOMElement> child_elem = child_node.get();
			auto child = deserialize(child_elem, o);
			prop->insert_child (o, index, std::move(child));
		
			hr = child_node->get_nextSibling(&child_node); assert(SUCCEEDED(hr));
		}
	}

	static void deserialize_value_collection (IXMLDOMElement* collection_elem, object* o, const value_collection_property* prop)
	{
		size_t index = 0;
		com_ptr<IXMLDOMNode> entry_node;
		auto hr = collection_elem->get_firstChild(&entry_node); assert(SUCCEEDED(hr));
		while (entry_node != nullptr)
		{
			com_ptr<IXMLDOMElement> entry_elem = entry_node.get();
			_variant_t index_attr_value;
			hr = entry_elem->getAttribute(_bstr_t(prop->_index_attr_name), index_attr_value.GetAddress()); assert(SUCCEEDED(hr));
			size_t index;
			bool converted = uint32_property_traits::from_string(bstr_to_utf8(index_attr_value.bstrVal), index); assert(converted);

			_variant_t value_attr_value;
			hr = entry_elem->getAttribute(_bstr_t(prop->_value_attr_name), value_attr_value.GetAddress()); assert(SUCCEEDED(hr));
			
			auto value_utf8 = bstr_to_utf8(value_attr_value.bstrVal);
			if (prop->can_set())
			{
				bool converted = prop->set_value(o, index, value_utf8);
				assert(converted);
			}
			else if (prop->can_insert())
			{
				bool converted = prop->insert_value (o, index, value_utf8);
				assert(converted);
			}
			else
				assert(false);

			hr = entry_node->get_nextSibling(&entry_node); assert(SUCCEEDED(hr));
		}
	}

	void deserialize_to (IXMLDOMElement* element, object* o)
	{
		//_bstr_t name;
		//auto hr = element->get_nodeName(name.GetAddress()); assert(SUCCEEDED(hr));

		com_ptr<IXMLDOMNamedNodeMap> attrs;
		auto hr = element->get_attributes (&attrs); assert(SUCCEEDED(hr));
		long attr_count;
		hr = attrs->get_length(&attr_count); assert(SUCCEEDED(hr));
		for (long i = 0; i < attr_count; i++)
		{
			com_ptr<IXMLDOMNode> attr_node;
			hr = attrs->get_item(i, &attr_node); assert(SUCCEEDED(hr));
			com_ptr<IXMLDOMAttribute> attr = attr_node.get();
			_bstr_t namebstr;
			hr = attr->get_name(namebstr.GetAddress()); assert(SUCCEEDED(hr));
			auto name = bstr_to_utf8(namebstr);

			bool is_factory_property = std::any_of (o->type()->factory_props().begin(), o->type()->factory_props().end(), [&name](auto fp) { return strcmp(name.c_str(), fp->_name) == 0; });
			if (!is_factory_property)
			{			
				_bstr_t valuebstr;
				hr = attr->get_text(valuebstr.GetAddress()); assert(SUCCEEDED(hr));
				auto value = bstr_to_utf8(valuebstr);

				auto prop = o->type()->find_property(name.c_str());
				if (prop == nullptr)
					assert(false); // error handling for this not implemented
				auto value_prop = dynamic_cast<const value_property*>(prop);
				if (value_prop == nullptr)
					assert(false); // error handling for this not implemented
				bool set_successful = value_prop->try_set_from_string(o, value.c_str());
				if (!set_successful)
					assert(false); // error handling for this not implemented
			}
		}

		com_ptr<IXMLDOMNode> child_node;
		hr = element->get_firstChild(&child_node); assert(SUCCEEDED(hr));
		while (child_node != nullptr)
		{
			com_ptr<IXMLDOMElement> child_elem = child_node.get();
			_bstr_t namebstr;
			hr = child_elem->get_nodeName(namebstr.GetAddress()); assert(SUCCEEDED(hr));
			auto name = bstr_to_utf8(namebstr);
			
			auto prop = o->type()->find_property(name.c_str());
			if (prop == nullptr)
				assert(false); // error handling for this not implemented

			if (auto vc_prop = dynamic_cast<const value_collection_property*>(prop))
				deserialize_value_collection (child_elem, o, vc_prop);
			else if (auto oc_prop = dynamic_cast<const object_collection_property*>(prop))
				deserialize_object_collection (child_elem, o, oc_prop);
			else
				assert(false); // error handling for this not implemented

			hr = child_node->get_nextSibling(&child_node); assert(SUCCEEDED(hr));
		}

		assert(false); // not implemented
	}
}
