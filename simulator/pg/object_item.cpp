
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "object_item.h"
#include "include/pg/property_grid.h"
#include "edge/PropDefs.h"
#include "EdgeIDL.h"

using namespace pg;
using namespace edge;

extern HRESULT MakeGroupItem (IObjectItem* parent, wil::unique_bstr idlName, IGroupItem** ppItem);

struct ObjectItemChildManager : IObjectItemChildManager, IPropertyChangeSink, IObjectCollectionChangeEvents
{
	ULONG _refCount = 0;
	ULONG _sig = 0xAA55000a;
	WeakRefToThis _weakRefToThis;
	IObjectItem* _owner;
	com_ptr<IObjectList> _selected_objects;
	vector_nothrow<com_ptr<IGroupItem>> _children;

	std::unordered_map<IUnknown*, AdviseSinkToken> _tokens;
	AdviseSinkToken _collectionChangeToken;

	HRESULT InitInstance (IObjectItem* owner, IObjectList* selected_objects)
	{
		auto hr = _weakRefToThis.InitInstance(AsUnknown()); RETURN_IF_FAILED(hr);

		_owner = owner;
		_selected_objects = selected_objects;

		BOOL unused;
		on_selected_objects_inserted({ .changeType = Insert, .setInsertRemoveArgs = { 0, _selected_objects->size() } }, &unused);

		hr = AdviseSink<IObjectCollectionChangeEvents>(_selected_objects, _weakRefToThis, &_collectionChangeToken); RETURN_IF_FAILED(hr);

		return S_OK;
	}

	~ObjectItemChildManager()
	{
		on_selected_objects_removing({ .changeType = Remove, .setInsertRemoveArgs = { 0, _selected_objects->size() } });
	}

