
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "edge/unordered_map_nothrow.h"

using namespace edge;

struct TreeSelection : IObjectList, IConnectionPointContainer, IObjectCollectionChangeEvents, IVlanSelectionEvents, IStpPropertyChangeSink
{
	ULONG _refCount = 0;
	WeakRefToThis _weakRefToThis;
	com_ptr<IObjectList> _selection;
	com_ptr<IVlanSelection> _vlanSel;
	AdviseSinkToken _vlanChangeToken;
	AdviseSinkToken _selectionEventsToken;
	com_ptr<ConnectionPointImpl<IObjectCollectionChangeEvents>> _occCP;
	unordered_map_nothrow<IBridge*, std::pair<AdviseSinkToken, ULONG>> _stpPropChangeTokens;

	HRESULT InitInstance (IObjectList* selection, IVlanSelection* vlanSel)
	{
		auto hr = _weakRefToThis.InitInstance(AsUnknown()); RETURN_IF_FAILED(hr);

		hr = MakeConnectionPoint<IObjectCollectionChangeEvents>(this, &_occCP); RETURN_IF_FAILED(hr);

		_selection = selection;
		_vlanSel = vlanSel;

		if (_vlanSel)
		{
			hr = AdviseSink<IVlanSelectionEvents>(_vlanSel, _weakRefToThis, &_vlanChangeToken); RETURN_IF_FAILED(hr);
		}

		hr = AdviseSink<IObjectCollectionChangeEvents>(_selection, _weakRefToThis, &_selectionEventsToken); RETURN_IF_FAILED(hr);
		if (_selection->size())
		{
			// Notify the initial selection is being inserted.
			ObjectCollectionChangeArgs args = { .changeType = CollectionChangeType::Insert };
			args.setInsertRemoveArgs.count = _selection->size();
			hr = OnObjectsInserted (&args); RETURN_IF_FAILED(hr);
		}

		return S_OK;
	}

