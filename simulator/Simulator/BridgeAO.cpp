
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "SimulatorAO_h.h"
#include "edge/com.h"
#include "resource.h"

class BridgeAOImpl : public IBridgeAO, IGetWrappedObject
{
	ULONG _refCount = 0;
	com_ptr<IBridge> _bridge;

public:
	HRESULT InitInstance (IBridge* bridge)
	{
		_bridge = bridge;
		return S_OK;
	}

	IUnknown* AsUnknown() { return static_cast<IBridgeAO*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IDispatch>(this, riid, ppvObject)
			|| TryQI<IBridgeAO>(this, riid, ppvObject)
			|| TryQI<IGetWrappedObject>(this, riid, ppvObject)
		)
			return S_OK;

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	IMPLEMENT_IDISPATCH_(IBridgeAO, nullptr, ID_TYPELIB_SIMULATOR_AO);

	#pragma region IBridgeAO
	virtual HRESULT STDMETHODCALLTYPE get_STPVersion (enum STPVersion *pVersion) override
	{
		return _bridge.try_query<IBridgeProperties>()->get_STPVersion(pVersion);
	}

	virtual HRESULT STDMETHODCALLTYPE put_STPVersion (enum STPVersion version) override
	{
		return _bridge.try_query<IBridgeProperties>()->put_STPVersion(version);
	}

	virtual HRESULT STDMETHODCALLTYPE LoadTestMstConfig1() override
	{
		auto treeCount = 1 + STP_GetMstiCount(_bridge->stp_bridge());

		STP_CONFIG_TABLE_ENTRY entries[1 + max_vlan_number];

		// VLAN0 does not exist.
		entries[0] = { 0, 0 };

		unsigned char treeIndex = 0;
		for (unsigned int vid = 1; vid <= max_vlan_number; vid++)
		{
			entries[vid] = { .unused = 0, .treeIndex = treeIndex };
			treeIndex++;
			if (treeIndex == treeCount)
				treeIndex = 0;
		}

		STP_SetMstConfigTable(_bridge->stp_bridge(), entries, 1 + max_vlan_number, ::GetMessageTime());
		return S_OK;
	}
	#pragma endregion

	// IGetWrappedObject
	virtual HRESULT STDMETHODCALLTYPE GetWrappedObject (REFIID riid, void** ppvObject) override
	{
		return _bridge->QueryInterface(riid, ppvObject);
	}
};

HRESULT CreateBridgeAO (IBridge* bridge, IBridgeAO** ppBridgeAO)
{
	auto p = com_ptr(new (std::nothrow) BridgeAOImpl()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(bridge); RETURN_IF_FAILED(hr);
	*ppBridgeAO = p.detach();
	return S_OK;
}