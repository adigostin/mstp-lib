
#include "pch.h"
#include "SimulatorInterfaces.h"

using namespace std;

class Project : public IProject
{
	ULONG _refCount = 1;
	vector<unique_ptr<PhysicalBridge>> _bridges;
	EventManager _em;

public:
	virtual const std::vector<std::unique_ptr<PhysicalBridge>>& GetBridges() const override final { return _bridges; }

	virtual void InsertBridge(size_t index, std::unique_ptr<PhysicalBridge>&& bridge) override final
	{
		_bridges.push_back(move(bridge));
		BridgeInsertedEvent::InvokeHandlers(_em, this, index, bridge.get());
	}

	virtual void RemoveBridge(size_t index) override final
	{
		BridgeRemovingEvent::InvokeHandlers(_em, this, index, _bridges[index].get());
		_bridges.erase (_bridges.begin() + index);
	}

	virtual BridgeInsertedEvent::Subscriber GetBridgeInsertedEvent() override final { return BridgeInsertedEvent::Subscriber(_em); }
	virtual BridgeRemovingEvent::Subscriber GetBridgeRemovingEvent() override final { return BridgeRemovingEvent::Subscriber(_em); }

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override final { throw NotImplementedException(); }
	
	virtual ULONG STDMETHODCALLTYPE AddRef() override final
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override final
	{
		auto newRefCount = InterlockedDecrement (&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
};

extern const ProjectFactory projectFactory = [] { return ComPtr<IProject>(new Project(), false); };
