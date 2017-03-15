
#include "pch.h"
#include "Simulator.h"

using namespace std;

class Selection : public ISelection
{
	ULONG _refCount = 1;
	vector<ComPtr<Object>> _objects;
	EventManager _em;

	virtual ~Selection()
	{
		assert (_refCount == 0);
	}

	virtual const vector<ComPtr<Object>>& GetObjects() const override final { return _objects; }

	void AddInternal (Object* o)
	{
		_objects.push_back(ComPtr<Object>(o));
		AddedToSelectionEvent::InvokeHandlers(_em, this, o);
		SelectionChangedEvent::InvokeHandlers(_em, this);
	}

	void RemoveInternal (Object* o)
	{
		auto it = find (_objects.begin(), _objects.end(), o);
		assert (it != _objects.end());

		RemovingFromSelectionEvent::InvokeHandlers (_em, this, o);
		SelectionChangedEvent::InvokeHandlers(_em, this);

		_objects.erase(it);
	}

	virtual void Clear() override final
	{
		if (!_objects.empty())
		{
			for (auto o : _objects)
				RemovingFromSelectionEvent::InvokeHandlers(_em, this, o);
			_objects.clear();
			SelectionChangedEvent::InvokeHandlers(_em, this);
		}
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

extern const SelectionFactory selectionFactory = [] { return ComPtr<ISelection>(new Selection(), false); };