	~TreeSelection()
	{
		if (_selection->size())
		{
			// Notify the remaining selection is being removed.
			ObjectCollectionChangeArgs args = { .changeType = CollectionChangeType::Remove };
			args.setInsertRemoveArgs.count = _selection->size();
			auto hr = OnObjectsRemoving(&args); LOG_IF_FAILED(hr);
		}
		_selectionEventsToken.reset();
		_ASSERT(_stpPropChangeTokens.empty());
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
			|| TryQI<IVlanSelectionEvents>(this, riid, ppvObject)
			|| TryQI<IStpPropertyChangeSink>(this, riid, ppvObject)
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

	uint32_t selected_tree_index(IBridge* bridge) const
	{
		if (!_vlanSel)
			return 0;

		DWORD vlan = 0;
		auto hr = _vlanSel->GetSelectedVlan(&vlan); LOG_IF_FAILED(hr);
		return STP_GetTreeIndexFromVlanNumber(bridge->stp_bridge(), vlan);
	}

	#pragma region IObjectList
	virtual uint32_t ObjectCount() const override
	{
		return _selection->size();
	}

	virtual IDispatch* ObjectAt (uint32_t index) const override
	{
		IDispatch* outer = _selection->ObjectAt(index);
		if (auto b = wil::try_com_query_nothrow<IBridge>(outer))
			return wil::try_com_query_nothrow<IDispatch>(b->TreeAt(selected_tree_index(b)));

		if (auto p = wil::try_com_query_nothrow<IPort>((*_selection)[index]))
			return wil::try_com_query_nothrow<IDispatch>(p->treeAt(selected_tree_index(p->bridge())));

		_ASSERT(false); return { };
	}

	virtual HRESULT STDMETHODCALLTYPE GetListTitle(BSTR* pbstrTitle) noexcept override
	{
		HRESULT hr;

		*pbstrTitle = nullptr;

		if (empty())
			return S_FALSE;

		if (size() == 1)
		{
			wil::unique_bstr title;
			hr = GetInstanceName(front(), &title); RETURN_IF_FAILED(hr);
			if (!_vlanSel)
			{
				*pbstrTitle = title.release();
				return S_OK;
			}

			DWORD vlan = 0;
			hr = _vlanSel->GetSelectedVlan(&vlan); LOG_IF_FAILED(hr);
			wil::unique_process_heap_string str;
			hr = wil::str_printf_nothrow(str, L"%s (VLAN %u)", title, vlan); RETURN_IF_FAILED(hr);
			*pbstrTitle = SysAllocString(str.get()); RETURN_IF_NULL_ALLOC(*pbstrTitle);
			return S_OK;
		}

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
		/*
		uint32_t vlan = 0;
		do
		{
			if (table[vlan].treeIndex == _tree_index)
				break;
			vlan++;
		} while (vlan < entryCount);

		wil::unique_process_heap_string str;
		if (_tree_index == 0)
		{
			hr = wil::str_printf_nothrow (str, L"VLAN %u (CIST)", vlan); RETURN_IF_FAILED(hr);
		}
		else
		{
			hr = wil::str_printf_nothrow (str, L"VLAN %u (MSTI %u)", vlan, _tree_index); RETURN_IF_FAILED(hr);
		}

		*pName = SysAllocString(str.get()); RETURN_IF_NULL_ALLOC(*pName);
		*/
		wil::unique_process_heap_string str;
		hr = wil::str_printf_nothrow(str, L"%s[%u]", result.bstrVal, size()); RETURN_IF_FAILED(hr);
		*pbstrTitle = SysAllocString(str.get()); RETURN_IF_NULL_ALLOC(*pbstrTitle);
		return S_OK;

	}
	#pragma endregion

	// IStpPropertyChangeSink
	virtual HRESULT STDMETHODCALLTYPE OnStpPropertyChanging (IBridge* b, unsigned int portIndex,
		unsigned int treeIndex, STP_PROPERTY prop, unsigned int timestamp) noexcept override
	{
		// See comments in OnStpPropertyChanged.
		if (prop == STP_PROPERTY_STP_VERSION
			|| (prop == STP_PROPERTY_MST_CONFIG_TABLE && STP_GetStpVersion(b->stp_bridge()) >= STP_VERSION_MSTP))
		{
			ObjectCollectionChangeArgs args = { .changeType = CollectionChangeType::Set, .setInsertRemoveArgs = { .count = 1 } };
			args.setInsertRemoveArgs.count = _selection->size();
			_occCP->Notify([this,&args](IObjectCollectionChangeEvents* s) {
				return s->OnCollectionChanging(AsUnknown(), &args);
			});
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnStpPropertyChanged (IBridge* b, unsigned int portIndex,
		unsigned int treeIndex, STP_PROPERTY prop, unsigned int timestamp) noexcept override
	{
		// If the STP version is changed to/from MSTP, or if the MST config table is changed, the mapping
		// of VLANs to trees changes. In that case, we need to notify that the selection has changed.
		if (prop == STP_PROPERTY_STP_VERSION
			|| (prop == STP_PROPERTY_MST_CONFIG_TABLE && STP_GetStpVersion(b->stp_bridge()) >= STP_VERSION_MSTP))
		{
			// Here we'd have to find out which index in the selection is the bridge whose property changed,
			// or which ports belong to the bridge whose property changed, and notify that those objects
			// have been changed. For ports that's too complicated, so we just notify that all selection changed.
			ObjectCollectionChangeArgs args = { .changeType = CollectionChangeType::Set };
			args.setInsertRemoveArgs.count = _selection->size();
			_occCP->Notify([this,&args](IObjectCollectionChangeEvents* s) {
				return s->OnCollectionChanged(AsUnknown(), &args);
			});
		}

		return S_OK;
	}

	HRESULT OnObjectsInserting (const ObjectCollectionChangeArgs* args)
	{
		HRESULT hr;

		auto& a = args->setInsertRemoveArgs;
		_ASSERT(a.count == 1); // only this supported for now
		if (auto b = wil::try_com_query_nothrow<IBridge>(a.childObjs[0]))
		{
			auto bridgeTree = b->TreeAt(selected_tree_index(b));
			com_ptr<IDispatch> bridgeTreeDisp = wil::try_com_query_nothrow<IDispatch>(bridgeTree);
			ObjectCollectionChangeArgs treeArgs = *args;
			treeArgs.setInsertRemoveArgs.childObjs = bridgeTreeDisp.addressof();
			hr = _occCP->Notify([this,&treeArgs](IObjectCollectionChangeEvents* s) {
				return s->OnCollectionChanging(AsUnknown(), &treeArgs); }); LOG_IF_FAILED(hr);
			return S_OK;
		}
		else if (auto p = wil::try_com_query_nothrow<IPort>(a.childObjs[0]))
		{
			auto portTree = p->treeAt(selected_tree_index(p->bridge()));
			com_ptr<IDispatch> portTreeDisp = wil::try_com_query_nothrow<IDispatch>(portTree);
			ObjectCollectionChangeArgs treeArgs = *args;
			treeArgs.setInsertRemoveArgs.childObjs = portTreeDisp.addressof();
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

	HRESULT OnObjectsInserted (const ObjectCollectionChangeArgs* args)
	{
		HRESULT hr;

		_occCP->Notify([this,args](IObjectCollectionChangeEvents* s) {
			return s->OnCollectionChanged(AsUnknown(), args);
		});

		auto& a = args->setInsertRemoveArgs;

		if (   wil::try_com_query_nothrow<IBridge>((*_selection)[a.index])
			|| wil::try_com_query_nothrow<IPort>((*_selection)[a.index]))
		{
			// Once insertion of bridges or ports is done, we need to start listening for events that
			// might change the vlan/tree mapping. This mapping changes when changing the STP version
			// to/from MSTP, and when changing the MST config table while the STP version is MSTP.
			if (wil::try_com_query_nothrow<IBridge>(_selection->ObjectAt(a.index)))
			{
				for (uint32_t i = 0; i < a.count; i++)
				{
					com_ptr<IBridge> b;
					hr = _selection->ObjectAt(a.index + i)->QueryInterface(IID_PPV_ARGS(b.addressof())); RETURN_IF_FAILED(hr);
					auto it = _stpPropChangeTokens.find(b); RETURN_HR_IF(E_UNEXPECTED, it != _stpPropChangeTokens.end());
					AdviseSinkToken token;
					hr = AdviseSink<IStpPropertyChangeSink>(b, _weakRefToThis, &token); RETURN_IF_FAILED(hr);
					bool inserted = _stpPropChangeTokens.try_insert({ b, std::make_pair(std::move(token), 1u) }); RETURN_HR_IF(E_OUTOFMEMORY, !inserted);
				}
			}
			else //if (wil::try_com_query_nothrow<IPort>(_selection->ObjectAt(a.index)))
			{
				for (uint32_t i = 0; i < a.count; i++)
				{
					com_ptr<IPort> p;
					hr = _selection->ObjectAt(a.index + i)->QueryInterface(IID_PPV_ARGS(p.addressof())); RETURN_IF_FAILED(hr);
					auto it = _stpPropChangeTokens.find(p->bridge());
					if (it == _stpPropChangeTokens.end())
					{
						AdviseSinkToken token;
						hr = AdviseSink<IStpPropertyChangeSink>(p->bridge(), _weakRefToThis, &token); RETURN_IF_FAILED(hr);
						bool inserted = _stpPropChangeTokens.try_insert({ p->bridge(), std::make_pair(std::move(token), 1u) }); RETURN_HR_IF(E_OUTOFMEMORY, !inserted);
					}
					else
					{
						it->second.second++;
					}
				}
			}
		}
		else if (wil::try_com_query_nothrow<IWire>((*_selection)[a.index]))
		{
		}
		else
			RETURN_HR(E_NOTIMPL);

		return S_OK;
	}

	HRESULT OnObjectsRemoving (const ObjectCollectionChangeArgs* args)
	{
		HRESULT hr;

		auto& a = args->setInsertRemoveArgs;

		if (   wil::try_com_query_nothrow<IBridge>((*_selection)[a.index])
			|| wil::try_com_query_nothrow<IPort>((*_selection)[a.index]))
		{
			if (wil::try_com_query_nothrow<IBridge>(_selection->ObjectAt(a.index)))
			{
				for (uint32_t i = 0; i < a.count; i++)
				{
					com_ptr<IBridge> b;
					hr = _selection->ObjectAt(a.index + i)->QueryInterface(IID_PPV_ARGS(b.addressof())); _ASSERT(SUCCEEDED(hr));
					auto it = _stpPropChangeTokens.find(b);
					_ASSERT(it != _stpPropChangeTokens.end());
					_ASSERT(it->second.second == 1);
					_stpPropChangeTokens.erase(it);
				}
			}
			else //if (auto p = wil::try_com_query_nothrow<IPort>(_selection->ObjectAt(a.index)))
			{
				for (uint32_t i = 0; i < a.count; i++)
				{
					com_ptr<IPort> p;
					hr = _selection->ObjectAt(a.index + i)->QueryInterface(IID_PPV_ARGS(p.addressof())); _ASSERT(SUCCEEDED(hr));
					auto it = _stpPropChangeTokens.find(p->bridge());
					_ASSERT(it != _stpPropChangeTokens.end());
					if (it->second.second > 1)
						it->second.second--;
					else
						_stpPropChangeTokens.erase(it);
				}
			}
		}
		else if (wil::try_com_query_nothrow<IWire>((*_selection)[a.index]))
		{
		}
		else
			RETURN_HR(E_NOTIMPL);

		_occCP->Notify([this,args](IObjectCollectionChangeEvents* s) {
			return s->OnCollectionChanging(AsUnknown(), args);
		});

		return S_OK;
	}

	HRESULT OnObjectsRemoved (const ObjectCollectionChangeArgs* args)
	{
		HRESULT hr;

		auto& a = args->setInsertRemoveArgs;
		if (wil::try_com_query_nothrow<IBridge>(a.childObjs[0]))
		{
			vector_nothrow<IDispatch*> trees;
			bool reserved = trees.try_reserve(a.count); RETURN_HR_IF(E_OUTOFMEMORY, !reserved);

			for (uint32_t i = 0; i < a.count; i++)
			{
				auto b = wil::try_com_query_nothrow<IBridge>(a.childObjs[i]);
				auto bridgeTree = b->TreeAt(selected_tree_index(b));
				trees.try_push_back(wil::try_com_query_nothrow<IDispatch>(bridgeTree));
			}

			ObjectCollectionChangeArgs treeArgs = *args;
			treeArgs.setInsertRemoveArgs.childObjs = trees.data();
			hr = _occCP->Notify([this,&treeArgs](IObjectCollectionChangeEvents* s) {
				return s->OnCollectionChanged(AsUnknown(), &treeArgs);
			}); LOG_IF_FAILED(hr);
			
			return S_OK;
		}
		else if (auto p = wil::try_com_query_nothrow<IPort>(a.childObjs[0]))
		{
			vector_nothrow<IDispatch*> trees;
			bool reserved = trees.try_reserve(a.count); RETURN_HR_IF(E_OUTOFMEMORY, !reserved);

			for (uint32_t i = 0; i < a.count; i++)
			{
				auto port = wil::try_com_query_nothrow<IPort>(a.childObjs[i]);
				auto portTree = port->treeAt(selected_tree_index(port->bridge()));
				trees.try_push_back(wil::try_com_query_nothrow<IDispatch>(portTree));
			}

			ObjectCollectionChangeArgs treeArgs = *args;
			treeArgs.setInsertRemoveArgs.childObjs = trees.data();
			hr = _occCP->Notify([this,&treeArgs](IObjectCollectionChangeEvents* s) {
				return s->OnCollectionChanged(AsUnknown(), &treeArgs);
			}); LOG_IF_FAILED(hr);
			
			return S_OK;
		}
		else
		{
			_ASSERT(a.count == 1); // only this supported for now
			RETURN_HR(E_NOTIMPL);
		}
	}

	#pragma region IObjectCollectionChangeEvents
	virtual HRESULT STDMETHODCALLTYPE OnCollectionChanging (IUnknown *sender, const struct ObjectCollectionChangeArgs *args) override
	{
		if (args->changeType == CollectionChangeType::Insert)
			return OnObjectsInserting(args);

		if (args->changeType == CollectionChangeType::Remove)
			return OnObjectsRemoving(args);

		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE OnCollectionChanged (IUnknown *sender, const struct ObjectCollectionChangeArgs *args) override
	{
		if (args->changeType == CollectionChangeType::Insert)
			return OnObjectsInserted(args);

		if (args->changeType == CollectionChangeType::Remove)
			return OnObjectsRemoved(args);

		RETURN_HR(E_NOTIMPL);
	}

	#pragma region IVlanSelectionEvents
	virtual HRESULT STDMETHODCALLTYPE OnVlanSelectionChanging (DWORD dwOld) override
	{
		ObjectCollectionChangeArgs occ = { .changeType = CollectionChangeType::Set, .setInsertRemoveArgs = { .index = 0, .count = _selection->size() } };
		_occCP->Notify([this,&occ](IObjectCollectionChangeEvents* e) {
			return e->OnCollectionChanging(AsUnknown(), &occ);
		});
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE OnVlanSelectionChanged (DWORD dwNew) override
	{
		ObjectCollectionChangeArgs occ = { .changeType = CollectionChangeType::Set, .setInsertRemoveArgs = { .index = 0, .count = _selection->size() } };
		_occCP->Notify([this,&occ](IObjectCollectionChangeEvents* e) {
			return e->OnCollectionChanged(AsUnknown(), &occ);
		});
		return S_OK;
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
