
#include "pch.h"
#include "SimulatorDefs.h"

using namespace std;

class Selection : public ISelection
{
	ULONG _refCount = 1;
	vector<Object*> _objects;

	virtual ~Selection()
	{
		assert (_refCount == 0);
	}

	virtual const vector<Object*>& GetObjects() const override final { return _objects; }

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
};

extern const SelectionFactory selectionFactory = [] { return ComPtr<ISelection>(new Selection(), false); };
