
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pg_internal.h"

using namespace pg;
using namespace edge;

class object_collection_item : public object_collection_item_i
{
	ULONG _refCount = 0;
	IGroupItem* _parent;
	DISPID _prop;

public:
	HRESULT InitInstance (IGroupItem* parent, DISPID prop)
	{
		_parent = parent;
		_prop = prop;
		return S_OK;
	}

	IUnknown* AsUnknown() { return static_cast<IPGPropertyItem*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IItem>(static_cast<IPGPropertyItem*>(this), riid, ppvObject)
		)
			return S_OK;

		//if (riid == __uuidof(IWeakRef))
		//	return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region IItem
	virtual ULONG PerformLayoutCount() const noexcept override
	{
		_ASSERT(false); return { };
	}

	virtual void ResetPerformLayoutCount() noexcept override
	{
		_ASSERT(false);
	}

	virtual LONG Height() const noexcept override
	{
		_ASSERT(false); return { };
	}

	virtual HCURSOR cursor_at (POINT pd, LONG item_y) const override final
	{
		_ASSERT(false); return { };
	}

	virtual bool selectable() const override final
	{
		_ASSERT(false); return { };
	}

	virtual HRESULT STDMETHODCALLTYPE ProcessMouseDown (const edge::mouse_ud_args& ma, LONG item_y) noexcept override
	{
		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE ProcessMouseUp (const edge::mouse_ud_args& ma, LONG item_y) noexcept override
	{
		RETURN_HR(E_NOTIMPL);
	}

	virtual wil::unique_process_heap_string description_title() const override final
	{
		_ASSERT(false); return { };
	}

	virtual wil::unique_process_heap_string description_text() const override final
	{
		_ASSERT(false); return { };
	}

	STDMETHOD(GetValue)(read_state* pState, BSTR* pbstrValueText) override { RETURN_HR(E_NOTIMPL); }
	#pragma endregion

	#pragma region IPGPropertyItem
	virtual IItem* as_item() override final { return static_cast<IPGPropertyItem*>(this); }
	virtual IGroupItem* parent() const noexcept override { return _parent; }
	//virtual const edge::property* property() const override final { return _prop; }

	// These two functions are called from code in the object_item class, which listens to corresponding events.
	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanging (const PropertyChangeArgs *args) override
	{
		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanged (const PropertyChangeArgs *args) override
	{
		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	virtual ITypeInfo* TypeInfo() const override
	{
		_ASSERT(false); return { };
	}

	virtual DISPID property() const override final
	{
		return _prop;
	}

	virtual VARENUM VarType() const override
	{
		_ASSERT(false); return { };
	}

	virtual WORD GetterFuncIndex() const override
	{
		_ASSERT(false); return { };
	}

	bool multiple_child_types() const
	{
		//auto& objs = parent()->parent()->objects();
		_ASSERT(false); return false;
	}

	#pragma region IExpandableItem
	virtual uint32_t child_count() const override final { return 0; }

	virtual IItem* child_at(uint32_t index) const override final
	{
		_ASSERT(false); return { };
	}

	virtual bool expanded() const override final
	{
		return false;
	}

	virtual void expand() override final
	{
	}

	virtual void collapse() override final
	{
	}
	#pragma endregion

	#pragma region collection_item_i
	virtual size_t collection_entry_count() const override
	{
		return size_t();
	}
	virtual collection_existing_child_item_i* collection_entry_at(size_t index) const override
	{
		return nullptr;
	}

	virtual collection_new_child_item_i* collection_new_entry() const override
	{
		return nullptr;
	}
	#pragma endregion
};

HRESULT MakeObjectCollectionItem (IGroupItem* parent, DISPID prop, IPGPropertyItem** ppItem)
{
	auto p = com_ptr (new (std::nothrow) object_collection_item()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(parent, prop); RETURN_IF_FAILED(hr);
	*ppItem = p.detach();
	return S_OK;
}
