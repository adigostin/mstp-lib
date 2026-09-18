
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "dispids.h"
#include "edge/Z80Xml.h"
#include "edge/unordered_map_nothrow.h"

const wchar_t ProjectElementName[] = L"StpProject2";

class StpProjectImpl : public IStpProject, IProjectProperties, IConnectionPointContainer, IXmlParent, IBridgeEvents
{
	ULONG _refCount = 0;
	ULONG _sig = 0xAA550004;
	WeakRefToThis _weakRefToThis;
	wil::unique_process_heap_string _path;
	vector_nothrow<com_ptr<IBridge>> _bridges;
	vector_nothrow<com_ptr<IWire>> _wires;
	mac_address _nextBridgeAddress = { 0x00, 0xAA, 0x55, 0xAA, 0x55, 0x80 };
	bool _simulationPaused = false;
	bool _changedFlag = false;
	com_ptr<ConnectionPointImpl<IPropertyChangeSink>> _propChangeCP;
	com_ptr<ConnectionPointImpl<IInvalidateSink>> _invalidateCP;
	com_ptr<ConnectionPointImpl<IProjectEventsSink>> _projectEventsCP;
	unordered_map_nothrow<IBridge*, AdviseSinkToken> _bridgeEventsTokens;

public:
	HRESULT InitInstance()
	{
		auto hr = _weakRefToThis.InitInstance(AsUnknown()); RETURN_IF_FAILED(hr);
		hr = MakeConnectionPoint(this, &_propChangeCP); RETURN_IF_FAILED(hr);
		hr = MakeConnectionPoint(this, &_invalidateCP); RETURN_IF_FAILED(hr);
		hr = MakeConnectionPoint(this, &_projectEventsCP); RETURN_IF_FAILED(hr);
		return S_OK;
	}

	~StpProjectImpl()
	{
		// Need to call remove_bridge explicitly in order to unregister the event handlers that we registered in insert_bridge.
		while(!_wires.empty())
			RemoveWire((ULONG)_wires.size() - 1);
		while(!_bridges.empty())
			RemoveBridge((ULONG)_bridges.size() - 1);
	}

	IUnknown* AsUnknown() { return static_cast<IStpProject*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IProjectProperties>(this, riid, ppvObject)
			|| TryQI<IDispatch>(this, riid, ppvObject)
			|| TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IStpProject>(this, riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject)
			|| TryQI<IXmlParent>(this, riid, ppvObject)
			|| TryQI<IBridgeEvents>(this, riid, ppvObject)
		)
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	IMPLEMENT_IDISPATCH(IProjectProperties);

	#pragma region IProjectProperties
	virtual HRESULT STDMETHODCALLTYPE get_Bridges (SAFEARRAY** ppsaItems) override
	{
		return GetItems (_bridges.size(), [this](ULONG i, IDispatch** ppDisp) -> HRESULT {
			return _bridges[i]->QueryInterface(ppDisp);
		}, ppsaItems);
	}

