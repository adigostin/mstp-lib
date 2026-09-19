
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "include/pg/property_grid.h"
#include "object_item.h"

using namespace edge;
using namespace pg;

class root_item : public IRootItem, IObjectCollectionChangeEvents
{
	ULONG _refCount = 0;
	ULONG _sig = 0xAA55000D;
	WeakRefToThis _weakRefToThis;
	IPGInternal* _grid;
	bool _showEmptySel;
	com_ptr<IObjectList> _ol;
	edge::string_convert_context_i* _scc;
	com_ptr<IObjectItemChildManager> _child_manager;

	static constexpr LONG udPadding = 5;

	struct layout
	{
		wil::unique_process_heap_string text;
		LONG height;
	};

	std::optional<layout> _layout;

	AdviseSinkToken _collectionChangeToken;

public:
	HRESULT InitInstance (IPGInternal* grid, bool showEmptySel, IObjectList* ol, edge::string_convert_context_i* scc)
	{
		HRESULT hr;
		
		_grid = grid;
		_showEmptySel = showEmptySel;
		_ol = ol;
		_scc = scc;

		hr = _weakRefToThis.InitInstance(AsUnknown()); RETURN_IF_FAILED(hr);

		hr = MakeObjectItemChildManager(this, ol, &_child_manager); RETURN_IF_FAILED(hr);

		// Start listening to changes in the list of objects (objects arriving or removing).
		hr = AdviseSink<IObjectCollectionChangeEvents>(_ol, _weakRefToThis, &_collectionChangeToken); RETURN_IF_FAILED(hr);

		//PerformLayoutDC(hdc, dpi(_grid->HWnd()));
		return S_OK;
	}

	IUnknown* AsUnknown() { return static_cast<IItem*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IItem>(this, riid, ppvObject)
			|| TryQI<IObjectCollectionChangeEvents>(this, riid, ppvObject)
			|| TryQI<IExpandableItem>(this, riid, ppvObject)
			|| TryQI<IRootItem>(this, riid, ppvObject)
			|| TryQI<IObjectItem>(this, riid, ppvObject)
		)
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region IRootItem
	virtual IPGInternal* grid() const override final { return _grid; }

	virtual edge::string_convert_context_i* app_context() const override final { return _scc; }
	#pragma endregion

	#pragma region IObjectItem
	virtual IGroupItem* ChildGroupItemAt(uint32_t index) const override { return _child_manager->ChildAt(index); }

	virtual IObjectList* objects() override final { return _ol; }
	#pragma endregion

	#pragma region IItem
	virtual IExpandableItem* parent() const override { return nullptr; }

