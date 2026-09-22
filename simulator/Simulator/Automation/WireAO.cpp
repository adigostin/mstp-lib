// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "../pch.h"
#include "../simulator.h"
#include "../SimulatorAO_h.h"
#include "../resource.h"

class WireAOImpl : public IWireAO, IGetWrappedObject
{
	ULONG _refCount = 0;
	com_ptr<IWire> _wire;

public:
	HRESULT InitInstance(IWire* wire)
	{
		_wire = wire;
		return S_OK;
	}

	IUnknown* AsUnknown() { return static_cast<IWireAO*>(this); }

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;
		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IDispatch>(this, riid, ppvObject)
			|| TryQI<IWireAO>(this, riid, ppvObject)
			|| TryQI<IGetWrappedObject>(this, riid, ppvObject))
			return S_OK;
		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }
	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }

	IMPLEMENT_IDISPATCH_(IWireAO, nullptr, ID_TYPELIB_SIMULATOR_AO);

	virtual HRESULT STDMETHODCALLTYPE GetWrappedObject(REFIID riid, void** ppvObject) override
	{
		return _wire->QueryInterface(riid, ppvObject);
	}
};

HRESULT CreateWireAO(IWire* wire, IWireAO** ppWireAO)
{
	RETURN_HR_IF(E_POINTER, !ppWireAO);
	*ppWireAO = nullptr;
	auto p = com_ptr(new (std::nothrow) WireAOImpl()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(wire); RETURN_IF_FAILED(hr);
	*ppWireAO = p.detach();
	return S_OK;
}
