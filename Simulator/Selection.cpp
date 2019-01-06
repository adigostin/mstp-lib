
#include "pch.h"
#include "Simulator.h"
#include "Bridge.h"
#include "Wire.h"
#include "Port.h"
#include "events.h"

using namespace std;
using namespace edge;

class Selection : public event_manager, public ISelection
{
	IProject* const _project;
	ULONG _refCount = 1;
	vector<object*> _objects;

public:
	Selection (IProject* project)
		: _project(project)
	{
		_project->GetWireRemovingEvent().add_handler (&OnWireRemovingFromProject, this);
		_project->GetBridgeRemovingEvent().add_handler (&OnBridgeRemovingFromProject, this);
	}

	~Selection()
	{
		_project->GetBridgeRemovingEvent().remove_handler (&OnBridgeRemovingFromProject, this);
		_project->GetWireRemovingEvent().remove_handler (&OnWireRemovingFromProject, this);
	}

	static void OnBridgeRemovingFromProject (void* callbackArg, IProject* project, size_t index, Bridge* b)
	{
		auto selection = static_cast<Selection*>(callbackArg);
		for (size_t i = 0; i < selection->_objects.size(); )
		{
			auto so = selection->_objects[i];
			if ((so == b) || ((dynamic_cast<Port*>(so) != nullptr) && (static_cast<Port*>(so)->bridge() == b)))
				selection->RemoveInternal(i);
			else
				i++;
		}

		selection->event_invoker<ChangedEvent>()(selection);
	}

	static void OnWireRemovingFromProject (void* callbackArg, IProject* project, size_t index, Wire* w)
	{
		auto selection = static_cast<Selection*>(callbackArg);
		for (size_t i = 0; i < selection->_objects.size(); )
		{
			auto so = selection->_objects[i];
			if (so == w)
				selection->RemoveInternal(i);
			else
				i++;
		}

		selection->event_invoker<ChangedEvent>()(selection);
	}

	virtual const vector<object*>& GetObjects() const override final { return _objects; }

	void AddInternal (object* o)
	{
		_objects.push_back(o);
		event_invoker<AddedToSelectionEvent>()(this, o);
	}

	void RemoveInternal (size_t index)
	{
		event_invoker<RemovingFromSelectionEvent>()(this, _objects[index]);
		_objects.erase(_objects.begin() + index);
	}

	virtual void Clear() override final
	{
		if (!_objects.empty())
		{
			while (!_objects.empty())
				RemoveInternal (_objects.size() - 1);
			event_invoker<ChangedEvent>()(this);
		}
	}

	virtual void Select (object* o) override final
	{
		if (o == nullptr)
			throw invalid_argument("Parameter may not be nullptr.");

		if ((_objects.size() != 1) || (_objects[0] != o))
		{
			while (!_objects.empty())
				RemoveInternal(_objects.size() - 1);
			AddInternal(o);
			event_invoker<ChangedEvent>()(this);
		}
	}

	virtual void Add (object* o) override final
	{
		if (o == nullptr)
			throw invalid_argument("Parameter may not be nullptr.");

		if (std::find (_objects.begin(), _objects.end(), o) != _objects.end())
			throw invalid_argument("Object was already added to selection.");

		AddInternal(o);
		event_invoker<ChangedEvent>()(this);
	}

	virtual void Remove (object* o) override final
	{
		if (o == nullptr)
			throw invalid_argument("Parameter may not be nullptr.");

		auto it = std::find (_objects.begin(), _objects.end(), o);
		if (it == _objects.end())
			throw invalid_argument("Object is not selected.");
		size_t index = it - _objects.begin();

		RemoveInternal(index);
		event_invoker<ChangedEvent>()(this);
	}

	virtual AddedToSelectionEvent::subscriber GetAddedToSelectionEvent() override final { return AddedToSelectionEvent::subscriber(this); }

	virtual RemovingFromSelectionEvent::subscriber GetRemovingFromSelectionEvent() override final { return RemovingFromSelectionEvent::subscriber(this); }

	virtual ChangedEvent::subscriber GetChangedEvent() override final { return ChangedEvent::subscriber(this); }
};

template<typename... Args>
static std::unique_ptr<ISelection> Create (Args... args)
{
	return std::make_unique<Selection>(std::forward<Args>(args)...);
}

const SelectionFactory selectionFactory = &Create;
