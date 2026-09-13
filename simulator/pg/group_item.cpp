
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pg_internal.h"
#include "edge/PropDefs.h"

using namespace pg;
using namespace edge;

extern HRESULT MakePropertyItem (IGroupItem* parent, DISPID prop, IPGPropertyItem** ppItem);

class group_item : public IGroupItem
{
	ULONG _refCount = 0;
	ULONG _sig = 0xAA550009;
	IObjectItem* _parent;
	wil::unique_bstr _idlName;
	const LONG udPadding = 3;

	struct layout
	{
		wil::unique_process_heap_string text;
		LONG height;
	};

	std::optional<layout> _layout;

	vector_nothrow<com_ptr<IPGPropertyItem>> _children;

public:
	HRESULT InitInstance (IObjectItem* parent, wil::unique_bstr idlName)
	{
		_parent = parent;
		_idlName = std::move(idlName);
//		_parent->objects()->objects_change().add_handler<&group_item::on_selected_objects_change>(this);
		auto grid = root()->grid();
		auto dc = wil::GetDC(grid->HWnd());
		UINT dpi = edge::dpi(grid->HWnd());
		//PerformLayoutDC(dc.get(), dpi);
		auto root = this->root();
		std::vector<DISPID> props;
		auto hr = make_property_list (_parent->objects(), _idlName.get(), props); LOG_IF_FAILED(hr);
		for (DISPID prop : props)
		{
			com_ptr<IPGPropertyItem> child;
			hr = MakePropertyItem(this, prop, &child); LOG_IF_FAILED(hr);
			_children.try_push_back(std::move(child));
		}

		return S_OK;
	}

	~group_item()
	{
//		_parent->objects()->objects_change().remove_handler<&group_item::on_selected_objects_change>(this);
	}

	IUnknown* AsUnknown() { return static_cast<IItem*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IItem>(this, riid, ppvObject)
			|| TryQI<IGroupItem>(this, riid, ppvObject)
			|| TryQI<IExpandableItem>(this, riid, ppvObject)
		)
			return S_OK;

		//if (riid == __uuidof(IWeakRef))
		//	return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region IGroupItem
	virtual IObjectItem* parent() const noexcept override { return _parent; }

	virtual const wchar_t* group() const override { return _idlName.get(); }

	virtual std::span<com_ptr<IPGPropertyItem> const> children() const override { return _children; }
	#pragma endregion

