
#include "pch.h"
#include "simulator.h"
#include "bridge.h"
#include "wire.h"
#include "port.h"
#include "events.h"

using namespace edge;

class selection : public event_manager, public selection_i
{
	project_i* const _project;
	ULONG _refCount = 1;
	std::vector<object*> _objects;

public:
	selection (project_i* project)
		: _project(project)
	{
		_project->property_changing().add_handler (&on_project_property_changing, this);
	}

	~selection()
	{
		_project->property_changing().remove_handler (&on_project_property_changing, this);
	}

	static void on_project_property_changing (void* callback_arg, object* project_obj, const property_change_args& args)
	{
		auto s = static_cast<class selection*>(callback_arg);
		auto project = dynamic_cast<project_i*>(project_obj);
		if ((args.property == project->bridges_prop()) && (args.type == collection_property_change_type::remove))
		{
			bridge* b = project->bridges()[args.index].get();

			for (size_t i = 0; i < s->_objects.size(); )
			{
				auto so = s->_objects[i];
				if ((so == b) || ((dynamic_cast<port*>(so) != nullptr) && (static_cast<port*>(so)->bridge() == b)))
					s->remove_internal(i);
				else
					i++;
			}

			s->event_invoker<changed_e>()(s);
		}
		else if ((args.property == project->wires_prop()) && (args.type == collection_property_change_type::remove))
		{
			wire* w = project->wires()[args.index].get();

			for (size_t i = 0; i < s->_objects.size(); )
			{
				auto so = s->_objects[i];
				if (so == w)
					s->remove_internal(i);
				else
					i++;
			}

			s->event_invoker<changed_e>()(s);
		}
	}

	virtual const std::vector<object*>& objects() const override final { return _objects; }

	void add_internal (object* o)
	{
		_objects.push_back(o);
		event_invoker<added_e>()(this, o);
	}

	void remove_internal (size_t index)
	{
		event_invoker<removing_e>()(this, _objects[index]);
		_objects.erase(_objects.begin() + index);
	}

	virtual void clear() override final
	{
		if (!_objects.empty())
		{
			while (!_objects.empty())
				remove_internal (_objects.size() - 1);
			event_invoker<changed_e>()(this);
		}
	}

	virtual void select (object* o) override final
	{
		if (o == nullptr)
			throw std::invalid_argument("Parameter may not be nullptr.");

		if ((_objects.size() != 1) || (_objects[0] != o))
		{
			while (!_objects.empty())
				remove_internal(_objects.size() - 1);
			add_internal(o);
			event_invoker<changed_e>()(this);
		}
	}

	virtual void add (object* o) override final
	{
		if (o == nullptr)
			throw std::invalid_argument("Parameter may not be nullptr.");

		if (std::find (_objects.begin(), _objects.end(), o) != _objects.end())
			throw std::invalid_argument("Object was already added to selection.");

		add_internal(o);
		event_invoker<changed_e>()(this);
	}

	virtual void remove (object* o) override final
	{
		if (o == nullptr)
			throw std::invalid_argument("Parameter may not be nullptr.");

		auto it = std::find (_objects.begin(), _objects.end(), o);
		if (it == _objects.end())
			throw std::invalid_argument("Object is not selected.");
		size_t index = it - _objects.begin();

		remove_internal(index);
		event_invoker<changed_e>()(this);
	}

	virtual added_e::subscriber added() override final { return added_e::subscriber(this); }

	virtual removing_e::subscriber removing() override final { return removing_e::subscriber(this); }

	virtual changed_e::subscriber changed() override final { return changed_e::subscriber(this); }
};

extern const selection_factory_t selection_factory = [](project_i* project) -> std::unique_ptr<selection_i>
{
	return std::make_unique<selection>(project);
};
