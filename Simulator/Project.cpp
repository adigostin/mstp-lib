
#include "pch.h"
#include "SimulatorDefs.h"

using namespace std;

class Project : public IProject
{
	ULONG _refCount = 1;
	vector<ComPtr<PhysicalBridge>> _bridges;
	EventManager _em;
	std::array<uint8_t, 6> _nextMacAddress = { 0x00, 0xAA, 0x55, 0xAA, 0x55, 0x01 };

public:
	virtual const vector<ComPtr<PhysicalBridge>>& GetBridges() const override final { return _bridges; }

	virtual void InsertBridge(size_t index, PhysicalBridge* bridge) override final
	{
		_bridges.push_back(ComPtr<PhysicalBridge>(bridge));
		bridge->GetInvalidateEvent().AddHandler (&OnBridgeInvalidate, this);
		BridgeInsertedEvent::InvokeHandlers (_em, this, index, bridge);
		ProjectInvalidateEvent::InvokeHandlers (_em, this);
	}

	virtual void RemoveBridge(size_t index) override final
	{
		PhysicalBridge* bridge = _bridges[index];
		BridgeRemovingEvent::InvokeHandlers(_em, this, index, bridge);
		bridge->GetInvalidateEvent().RemoveHandler(&OnBridgeInvalidate, this);
		_bridges.erase (_bridges.begin() + index);
		ProjectInvalidateEvent::InvokeHandlers(_em, this);
	}

	static void OnBridgeInvalidate (void* callbackArg, PhysicalBridge* bridge)
	{
		auto project = static_cast<Project*>(callbackArg);
		ProjectInvalidateEvent::InvokeHandlers (project->_em, project);
	}

	virtual BridgeInsertedEvent::Subscriber GetBridgeInsertedEvent() override final { return BridgeInsertedEvent::Subscriber(_em); }
	virtual BridgeRemovingEvent::Subscriber GetBridgeRemovingEvent() override final { return BridgeRemovingEvent::Subscriber(_em); }
	virtual ProjectInvalidateEvent::Subscriber GetProjectInvalidateEvent() override final { return ProjectInvalidateEvent::Subscriber(_em); }

	virtual array<uint8_t, 6> AllocNextMacAddress() override final
	{
		auto result = _nextMacAddress;
		_nextMacAddress[5]++;
		if (_nextMacAddress[5] == 0)
		{
			_nextMacAddress[4]++;
			if (_nextMacAddress[4] == 0)
				throw NotImplementedException();
		}

		return result;
	}

	#pragma region IUnknown
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
	#pragma endregion
};

extern const ProjectFactory projectFactory = [] { return ComPtr<IProject>(new Project(), false); };
