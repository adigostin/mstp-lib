
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "SimulatorAO_h.h"
#include "edge/com.h"
#include "resource.h"

HRESULT CreateBridgeAO (IBridge* bridge, IBridgeAO** ppBridgeAO);

class ProjectAOImpl : public IProjectAO
{
	ULONG _refCount = 0;
	com_ptr<IStpProject> _project;

public:
	HRESULT InitInstance (IStpProject* project)
	{
		_project = project;
		return S_OK;
	}

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(this, riid, ppvObject)
			|| TryQI<IDispatch>(this, riid, ppvObject)
			|| TryQI<IProjectAO>(this, riid, ppvObject)
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
	#pragma endregion
};

HRESULT CreateProjectAO (IStpProject* project, IProjectAO** ppProjectAO)
{
	auto p = com_ptr(new (std::nothrow) ProjectAOImpl());
	RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(project); RETURN_IF_FAILED(hr);
	*ppProjectAO = p.detach();
	return S_OK;
}