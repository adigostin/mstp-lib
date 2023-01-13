
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "object.h"
#include "value_property.h"
#include "value_collection_property.h"
#include "object_property.h"
#include "object_collection_property.h"

namespace edge
{
	//static
	std::string string_convert_exception::make_string (std::string_view str, const char* type_name)
	{
		auto res = std::string("Cannot convert \"");
		res += str;
		res += "\" to type \"";
		res += type_name;
		res += "\".";
		return res;
	}

	string_convert_exception::string_convert_exception (const char* str)
		: _message(str)
	{ }

	string_convert_exception::string_convert_exception (std::string_view str, const char* type_name)
		: _message(make_string(str, type_name))
	{ }	type::type (const type* base_type, std::span<const property* const> props) noexcept
		: base_type(base_type), props(props)
	{ }

	const property* type::find_property (const char* name) const
	{
		for (auto p : props)
		{
			if ((p->name() == name) || (strcmp(p->name(), name) == 0))
				return p;
		}

		if (base_type)
			return base_type->find_property(name);
		else
			return nullptr;
	}

	bool type::has_property (const property* p) const
	{
		for (auto prop : props)
		{
			if (prop == p)
				return true;
		}

		if (base_type)
			return base_type->has_property(p);

		return false;
	}

	bool type::is_same_or_derived_from (const type* t) const
	{
		if (this == t)
			return true;

		if (base_type == nullptr)
			return false;

		return base_type->is_same_or_derived_from(t);
	}

	bool type::is_same_or_derived_from (const type& t) const { return is_same_or_derived_from (&t); }

	// ========================================================================
	// concrete_type

	std::vector<const concrete_type*>* concrete_type::_known_types;

	concrete_type::concrete_type (const char* name, const type* base_type, std::span<const property* const> props) noexcept
		: base (base_type, props), name(name)
	{
		// The C++ standard guarantees that "known_types" (a POD) is zeroed before any constructor is executed.
		if (_known_types == nullptr)
			_known_types = new std::vector<const concrete_type*>;
		_known_types->push_back(this);
	}

	concrete_type::~concrete_type()
	{
		auto it = std::find(_known_types->begin(), _known_types->end(), this);
		rassert (it != _known_types->end());
		_known_types->erase(it);
		if (_known_types->empty())
		{
			delete _known_types;
			_known_types = nullptr;
		}
	}

	std::unique_ptr<object> concrete_type::create() const
	{
		return this->create({ }, nullptr);
	}

	const std::vector<const concrete_type*>& concrete_type::known_types() { return *_known_types; }

	concrete_type::property_enumerator::property_enumerator (const concrete_type* type)
		: _type(type), _prop_index(0)
	{
		while (_type->props.size() == 0)
		{
			_type = _type->base_type;
			if (!_type)
				break;
		}
	}

	concrete_type::property_enumerator& concrete_type::property_enumerator::operator++()
	{
		#ifdef _DEBUG
		rassert(_type);
		rassert(_prop_index < _type->props.size());
		#endif

		_prop_index++;
		while(_prop_index == _type->props.size())
		{
			_type = _type->base_type;
			if (!_type)
				break;
			_prop_index = 0;
		}

		return *this;
	}

	std::vector<const property*> concrete_type::property_enumerator::to_vector()
	{
		std::vector<const property*> res;
		while (_type)
		{
			res.push_back(_type->props[_prop_index]);
			this->operator++();
		}
		return res;
	}

	// ========================================================================
	// object
	/*
	void object::on_property_changing (const property_change_args& args)
	{
		property_changing_e::invoker(em()).invoke(this, args);
	}

	void object::on_property_changed (const property_change_args& args)
	{
		property_changed_e::invoker(em()).invoke(this, args);
	}
	*/
	object::enumerator_i* object::make_enumerator()
	{
		std::vector<object*> children;

		for (const edge::type* t = this->type(); t != nullptr; t = t->base_type)
		{
			for (auto prop : t->props)
			{
				if (auto obj_prop = dynamic_cast<const object_property*>(prop))
				{
					if (auto child = obj_prop->get(this))
						children.push_back(child);
				}
				else if (auto oc_prop = dynamic_cast<const object_collection_property*>(prop))
				{
					for (size_t i = 0; i < oc_prop->size(this); i++)
					{
						if (auto child = oc_prop->at(this, i))
							children.push_back(child);
					}
				}
				else if (auto value_prop = dynamic_cast<const value_property*>(prop))
				{ }
				else if (auto vc_prop = dynamic_cast<const value_collection_property*>(prop))
				{ }
				else
					rassert(false);
			}
		}

		return edge::make_container_enumerator<enumerator_i>(std::move(children));
	}

	// ========================================================================

	bool same_type (const char* type_name1, const char* type_name2)
	{
		return (type_name1 == type_name2)
			|| (strcmp (type_name1, type_name2) == 0);
	}

	bool same_type (const concrete_type* type1, const concrete_type* type2)
	{
		return (type1 == type2) || same_type(type1->name, type2->name);
	}

	bool same_type (const object* obj1, const object* obj2)
	{
		return (obj1 == obj2) || same_type (obj1->type(), obj2->type());
	}
}