	virtual HRESULT STDMETHODCALLTYPE PerformLayout (const PaintResources& ctx) noexcept override
	{
		HRESULT hr;

		LONG layout_width = _grid->ValueColumnRight(ctx.dpi) - _grid->ExpandColumnLeft(ctx.dpi) - 2 * title_lr_padding;
		if (layout_width < 0)
		{
			_layout.reset();
			return S_FALSE;
		}
		
		wil::unique_process_heap_string text;
		if (_ol->empty())
		{
			if (_showEmptySel)
			{
				text = wil::make_process_heap_string_nothrow(L"(no selection)");
			}
		}
		else if (_ol->size() == 1)
		{
			com_ptr<ITypeInfo> ti;
			hr = _ol->front()->GetTypeInfo(0, LANG_INVARIANT, &ti); RETURN_IF_FAILED(hr);
			LPOLESTR name = const_cast<LPOLESTR>(L"__id");
			MEMBERID memid;
			DISPPARAMS params = { };
			wil::unique_variant result;
			EXCEPINFO exception;
			UINT uArgErr;
			if (SUCCEEDED(ti->GetIDsOfNames (&name, 1, &memid))
				&& SUCCEEDED(ti->Invoke(_ol->front(), memid, DISPATCH_PROPERTYGET, &params, &result, &exception, &uArgErr))
				&& result.vt == VT_BSTR)
			{
				text = wil::make_process_heap_string_nothrow(result.bstrVal);
			}
			else
			{
				text = wil::make_process_heap_string_nothrow(L"Properties");
			}
		}
		else
		{
			wil::unique_process_heap_string str;
			hr = wil::str_printf_nothrow (str, L"%u elements", _ol->size());
			text = std::move(str);
		}

		LONG udPadding = (LONG)std::round(this->udPadding * ctx.dpi / 96.0f);

		_layout = layout {
			.text = std::move(text),
			.height = udPadding + ctx.tmCaptionFont.tmHeight - ctx.tmCaptionFont.tmInternalLeading + udPadding
		};

		_grid->InvalidateItem(this);

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Paint (HDC hdc, const PaintResources& ctx,
		PaintItemFlags flags, LONG y, edge::IThemeColorProvider* tcp) const noexcept override
	{
		if (!_layout)
			return S_FALSE;

		COLORREF back = tcp->color_win32(theme_color::active_caption_back);
		wil::unique_hbrush backbrush (CreateSolidBrush(back));
		RECT rect = { _grid->ExpandColumnLeft(ctx.dpi), y, _grid->ValueColumnRight(ctx.dpi), y + _layout->height };
		FillRect(hdc, &rect, backbrush.get());

		LONG udPadding = (LONG)std::round(this->udPadding * ctx.dpi / 96.0f);
		rect.top = rect.top + udPadding - ctx.tmCaptionFont.tmInternalLeading / 2;
		auto undosel = wil::SelectObject(hdc, ctx.captionFont.get());
		DrawTextW (hdc, _layout->text.get(), -1, &rect, DT_SINGLELINE | DT_CENTER | DT_TOP);

		return S_OK;
	}

	virtual LONG Height() const noexcept override
	{
		return _layout ? _layout->height : 0;
	}

	virtual HCURSOR cursor_at (POINT pd, LONG item_y) const override { return ::LoadCursor(nullptr, IDC_ARROW); }
	virtual bool selectable() const override { return false; }
	virtual HRESULT STDMETHODCALLTYPE ProcessMouseDown (const edge::mouse_ud_args& ma, LONG item_y) noexcept override { return S_OK; }
	virtual HRESULT STDMETHODCALLTYPE ProcessMouseUp (const edge::mouse_ud_args& ma, LONG item_y) noexcept override { return S_OK; }
	virtual wil::unique_process_heap_string description_title() const override { return { }; }
	virtual wil::unique_process_heap_string description_text() const override { return { }; }
	virtual root_item* as_root() override final { return this; }
	virtual IExpandableItem* AsExpandable() override { return this; }
	#pragma endregion

	#pragma region IExpandableItem
	virtual IItem* as_item() override { return this; }

	virtual uint32_t child_count() const override { return _child_manager->ChildCount(); }
	
	virtual IGroupItem* child_at(uint32_t index) const override final
	{
		return _child_manager->ChildAt(index);
	}

	virtual bool expanded() const override { return true; }
	virtual void expand() override { _ASSERT(false); }
	virtual void collapse() override { _ASSERT(false); }
	#pragma endregion

	#pragma region IObjectCollectionChangeEvents
	virtual HRESULT STDMETHODCALLTYPE OnCollectionChanging (IUnknown *sender, const ObjectCollectionChangeArgs *args) override
	{
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnCollectionChanged (IUnknown *sender, const ObjectCollectionChangeArgs *args) override
	{
		return root()->grid()->NotifyLayoutChangedTree(this);
	}
	#pragma endregion
};

HRESULT MakeRootItem (IPGInternal* grid, bool showEmptySel, IObjectList* objects,
					  edge::string_convert_context_i* scc, IRootItem** ppRootItem)
{
	auto p = com_ptr(new (std::nothrow) root_item()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(grid, showEmptySel, objects, scc); RETURN_IF_FAILED(hr);
	*ppRootItem = p.detach();
	return S_OK;
}
