
#include "pch.h"
#include "simulator.h"
#include "bridge.h"
#include "wire.h"
#include "port.h"
#include "events.h"

using namespace std;
using namespace edge;

class selection : public event_manager, public selection_i
{
	project_i* const _project;
	ULONG _refCount = 1;
	vector<object*> _objects;

public:
	selection (project_i* project)
		: _project(project)
	{
		_project->wire_removing().add_handler (&on_wire_removing_from_project, this);
		_project->bridge_removing().add_handler (&on_bridge_removing_from_project, this);
	}

	~selection()
	{
		_project->bridge_removing().remove_handler (&on_bridge_removing_from_project, this);
		_project->wire_removing().remove_handler (&on_wire_removing_from_project, this);
	}

	static void on_bridge_removing_from_project (void* callbackArg, project_i* project, size_t index, bridge* b)
	{
		auto s = static_cast<selection*>(callbackArg);
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

	static void on_wire_removing_from_project (void* callbackArg, project_i* project, size_t index, wire* w)
	{
		auto s = static_cast<selection*>(callbackArg);
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

	virtual const vector<object*>& objects() const override final { return _objects; }

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
			throw invalid_argument("Parameter may not be nullptr.");

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
			throw invalid_argument("Parameter may not be nullptr.");

		if (std::find (_objects.begin(), _objects.end(), o) != _objects.end())
			throw invalid_argument("Object was already added to selection.");

		add_internal(o);
		event_invoker<changed_e>()(this);
	}

	virtual void remove (object* o) override final
	{
		if (o == nullptr)
			throw invalid_argument("Parameter may not be nullptr.");

		auto it = std::find (_objects.begin(), _objects.end(), o);
		if (it == _objects.end())
			throw invalid_argument("Object is not selected.");
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
