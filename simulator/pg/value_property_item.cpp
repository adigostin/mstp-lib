
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pg_internal.h"
#include "edge/PropDefs.h"

using namespace edge;
using namespace pg;

class value_property_item : public IPGValuePropertyItem, IObjectCollectionChangeEvents
{
	ULONG _refCount = 0;
	ULONG _sig = 0xAA55000F;
	WeakRefToThis _weakRefToThis;
	IGroupItem* _parent;
	com_ptr<ITypeInfo> _typeInfo;
	DISPID _prop;
	VARENUM _vartype;
	vector_nothrow<std::pair<wil::unique_bstr, LONG>> _nvps;
	WORD _getterFuncIndex;
	WORD _setterFuncIndex;
	GUID _editorGuid = { };

	struct layout
	{
		LONG             height;
		wil::unique_bstr nameText;
		read_state       valueState;
		wil::unique_bstr valueText;
		bool             valueChangedFromDefault;

		//wil::unique_process_heap_string value;
	};

	std::optional<layout> _layout;
	ULONG _performLayoutCount = 0;

	AdviseSinkToken _objListChangeToken;

	const LONG udPadding = 2;

public:
	HRESULT InitInstance (IGroupItem* parent, ITypeInfo* typeInfo, DISPID prop, VARENUM vartype,
						  vector_nothrow<std::pair<wil::unique_bstr, LONG>> nvps,
						  WORD getterFuncIndex, WORD setterFuncIndex)
	{
		HRESULT hr = _weakRefToThis.InitInstance(AsUnknown()); RETURN_IF_FAILED(hr);

		_parent = parent;
		_typeInfo = typeInfo;
		_prop = prop;
		_vartype = vartype;
		_nvps = std::move(nvps);
		_getterFuncIndex = getterFuncIndex;
		_setterFuncIndex = setterFuncIndex;

		com_ptr<ITypeInfo2> ti2;
		hr = typeInfo->QueryInterface(IID_PPV_ARGS(ti2.addressof())); RETURN_IF_FAILED(hr);
		wil::unique_variant data;
		if (SUCCEEDED(ti2->GetFuncCustData(_getterFuncIndex, guidPropertyCustomEditor, &data)))
		{
			if (data.vt == VT_BSTR)
				CLSIDFromString(V_BSTR(&data), &_editorGuid);
		}

		hr = AdviseSink<IObjectCollectionChangeEvents>(_parent->parent()->objects(), _weakRefToThis, &_objListChangeToken); RETURN_IF_FAILED(hr);
		return S_OK;
	}

	IUnknown* AsUnknown() { return static_cast<IPGValuePropertyItem*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IItem>(this, riid, ppvObject)
			|| TryQI<IObjectCollectionChangeEvents>(this, riid, ppvObject)
			|| TryQI<IPGValuePropertyItem>(this, riid, ppvObject)
		)
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region IObjectCollectionChangeEvents
	virtual HRESULT OnCollectionChanging (IUnknown* sender, const struct ObjectCollectionChangeArgs* args) override
	{
		return S_OK;
	}

	virtual HRESULT OnCollectionChanged (IUnknown* sender, const struct ObjectCollectionChangeArgs* args) override
	{
		auto grid = root()->grid();
		grid->NotifyLayoutChangedTree(this);
		return S_OK;
	}
	#pragma endregion

	virtual IGroupItem* parent() const noexcept override
	{
		return _parent;
	}

	virtual HRESULT STDMETHODCALLTYPE PerformLayout (const PaintResources& ctx) noexcept override
	{
		_performLayoutCount++;

		HRESULT hr;
		wil::unique_bstr nameText;
		hr = GetNameText(this, nameText); RETURN_IF_FAILED(hr);
		read_state state;
		wil::unique_bstr valueText;
		bool changedFromDefault;
		hr = MakeValueLayout (state, valueText, changedFromDefault); RETURN_IF_FAILED(hr);

		LONG udPadding = (LONG)std::roundf(this->udPadding * ctx.dpi / 96.0f);

		_layout = layout {
			.height = udPadding + ctx.tmNormalFont.tmHeight - ctx.tmNormalFont.tmInternalLeading + udPadding,
			.nameText = std::move(nameText),
			.valueState = state,
			.valueText = std::move(valueText),
			.valueChangedFromDefault = changedFromDefault,
		};

		return S_OK;
	}

	virtual ULONG PerformLayoutCount() const noexcept override { return _performLayoutCount; }
	virtual void ResetPerformLayoutCount() noexcept override { _performLayoutCount = 0; }
	
