
#include "pch.h"
#include "Simulator.h"
#include "Bridge.h"
#include "Wire.h"
#include "Port.h"

using namespace std;

class Selection : public EventManager, public ISelection
{
	IProjectPtr const _project;
	ULONG _refCount = 1;
	vector<Object*> _objects;

public:
	Selection (IProject* project)
		: _project(project)
	{
		_project->GetWireRemovingEvent().AddHandler (&OnWireRemovingFromProject, this);
		_project->GetBridgeRemovingEvent().AddHandler (&OnBridgeRemovingFromProject, this);
	}

private:
	~Selection()
	{
		_project->GetBridgeRemovingEvent().RemoveHandler (&OnBridgeRemovingFromProject, this);
		_project->GetWireRemovingEvent().RemoveHandler (&OnWireRemovingFromProject, this);
	}

	static void OnBridgeRemovingFromProject (void* callbackArg, IProject* project, size_t index, Bridge* b)
	{
		auto selection = static_cast<Selection*>(callbackArg);
		for (size_t i = 0; i < selection->_objects.size(); )
		{
			auto so = selection->_objects[i];
			if ((so == b) || ((dynamic_cast<Port*>(so) != nullptr) && (static_cast<Port*>(so)->GetBridge() == b)))
				selection->RemoveInternal(i);
			else
				i++;
		}

		ChangedEvent::InvokeHandlers(*selection, selection);
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

		ChangedEvent::InvokeHandlers(*selection, selection);
	}

	virtual const vector<Object*>& GetObjects() const override final { return _objects; }

	void AddInternal (Object* o)
	{
		_objects.push_back(o);
		AddedToSelectionEvent::InvokeHandlers(*this, this, o);
	}

	void RemoveInternal (size_t index)
	{
		RemovingFromSelectionEvent::InvokeHandlers (*this, this, _objects[index]);
		_objects.erase(_objects.begin() + index);
	}

	virtual void Clear() override final
	{
		if (!_objects.empty())
		{
			while (!_objects.empty())
				RemoveInternal (_objects.size() - 1);
			ChangedEvent::InvokeHandlers(*this, this);
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
			ChangedEvent::InvokeHandlers(*this, this);
		}
	}

	virtual void Add (Object* o) override final
	{
		if (o == nullptr)
			throw invalid_argument("Parameter may not be nullptr.");

		if (std::find (_objects.begin(), _objects.end(), o) != _objects.end())
			throw invalid_argument("Object was already added to selection.");

		AddInternal(o);
		ChangedEvent::InvokeHandlers(*this, this);
	}

	virtual AddedToSelectionEvent::Subscriber GetAddedToSelectionEvent() override final { return AddedToSelectionEvent::Subscriber(this); }

	virtual RemovingFromSelectionEvent::Subscriber GetRemovingFromSelectionEvent() override final { return RemovingFromSelectionEvent::Subscriber(this); }

	virtual ChangedEvent::Subscriber GetChangedEvent() override final { return ChangedEvent::Subscriber(this); }

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return E_NOTIMPL; }

	virtual ULONG STDMETHODCALLTYPE AddRef() override final
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override final
	{
		assert (_refCount > 0);
		ULONG newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
};

template<typename... Args>
static ISelectionPtr Create (Args... args)
{
	return ISelectionPtr(new Selection (std::forward<Args>(args)...), false);
}

const SelectionFactory selectionFactory = &Create;