	IUnknown* AsUnknown() { return static_cast<IObjectItemChildManager*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IObjectItemChildManager>(this, riid, ppvObject)
			|| TryQI<IPropertyChangeSink>(this, riid, ppvObject)
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

	virtual ULONG ChildCount() override { return _children.size(); }

	virtual IGroupItem* ChildAt(ULONG i) override { return _children[i]; }

	virtual IObjectList* selected_objects() const { return _selected_objects; }

	void register_property_change_events (range_t range)
	{
		for (uint32_t i = range.from; i < range.to; i++)
		{
			auto pUnkSource = wil::try_com_query_nothrow<IUnknown>((*_selected_objects)[i]);
			auto it = _tokens.find(pUnkSource);
			_ASSERT(it == _tokens.end());
			AdviseSinkToken token;
			auto hr = AdviseSink<IPropertyChangeSink>(pUnkSource, _weakRefToThis, &token); LOG_IF_FAILED(hr);
			_tokens[pUnkSource] = std::move(token);
		}
	}

	void unregister_property_change_events (range_t range)
	{
		for (int i = (int)range.to - 1; i >= (int)range.from; i--)
		{
			auto pUnkSource = wil::try_com_query_nothrow<IUnknown>((*_selected_objects)[i]);
			auto it = _tokens.find(pUnkSource);
			_ASSERT(it != _tokens.end());
			_tokens.erase(it);
		}
	}

	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanging (IUnknown *obj, DISPID dispID, const PropertyChangeArgs* args) override
	{
		for (auto& gi : _children)
		{
			for (auto& pi : gi->children())
			{
				if (pi->property() == dispID)
					pi->OnPropertyChanging(args);
			}
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanged (IUnknown* obj, DISPID dispID, const PropertyChangeArgs *args) override
	{
		for (auto& gi : this->_children)
		{
			for (auto& pi : gi->children())
			{
				if (pi->property() == dispID)
					pi->OnPropertyChanged(args);
			}
		}

		return S_OK;
	}

	static HRESULT make_group_list (IObjectList* objs, vector_nothrow<wil::unique_bstr>& groups)
	{
		groups.clear();
		if (objs->empty())
			return S_OK;

		com_ptr<ITypeInfo> typeInfo;
		auto hr = objs->front()->GetTypeInfo(0, LANG_INVARIANT, &typeInfo); RETURN_IF_FAILED(hr);
		TYPEATTR* typeAttr;
		hr = typeInfo->GetTypeAttr(&typeAttr); RETURN_IF_FAILED(hr);
		auto releaseTypeAttr = wil::scope_exit([&typeInfo,typeAttr] { typeInfo->ReleaseTypeAttr(typeAttr); });

		for (uint32_t i = 1; i < objs->size(); i++)
		{
			com_ptr<ITypeInfo> ti;
			hr = (*objs)[i]->GetTypeInfo(0, LANG_INVARIANT, &ti); RETURN_IF_FAILED(hr);
			TYPEATTR* ta;
			hr = ti->GetTypeAttr(&ta); RETURN_IF_FAILED(hr);
			auto releaseTA = wil::scope_exit([&ti,ta] { ti->ReleaseTypeAttr(ta); });
			if (!IsEqualGUID(typeAttr->guid, ta->guid))
				return S_OK; // Not going to handle multiple types selected, so just return an empty group list.
		}

		com_ptr<ITypeInfo2> ti2;
		hr = typeInfo->QueryInterface(&ti2); RETURN_IF_FAILED(hr);

		struct GroupNamePrio
		{
			wil::unique_bstr idlName;
			wil::unique_process_heap_string name;
			LONG prio;
		};

		vector_nothrow<GroupNamePrio> sorted;

		for (WORD i = 0; i < typeAttr->cFuncs; i++)
		{
			FUNCDESC* fd;
			hr = typeInfo->GetFuncDesc(i, &fd); RETURN_IF_FAILED(hr);
			auto releaseFundDesc = wil::scope_exit([ti=typeInfo.get(), fd] { ti->ReleaseFuncDesc(fd); });
			if ((fd->wFuncFlags & FUNCFLAG_FNONBROWSABLE) == 0 && fd->invkind == INVOKE_PROPERTYGET)
			{
				wil::unique_bstr idlName;
				wil::unique_variant data;
				if (SUCCEEDED(ti2->GetFuncCustData(i, guidPropertyGroup, &data)) && data.vt == VT_BSTR)
					idlName = wil::unique_bstr(data.release().bstrVal);

				wil::unique_process_heap_string name;
				LONG prio = 0;
				if (idlName)
				{
					if (const wchar_t* sep = wcschr(idlName.get(), L'\\'))
					{
						name = wil::make_process_heap_string_nothrow(idlName.get(), sep - idlName.get()); RETURN_IF_NULL_ALLOC(name);
						prio = wcstol(sep + 1, nullptr, 10);
					}
					else
					{
						name = wil::make_process_heap_string_nothrow(idlName.get()); RETURN_IF_NULL_ALLOC(name);
						prio = 0;
					}
				}

				auto it = sorted.begin();
				while (true)
				{
					if (it == sorted.end())
					{
						sorted.try_insert(it, { std::move(idlName), std::move(name), prio });
						break;
					}

					int cmp = wcscmp(it->name ? it->name.get() : L"", name ? name.get() : L"");

					if (it->prio > prio || (it->prio == prio && cmp > 0))
					{
						sorted.try_insert(it, { std::move(idlName), std::move(name), prio });
						break;
					}

					if (it->prio == prio && cmp == 0)
						break;

					it++;
				}

				// Have we looked at this group in a previous property?
				//if (groups.contains(groupName))
				//	continue;
				//bool seen_before = false;
				//for (auto pe1 = objs.front()->type()->make_property_enumerator(); *pe1 != *pe; pe1++)
				//{
				//	if (auto uiprop1 = dynamic_cast<const ui_property_i*>(*pe1); uiprop1 && uiprop1->group() == uiprop->group())
				//	{
				//		seen_before = true;
				//		break;
				//	}
				//}
				//if (seen_before)
				//	continue;

				//bool missing_in_any_other_object = false;
				//for (uint32_t i = 1; i < objs->size(); i++)
				//{
				//	bool present_in_this_object = false;
				//	for (auto pe1 = objs[i]->type()->make_property_enumerator(); pe1; pe1++)
				//	{
				//		if (auto uiprop1 = dynamic_cast<const ui_property_i*>(*pe1); uiprop1 && uiprop1->group() == uiprop->group())
				//		{
				//			present_in_this_object = true;
				//			break;
				//		}
				//	}
				//
				//	if (!present_in_this_object)
				//	{
				//		missing_in_any_other_object = true;
				//		break;
				//	}
				//}
				//
				//if (!missing_in_any_other_object)
				//	groups.insert(uiprop->group());
			}
		}

		bool reserved = groups.try_reserve(sorted.size()); RETURN_HR_IF(E_OUTOFMEMORY, !reserved);
		for (auto& np : sorted)
			groups.try_push_back(std::move(np.idlName));
		return S_OK;
	}

	#pragma region IObjectCollectionChangeEvents
	virtual HRESULT STDMETHODCALLTYPE OnCollectionChanging (IUnknown *sender, const ObjectCollectionChangeArgs* args) override
	{
		if (args->changeType == CollectionChangeType::Insert)
			return on_selected_objects_inserting(args);

		if (args->changeType == CollectionChangeType::Remove)
			return on_selected_objects_removing(*args);

		if (args->changeType == CollectionChangeType::Set)
			return on_selected_objects_replacing(args);

		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE OnCollectionChanged (IUnknown *sender, const ObjectCollectionChangeArgs* args) override
	{
		HRESULT hr;
		auto grid = _owner->as_item()->root()->grid();

		if (args->changeType == CollectionChangeType::Insert)
		{
			BOOL itemsInserted;
			hr = on_selected_objects_inserted(*args, &itemsInserted); RETURN_IF_FAILED(hr);
			if (itemsInserted)
				grid->NotifyLayoutChangedTree(_owner->as_item());
			return S_OK;
		}

		if (args->changeType == CollectionChangeType::Remove)
		{
			BOOL itemsInserted;
			hr = on_selected_objects_removed(args, &itemsInserted); RETURN_IF_FAILED(hr);
			if (itemsInserted)
				grid->NotifyLayoutChangedTree(_owner->as_item());
			return S_OK;
		}

		if (args->changeType == CollectionChangeType::Set)
			return on_selected_objects_replaced(args);

		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	HRESULT on_selected_objects_inserting (const ObjectCollectionChangeArgs* args)
	{
		HRESULT hr;

		auto grid = _owner->as_item()->root()->grid();

		// For each existing group item, we look at its group and we check if it's missing
		// in any of the objects to insert. If it's missing, we remove that group item.

		auto& a = args->setInsertRemoveArgs;
		bool invalidate = false;
		for (uint32_t ci = 0; ci < _children.size(); )
		{
			const wchar_t* existingGroupName = _children[ci]->group();

			bool missing_in_objects_to_insert = false;
			for (IDispatch* oi : std::span<IDispatch* const>(a.childObjs, a.count))
			{
				bool present_in_this_object = false;
			
				if (oi)
				{
					com_ptr<ITypeInfo> ti;
					hr = oi->GetTypeInfo(0, LANG_INVARIANT, &ti); RETURN_IF_FAILED(hr);

					com_ptr<ITypeInfo2> ti2;
					hr = ti->QueryInterface(IID_PPV_ARGS(&ti2)); RETURN_IF_FAILED(hr);

					TYPEATTR* attr;
					hr = ti->GetTypeAttr(&attr); RETURN_IF_FAILED(hr);
					auto releaseattr = wil::scope_exit([&ti,attr] { ti->ReleaseTypeAttr(attr); });

					for (WORD fi = 0; fi < attr->cFuncs; fi++)
					{
						FUNCDESC* fd;
						hr = ti->GetFuncDesc(fi, &fd); RETURN_IF_FAILED(hr);
						auto releasefd = wil::scope_exit([&ti,fd] { ti->ReleaseFuncDesc(fd); });

						if ((fd->wFuncFlags & FUNCFLAG_FNONBROWSABLE) || fd->invkind != INVOKE_PROPERTYGET)
							continue;
						
						wil::unique_variant custData;
						wil::unique_bstr name;
						if (SUCCEEDED(ti2->GetFuncCustData(fi, guidPropertyGroup, &custData)) && custData.vt == VT_BSTR)
							name = wil::unique_bstr(custData.release().bstrVal);

						if ((!name && !existingGroupName)
							|| (name && existingGroupName && !wcscmp(name.get(), existingGroupName)))
						{
							present_in_this_object = true;
							break;
						}
					}
				}

				if (!present_in_this_object)
				{
					missing_in_objects_to_insert = true;
					break;
				}
			}

			if (missing_in_objects_to_insert)
			{
				grid->NotifyItemRemoving(_children[ci].get());
				_children.erase(_children.begin() + ci);
				invalidate = true;
			}
			else
				ci++;
		}

		if (invalidate)
			::InvalidateRect(grid->HWnd(), 0, 0);

		return S_OK;
	}

	HRESULT on_selected_objects_inserted (const ObjectCollectionChangeArgs& args, BOOL* pbGroupItemsCreated)
	{
		*pbGroupItemsCreated = FALSE;

		auto grid = _owner->as_item()->root()->grid();
		auto& a = args.setInsertRemoveArgs;
		if (a.count == _selected_objects->size())
		{
			// Objects have been inserted into an empty object list. We create group items for all of their groups.
			vector_nothrow<wil::unique_bstr> groups;
			auto hr = make_group_list(_selected_objects, groups); RETURN_IF_FAILED(hr);
			for (auto& group : groups)
			{
				com_ptr<IGroupItem> gi;
				hr = MakeGroupItem(_owner, std::move(group), &gi); RETURN_IF_FAILED(hr);
				_children.try_push_back(std::move(gi));
				*pbGroupItemsCreated = TRUE;
			}
		}
	
		register_property_change_events({ a.index, a.index + a.count });

		return S_OK;
	}

	HRESULT on_selected_objects_removing (const ObjectCollectionChangeArgs& args)
	{
		auto grid = _owner->as_item()->root()->grid();

		auto& a = args.setInsertRemoveArgs;
		unregister_property_change_events({ a.index, a.index + a.count });

		bool invalidate = false;
		if (a.count == _selected_objects->size())
		{
			// Last remaining objects are being removed from the list.
			while (_children.size())
			{
				grid->NotifyItemRemoving(_children.back().get());
				_children.erase(_children.end() - 1);
				invalidate = true;
			}
		}

		if (invalidate)
			::InvalidateRect(grid->HWnd(), 0, 0);

		return S_OK;
	}

	HRESULT on_selected_objects_removed (const ObjectCollectionChangeArgs* args, BOOL* pbGroupItemsCreated)
	{
		// Find groups that are present in all remaining objects and missing in some removed objects,
		// create group items for every such group, and insert them at the right place.

		*pbGroupItemsCreated = FALSE;

		vector_nothrow<wil::unique_bstr> new_groups;
		auto hr = make_group_list(_selected_objects, new_groups); RETURN_IF_FAILED(hr);
		auto new_it = new_groups.begin();
		uint32_t i = 0;
		auto root = _owner->as_item()->root();
		while (new_it != new_groups.end())
		{
			if ((i == _children.size()) || wcscmp(_children[i]->group(), new_it->get()))
			{
				com_ptr<IGroupItem> gi;
				hr = MakeGroupItem(_owner, std::move(*new_it), &gi); RETURN_IF_FAILED(hr);
				_children.try_insert(_children.begin() + i, { std::move(gi) });
				*pbGroupItemsCreated = TRUE;
			}
			new_it++;
			i++;
		}

		return S_OK;
	}

	HRESULT on_selected_objects_replacing (const ObjectCollectionChangeArgs* args)
	{
		// When replacing objects in a selection, we may not know what the incoming objects are.
		// Requiring implementors to provide these would put too much burden on them.
		// For example when mstp-lib calls the "changing" callback with STP_PROPERTY_STP_VERSION,
		// we don't know what the new stp version is going to be, so we don't know what
		// bridge/port trees we'll need to select.
		//
		// To keep things simple, we clear all our children; we'll probably recreate most of
		// them in on_selected_objects_replaced(), but that's an acceptable tradeoff.

		auto grid = _owner->as_item()->root()->grid();

		auto& a = args->setInsertRemoveArgs;
		unregister_property_change_events({ a.index, a.index + a.count });

		while (_children.size())
		{
			grid->NotifyItemRemoving(_children.back().get());
			_children.erase(_children.end() - 1);
		}

		::InvalidateRect(grid->HWnd(), 0, 0);

		return S_OK;
	}

	HRESULT on_selected_objects_replaced (const ObjectCollectionChangeArgs* args)
	{
		HRESULT hr;

		// TODO: optimize this, then take care to call register_property_change_events() here.

		BOOL groupItemsCreated1;
		hr = on_selected_objects_inserted (*args, &groupItemsCreated1); RETURN_IF_FAILED(hr);

		BOOL groupItemsCreated2;
		hr = on_selected_objects_removed (args, &groupItemsCreated2); RETURN_IF_FAILED(hr);

		if (groupItemsCreated1 || groupItemsCreated2)
		{
			auto grid = _owner->as_item()->root()->grid();
			grid->NotifyLayoutChangedTree(_owner->as_item());
		}

		return S_OK;
	}
};

HRESULT pg::MakeObjectItemChildManager (IObjectItem* owner, IObjectList* selected_objects, IObjectItemChildManager** ppMan)
{
	auto p = com_ptr(new (std::nothrow) ObjectItemChildManager()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(owner, selected_objects); RETURN_IF_FAILED(hr);
	*ppMan = p.detach();
	return S_OK;
}