	virtual HRESULT STDMETHODCALLTYPE put_Bridges (SAFEARRAY* psaItems) override
	{
		// We don't support replacing items with this function, we only support adding them once.
		RETURN_HR_IF(E_UNEXPECTED, !_bridges.empty());

		// This is meant to be called only from LoadXml, no need to set dirty flag or send notifications.

		return PutItems<IBridge> (psaItems, [this](IBridge* bridge) -> HRESULT {
			AdviseSinkToken token;
			auto hr = AdviseSink<IBridgeEvents>(bridge, _weakRefToThis, &token); RETURN_IF_FAILED(hr);
			bool inserted = _bridgeEventsTokens.try_insert({ bridge, std::move(token) }); RETURN_HR_IF(E_OUTOFMEMORY, !inserted);
			bool pushed = _bridges.try_push_back(bridge); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);
			bridge->set_parent(this);
			return S_OK;
		});

	}

	virtual HRESULT STDMETHODCALLTYPE get_Wires (SAFEARRAY** ppsaItems) override
	{
		return GetItems (_wires.size(), [this](ULONG i, IDispatch** ppDisp) -> HRESULT {
			return _wires[i]->QueryInterface(ppDisp);
		 }, ppsaItems);
	}

	virtual HRESULT STDMETHODCALLTYPE put_Wires (SAFEARRAY* psaItems) override
	{
		// We don't support replacing items with this function, we only support adding them once.
		RETURN_HR_IF(E_UNEXPECTED, !_wires.empty());

		// This is meant to be called only from LoadXml, no need to set dirty flag or send notifications.

		return PutItems<IWire> (psaItems, [this](IWire* wire) -> HRESULT {
			bool pushed = _wires.try_push_back(std::move(wire)); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);
			wire->set_parent(this);
			return S_OK;
		});
	}

	virtual HRESULT STDMETHODCALLTYPE get_NextBridgeAddress (BSTR* pbstrNextBridgeAddress) override
	{
		return BridgeAddressToString (_nextBridgeAddress, pbstrNextBridgeAddress);
	}

	virtual HRESULT STDMETHODCALLTYPE put_NextBridgeAddress (BSTR bstrNextBridgeAddress) override
	{
		return BridgeAddressFromString (bstrNextBridgeAddress, _nextBridgeAddress);
	}
	#pragma endregion

	#pragma region IConnectionPointContainer
	virtual HRESULT STDMETHODCALLTYPE EnumConnectionPoints (IEnumConnectionPoints **ppEnum) override
	{
		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE FindConnectionPoint (REFIID riid, IConnectionPoint **ppCP) override
	{
		if (riid == __uuidof(IPropertyChangeSink))
			return wil::com_query_to_nothrow(_propChangeCP, ppCP);
		if (riid == __uuidof(IInvalidateSink))
			return wil::com_query_to_nothrow(_invalidateCP, ppCP);
		if (riid == __uuidof(IProjectEventsSink))
			return wil::com_query_to_nothrow(_projectEventsCP, ppCP);
		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	#pragma region IXmlParent
	virtual HRESULT STDMETHODCALLTYPE GetChildSerializeInfo (DISPID dispidProperty, IDispatch* pChild, BSTR* pbstrXmlElementName,
															 DISPID** ppFactoryDispids, ULONG* pcFactoryDispids) override
	{
		RETURN_HR_IF_NULL(E_POINTER, ppFactoryDispids);
		RETURN_HR_IF_NULL(E_POINTER, pcFactoryDispids);
		*ppFactoryDispids = nullptr;
		*pcFactoryDispids = 0;

		if (dispidProperty == dispidBridges)
		{
			auto elementName = wil::make_bstr_nothrow(L"Bridge"); RETURN_IF_NULL_ALLOC(elementName);
			auto dispids = wil::make_unique_cotaskmem_nothrow<DISPID[]>(3); RETURN_IF_NULL_ALLOC(dispids);
			dispids.get()[0] = dispidPortCount;
			dispids.get()[1] = dispidMstiCount;
			dispids.get()[2] = dispidBridgeAddress;
			*pbstrXmlElementName = elementName.release();
			*ppFactoryDispids = dispids.release();
			*pcFactoryDispids = 3;
			return S_OK;
		}
		
		if (dispidProperty == dispidWires)
		{
			*pbstrXmlElementName = SysAllocString(L"Wire"); RETURN_IF_NULL_ALLOC(*pbstrXmlElementName);
			return S_OK;
		}

		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE GetChildDeserializeInfo (
		_In_ DISPID dispidProperty,
		_In_ PCWSTR xmlElementName,
		_Outptr_ ITypeInfo** ppTypeInfo,
		_Outptr_result_buffer_(*pcFactoryDispids) DISPID** ppFactoryDispids,
		_Out_ ULONG* pcFactoryDispids) override
	{
		HRESULT hr;

		com_ptr<ITypeInfo> ti;
		hr = GetTypeInfo(0, LANG_INVARIANT, &ti); RETURN_IF_FAILED(hr);
		com_ptr<ITypeLib> typeLib;
		hr = ti->GetContainingTypeLib(&typeLib, nullptr); RETURN_IF_FAILED(hr);

		if (dispidProperty == dispidBridges)
		{
			hr = typeLib->GetTypeInfoOfGuid(__uuidof(IBridgeProperties), ppTypeInfo); RETURN_IF_FAILED(hr);
			auto dispids = wil::make_unique_cotaskmem_nothrow<DISPID[]>(3); RETURN_IF_NULL_ALLOC(dispids);
			dispids.get()[0] = dispidPortCount;
			dispids.get()[1] = dispidMstiCount;
			dispids.get()[2] = dispidBridgeAddress;
			*ppFactoryDispids = dispids.release();
			*pcFactoryDispids = 3;
			return S_OK;
		}

		if (dispidProperty == dispidWires)
		{
			hr = typeLib->GetTypeInfoOfGuid(__uuidof(IWireProperties), ppTypeInfo); RETURN_IF_FAILED(hr);
			*ppFactoryDispids = nullptr;
			*pcFactoryDispids = 0;
			return S_OK;
		}

		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateChild (DISPID dispidProperty, PCWSTR xmlElementName,
												   const VARIANT* factoryPropValues, ULONG factoryPropCount,
												   IDispatch** childOut) override
	{
		HRESULT hr;
		if (dispidProperty == dispidBridges)
		{
			RETURN_HR_IF(E_INVALIDARG, factoryPropCount != 3);
			RETURN_HR_IF(E_INVALIDARG, factoryPropValues[0].vt != VT_UI4);
			RETURN_HR_IF(E_INVALIDARG, factoryPropValues[1].vt != VT_UI4);
			RETURN_HR_IF(E_INVALIDARG, factoryPropValues[2].vt != VT_BSTR);
			mac_address addr;
			hr = BridgeAddressFromString (V_BSTR(&factoryPropValues[2]), addr); RETURN_IF_FAILED(hr);
			com_ptr<IBridge> bridge;
			hr = MakeBridge(V_UI4(&factoryPropValues[0]), V_UI4(&factoryPropValues[1]), addr, &bridge); RETURN_IF_FAILED(hr);
			return bridge->QueryInterface(IID_PPV_ARGS(childOut));
		}

		if (dispidProperty == dispidWires)
		{
			RETURN_HR_IF(E_INVALIDARG, factoryPropCount != 0);
			com_ptr<IWire> wire;
			hr = MakeWire(&wire); RETURN_IF_FAILED(hr);
			wire->set_parent(this);
			return wire->QueryInterface(IID_PPV_ARGS(childOut));
		}

		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	virtual HRESULT InsertBridge (ULONG i, IBridge* b) override
	{
		HRESULT hr;
		auto disp = wil::try_com_query_nothrow<IDispatch>(b);
		auto args = MakeObjectCollectionPropertyChangeArgs(CollectionChangeType::Insert, i, 1, disp.addressof());
		hr = NotifyPropertyChanging (_propChangeCP, AsUnknown(), dispidBridges, &args); LOG_IF_FAILED(hr);
		b->set_parent(this);
		bool inserted = _bridges.try_insert(_bridges.begin() + i, b); RETURN_HR_IF(E_OUTOFMEMORY, !inserted);
		disp = nullptr;
		hr = NotifyPropertyChanged (_propChangeCP, AsUnknown(), dispidBridges, &args); LOG_IF_FAILED(hr);
		AdviseSinkToken token;
		hr = AdviseSink<IBridgeEvents>(b, _weakRefToThis, &token); RETURN_IF_FAILED(hr);
		inserted = _bridgeEventsTokens.try_insert({ b, std::move(token) }); RETURN_HR_IF(E_OUTOFMEMORY, !inserted);
		return S_OK;
	}

	virtual HRESULT RemoveBridge (ULONG i, _Outptr_opt_ IBridge** ppRemoved = nullptr) override
	{
		com_ptr<IBridge> res = _bridges[i].get();

		if (std::any_of (_wires.begin(), _wires.end(), [&res, this](IWire* w) {
			return any_of (w->points().begin(), w->points().end(), [&res, this] (wire_end p) {
				return std::holds_alternative<connected_wire_end>(p) && (std::get<connected_wire_end>(p)->bridge() == res);
			});
		}))
			_ASSERT(false); // can't remove a connected bridge

		auto it = _bridgeEventsTokens.find(res); _ASSERT(it != _bridgeEventsTokens.end());
		_bridgeEventsTokens.erase(it);
		
		com_ptr<IDispatch> child;
		auto args = MakeObjectCollectionPropertyChangeArgs(CollectionChangeType::Remove, i, 1, child.addressof());
		auto hr = NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidBridges, &args); LOG_IF_FAILED(hr);
		_bridges.erase(_bridges.begin() + i);
		res->set_parent(nullptr);
		child = wil::try_com_query_nothrow<IDispatch>(res);
		hr = NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidBridges, &args); LOG_IF_FAILED(hr);

		if (ppRemoved)
			*ppRemoved = res.detach();
		return S_OK;
	}

	virtual ULONG STDMETHODCALLTYPE BridgeCount() const noexcept override
	{
		return _bridges.size();
	}

	virtual IBridge* STDMETHODCALLTYPE BridgeAt(ULONG index) const noexcept override
	{
		FAIL_FAST_IF(index >= _bridges.size());
		return _bridges[index];
	}

	virtual HRESULT InsertWire (ULONG i, com_ptr<IWire> wire) override
	{
		auto raw = wil::try_com_query_nothrow<IDispatch>(wire);
		auto args = MakeObjectCollectionPropertyChangeArgs(CollectionChangeType::Insert, i, 1, raw.addressof());
		auto hr = NotifyPropertyChanging (_propChangeCP, AsUnknown(), dispidWires, &args); LOG_IF_FAILED(hr);
		wire->set_parent(this);
		bool inserted = _wires.try_insert(_wires.begin() + i, std::move(wire)); RETURN_HR_IF(E_OUTOFMEMORY, !inserted);
		raw = nullptr;
		hr = NotifyPropertyChanged (_propChangeCP, AsUnknown(), dispidWires, &args); LOG_IF_FAILED(hr);
		return S_OK;
	}

	virtual HRESULT RemoveWire (ULONG i, _Outptr_opt_ IWire** ppRemoved = nullptr) override
	{
		auto res = _wires[i];

		com_ptr<IDispatch> child;
		auto args = MakeObjectCollectionPropertyChangeArgs(CollectionChangeType::Remove, i, 1, child.addressof());
		auto hr = NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidWires, &args); LOG_IF_FAILED(hr);
		_wires.erase(_wires.begin() + i);
		res->set_parent(nullptr);
		child = wil::try_com_query_nothrow<IDispatch>(res);
		hr = NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidWires, &args); LOG_IF_FAILED(hr);
		if (ppRemoved)
			*ppRemoved = res.detach();
		return S_OK;
	}

	virtual ULONG STDMETHODCALLTYPE WireCount() const noexcept override { return (ULONG)_wires.size(); }

	virtual IWire* STDMETHODCALLTYPE WireAt(ULONG index) const noexcept override
	{
		FAIL_FAST_IF(index >= _wires.size());
		return _wires[index];
	}

	#pragma region IBridgeEvents
	virtual HRESULT STDMETHODCALLTYPE OnLogLineGenerated (IBridge*, const BridgeLogLine*) override { return S_OK; }

	virtual HRESULT STDMETHODCALLTYPE OnLogCleared (IBridge*) override { return S_OK; }

	virtual HRESULT STDMETHODCALLTYPE OnPacketTransmit (IBridge* bridge, ULONG txPortIndex, packet_t&& pi) override
	{
		IPort* tx_port = bridge->PortAt(txPortIndex);
		IPort* rx_port = find_connected_port(tx_port);
		if (rx_port != nullptr)
			rx_port->bridge()->enqueue_received_packet(std::move(pi), rx_port->port_index());

		return S_OK;
	}
	#pragma endregion

	// IStpProject
	virtual bool IsWireForwarding (IWire* wire, unsigned int vlanNumber, _Out_opt_ bool* hasLoop) const override final
	{
		if (!std::holds_alternative<connected_wire_end>(wire->p0()) || !std::holds_alternative<connected_wire_end>(wire->p1()))
			return false;

		auto portA = std::get<connected_wire_end>(wire->p0());
		auto portB = std::get<connected_wire_end>(wire->p1());
		bool portAFw = portA->IsForwarding(vlanNumber);
		bool portBFw = portB->IsForwarding(vlanNumber);
		if (!portAFw || !portBFw)
			return false;

		if (hasLoop != nullptr)
		{
			std::unordered_set<IPort*> txPorts;

			std::function<bool(IPort* txPort)> transmitsTo = [this, vlanNumber, &txPorts, &transmitsTo, targetPort=portA](IPort* txPort) -> bool
			{
				if (txPort->IsForwarding(vlanNumber))
				{
					IPort* rx = find_connected_port(txPort);
					if ((rx != nullptr) && rx->IsForwarding(vlanNumber))
					{
						txPorts.insert(txPort);

						for (unsigned int i = 0; i < (unsigned int) rx->bridge()->PortCount(); i++)
						{
							if ((i != rx->port_index()) && rx->IsForwarding(vlanNumber))
							{
								IPort* otherTxPort = rx->bridge()->PortAt(i);
								if (otherTxPort == targetPort)
									return true;

								if (txPorts.find(otherTxPort) != txPorts.end())
									return false;

								if (transmitsTo(otherTxPort))
									return true;
							}
						}
					}
				}

				return false;
			};

			*hasLoop = transmitsTo(portA);
		}

		return true;
	}

	virtual mac_address alloc_mac_address_range (size_t count) override final
	{
		if (count >= 128)
			throw std::range_error("count must be lower than 128.");

		auto result = _nextBridgeAddress;
		_nextBridgeAddress[5] += (uint8_t)count;
		if (_nextBridgeAddress[5] < count)
		{
			_nextBridgeAddress[4]++;
			if (_nextBridgeAddress[4] == 0)
				WI_ASSERT(false); // not implemented
		}

		return result;
	}

	virtual HRESULT STDMETHODCALLTYPE GetFilePath (BSTR* pbstrFilePath) override
	{
		if (!_path)
		{
			if (pbstrFilePath)
				*pbstrFilePath = nullptr;
			return S_FALSE;
		}

		if (pbstrFilePath)
		{
			*pbstrFilePath = SysAllocString(_path.get()); RETURN_IF_NULL_ALLOC(*pbstrFilePath);
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Save (const wchar_t* path) override
	{
		HRESULT hr;

		_ASSERT (path || (_path && _path.get()[0]));

		// Save XML to temporary stream first. During debugging, the SaveToXml file crashes often and we lose the file content.
		auto memStreamRaw = SHCreateMemStream(nullptr, 0); RETURN_IF_NULL_ALLOC(memStreamRaw);
		com_ptr<IStream> memStream;
		memStream.attach (memStreamRaw);
		hr = SaveToXml(this, ProjectElementName, 0, memStream); RETURN_IF_FAILED(hr);

		STATSTG stat;
		hr = memStream->Stat(&stat, STATFLAG_NONAME); RETURN_IF_FAILED(hr);
		RETURN_HR_IF(ERROR_FILE_TOO_LARGE, !!stat.cbSize.HighPart);
		hr = memStream->Seek({ 0 }, STREAM_SEEK_SET, nullptr); RETURN_IF_FAILED(hr);

		com_ptr<IStream> stream;
		hr = SHCreateStreamOnFile(path ? path : _path.get(), STGM_CREATE | STGM_WRITE | STGM_SHARE_DENY_WRITE, &stream); RETURN_IF_FAILED_EXPECTED(hr);
		hr = IStream_Copy(memStream, stream, stat.cbSize.LowPart); RETURN_IF_FAILED(hr);
		stream.reset();

		if (path) {
			_path = wil::make_process_heap_string_nothrow(path); RETURN_IF_FAILED(hr);
		}

		this->SetChangedFlag(false);
		//saved_e::invoker(_em).invoke(this);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Load (const wchar_t* filePath) override
	{
		HRESULT hr;

		com_ptr<IStream> stream;
		hr = SHCreateStreamOnFileEx(filePath, STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL, 0, nullptr, &stream); RETURN_IF_FAILED_EXPECTED(hr);
		hr = LoadFromXml (this, ProjectElementName, stream.get()); RETURN_IF_FAILED(hr);

		_path = wil::make_process_heap_string_nothrow(filePath); RETURN_IF_NULL_ALLOC(_path);
		this->SetChangedFlag(false);
		_invalidateCP->Notify([](IInvalidateSink* sink) { return sink->OnInvalidate(nullptr); });
		_projectEventsCP->Notify([this](IProjectEventsSink* sink) { return sink->OnProjectLoaded(this); });
		return S_OK;
	}

	virtual void pause_simulation() override final
	{
		_simulationPaused = true;
		NotifyInvalidate(_invalidateCP, nullptr);
	}

	virtual void resume_simulation() override final
	{
		_simulationPaused = false;
		NotifyInvalidate(_invalidateCP, nullptr);
	}

	virtual bool simulation_paused() const override final { return _simulationPaused; }

	virtual bool GetChangedFlag() const override final { return _changedFlag; }

	virtual void SetChangedFlag (bool changedFlag) override final
	{
		if (_changedFlag != changedFlag)
		{
			_changedFlag = changedFlag;
			_projectEventsCP->Notify([this](IProjectEventsSink* sink) { return sink->OnProjectChangedFlagChanged(this); });
		}
	}

	//virtual const edge::typed_object_collection_property1<bridge>* bridges_property() const override final { return &bridges_prop; }

	//virtual const edge::typed_object_collection_property1<wire>* wires_property() const override final { return &wires_prop; }

	//virtual edge::property_changing_e::subscriber property_changing() override final { return edge::property_changing_e::subscriber(_em); }

	//virtual edge::property_changed_e::subscriber property_changed() override final { return edge::property_changed_e::subscriber(_em); }

	mac_address next_mac_address() const { return _nextBridgeAddress; }
};

//const edge::typed_object_collection_property1<bridge> StpProjectImpl::bridges_prop = {
//	"Bridges",
//	&StpProjectImpl::bridge_count,
//	&StpProjectImpl::bridge_at,
//	&StpProjectImpl::insert_bridge,
//	&StpProjectImpl::remove_bridge,
//};
//
//const edge::typed_object_collection_property1<wire> StpProjectImpl::wires_prop {
//	"Wires",
//	&StpProjectImpl::wire_count,
//	&StpProjectImpl::wire_at,
//	&StpProjectImpl::insert_wire,
//	&StpProjectImpl::remove_wire,
//};

HRESULT MakeProject (IStpProject** ppProject)
{
	auto p = com_ptr (new (std::nothrow) StpProjectImpl()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(); RETURN_IF_FAILED(hr);
	*ppProject = p.detach();
	return S_OK;
}
