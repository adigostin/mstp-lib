
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "Simulator_.h"
#include "edge/vector_nothrow.h"

using namespace edge;

class selection : public ISelection, IPropertyChangeSink, IConnectionPointContainer
{
	ULONG _refCount = 0;
	ULONG _sig = 0xAA550001;
	IStpProject* _project;
	vector_nothrow<com_ptr<IDispatch>> _objects;
	AdviseSinkToken _token;
	WeakRefToThis _weakRefToThis;
	com_ptr<ConnectionPointImpl<IObjectCollectionChangeEvents>> _occCP;

public:
	HRESULT InitInstance (IStpProject* project)
	{
		_project = project;

		auto hr = _weakRefToThis.InitInstance(static_cast<ISelection*>(this)); RETURN_IF_FAILED(hr);

		hr = MakeConnectionPoint(this, &_occCP); RETURN_IF_FAILED(hr);

		hr = AdviseSink<IPropertyChangeSink>(project, _weakRefToThis, &_token); LOG_IF_FAILED(hr);
		return S_OK;
	}

	IUnknown* AsUnknown() { return static_cast<ISelection*>(this); }
	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<ISelection>(this, riid, ppvObject)
			|| TryQI<IPropertyChangeSink>(this, riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject)
		)
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		RETURN_HR(E_NOINTERFACE);
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region IConnectionPointContainer
	virtual HRESULT STDMETHODCALLTYPE EnumConnectionPoints (IEnumConnectionPoints **ppEnum) override
	{
		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE FindConnectionPoint (REFIID riid, IConnectionPoint **ppCP) override
	{
		if (riid == __uuidof(IObjectCollectionChangeEvents))
			return wil::com_query_to_nothrow(_occCP, ppCP);
		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	#pragma region IPropertyChangeSink
	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanging (IUnknown* obj, DISPID dispID, const PropertyChangeArgs* args) override
	{
		auto project = wil::try_com_query_nothrow<IStpProject>(obj);
		if (dispID == dispidBridges)
		{
			WI_ASSERT(args->propertyType == PropertyType::Collection);
			if (args->collectionChangeArgs.changeType == CollectionChangeType::Remove)
			{
				// Removing bridge.
				RETURN_HR_IF(E_NOTIMPL, args->collectionChangeArgs.setInsertRemoveArgs.count != 1);
				ULONG index = args->collectionChangeArgs.setInsertRemoveArgs.index;
				auto disp = wil::try_com_query_nothrow<IDispatch>(project->BridgeAt(index));
				auto it = std::find(_objects.begin(), _objects.end(), disp.get());
				if (it != _objects.end())
				{
					uint32_t i = (uint32_t)(it - _objects.begin());
					remove_internal(i, 1);
				}
			}
		}
		else if (dispID == dispidWires)
		{
			WI_ASSERT(args->propertyType == PropertyType::Collection);
			if (args->collectionChangeArgs.changeType == CollectionChangeType::Remove)
			{
				// Removing wire.
				RETURN_HR_IF(E_NOTIMPL, args->collectionChangeArgs.setInsertRemoveArgs.count != 1);
				ULONG index = args->collectionChangeArgs.setInsertRemoveArgs.index;
				auto disp = wil::try_com_query_nothrow<IDispatch>(project->WireAt(index));
				auto it = std::find(_objects.begin(), _objects.end(), disp.get());
				if (it != _objects.end())
				{
					uint32_t i = (uint32_t)(it - _objects.begin());
					remove_internal(i, 1);
				}
			}
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanged (IUnknown* obj, DISPID dispID, const PropertyChangeArgs* args) override
	{
		return S_OK;
	}
	#pragma endregion

	#pragma region IObjectList
	virtual uint32_t ObjectCount() const override { return _objects.size(); }
	
	virtual IDispatch* ObjectAt (uint32_t index) const override { return _objects[index]; }

	virtual HRESULT STDMETHODCALLTYPE GetListTitle (BSTR* pbstrTitle) noexcept override
	{
		HRESULT hr;

		*pbstrTitle = nullptr;

		if (empty())
			return S_FALSE;

		if (size() == 1)
			return GetInstanceName(front(), pbstrTitle);

		com_ptr<ITypeInfo> ti;
		hr = AllSameType(begin(), end(), &ti); RETURN_IF_FAILED(hr);
		if (hr == S_FALSE)
		{
			*pbstrTitle = SysAllocString(L"(Multiple Selection)"); RETURN_IF_NULL_ALLOC(*pbstrTitle);
			return S_OK;
		}

		// All same type.
		LPOLESTR classNamePN = const_cast<LPOLESTR>(L"ClassName");
		MEMBERID memid;
		hr = front()->GetIDsOfNames(IID_NULL, &classNamePN, 1, LANG_INVARIANT, &memid);
		if (hr == DISP_E_UNKNOWNNAME)
			return S_FALSE;
		RETURN_IF_FAILED(hr);

		DISPPARAMS params = { };
		wil::unique_variant result;
		EXCEPINFO exception;
		UINT uArgErr;
		hr = front()->Invoke(memid, IID_NULL, LANG_INVARIANT, DISPATCH_PROPERTYGET,
			&params, &result, &exception, &uArgErr); RETURN_IF_FAILED(hr);
		RETURN_HR_IF(DISP_E_BADVARTYPE, result.vt != VT_BSTR);

		wil::unique_process_heap_string str;
		hr = wil::str_printf_nothrow(str, L"%s[%u]", result.bstrVal, size()); RETURN_IF_FAILED(hr);
		*pbstrTitle = SysAllocString(str.get()); RETURN_IF_NULL_ALLOC(*pbstrTitle);
		return S_OK;
	}
	#pragma endregion

	HRESULT STDMETHODCALLTYPE add_internal (IDispatch* o) noexcept
	{
		ObjectCollectionChangeArgs occ = { .changeType = Insert, .setInsertRemoveArgs = { .index = (ULONG)_objects.size(), .count = 1, .childObjs = &o } };
		_occCP->Notify([this,&occ](IObjectCollectionChangeEvents* e) {
			return e->OnCollectionChanging(AsUnknown(), &occ);
		});
		_objects.try_push_back(o);
		occ.setInsertRemoveArgs.childObjs = nullptr;
		_occCP->Notify([this,&occ](IObjectCollectionChangeEvents* e) {
			return e->OnCollectionChanged(AsUnknown(), &occ);
		});
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE remove_internal (uint32_t index, uint32_t size) noexcept
	{
		if (size)
		{
			ObjectCollectionChangeArgs occ = { .changeType = CollectionChangeType::Remove, .setInsertRemoveArgs = { .index = (ULONG)index, .count = (ULONG)size } };
			_occCP->Notify([this,&occ](IObjectCollectionChangeEvents* e) { return e->OnCollectionChanging(AsUnknown(), &occ); });
			std::vector<com_ptr<IDispatch>> removed;
			std::copy(_objects.begin() + index, _objects.begin() + index + size, std::back_inserter(removed));
			_objects.erase(_objects.begin() + index, _objects.begin() + index + size);
			vector_nothrow<IDispatch*> punks;
			for (auto d : removed)
				punks.try_push_back(d.try_query<IDispatch>().get());
			occ.setInsertRemoveArgs.childObjs = punks.data();
			_occCP->Notify([this,&occ](IObjectCollectionChangeEvents* e) { return e->OnCollectionChanged(AsUnknown(), &occ); });
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Clear() noexcept override
	{
		return remove_internal(0, _objects.size());
	}

	virtual HRESULT STDMETHODCALLTYPE Select (IDispatch* o) noexcept override
	{
		RETURN_HR_IF(E_INVALIDARG, o == nullptr);

		if ((_objects.size() != 1) || (_objects[0] != o))
		{
			remove_internal (0, _objects.size());
			add_internal(o);
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Add (IDispatch* o) noexcept override
	{
		RETURN_HR_IF(E_INVALIDARG, o == nullptr);

		if (std::find (_objects.begin(), _objects.end(), o) != _objects.end())
			RETURN_HR(HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));

		return add_internal(o);
	}

	virtual HRESULT STDMETHODCALLTYPE Remove (IDispatch* o) noexcept override
	{
		RETURN_HR_IF(E_INVALIDARG, o == nullptr);

		auto it = std::find (_objects.begin(), _objects.end(), o);
		if (it == _objects.end())
			RETURN_HR(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
		
		uint32_t index = (uint32_t)(it - _objects.begin());
		return remove_internal(index, 1);
	}
};

extern HRESULT selection_factory(IStpProject* project, ISelection** ppSelection)
{
	auto p = com_ptr(new (std::nothrow) selection()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(project); RETURN_IF_FAILED(hr);
	*ppSelection = p.detach();
	return S_OK;
};
