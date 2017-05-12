
#include "pch.h"
#include "Simulator.h"
#include "Bridge.h"
#include "Wire.h"

using namespace std;

class Selection : public ISelection
{
	IProject* const _project;
	ULONG _refCount = 1;
	vector<Object*> _objects;
	EventManager _em;

public:
	Selection (IProject* project)
		: _project(project)
	{
		_project->GetWireRemovingEvent().AddHandler (&OnWireRemovingFromProject, this);
		_project->GetBridgeRemovingEvent().AddHandler (&OnBridgeRemovingFromProject, this);
	}

	virtual ~Selection()
	{
		_project->GetBridgeRemovingEvent().RemoveHandler (&OnBridgeRemovingFromProject, this);
		_project->GetWireRemovingEvent().RemoveHandler (&OnWireRemovingFromProject, this);
	}

	static void OnBridgeRemovingFromProject (void* callbackArg, IProject* project, size_t index, Bridge* b) { static_cast<Selection*>(callbackArg)->OnObjectRemovingFromProject(b); }

	static void OnWireRemovingFromProject (void* callbackArg, IProject* project, size_t index, Wire* w) { static_cast<Selection*>(callbackArg)->OnObjectRemovingFromProject(w); }

	void OnObjectRemovingFromProject (Object* o)
	{
		auto it = find (_objects.begin(), _objects.end(), o);
		if (it != _objects.end())
		{
			RemoveInternal(it - _objects.begin());
			ChangedEvent::InvokeHandlers(_em, this);
		}
	}

	virtual const vector<Object*>& GetObjects() const override final { return _objects; }

	void AddInternal (Object* o)
	{
		_objects.push_back(o);
		AddedToSelectionEvent::InvokeHandlers(_em, this, o);
	}

	void RemoveInternal (size_t index)
	{
		RemovingFromSelectionEvent::InvokeHandlers (_em, this, _objects[index]);
		_objects.erase(_objects.begin() + index);
	}

	virtual void Clear() override final
	{
		if (!_objects.empty())
		{
			while (!_objects.empty())
				RemoveInternal (_objects.size() - 1);
			ChangedEvent::InvokeHandlers(_em, this);
		}
	}

	virtual void Select(Object* o) override final
	{
		if (o == nullptr)
			throw invalid_argument("Parameter may not be nullptr.");

		if ((_objects.size() != 1) || (_objects[0] != o))
		{
			while (!_objects.empty())
				RemoveInternal(_objects.size() - 1);
			AddInternal(o);
			ChangedEvent::InvokeHandlers(_em, this);
		}
	}

	virtual void Add (Object* o) override final
	{
		if (o == nullptr)
			throw invalid_argument("Parameter may not be nullptr.");

		if (std::find (_objects.begin(), _objects.end(), o) != _objects.end())
			throw invalid_argument("Object was already added to selection.");

		AddInternal(o);
		ChangedEvent::InvokeHandlers(_em, this);
	}

	virtual AddedToSelectionEvent::Subscriber GetAddedToSelectionEvent() override final { return AddedToSelectionEvent::Subscriber(_em); }

	virtual RemovingFromSelectionEvent::Subscriber GetRemovingFromSelectionEvent() override final { return RemovingFromSelectionEvent::Subscriber(_em); }

	virtual ChangedEvent::Subscriber GetChangedEvent() override final { return ChangedEvent::Subscriber(_em); }
};

template<typename... Args>
static ISelection* Create (Args... args)
{
	return new Selection (std::forward<Args>(args)...);
}

const SelectionFactory selectionFactory = Create;