	static HRESULT make_property_list (IObjectList* objs, const wchar_t* group, std::vector<DISPID>& props)
	{
		HRESULT hr;
		
		props.clear();

		if (!objs->empty())
		{
			wil::com_ptr_nothrow<ITypeInfo> typeInfo;
			hr = objs->front()->GetTypeInfo(0, InvariantLCID, &typeInfo); RETURN_IF_FAILED(hr);
			TYPEATTR* typeAttr;
			hr = typeInfo->GetTypeAttr(&typeAttr); RETURN_IF_FAILED(hr);
			auto releaseTypeAttr = wil::scope_exit([ti=typeInfo.get(), typeAttr] { ti->ReleaseTypeAttr(typeAttr); });

			com_ptr<ITypeInfo2> ti2;
			hr = typeInfo->QueryInterface(&ti2); RETURN_IF_FAILED(hr);

			for (WORD i = 0; i < typeAttr->cFuncs; i++)
			{
				FUNCDESC* fd;
				hr = typeInfo->GetFuncDesc(i, &fd); RETURN_IF_FAILED(hr);
				auto releaseFundDesc = wil::scope_exit([ti=typeInfo.get(), fd] { ti->ReleaseFuncDesc(fd); });
				if ((fd->wFuncFlags & FUNCFLAG_FNONBROWSABLE) == 0 && fd->invkind == INVOKE_PROPERTYGET)
				{
					wil::unique_bstr groupName;
					wil::unique_variant data;
					if (SUCCEEDED(ti2->GetFuncCustData(i, guidPropertyGroup, &data)) && data.vt == VT_BSTR)
						groupName = wil::unique_bstr(data.release().bstrVal);
					if ((!groupName && !group) || (groupName && group && !wcscmp(groupName.get(), group)))
						props.push_back(fd->memid);
				/*
				if (auto uiprop = dynamic_cast<const ui_property_i*>(*pe);
					uiprop && uiprop->group() == group)
				{
					bool missing_in_any_other_object = false;
					for (size_t i = 1; i < objs.size(); i++)
					{
						if (!objs[i]->type()->has_property(*pe))
						{
							missing_in_any_other_object = true;
							break;
						}
					}

					if (!missing_in_any_other_object)
						props.insert(*pe);
				}
				*/
				}
			}
		}

		return S_OK;
	}
/*
	void on_selected_objects_change (const IObjectList::change_args& args)
	{
		if (std::holds_alternative<IObjectList::inserting_args>(args))
			on_selected_objects_inserting(std::get<IObjectList::inserting_args>(args));
		else if (std::holds_alternative<IObjectList::inserted_args>(args))
			on_selected_objects_inserted(std::get<IObjectList::inserted_args>(args));
		else if (std::holds_alternative<IObjectList::removing_args>(args))
			on_selected_objects_removing(std::get<IObjectList::removing_args>(args));
		else if (std::holds_alternative<IObjectList::removed_args>(args))
			on_selected_objects_removed(std::get<IObjectList::removed_args>(args));
		else if (std::holds_alternative<IObjectList::replacing_args>(args))
			on_selected_objects_replacing(std::get<IObjectList::replacing_args>(args));
		else if (std::holds_alternative<IObjectList::replaced_args>(args))
			on_selected_objects_replaced(std::get<IObjectList::replaced_args>(args));
		else
			_ASSERT(false);
	}

	void on_selected_objects_inserting (const IObjectList::inserting_args& args)
	{
		// For each existing property item, we look at its property and we check if it's missing
		// in any of the objects to insert. If it's missing, we remove that property item.
		// Otherwise we keep it and ask it to reconcile itself.
		auto root = this->root();

		for (uint32_t i = 0; i < _children.size(); )
		{
			bool missing_in_objects_to_insert = false;
			for (IDispatch* oi : args.objects_to_insert)
			{
				_ASSERT(false);
				//if (!oi || !oi->type()->has_property(_children[i]->property()))
				//{
				//	missing_in_objects_to_insert = true;
				//	break;
				//}
			}

			if (missing_in_objects_to_insert)
			{
				grid()->NotifyItemRemoving(_children[i].get());
				_children.erase(_children.begin() + i);
			}
			else
				i++;
		}
	}

	void on_selected_objects_inserted (const IObjectList::inserted_args& args)
	{
		_ASSERT (args.size);

		this->PerformLayout();

		auto* objs = parent()->objects();
		if (args.size == objs->size())
		{
			// Objects have been inserted into an empty object list. We make property group items for all of their groups.
			std::vector<DISPID> props;
			auto hr = make_property_list (parent()->objects(), _group, props); LOG_IF_FAILED(hr);
			for (DISPID prop : props)
			{
				_ASSERT(false);
				//_children.push_back(make_child_item(prop));
			}
		}
	}

	void on_selected_objects_removing (const IObjectList::removing_args& args)
	{
		_ASSERT (args.size);

		if (args.size == parent()->objects()->size())
		{
			// Last remaining objects removed from list.
			_ASSERT(false);
			//grid()->NotifyItemRemoving(_children);
			//_children.clear();
		}
	}

	void on_selected_objects_removed (const IObjectList::removed_args& args)
	{
		// Find properties that are present in all remaining objects and missing in some removed objects,
		// create property items for every such property, and insert them at the right place.
		// For the property items that are there already, call on_selected_objects_removed.

		auto root = this->root();
		std::vector<DISPID> new_props;
		auto hr = make_property_list (parent()->objects(), _group, new_props); LOG_IF_FAILED(hr);
		auto new_it = new_props.begin();
		uint32_t i = 0;
		while (new_it != new_props.end())
		{
			if ((i == _children.size()) || (_children[i]->property() != *new_it))
				_ASSERT(false);
				//_children.insert(_children.begin() + i, make_property_item(this, *new_it));
			new_it++;
			i++;
		}
	}

	void on_selected_objects_replacing (const IObjectList::replacing_args& args)
	{
		_ASSERT(false);
	}

	void on_selected_objects_replaced (const IObjectList::replaced_args& args)
	{
		_ASSERT(false);
	}
*/
	#pragma region IExpandableItem
	virtual IItem* as_item() override { return this; }

