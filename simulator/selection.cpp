
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "bridge.h"
#include "wire.h"
#include "port.h"
#include "edge/events.h"

using namespace edge;

class selection : public selection_i
{
	event_manager _em;
	project_i* const _project;
	std::vector<object*> _objects;

public:
	selection (project_i* project)
		: _project(project)
	{
		_project->property_changing().add_handler<&selection::on_project_property_changing>(this);
	}

	~selection()
	{
		clear();
		_project->property_changing().remove_handler<&selection::on_project_property_changing>(this);
	}

	void on_project_property_changing (object* project_obj, const property_change_args& args)
	{
		auto project = dynamic_cast<project_i*>(project_obj);
		if (args.property == project->bridges_property())
		{
			auto& oc_args = dynamic_cast<const edge::object_collection_property_change_args&>(args);
			if (oc_args.type == collection_property_change_type::remove)
			{
				bridge* b = project->bridges()[oc_args.index].get();
				size_t i = this->index_of(b);
				remove_internal(i, 1);
			}
		}
		else if (args.property == project->wires_property())
		{
			auto& oc_args = dynamic_cast<const edge::object_collection_property_change_args&>(args);
			if (oc_args.type == collection_property_change_type::remove)
			{
				wire* w = project->wires()[oc_args.index].get();
				size_t i = this->index_of(w);
				remove_internal(i, 1);
			}
		}
	}

	virtual size_t size() const override { return _objects.size(); }
	
	virtual edge::object* operator[](size_t index) const override { return _objects[index]; }

	virtual change_e::subscriber objects_change() override { return change_e::subscriber(_em); }

	virtual const std::vector<object*>& objects() const override final { return _objects; }

	void add_internal (object* o)
	{
		change_e::invoker(_em).invoke(inserting_args{ { &o, 1 } });
		_objects.push_back(o);
		change_e::invoker(_em).invoke(inserted_args{ _objects.size() - 1, 1 });
	}

	void remove_internal (size_t index, size_t size)
	{
		if (size)
		{
			change_e::invoker(_em).invoke(removing_args{ index, size });
			std::vector<object*> removed;
			std::copy(_objects.begin() + index, _objects.begin() + index + size, std::back_inserter(removed));
			_objects.erase(_objects.begin() + index, _objects.begin() + index + size);
			change_e::invoker(_em).invoke(removed_args{ removed });
		}
	}

	virtual void clear() override final
	{
		remove_internal(0, _objects.size());
	}

	virtual void select (object* o) override final
	{
		if (o == nullptr)
			throw std::invalid_argument("Parameter may not be nullptr.");

		if ((_objects.size() != 1) || (_objects[0] != o))
		{
			remove_internal (0, _objects.size());
			add_internal(o);
		}
	}

	virtual void add (object* o) override final
	{
		if (o == nullptr)
			throw std::invalid_argument("Parameter may not be nullptr.");

		if (std::find (_objects.begin(), _objects.end(), o) != _objects.end())
			throw std::invalid_argument("Object already in selection.");

		add_internal(o);
	}

	virtual void remove (object* o) override final
	{
		if (o == nullptr)
			throw std::invalid_argument("Parameter may not be nullptr.");

		auto it = std::find (_objects.begin(), _objects.end(), o);
		if (it == _objects.end())
			throw std::invalid_argument("Object not in selection.");
		size_t index = it - _objects.begin();

		remove_internal(index, 1);
	}
};

extern std::unique_ptr<selection_i> selection_factory(project_i* project)
{
	return std::make_unique<selection>(project);
};
