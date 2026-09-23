
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "../pch.h"
#include "../Simulator.h"
#include "../SimulatorAO.h"
#include "../resource.h"

HRESULT CreateBridgeAO (IBridge* bridge, IBridgeAO** ppBridgeAO);
HRESULT CreateWireAO (IWire* wire, IWireAO** ppWireAO);

class ProjectAOImpl : public IProjectAO, IGetWrappedObject
{
	ULONG _refCount = 0;
	com_ptr<IStpProject> _project;

public:
	HRESULT InitInstance (IStpProject* project)
	{
		_project = project;
		return S_OK;
	}

	IUnknown* AsUnknown() { return static_cast<IProjectAO*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IDispatch>(this, riid, ppvObject)
			|| TryQI<IProjectAO>(this, riid, ppvObject)
			|| TryQI<IGetWrappedObject>(this, riid, ppvObject)
		)
			return S_OK;

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	IMPLEMENT_IDISPATCH_(IProjectAO, nullptr, ID_TYPELIB_SIMULATOR_AO);

	#pragma region IProjectAO
	virtual HRESULT STDMETHODCALLTYPE AddBridge (DWORD portCount, DWORD mstiCount, IBridgeAO** ppBridgeAO) override
	{
		if (!ppBridgeAO) return E_POINTER;
		*ppBridgeAO = nullptr;

		HRESULT hr;
		DWORD rangeSize = (portCount + 15) & ~15; // Round up to the next multiple of 16 for the MAC address range.
		mac_address bridgeAddress;
		hr = _project->AllocMACAddressRange(rangeSize, bridgeAddress); LOG_HR_IF(hr, FAILED(hr));
		com_ptr<IBridge> bridge;
		hr = MakeBridge(portCount, mstiCount, bridgeAddress, &bridge); RETURN_IF_FAILED(hr);
		com_ptr<IBridgeAO> bridgeAO;
		hr = CreateBridgeAO(bridge, &bridgeAO); RETURN_IF_FAILED(hr);
		hr = _project->AddBridge(bridge); RETURN_IF_FAILED(hr);
		*ppBridgeAO = bridgeAO.detach();
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE AddWire (IPortAO* from, IPortAO* to, IWireAO** ppWire) override
	{
		HRESULT hr;

		RETURN_HR_IF(E_POINTER, !ppWire);
		*ppWire = nullptr;
		RETURN_HR_IF(E_POINTER, !from || !to);

		com_ptr<IGetWrappedObject> getWrapped0;
		hr = from->QueryInterface(IID_PPV_ARGS(getWrapped0.addressof())); RETURN_IF_FAILED(hr);
		com_ptr<IPort> port0;
		hr = getWrapped0->GetWrappedObject(IID_PPV_ARGS(port0.addressof())); RETURN_IF_FAILED(hr);
		RETURN_HR_IF(E_INVALIDARG, port0->bridge()->parent() != _project);

		com_ptr<IGetWrappedObject> getWrapped1;
		hr = to->QueryInterface(IID_PPV_ARGS(getWrapped1.addressof())); RETURN_IF_FAILED(hr);
		com_ptr<IPort> port1;
		hr = getWrapped1->GetWrappedObject(IID_PPV_ARGS(port1.addressof())); RETURN_IF_FAILED(hr);
		RETURN_HR_IF(E_INVALIDARG, port1->bridge()->parent() != _project);

		com_ptr<IWire> wire;
		hr = MakeWire(&wire); RETURN_IF_FAILED(hr);
		wire->set_p0(port0.get());
		wire->set_p1(port1.get());
		hr = _project->AddWire(std::move(wire)); RETURN_IF_FAILED(hr);
		return CreateWireAO(_project->WireAt(_project->WireCount() - 1), ppWire);
	}
	#pragma endregion

	virtual HRESULT STDMETHODCALLTYPE GetWrappedObject (REFIID riid, void** ppvObject) override
	{
		return _project->QueryInterface(riid, ppvObject);
	}
};

HRESULT CreateProjectAO (IStpProject* project, IProjectAO** ppProjectAO)
{
	auto p = com_ptr(new (std::nothrow) ProjectAOImpl());
	RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(project); RETURN_IF_FAILED(hr);
	*ppProjectAO = p.detach();
	return S_OK;
}