	virtual HRESULT STDMETHODCALLTYPE Paint (HDC hdc, const PaintResources& ctx,
		PaintItemFlags flags, LONG render_y, edge::IThemeColorProvider* tcp) const noexcept override
	{
		auto grid = root()->grid();

		COLORREF back_color = tcp->color_win32(theme_color::background);
		float back_luminance = (GetRValue(back_color) * 0.299f + GetGValue(back_color) * 0.587f + GetBValue(back_color) * 0.114f) / 255.0f;

		bool hot = (flags & PaintItemFlags::Hot) == PaintItemFlags::Hot;
		static const std::array<TRIVERTEX, 3> defaultGradient = {
			TRIVERTEX{ .Red = 63569, .Green = 63569, .Blue = 63569 }, // 97%
			TRIVERTEX{ .Red = 65535, .Green = 65535, .Blue = 65535 }, // 100%
			TRIVERTEX{ .Red = 60948, .Green = 60948, .Blue = 60948 }, // 93%
		};
		
		static const std::array<TRIVERTEX, 3> hotGradient = {
			TRIVERTEX{ .Red = 50462, .Green = 50462, .Blue = 50462 }, // 77%
			TRIVERTEX{ .Red = 58982, .Green = 58982, .Blue = 58982 }, // 90%
			TRIVERTEX{ .Red = 47841, .Green = 47841, .Blue = 47841 }, // 73%
		};
		std::array<TRIVERTEX, 3> v = hot ? hotGradient : defaultGradient;
		v[0].x = 0;
		v[0].y = render_y;
		v[1].x = grid->ValueColumnRight(ctx.dpi);
		v[1].y = render_y + _layout->height * 4 / 10;
		v[2].x = 0;
		v[2].y = render_y + _layout->height;

		GRADIENT_RECT gr[2] = { { 0, 1 }, { 1, 2 } };
		GradientFill (hdc, v.data(), (UINT)v.size(), gr, 2, GRADIENT_FILL_RECT_V);

		RECT rc = {
			.left = grid->ExpandColumnLeft(ctx.dpi) + (LONG)indent() * grid->IndentWidth(ctx.dpi),
			.top = render_y,
			.right = rc.left + grid->LineWidth(ctx.dpi),
			.bottom = render_y + _layout->height
		};
		FillRect(hdc, &rc, ctx.disabledForeBrush.get());
		rc.left = grid->ValueColumnLeft(ctx.dpi);
		rc.right = rc.left + 1;
		FillRect(hdc, &rc, ctx.disabledForeBrush.get());

		LONG lrPadding = (LONG)std::roundf(text_lr_padding * ctx.dpi / 96.0f);

		if (_layout->nameText)
		{
			RECT rc = {
				.left = grid->NameColumnLeft(indent(), ctx.dpi) + lrPadding,
				.top = render_y + udPadding - ctx.tmNormalFont.tmInternalLeading / 2,
				.right = grid->ValueColumnLeft(ctx.dpi) - grid->LineWidth(ctx.dpi) - lrPadding,
				.bottom = render_y + _layout->height
			};

			auto undo = wil::SelectObject (hdc, ctx.normalFont.get());
			DrawTextW (hdc, _layout->nameText.get(), -1, &rc, DT_SINGLELINE | DT_TOP | DT_LEFT);
		}	

		if (_layout->valueText)
		{
			LONG tmil = _layout->valueChangedFromDefault ? ctx.tmBoldFont.tmInternalLeading : ctx.tmNormalFont.tmInternalLeading;
			RECT rc = {
				.left = grid->ValueColumnLeft(ctx.dpi) + lrPadding,
				.top = render_y + udPadding - tmil / 2,
				.right = grid->ValueColumnRight(ctx.dpi) - lrPadding,
				.bottom = render_y + _layout->height
			};

			auto undo = wil::SelectObject (hdc, _layout->valueChangedFromDefault ? ctx.boldFont.get() : ctx.normalFont.get());
			std::optional<COLORREF> prev;
			if (grid->read_only() || (_editorGuid == CLSID_NULL && _setterFuncIndex == (WORD)-1))
				prev = SetTextColor(hdc, tcp->color_win32(theme_color::disabled_fore));
			DrawTextW (hdc, _layout->valueText.get(), -1, &rc, DT_SINGLELINE | DT_TOP | DT_LEFT | DT_END_ELLIPSIS);
			if (prev)
				SetTextColor(hdc, *prev);
		}

		return S_OK;
	}

	virtual HCURSOR cursor_at (POINT pd, LONG item_y) const override
	{
		if (root()->grid()->read_only() || (_editorGuid == CLSID_NULL && _setterFuncIndex == (WORD)-1))
			return ::LoadCursor(nullptr, IDC_ARROW);

		if (_vartype == VT_BOOL)
			return ::LoadCursor(nullptr, IDC_HAND);
		
		if (_nvps.size())
			return ::LoadCursor(nullptr, IDC_HAND);
		
		if (_editorGuid != CLSID_NULL)
			return ::LoadCursor(nullptr, IDC_HAND);
		
		return ::LoadCursor (nullptr, IDC_IBEAM);
	}

