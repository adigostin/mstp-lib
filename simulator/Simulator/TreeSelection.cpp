
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"

using namespace edge;

struct TreeSelection : IObjectList, IConnectionPointContainer, IObjectCollectionChangeEvents
{
	ULONG _refCount = 0;
	WeakRefToThis _weakRefToThis;
	com_ptr<IObjectList> _selection;
	DWORD _selectedVlan;
	AdviseSinkToken _vlanChangeToken;
	AdviseSinkToken _selectionEventsToken;
	com_ptr<ConnectionPointImpl<IObjectCollectionChangeEvents>> _occCP;

	HRESULT InitInstance (IObjectList* selection, IVlanSelection* vlanSel)
	{
		auto hr = _weakRefToThis.InitInstance(AsUnknown()); RETURN_IF_FAILED(hr);
		hr = MakeConnectionPoint<IObjectCollectionChangeEvents>(this, &_occCP); RETURN_IF_FAILED(hr);

		_selection = selection;

		hr = vlanSel->GetSelectedVlan(&_selectedVlan); RETURN_IF_FAILED(hr);
		// TODO: react to changes of the selected vlan
		//hr = AdviseSink<IVlanSelectionEvents>(vlanSel, _weakRefToThis, &_vlanChangeToken); RETURN_IF_FAILED(hr);

		hr = AdviseSink<IObjectCollectionChangeEvents>(_selection, _weakRefToThis, &_selectionEventsToken); RETURN_IF_FAILED(hr);
		return S_OK;
	}

	IUnknown* AsUnknown() { return static_cast<IObjectList*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IObjectList>(this, riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject)
			|| TryQI<IObjectCollectionChangeEvents>(this, riid, ppvObject)
			)
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
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

	#pragma region IObjectList
	virtual uint32_t size() const override
	{
		return _selection->size();
	}

	virtual IDispatch* operator[](uint32_t index) const override
	{
		IDispatch* outer = _selection->operator[](index);
		if (auto b = wil::try_com_query_nothrow<IBridge>((*_selection)[index]))
		{
			uint32_t treeIndex = STP_GetTreeIndexFromVlanNumber(b->stp_bridge(), _selectedVlan);
			return wil::try_com_query_nothrow<IDispatch>(b->trees()[treeIndex]);
		}
		else if (auto p = wil::try_com_query_nothrow<IPort>((*_selection)[index]))
		{
			uint32_t treeIndex = STP_GetTreeIndexFromVlanNumber(p->bridge()->stp_bridge(), _selectedVlan);
			return wil::try_com_query_nothrow<IDispatch>(p->treeAt(treeIndex));
		}
		else
		{
			_ASSERT(false); return { };
		}
	}
	#pragma endregion

