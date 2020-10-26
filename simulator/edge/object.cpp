
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "object.h"
#include "collections.h"
#include <cstring>

namespace edge
{
	type::type (const type* base_type, std::span<const property* const> props) noexcept
		: base_type(base_type), props(props)
	{ }

	void type::add_properties (std::vector<const property*>& properties) const
	{
		if (this->base_type != nullptr)
			this->base_type->add_properties (properties);

		for (auto p : props)
			properties.push_back (p);
	}

	std::vector<const property*> type::make_property_list() const
	{
		std::vector<const property*> props;
		this->add_properties (props);
		return props;
	}

	const property* type::find_property (const char* name) const
	{
		for (auto p : props)
		{
			if ((p->name == name) || (strcmp(p->name, name) == 0))
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

	// ========================================================================
	// object

	// TODO: move this somewhere else, then remove #include "collections.h"
	void object::walk_tree (const std::function<void(object*)>& f)
	{
		f(this);

		for (auto prop : this->type()->make_property_list())
		{
			if (auto obj_prop = dynamic_cast<const edge::object_property*>(prop))
			{
				auto child = obj_prop->get(this);
				child->walk_tree(f);
			}
			else if (auto oc_prop = dynamic_cast<const edge::object_collection_property*>(prop))
			{
				auto coll = oc_prop->collection_cast(this);
				for (size_t i = 0; i < coll->child_count(); i++)
				{
					auto child = coll->child_at(i);
					child->walk_tree(f);
				}
			}
		}
	}

	void object::on_property_changing (const property_change_args& args)
	{
		this->event_invoker<property_changing_e>()(this, args);
	}

	void object::on_property_changed (const property_change_args& args)
	{
		this->event_invoker<property_changed_e>()(this, args);
	}

	const type object::_type = { nullptr, { } };

	// ========================================================================
	// parent_i

	void parent_i::set_this_as_parent(object* child)
	{
		rassert (child->_parent == nullptr);
		child->_parent = this;
	}

	void parent_i::call_inserted_into_parent(object* child)
	{
		child->on_inserted_into_parent();
	}

	void parent_i::call_removing_from_parent(object* child)
	{
		child->on_removing_from_parent();
	}

	void parent_i::clear_this_as_parent(object* child)
	{
		rassert (child->_parent == this);
		child->_parent = nullptr;
	}
}
