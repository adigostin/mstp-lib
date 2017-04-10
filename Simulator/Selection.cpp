
#include "pch.h"
#include "Simulator.h"
#include "Bridge.h"
#include "Wire.h"

using namespace std;

class Selection : public ISelection
{
	ULONG _refCount = 1;
	IProject* const _project;
	vector<ComPtr<Object>> _objects;
	EventManager _em;

public:
	Selection (IProject* project)
		: _project(project)
	{
		_project->GetWireRemovingEvent().AddHandler (&OnWireRemoving, this);
		_project->GetBridgeRemovingEvent().AddHandler (&OnBridgeRemoving, this);
	}

	virtual ~Selection()
	{
		_project->GetBridgeRemovingEvent().RemoveHandler (&OnBridgeRemoving, this);
		_project->GetWireRemovingEvent().RemoveHandler (&OnWireRemoving, this);
	}

	static void OnBridgeRemoving (void* callbackArg, IProject* project, size_t index, Bridge* b) { static_cast<Selection*>(callbackArg)->OnObjectRemoving(b); }

	static void OnWireRemoving (void* callbackArg, IProject* project, size_t index, Wire* w) { static_cast<Selection*>(callbackArg)->OnObjectRemoving(w); }

	void OnObjectRemoving (Object* o)
	{
		auto it = find (_objects.begin(), _objects.end(), o);
		if (it != _objects.end())
			RemoveInternal(it - _objects.begin());
	}

	virtual const vector<ComPtr<Object>>& GetObjects() const override final { return _objects; }

	void AddInternal (Object* o)
	{
		_objects.push_back(ComPtr<Object>(o));
		AddedToSelectionEvent::InvokeHandlers(_em, this, o);

		SelectionChangedEvent::InvokeHandlers(_em, this);
	}

	void RemoveInternal (size_t index)
	{
		auto o = _objects[index].Get();
		RemovingFromSelectionEvent::InvokeHandlers (_em, this, o);
		_objects.erase(_objects.begin() + index);

		SelectionChangedEvent::InvokeHandlers(_em, this);
	}

	virtual void Clear() override final
	{
		while (!_objects.empty())
			RemoveInternal (_objects.size() - 1);
	}

	virtual void Select(Object* o) override final
	{
		if (o == nullptr)
			throw invalid_argument("Parameter may not be nullptr.");

		if ((_objects.size() != 1) || (_objects[0] != o))
		{
			this->Clear();
			AddInternal(o);
		}
	}

	virtual void Add (Object* o) override final
	{
		if (o == nullptr)
			throw invalid_argument("Parameter may not be nullptr.");

		if (std::find (_objects.begin(), _objects.end(), o) != _objects.end())
			throw invalid_argument("Object was already added to selection.");

		AddInternal(o);
	}

	virtual AddedToSelectionEvent::Subscriber GetAddedToSelectionEvent() override final { return AddedToSelectionEvent::Subscriber(_em); }

	virtual RemovingFromSelectionEvent::Subscriber GetRemovingFromSelectionEvent() override final { return RemovingFromSelectionEvent::Subscriber(_em); }

	virtual SelectionChangedEvent::Subscriber GetSelectionChangedEvent() override final { return SelectionChangedEvent::Subscriber(_em); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override final { return E_NOTIMPL; }

	virtual ULONG STDMETHODCALLTYPE AddRef(void) override final
	{
		return InterlockedIncrement (&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release(void) override final
	{
		auto newRefCount = InterlockedDecrement (&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
	#pragma endregion
};

extern const SelectionFactory selectionFactory = [](auto... params) { return ComPtr<ISelection>(new Selection(params...), false); };