	virtual uint32_t child_count() const override { return _children.size(); }

	virtual IItem* child_at(uint32_t index) const override { return _children[index].get(); }

	virtual bool expanded() const override
	{
		// Group items are currently always expanded.
		return true;
	}

	virtual void expand() override
	{
		_ASSERT(false);
	}

	virtual void collapse() override
	{
		_ASSERT(false);
	}
	#pragma region

	#pragma region IItem
	virtual HRESULT STDMETHODCALLTYPE PerformLayout (const PaintResources& ctx) noexcept override
	{
		if (_idlName && _idlName.get()[0])
		{
			auto grid = root()->grid();

			LONG layoutWidth = grid->ValueColumnRight(ctx.dpi) - grid->ExpandColumnLeft(ctx.dpi)
				- 2 * (title_lr_padding * ctx.dpi / 96);
			wil::unique_process_heap_string text;
			if (layoutWidth > 0)
			{
				if (auto f = wcschr(_idlName.get(), '\\'))
					text = wil::make_process_heap_string_nothrow(_idlName.get(), f - _idlName.get());
				else
					text = wil::make_process_heap_string_nothrow(_idlName.get());
			}

			LONG udPadding = (LONG)std::round(this->udPadding * ctx.dpi / 96.0f);

			_layout = layout {
				.text = std::move(text),
				.height = udPadding + ctx.tmCaptionFont.tmHeight - ctx.tmCaptionFont.tmInternalLeading + udPadding,
			};
		}

		return S_OK;
	}
	
	virtual HRESULT STDMETHODCALLTYPE Paint (HDC hdc, const PaintResources& ctx,
		PaintItemFlags flags, LONG y, edge::IThemeColorProvider* tcp) const noexcept override
	{
		if (!_layout)
			return S_FALSE;

		auto grid = this->root()->grid();
		COLORREF back = tcp->color_win32(theme_color::background);
		wil::unique_hbrush backbrush (CreateSolidBrush(back));
		RECT rect = { grid->ExpandColumnLeft(ctx.dpi), y, grid->ValueColumnRight(ctx.dpi), y + _layout->height };
		FillRect(hdc, &rect, backbrush.get());

		LONG udPadding = (LONG)std::round(this->udPadding * ctx.dpi / 96.0f);

		rect.left = grid->ExpandColumnLeft(ctx.dpi) + indent() * grid->IndentWidth(ctx.dpi);
		rect.top = rect.top + udPadding - ctx.tmCaptionFont.tmInternalLeading / 2;
		auto undosel = wil::SelectObject(hdc, ctx.captionFont.get());
		DrawTextW (hdc, _layout->text.get(), -1, &rect, DT_SINGLELINE | DT_LEFT | DT_TOP);

		return S_OK;
	}

	virtual LONG Height() const noexcept override
	{
		return _layout ? _layout->height : 0;
	}

	virtual HCURSOR cursor_at (POINT pd, LONG item_y) const override
	{
		return ::LoadCursor(nullptr, IDC_ARROW);
	}

	virtual bool selectable() const override { return false; }
	virtual HRESULT STDMETHODCALLTYPE ProcessMouseDown (const edge::mouse_ud_args& ma, LONG item_y) noexcept override { RETURN_HR(E_NOTIMPL); }
	virtual HRESULT STDMETHODCALLTYPE ProcessMouseUp (const edge::mouse_ud_args& ma, LONG item_y) noexcept override { return S_OK; }
	virtual wil::unique_process_heap_string description_title() const override { return { }; }
	virtual wil::unique_process_heap_string description_text() const override { return { }; }
	virtual IExpandableItem* AsExpandable() override { return this; }
	#pragma endregion
};

HRESULT MakeGroupItem (IObjectItem* parent, wil::unique_bstr idlName, IGroupItem** ppItem)
{
	auto p = com_ptr(new (std::nothrow) group_item()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(parent, std::move(idlName)); RETURN_IF_FAILED(hr);
	*ppItem = p.detach();
	return S_OK;
}