	virtual bool selectable() const override final { return true; }

	virtual LONG Height() const noexcept override
	{
		return _layout ? _layout->height : 0;
	}

	virtual wil::unique_process_heap_string description_title() const override final
	{
		return wil::make_process_heap_string_nothrow(L"description_title");
		//std::stringstream ss;
		//ss << property()->name() << " (" << property()->type_name() << ")";
		//return ss.str();
	}

	virtual wil::unique_process_heap_string description_text() const override final
	{
		return wil::make_process_heap_string_nothrow(L"description_text");
		//auto prop = dynamic_cast<const ui_property_i*>(this->property());
		//return (prop && prop->description()) ? std::string(prop->description()) : std::string();
	}

	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanging (const PropertyChangeArgs *args) override
	{
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanged (const PropertyChangeArgs *args) override
	{
		auto hr = MakeValueLayout (_layout->valueState, _layout->valueText, _layout->valueChangedFromDefault); RETURN_IF_FAILED(hr);		
		::InvalidateRect(root()->grid()->HWnd(), 0, 0);
		return S_OK;
	}

	HRESULT MakeValueLayout (read_state& state, wil::unique_bstr& valueText, bool& changedFromDefault) const
	{
		HRESULT hr;

		auto* objs = parent()->parent()->objects();

		wil::unique_variant value0;
		for (uint32_t i = 0; i < objs->size(); i++)
		{
			DISPPARAMS params = { };
			wil::unique_variant result;
			EXCEPINFO exception;
			UINT uArgErr;
			hr = TypeInfo()->Invoke((*objs)[i], property(), DISPATCH_PROPERTYGET, &params, &result, &exception, &uArgErr);
			if (hr == DISP_E_EXCEPTION)
			{
				SysFreeString(exception.bstrHelpFile);
				SysFreeString(exception.bstrSource);

				if (exception.bstrDescription)
				{
					state = read_state::read_exception;
					valueText.reset(exception.bstrDescription);
					changedFromDefault = true;
					return S_OK;
				}

				hr = exception.scode;
			}

			if (FAILED(hr))
			{
				wil::unique_process_heap_string str;
				hr = wil::str_printf_nothrow(str, L"<Err 0x%08x>", exception.scode); RETURN_IF_FAILED(hr);
				state = read_state::read_exception;
				valueText = wil::make_bstr_nothrow(str.get()); RETURN_IF_NULL_ALLOC(valueText);
				changedFromDefault = true;
				return S_OK;
			}

			if (i == 0)
			{
				if (_setterFuncIndex == (WORD)-1 && _editorGuid == GUID_NULL)
				{
					changedFromDefault = false;
				}
				else
				{
					hr = PropertyHasDefaultValue (objs->front(), property(), _getterFuncIndex); RETURN_IF_FAILED(hr);
					changedFromDefault = (hr != S_OK);
				}

				value0 = std::move(result);
			}
			else if ((result.vt != value0.vt) || VariantCompare(result, value0))
			{
				state = read_state::multiple_values;
				valueText = wil::make_bstr_nothrow(L"(multiple values)"); RETURN_IF_NULL_ALLOC(valueText);
				changedFromDefault = true;
				return S_OK;
			}
		}

		wil::unique_bstr layoutText;
		VARENUM vt = VarType(); // for enums this is VT_USERDEFINED, while the result of Invoke is VT_I4
		if (vt == VT_UI4 || vt == VT_UI2)
		{
			wil::unique_variant temp;
			hr = VariantChangeTypeEx (&temp, &value0, LANG_INVARIANT, 0, VT_BSTR); RETURN_IF_FAILED(hr);
			layoutText = wil::unique_bstr(temp.release().bstrVal);
		}
		else if (vt == VT_BOOL)
		{
			bool val = (V_BOOL(&value0) == VARIANT_TRUE);
			layoutText = wil::make_bstr_nothrow(val ? L"True" : L"False"); RETURN_IF_NULL_ALLOC(layoutText);
		}
		else if (vt == VT_BSTR)
		{
			layoutText = wil::unique_bstr(value0.release().bstrVal);
		}
		else if (vt == VT_USERDEFINED)
		{
			auto nvps = GetNVPs();
			auto it = std::find_if(nvps.begin(), nvps.end(), [&value0] (auto& p) { return p.second == V_I4(&value0); });
			if (it != nvps.end())
			{
				layoutText = wil::make_bstr_nothrow (it->first); RETURN_IF_NULL_ALLOC(layoutText);
			}
			else
			{
				hr = VariantChangeTypeEx(&value0, &value0, LANG_INVARIANT, 0, VT_BSTR); RETURN_IF_FAILED(hr);
				layoutText = wil::unique_bstr(value0.release().bstrVal);
			}
		}
		else
			RETURN_HR(E_NOTIMPL);

		state = read_state::ok;
		valueText = std::move(layoutText);
		changedFromDefault = changedFromDefault;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE ProcessMouseDown (const edge::mouse_ud_args& ma, LONG item_y) noexcept override
	{
		HRESULT hr;

		RETURN_HR_IF(E_UNEXPECTED, !_layout);

		auto grid = root()->grid();
		uint32_t dpi = edge::dpi(grid->HWnd());
		auto vcx = grid->ValueColumnLeft(dpi);
		if (ma.pt.x < vcx)
			return S_FALSE;

		bool readOnly = grid->read_only() || (_setterFuncIndex == (WORD)-1);

		if (_editorGuid != CLSID_NULL)
		{
			com_ptr<ICustomPropertyEditorFactory> factory;
			hr = CoGetClassObject (_editorGuid, CLSCTX_INPROC_SERVER, 0, IID_PPV_ARGS(&factory)); RETURN_IF_FAILED(hr);

			com_ptr<ICustomPropertyEditor> editor;
			factory->CreateEditor(_parent->parent()->objects(), &editor);
			wil::unique_variant selectedValue;
			editor->ShowModal(grid->HWnd(), &selectedValue, readOnly);
			return S_OK;
		}

		if (readOnly)
			return S_OK;

		if (_vartype == VT_BOOL)
		{
			static const std::pair<const wchar_t*, int> nvps[] = {
				{ L"False", 0 },
				{ L"True", 0 },
			};
			int selectedIndex;
			auto hr = grid->ShowEnumEditor(ma.pt, nvps, &selectedIndex); LOG_IF_FAILED(hr);
			if (hr == S_OK)
			{
				VARIANT newValue;
				newValue.vt = VT_BOOL;
				newValue.boolVal = selectedIndex ? VARIANT_TRUE : VARIANT_FALSE;
				hr = grid->change_property(*parent()->parent()->objects(), _typeInfo, _prop, &newValue); LOG_IF_FAILED(hr);
			}

			return S_OK;
		}

		if (_nvps.size())
		{
			auto nvpsData = reinterpret_cast<std::pair<const wchar_t*, int>*>(_nvps.data());
			int selectedIndex;
			hr = grid->ShowEnumEditor(ma.pt, { nvpsData, _nvps.size() }, &selectedIndex);
			if (hr == S_OK)
			{
				VARIANT newValue;
				InitVariantFromInt32 (_nvps[selectedIndex].second, &newValue);
				hr = grid->change_property(*parent()->parent()->objects(), _typeInfo, _prop, &newValue); LOG_IF_FAILED(hr);
			}

			return S_OK;
		}

		bool changedFromDefault = true;
		const wchar_t* editorText = L"";
		if (_layout->valueState == read_state::ok)
			editorText = _layout->valueText.get();

		_ASSERT(grid->selected_item() == this);
		if (grid->ShowTextEditorOnSelectedItem (changedFromDefault, editorText) == S_OK)
		{
			//editor->on_mouse_down (button, mks, pp, pd);
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE ProcessMouseUp (const edge::mouse_ud_args& ma, LONG item_y) noexcept override
	{
		return S_OK;
	}

	virtual ITypeInfo* TypeInfo() const override
	{
		return _typeInfo;
	}

	virtual DISPID property() const override final
	{
		return _prop;
	}

	virtual VARENUM VarType() const override
	{
		return _vartype;
	}

	virtual WORD GetterFuncIndex() const override
	{
		return _getterFuncIndex;
	}

	virtual std::span<std::pair<const wchar_t*, int> const> GetNVPs() const override
	{
		auto nvpsData = reinterpret_cast<const std::pair<const wchar_t*, int>*>(_nvps.data());
		return { nvpsData, _nvps.size() };
	}

	STDMETHOD(GetValue)(read_state* pState, BSTR* pbstrText) override
	{
		RETURN_HR_IF(E_UNEXPECTED, !_layout);
		*pState = _layout->valueState;
		*pbstrText = SysAllocString(_layout->valueText.get()); RETURN_IF_NULL_ALLOC(*pbstrText);
		return S_OK;
	}
};

HRESULT MakeValuePropertyItem (IGroupItem* parent, ITypeInfo* typeInfo, DISPID prop, VARENUM vartype,
							   vector_nothrow<std::pair<wil::unique_bstr, LONG>> nvps,
							   WORD getterFuncIndex, WORD setterFuncIndex, IPGPropertyItem** ppItem)
{
	auto p = com_ptr(new (std::nothrow) value_property_item()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(parent, typeInfo, prop, vartype, std::move(nvps), getterFuncIndex, setterFuncIndex);
	*ppItem = p.detach();
	return S_OK;
}
