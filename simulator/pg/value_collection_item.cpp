
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pg_internal.h"

using namespace edge;
using namespace pg;

extern std::unique_ptr<value_collection_child_item_i> make_value_collection_child_item (value_collection_item_i* parent);
extern std::unique_ptr<collection_new_child_item_i> make_collection_new_child_item (collection_item_i* parent);

class value_collection_item : public value_collection_item_i
{
	ULONG _refCount = 0;
	IGroupItem* const _parent;
	DISPID const _prop;
	ULONG _performLayoutCount = 0;

	// TODO: get rid of this type, as we need to handle anyway all 4 combinations of null/non-null.
	struct children_t
	{
		std::vector<std::unique_ptr<value_collection_child_item_i>> existing_values;
		std::unique_ptr<collection_new_child_item_i> new_value;
	};

	std::optional<children_t> _children;

public:
	value_collection_item (IGroupItem* parent, DISPID prop)
		: _parent(parent), _prop(prop)
	{ }

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
	virtual HRESULT STDMETHODCALLTYPE PerformLayout (const PaintResources& ctx) noexcept override
	{
		_performLayoutCount++;
		RETURN_HR(E_NOTIMPL);
	}

	virtual ULONG PerformLayoutCount() const noexcept override { return _performLayoutCount; }
	virtual void ResetPerformLayoutCount() noexcept override { _performLayoutCount = 0; }

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

	#pragma region IExpandableItem
	virtual IItem* as_item() override final { return static_cast<IPGPropertyItem*>(this); }

	virtual uint32_t child_count() const override final
	{
		_ASSERT(false); return { };
	}

	virtual IItem* child_at (uint32_t index) const override final
	{
		_ASSERT(false); return { };
	}

	virtual bool expanded() const override final
	{
		return _children.has_value();
	}

	virtual void expand() override final
	{
		_ASSERT(false);
		/*
		_ASSERT (!_children);
		_children.emplace();
		auto& objs = parent()->parent()->objects();
		size_t child_count = property()->size(objs[0]);
		bool all_same_child_count = objs.all ([child_count, p=property()](object* o) { return p->size(o) == child_count; });
		_ASSERT (all_same_child_count);
		auto root = this->root();
		
		for (size_t i = 0; i < child_count; i++)
			_children->existing_values.push_back(make_value_collection_child_item(this));

		if (property()->can_insert_remove())
			_children->new_value = make_collection_new_child_item(this);

		this->grid()->invalidate();
		*/
	}

	virtual void collapse() override final
	{
		if (_children)
		{
			_children.reset();
			::InvalidateRect(root()->grid()->HWnd(), 0, 0);
		}
	}
	#pragma endregion

	#pragma region IPGPropertyItem
	virtual IGroupItem* parent() const override { return _parent; }

	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanging (const PropertyChangeArgs *args) override
	{
		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanged (const PropertyChangeArgs *args) override
	{
		RETURN_HR(E_NOTIMPL);
	}

	void on_selected_objects_inserting (std::span<IDispatch* const> objects_to_insert)
	{
	}

	void on_selected_objects_inserted (range_t range)
	{
		_ASSERT(false);
		//PerformLayout();
	}

	void on_selected_objects_removing (range_t range)
	{
	}

	void on_selected_objects_removed (std::span<IDispatch* const> objects_removed)
	{
		_ASSERT(false);
		//PerformLayout();
	}

	void on_selected_objects_changing (size_t from_index, std::span<IDispatch* const> objects_to_insert)
	{
		_ASSERT(false);
	}

	void on_selected_objects_changed (size_t from_index, std::span<IDispatch* const> objects_removed)
	{
		_ASSERT(false);
	}
	#pragma endregion

	#pragma region collection_item_i
	virtual size_t collection_entry_count() const override final
	{
		return _children ? _children->existing_values.size() : 0;
	}

	virtual collection_existing_child_item_i* collection_entry_at (size_t index) const override final
	{
		return _children->existing_values[index].get();
	}

	virtual collection_new_child_item_i* collection_new_entry() const override final
	{
		return _children ? _children->new_value.get() : nullptr;
	}
	#pragma endregion
};

std::unique_ptr<IPGPropertyItem> make_value_collection_item (IGroupItem* parent, DISPID prop)
{
	return std::make_unique<value_collection_item>(parent, prop);
}
