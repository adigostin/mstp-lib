
#include "pch.h"
#include "Simulator.h"
#include "Wire.h"
#include "Bridge.h"

using namespace std;

class Project : public IProject
{
	ULONG _refCount = 1;
	vector<ComPtr<Bridge>> _bridges;
	vector<ComPtr<Wire>> _wires;
	EventManager _em;
	std::array<uint8_t, 6> _nextMacAddress = { 0x00, 0xAA, 0x55, 0xAA, 0x55, 0x80 };

public:
	virtual const vector<ComPtr<Bridge>>& GetBridges() const override final { return _bridges; }

	virtual void InsertBridge (size_t index, Bridge* bridge) override final
	{
		if (index > _bridges.size())
			throw invalid_argument("index");

		_bridges.push_back(bridge);
		bridge->GetInvalidateEvent().AddHandler (&OnObjectInvalidate, this);
		BridgeInsertedEvent::InvokeHandlers (_em, this, index, bridge);
		ProjectInvalidateEvent::InvokeHandlers (_em, this);
	}

	virtual void RemoveBridge(size_t index) override final
	{
		if (index >= _bridges.size())
			throw invalid_argument("index");

		BridgeRemovingEvent::InvokeHandlers(_em, this, index, _bridges[index]);
		_bridges[index]->GetInvalidateEvent().RemoveHandler (&OnObjectInvalidate, this);
		_bridges.erase (_bridges.begin() + index);
		ProjectInvalidateEvent::InvokeHandlers (_em, this);
	}

	virtual const vector<ComPtr<Wire>>& GetWires() const override final { return _wires; }

	virtual void InsertWire (size_t index, Wire* wire) override final
	{
		if (index > _wires.size())
			throw invalid_argument("index");

		_wires.push_back(wire);
		wire->GetInvalidateEvent().AddHandler (&OnObjectInvalidate, this);
		WireInsertedEvent::InvokeHandlers (_em, this, index, wire);
		ProjectInvalidateEvent::InvokeHandlers (_em, this);
	}

	virtual void RemoveWire (size_t index) override final
	{
		if (index >= _wires.size())
			throw invalid_argument("index");

		WireRemovingEvent::InvokeHandlers(_em, this, index, _wires[index]);
		_wires[index]->GetInvalidateEvent().RemoveHandler (&OnObjectInvalidate, this);
		_wires.erase(_wires.begin() + index);
		ProjectInvalidateEvent::InvokeHandlers (_em, this);
	}

	static void OnObjectInvalidate (void* callbackArg, Object* object)
	{
		auto project = static_cast<Project*>(callbackArg);
		ProjectInvalidateEvent::InvokeHandlers (project->_em, project);
	}

	virtual BridgeInsertedEvent::Subscriber GetBridgeInsertedEvent() override final { return BridgeInsertedEvent::Subscriber(_em); }
	virtual BridgeRemovingEvent::Subscriber GetBridgeRemovingEvent() override final { return BridgeRemovingEvent::Subscriber(_em); }

	virtual WireInsertedEvent::Subscriber GetWireInsertedEvent() override final { return WireInsertedEvent::Subscriber(_em); }
	virtual WireRemovingEvent::Subscriber GetWireRemovingEvent() override final { return WireRemovingEvent::Subscriber(_em); }

	virtual ProjectInvalidateEvent::Subscriber GetProjectInvalidateEvent() override final { return ProjectInvalidateEvent::Subscriber(_em); }

	virtual Port* FindReceivingPort (Port* txPort) const override final
	{
		for (auto& w : _wires)
		{
			for (size_t i = 0; i < 2; i++)
			{
				auto& thisEnd = w->GetPoints()[i];
				if (holds_alternative<ConnectedWireEnd>(thisEnd) && (get<ConnectedWireEnd>(thisEnd) == txPort))
				{
					auto& otherEnd = w->GetPoints()[1 - i];
					if (holds_alternative<ConnectedWireEnd>(otherEnd))
						return get<ConnectedWireEnd>(otherEnd);
					else
						return nullptr;
				}
			}
		}

		return nullptr;
	}

	virtual array<uint8_t, 6> AllocMacAddressRange (size_t count) override final
	{
		if (count >= 128)
			throw range_error("count must be lower than 128.");

		auto result = _nextMacAddress;
		_nextMacAddress[5] += (uint8_t)count;
		if (_nextMacAddress[5] < count)
		{
			_nextMacAddress[4]++;
			if (_nextMacAddress[4] == 0)
				throw not_implemented_exception();
		}

		return result;
	}

	virtual pair<Wire*, size_t> GetWireConnectedToPort (const Port* port) const override final
	{
		for (auto& w : _wires)
		{
			if (holds_alternative<ConnectedWireEnd>(w->GetP0()) && (get<ConnectedWireEnd>(w->GetP0()) == port))
				return { w, 0 };
			else if (holds_alternative<ConnectedWireEnd>(w->GetP1()) && (get<ConnectedWireEnd>(w->GetP1()) == port))
				return { w, 1 };
		}

		return { };
	}

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override final { return E_NOTIMPL; }

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
