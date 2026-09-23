
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "../pch.h"
#include "../Simulator_.h"
#include "../SimulatorAO.h"
#include "../resource.h"

HRESULT CreateProjectAO(IStpProject* project, IProjectAO** ppProjectAO);

class ProjectWindowAOImpl : public IProjectWindowAO
{
	ULONG _refCount = 0;
	com_ptr<IProjectWindow> _projectWindow;
	com_ptr<IProjectAO> _projectAO;

public:
	HRESULT InitInstance(IProjectWindow* projectWindow)
	{
		_projectWindow = projectWindow;
		return S_OK;
	}

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(static_cast<IProjectWindowAO*>(this), riid, ppvObject)
			|| TryQI<IDispatch>(this, riid, ppvObject)
			|| TryQI<IProjectWindowAO>(this, riid, ppvObject))
			return S_OK;

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	IMPLEMENT_IDISPATCH_(IProjectWindowAO, nullptr, ID_TYPELIB_SIMULATOR_AO);

	#pragma region IProjectWindowAO
	virtual HRESULT STDMETHODCALLTYPE GetProject(IProjectAO** ppProjectAO) override
	{
		RETURN_HR_IF(E_POINTER, !ppProjectAO);
		*ppProjectAO = nullptr;

		if (!_projectAO)
		{
			auto hr = CreateProjectAO(_projectWindow->project(), &_projectAO); RETURN_IF_FAILED(hr);
		}

		return _projectAO.copy_to(ppProjectAO);
	}

	virtual HRESULT STDMETHODCALLTYPE SelectBridge(IBridgeAO* bridgeAO) override
	{
		RETURN_HR_IF(E_POINTER, !bridgeAO);
		com_ptr<ISelection> selection;
		auto hr = _projectWindow->GetSelection(&selection); RETURN_IF_FAILED(hr);
		com_ptr<IGetWrappedObject> getWrapped;
		hr = bridgeAO->QueryInterface(IID_PPV_ARGS(getWrapped.addressof())); RETURN_IF_FAILED(hr);
		com_ptr<IDispatch> bridge;
		hr = getWrapped->GetWrappedObject(IID_PPV_ARGS(bridge.addressof())); RETURN_IF_FAILED(hr);
		return selection->Select(bridge);
	}

	virtual HRESULT STDMETHODCALLTYPE SelectWire(IWireAO* wire) override
	{
		RETURN_HR_IF(E_POINTER, !wire);
		com_ptr<ISelection> selection;
		auto hr = _projectWindow->GetSelection(&selection); RETURN_IF_FAILED(hr);
		com_ptr<IGetWrappedObject> getWrapped;
		hr = wire->QueryInterface(IID_PPV_ARGS(getWrapped.addressof())); RETURN_IF_FAILED(hr);
		com_ptr<IDispatch> dispatch;
		hr = getWrapped->GetWrappedObject(IID_PPV_ARGS(dispatch.addressof())); RETURN_IF_FAILED(hr);
		return selection->Select(dispatch);
	}

	virtual HRESULT STDMETHODCALLTYPE ClearSelection() override
	{
		com_ptr<ISelection> selection;
		auto hr = _projectWindow->GetSelection(&selection); RETURN_IF_FAILED(hr);
		return selection->Clear();
	}

	virtual HRESULT STDMETHODCALLTYPE DeleteSelection() override
	{
		com_ptr<ISelection> selection;
		auto hr = _projectWindow->GetSelection(&selection); RETURN_IF_FAILED(hr);
		return _projectWindow->project()->DeleteObjects(selection);
	}

	virtual HRESULT STDMETHODCALLTYPE SelectVlan(DWORD vlanNumber) override
	{
		com_ptr<IVlanSelection> vlanSelection;
		auto hr = _projectWindow->GetVlanSelection(&vlanSelection); RETURN_IF_FAILED(hr);
		return vlanSelection->SelectVlan(vlanNumber);
	}
	#pragma endregion
};

HRESULT CreateProjectWindowAO(IProjectWindow* projectWindow, IProjectWindowAO** ppProjectWindowAO)
{
	auto p = com_ptr(new (std::nothrow) ProjectWindowAOImpl()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(projectWindow); RETURN_IF_FAILED(hr);
	*ppProjectWindowAO = p.detach();
	return S_OK;
}
