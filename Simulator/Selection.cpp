
#include "pch.h"
#include "SimulatorDefs.h"

using namespace std;

class Selection : public ISelection
{
	ULONG _refCount = 1;
	vector<Object*> _objects;
	EventManager _em;

	virtual ~Selection()
	{
		assert (_refCount == 0);
	}

	virtual const vector<Object*>& GetObjects() const override final { return _objects; }

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
			throw NullArgumentException();

		if ((_objects.size() != 1) || (_objects[0] != o))
		{
			this->Clear();
			_objects.push_back(o);
			AddedToSelectionEvent::InvokeHandlers(_em, this, o);
			SelectionChangedEvent::InvokeHandlers(_em, this);
		}
	}

	virtual AddedToSelectionEvent::Subscriber GetAddedToSelectionEvent() override final { return AddedToSelectionEvent::Subscriber(_em); }

	virtual RemovingFromSelectionEvent::Subscriber GetRemovingFromSelectionEvent() override final { return RemovingFromSelectionEvent::Subscriber(_em); }

	virtual SelectionChangedEvent::Subscriber GetSelectionChangedEvent() override final { return SelectionChangedEvent::Subscriber(_em); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void ** ppvObject) override final
	{
		throw NotImplementedException();
	}

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