	#pragma region IObjectCollectionChangeEvents
	virtual HRESULT STDMETHODCALLTYPE OnCollectionChanging (IUnknown *sender, const struct ObjectCollectionChangeArgs *args) override
	{
		HRESULT hr;

		if (args->changeType == CollectionChangeType::Insert)
		{
			// Inserting
			auto& a = args->setInsertRemoveArgs;
			_ASSERT(a.count == 1); // only this supported for now
			if (auto b = wil::try_com_query_nothrow<IBridge>(a.childObjs[0]))
			{
				uint32_t treeIndex = STP_GetTreeIndexFromVlanNumber(b->stp_bridge(), _selectedVlan);
				com_ptr<IDispatch> tree = wil::try_com_query_nothrow<IDispatch>(b->trees()[treeIndex]);
				ObjectCollectionChangeArgs treeArgs = *args;
				treeArgs.setInsertRemoveArgs.childObjs = tree.addressof();
				hr = _occCP->Notify([this,&treeArgs](IObjectCollectionChangeEvents* s) {
					return s->OnCollectionChanging(AsUnknown(), &treeArgs); }); LOG_IF_FAILED(hr);
				return S_OK;
			}
			else if (auto p = wil::try_com_query_nothrow<IPort>(a.childObjs[0]))
			{
				uint32_t treeIndex = STP_GetTreeIndexFromVlanNumber(p->bridge()->stp_bridge(), _selectedVlan);
				com_ptr<IDispatch> tree = wil::try_com_query_nothrow<IDispatch>(p->treeAt(treeIndex));
				ObjectCollectionChangeArgs treeArgs = *args;
				treeArgs.setInsertRemoveArgs.childObjs = tree.addressof();
				hr = _occCP->Notify([this,&treeArgs](IObjectCollectionChangeEvents* s) {
					return s->OnCollectionChanging(AsUnknown(), &treeArgs); }); LOG_IF_FAILED(hr);
				return S_OK;
			}
			else if (auto w = wil::try_com_query_nothrow<IWire>(a.childObjs[0]))
			{
				// Just make sure the selection is empty or consists only of wires. Nothing to do.
				_ASSERT(_selection->all([](IDispatch* o) { return !!wil::try_com_query_nothrow<IWire>(o); }));
				return S_OK;
			}
			else
				RETURN_HR(E_NOTIMPL);
		}
		else if (args->changeType == CollectionChangeType::Remove)
		{
			// Removing
			hr = _occCP->Notify([this,args](IObjectCollectionChangeEvents* s) {
				return s->OnCollectionChanging(AsUnknown(), args); }); LOG_IF_FAILED(hr);
			return S_OK;
		}
		else
			RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE OnCollectionChanged (IUnknown *sender, const struct ObjectCollectionChangeArgs *args) override
	{
		HRESULT hr;

		if (args->changeType == CollectionChangeType::Insert)
		{
			// Inserted
			auto& a = args->setInsertRemoveArgs;
			_ASSERT(a.count == 1); // only this supported for now
			if (   wil::try_com_query_nothrow<IBridge>((*_selection)[a.index])
				|| wil::try_com_query_nothrow<IPort>((*_selection)[a.index]))
			{
				hr = _occCP->Notify([this,args](IObjectCollectionChangeEvents* s) {
					return s->OnCollectionChanged(AsUnknown(), args); }); LOG_IF_FAILED(hr);
				return S_OK;
			}
			else if (wil::try_com_query_nothrow<IWire>((*_selection)[a.index]))
			{
				RETURN_HR(E_NOTIMPL);
			}
			else
				RETURN_HR(E_NOTIMPL);
		}
		else if (args->changeType == CollectionChangeType::Remove)
		{
			// Removed
			auto& a = args->setInsertRemoveArgs;
			if (wil::try_com_query_nothrow<IBridge>(a.childObjs[0]))
			{
				vector_nothrow<IDispatch*> trees;
				bool reserved = trees.try_reserve(a.count); RETURN_HR_IF(E_OUTOFMEMORY, !reserved);

				for (uint32_t i = 0; i < a.count; i++)
				{
					auto b = wil::try_com_query_nothrow<IBridge>(a.childObjs[i]);
					uint32_t treeIndex = STP_GetTreeIndexFromVlanNumber(b->stp_bridge(), _selectedVlan);
					trees.try_push_back(wil::try_com_query_nothrow<IDispatch>(b->trees()[treeIndex]).get());
				}

				ObjectCollectionChangeArgs treeArgs = *args;
				treeArgs.setInsertRemoveArgs.childObjs = trees.data();
				hr = _occCP->Notify([this,&treeArgs](IObjectCollectionChangeEvents* s) {
					return s->OnCollectionChanged(AsUnknown(), &treeArgs); }); LOG_IF_FAILED(hr);
				return S_OK;
			}
			else if (auto p = wil::try_com_query_nothrow<IPort>(a.childObjs[0]))
			{
				_ASSERT(a.count == 1); // only this supported for now
				uint32_t treeIndex = STP_GetTreeIndexFromVlanNumber(p->bridge()->stp_bridge(), _selectedVlan);
				ObjectCollectionChangeArgs treeArgs = *args;
				com_ptr<IDispatch> tree = wil::try_com_query_nothrow<IDispatch>(p->treeAt(treeIndex));
				treeArgs.setInsertRemoveArgs.childObjs = tree.addressof();
				hr = _occCP->Notify([this,&treeArgs](IObjectCollectionChangeEvents* s) {
					return s->OnCollectionChanged(AsUnknown(), &treeArgs); }); LOG_IF_FAILED(hr);
				return S_OK;
			}
			else
			{
				_ASSERT(a.count == 1); // only this supported for now
				RETURN_HR(E_NOTIMPL);
			}
		}
		else
			RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion
};

HRESULT MakeTreeSelection (edge::IObjectList* selection, IVlanSelection* vlanSel, IObjectList** ppTreeSelection)
{
	auto p = com_ptr(new (std::nothrow) TreeSelection()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(selection, vlanSel); RETURN_IF_FAILED(hr);
	*ppTreeSelection = p.detach();
	return S_OK;
}
