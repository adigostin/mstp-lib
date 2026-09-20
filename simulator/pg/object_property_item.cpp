
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "include/pg/property_grid.h"
#include "object_item.h"

using namespace pg;
using namespace edge;
using namespace std::placeholders;

class object_picker_popup //: ID2DRenderEventsSink
{
	ULONG _refCount = 0;

	static constexpr DWORD style = WS_POPUPWINDOW;
	static constexpr DWORD ex_style = WS_EX_NOACTIVATE;
	static constexpr char bottom_hint[] = "Click a color name to select it, click a colored cell to edit the color.";

	static inline const WNDCLASSEX wnd_class = {
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_DBLCLKS | CS_DROPSHADOW,
		.hCursor = ::LoadCursor (nullptr, IDC_ARROW),
		.lpszClassName = L"object_picker_popup",
	};

	object_property_item_i*  const _item;
	std::function<void(ITypeInfo*)> const _callback;
	const edge::IThemeColorProvider* const _tcp;
	IPGInternal*             const _grid;
	float                          _pw;
	float                          _line_width;
	float                    const _lrpadding;
	float                    const _udpadding;
	std::vector<ITypeInfo*>  const _types;
	float                          _max_layout_width;
	float                          _max_layout_height;
	float                          _client_width;
	float                          _client_height;
	wil::unique_hwnd _hWnd;
	//text_layout_with_metrics const _bottom_hint_text;
	HHOOK _mouse_hook = nullptr;
	WeakRefToThis _weakRefToThis;

public:
	object_picker_popup (object_property_item_i* item, LONG item_y, std::function<void(ITypeInfo*)> callback, const edge::IThemeColorProvider* tcp)
		: _item(item)
		, _callback(callback)
		, _tcp(tcp)
		, _grid(item->root()->grid())
		//, _pw(edge::pixel_width(_grid->HWnd())) // we assume popup we're going to create will have same DPI
		//, _line_width(std::round(0.6f / _pw) * _pw)
		, _lrpadding(std::round(5 / _pw) * _pw)
		, _udpadding(std::round(5 / _pw) * _pw)
		, _types(make_types(item->property()))
		//, _max_layout_width(std::max_element(_types.begin(), _types.end(), [](auto& a, auto& b) { return a.second.width() < b.second.width(); })->second.width())
		//, _max_layout_height(std::max_element(_types.begin(), _types.end(), [](auto& a, auto& b){ return a.second.height() < b.second.height(); })->second.height())
		//, _client_width(_lrpadding + std::ceil(_max_layout_width / _pw) * _pw + _lrpadding)
		//, _client_height(std::accumulate(_types.begin(), _types.end(), 0.0f, [this](float a, auto& b) { return a + _udpadding + std::ceil(b.second.height() / _pw) * _pw + _udpadding + ((b.first != _types.back().first) ? _line_width : 0); }))
	{
		static const WNDCLASS wnd_class = {
			.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
			.lpfnWndProc = WndProc,
			.hInstance = (HINSTANCE)&__ImageBase,
			.hCursor = ::LoadCursor(nullptr, IDC_ARROW),
			.lpszClassName = L"object_picker_popup",
		};
		auto atom = RegisterClass(&wnd_class);

		RECT rect = _grid->calc_popup_window_pos(item, item_y, { (LONG)_client_width, (LONG)_client_height }, style, ex_style);
		int x = rect.left;
		int y = rect.top;
		int w = rect.right - rect.left;
		int h = rect.bottom - rect.top;
		_hWnd.reset (CreateWindowEx (ex_style, wnd_class.lpszClassName, L"", style,
									 x, y, w, h, _grid->HWnd(), nullptr, (HINSTANCE)&__ImageBase, nullptr));
		LOG_LAST_ERROR_IF_NULL(_hWnd);
		SetWindowLongPtr (_hWnd.get(), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

		::ShowWindow (_hWnd.get(), SW_SHOWNOACTIVATE);
	}

	~object_picker_popup()
	{
		::ShowWindow (_hWnd.get(), SW_HIDE);
	}
	/*
	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(this, riid, ppvObject)
			|| TryQI<ID2DRenderEventsSink>(this, riid, ppvObject)
		)
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion
	*/
	static std::vector<ITypeInfo*> make_types (DISPID prop)
	{
		std::vector<ITypeInfo*> types;

		_ASSERT(false);
		//for (auto t : concrete_type::known_types())
		//{
		//	if (t->is_same_or_derived_from(prop->child_type()))
		//	{
		//		text_layout_with_metrics name (dwrite_factory, text_format, t->name);
		//		types.push_back ({ t, std::move(name) });
		//	}
		//}

		return types;
	}

	static LRESULT CALLBACK WndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (auto This = reinterpret_cast<object_picker_popup*>(GetWindowLongPtr(hwnd, GWLP_USERDATA)))
		{
			if (msg == WM_LBUTTONDOWN)
			{
				POINT pp = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
				float y = 0;
				for (auto& t : This->_types)
				{
					_ASSERT(false);
					//y += (This->_udpadding + std::ceil(t.second.height() / This->_pw) * This->_pw + This->_udpadding + This->_line_width);
					//if (y >= pd.y)
					//{
					//	This->_callback(t.first);
					//	break;
					//}
				}

				return 0;
			}
		}

		return DefWindowProc (hwnd, msg, wparam, lparam);
	}
	/*
	#pragma region ID2DRenderEventsSink
	virtual HRESULT STDMETHODCALLTYPE OnD2DRender (HWND hWnd, ID2D1DeviceContext* dc) override
	{
		D2D1_COLOR_F fore_color = _tcp->color_d2d(theme_color::foreground);
		com_ptr<ID2D1SolidColorBrush> fore;
		dc->CreateSolidColorBrush (fore_color, &fore);

		D2D1_COLOR_F disabled_fore_color = _tcp->color_d2d(theme_color::disabled_fore);
		com_ptr<ID2D1SolidColorBrush> disabled_fore;
		dc->CreateSolidColorBrush (disabled_fore_color, &disabled_fore);

		uint32_t dpi = edge::dpi(hWnd);
		float pw = edge::pixel_width(dpi);
		dc->SetTransform(edge::dpi_transform(dpi));
		dc->Clear(_tcp->color_d2d(theme_color::background));
		float y = 0;
		for (auto& t : _types)
		{
			y += _udpadding;
			dc->DrawTextLayout ({ _lrpadding, y }, t.second, fore);
			y += std::ceil(_max_layout_height / pw) * pw;
			y += _udpadding;
			y += _line_width / 2;
			dc->DrawLine ({ 0, y }, { _client_width, y }, disabled_fore, _line_width);
			y += _line_width / 2;
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnBeforeD2DRender (HWND hWnd, ID2D1DeviceContext* dc) override { return S_OK; }

	virtual HRESULT STDMETHODCALLTYPE OnAfterD2DRender (HWND hWnd, ID2D1DeviceContext* dc) override { return S_OK; }

	virtual HRESULT STDMETHODCALLTYPE OnD2DDCReleasing (ID2D1DeviceContext* dc) override { return S_OK; }

	virtual HRESULT STDMETHODCALLTYPE OnD2DDCRecreated (ID2D1DeviceContext* dc) override { return S_OK; }
	#pragma endregion
	*/
	static LRESULT CALLBACK mouse_hook_proc(int Code, WPARAM wParam, LPARAM lParam);
};

class object_property_item : public object_property_item_i, IObjectList
{
	ULONG _refCount = 0;
	ULONG _sig = 0xAA55000B;
	IGroupItem* const _parent;
	DISPID const _prop;
	com_ptr<IObjectItemChildManager> _child_manager;
	//edge::text_layout_with_metrics _name;
	enum class value_state { all_null, multiple_selection, all_same_type };
	//std::pair<value_state, edge::text_layout_with_metrics> _value;

	static inline std::optional<object_picker_popup> _popup;

public:
	object_property_item (IGroupItem* parent, DISPID prop)
		: _parent(parent)
		, _prop(prop)
	{
		auto* objs = _parent->parent()->objects();
//		objs->objects_change().add_handler<&object_property_item::on_parent_objects_change>(this);
		//PerformLayout();
	}

	~object_property_item()
	{
		auto* objs = _parent->parent()->objects();
//		objs->objects_change().remove_handler<&object_property_item::on_parent_objects_change>(this);
		_popup.reset();
	}

	IUnknown* AsUnknown() { return static_cast<IPGPropertyItem*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IItem>(static_cast<IPGPropertyItem*>(this), riid, ppvObject)
			|| TryQI<IObjectList>(this, riid, ppvObject)
		)
			return S_OK;

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region IObjectList
	virtual uint32_t size() const override final { return _parent->parent()->objects()->size(); }
	virtual IDispatch* operator[](uint32_t index) const override final
	{
		_ASSERT(false); return { };
		//return _prop->get(_parent->parent()->objects()[index]);
	}
	virtual HRESULT STDMETHODCALLTYPE GetListTitle(BSTR* pbstrTitle) noexcept override { RETURN_HR(E_NOTIMPL); }
	#pragma endregion

	static std::vector<com_ptr<IDispatch>> get_child_selected_objects (DISPID prop, std::span<IDispatch* const> parent_objects)
	{
		std::vector<com_ptr<IDispatch>> res;
		res.reserve(parent_objects.size());
		for (IDispatch* o : parent_objects)
		{
			_ASSERT(false);
			//res.push_back(prop->get(o));
		}
		return res;
	}

	virtual IGroupItem* parent() const override final { return _parent; }
	/*
	text_layout_with_metrics make_name_layout() const
	{
		auto grid = this->grid();
		uint32_t dpi = edge::dpi(grid->HWnd());
		auto dwf = grid->renderer()->dwrite_factory();
		float ncx = grid->name_column_left(indent());
		float name_width = grid->value_column_left(dpi) - ncx;
		_ASSERT(false); return { };
		//return text_layout_with_metrics(dwf, grid->text_format(), _prop->name(), name_width);
	}
	*/
	/*
	std::pair<object_property_item::value_state, text_layout_with_metrics> make_value_layout() const
	{
		auto grid = root()->grid();

		uint32_t dpi = edge::dpi(grid->HWnd());
		LONG width = grid->ValueColumnRight(dpi) - grid->ValueColumnLeft(dpi) - grid->LineWidth(dpi) - 2 * text_lr_padding;
		if (width <= 0)
			return { };
		_ASSERT(false); return { };
		
		auto& objs = parent()->parent()->objects();
		if (objs.all ([prop=_prop](object* o) { return !prop->get(o); }))
			return { value_state::all_null, text_layout_with_metrics(dwf, grid->text_format(), "(not set)", width) };

		auto type = _prop->get(objs.front()) ? _prop->get(objs.front())->type() : nullptr;
		bool all_same_type = objs.all([prop=_prop,type](object* o) { return (prop->get(o) ? prop->get(o)->type() : nullptr) == type; });
		if (!all_same_type)
			return { value_state::multiple_selection, text_layout_with_metrics(dwf, grid->bold_text_format(), "(multiple selection)", width) };

		return { value_state::all_same_type, text_layout_with_metrics(dwf, grid->bold_text_format(), type->name, width) };
	}
	*/
	virtual HRESULT STDMETHODCALLTYPE PerformLayout (const PaintResources& ctx) noexcept override
	{
		RETURN_HR(E_NOTIMPL);
		/*
		_name = make_name_layout();
		_value = make_value_layout();

		grid()->invalidate_item(this);
		return S_OK;
		*/
	}

	RECT expand_button_click_rect (LONG item_y) const
	{
		//_ASSERT (_value.first == value_state::all_same_type);
		auto grid = root()->grid();
		uint32_t dpi = edge::dpi(grid->HWnd());
		LONG name_line_x = grid->ExpandColumnLeft(dpi) + indent() * grid->IndentWidth(dpi);
		return { name_line_x - grid->IndentWidth(dpi), item_y, name_line_x, item_y + this->Height() };
	}
	/*
	virtual void render (const render_context& rc, float y, bool selected, bool hot, bool focused) const override final
	{
		render_default_background(rc, y, selected, hot, focused);

		auto grid = root()->grid();
		uint32_t dpi = edge::dpi(grid->HWnd());
		auto lt = grid->line_width(dpi);
		float pw = edge::pixel_width(dpi);
		float height = std::ceil(this->Height() / pw) * pw;

		float name_line_x = grid->expand_column_left(dpi) + indent() * grid->indent_width();

		if (_value.first == value_state::all_same_type)
			render_expand_button(rc, y);

		rc.dc->DrawLine ({ name_line_x + lt/2, y }, { name_line_x + lt/2, y + height }, rc.disabled_fore, lt);
		rc.dc->DrawTextLayout ({ name_line_x + lt + text_lr_padding, y }, _name, rc.fore);

		if (auto& tl = _value.second)
		{
			float value_linex = grid->value_column_left(dpi);
			rc.dc->DrawLine ({ value_linex + lt/2, y }, { value_linex + lt/2, y + height }, rc.disabled_fore, lt);
			rc.dc->DrawTextLayout ({ grid->value_column_left(dpi) + lt + text_lr_padding, y }, _value.second, rc.fore);
		}
	}
	*/
	virtual LONG Height() const noexcept override
	{
		_ASSERT(false); return { };
		//return (LONG)std::max(_name.height(), _value.second.height());
	}

	virtual HCURSOR cursor_at (POINT pd, LONG item_y) const override final
	{
		auto grid = root()->grid();
		if (grid->read_only())
			return ::LoadCursor(nullptr, IDC_ARROW);

		return ::LoadCursor(nullptr, IDC_HAND);
	}

	virtual bool selectable() const override final { return true; }

	virtual HRESULT STDMETHODCALLTYPE ProcessMouseDown (const edge::mouse_ud_args& ma, LONG item_y) noexcept override
	{
		RETURN_HR(E_NOTIMPL);
		//if (_value.first == value_state::all_same_type)
		//{
		//	auto rc = expand_button_click_rect(item_y);
		//	if (PtInRect(&rc, ma.pt))
		//	{
		//		if (expanded())
		//			collapse();
		//		else
		//			expand();
		//	}
		//}
	}

	virtual HRESULT STDMETHODCALLTYPE ProcessMouseUp (const edge::mouse_ud_args& ma, LONG item_y) noexcept override
	{
		auto grid = root()->grid();
		uint32_t dpi = edge::dpi(grid->HWnd());
		if (grid->read_only())
			return S_OK;
		auto vcx = grid->ValueColumnLeft(dpi);
		if (ma.pt.x < vcx)
			return S_OK;

		_popup.emplace(this, item_y, std::bind(&object_property_item::on_object_picked, this, std::placeholders::_1), grid->tcp());
		return S_OK;
	}

	virtual wil::unique_process_heap_string description_title() const override final
	{
		_ASSERT(false); return { };
		//return _prop->name();
	}

	virtual wil::unique_process_heap_string description_text() const override final
	{
		_ASSERT(false); return { };
		//auto ui_prop = dynamic_cast<const ui_property_i*>(property());
		//return ui_prop && ui_prop->description() ? std::string(ui_prop->description()) : std::string();
	}

	STDMETHOD(GetValue)(read_state* pState, BSTR* pbstrValueText) override { RETURN_HR(E_NOTIMPL); }

	#pragma region IPGPropertyItem
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

	// Following two function are called by the parent item (itself of type object_item) when our property changes.
	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanging (const PropertyChangeArgs* args) override
	{
		RETURN_HR(E_NOTIMPL);
		//auto* objprop_args = checked_static_cast<const object_property_change_args*>(&args);
		//object* child_object_to_insert = objprop_args->other_child;
	}

	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanged (const PropertyChangeArgs* args) override
	{
		RETURN_HR(E_NOTIMPL);
		//auto* objprop_args = checked_static_cast<const object_property_change_args*>(&args);
		//object* child_object_removed = objprop_args->other_child;
		//PerformLayout();
	}
	#pragma endregion
	/*
	void on_parent_objects_change (const change_args& args)
	{
		if (auto* inserting = std::get_if<inserting_args>(&args))
		{
			std::vector<object*> children;
			std::transform(inserting->objects_to_insert.begin(), inserting->objects_to_insert.end(), std::back_inserter(children), [p=_prop](object* parent) { return p->get(parent); });
			change_e::invoker(_em).invoke(inserting_args{ children });
		}
		else if (auto* inserted = std::get_if<inserted_args>(&args))
		{
			change_e::invoker(_em).invoke(args);
			PerformLayout();
		}
		else if (auto* removing = std::get_if<removing_args>(&args))
		{
			change_e::invoker(_em).invoke(args);
		}
		else if (auto* removed = std::get_if<removed_args>(&args))
		{
			std::vector<object*> children;
			std::transform(removed->objects_removed.begin(), removed->objects_removed.end(), std::back_inserter(children), [p=_prop](object* parent) { return p->get(parent); });
			change_e::invoker(_em).invoke(removed_args{ children });
			PerformLayout();
		}
		else if (auto* replacing = std::get_if<replacing_args>(&args))
		{
			std::vector<object*> children_to_insert;
			std::transform(replacing->new_objs.begin(), replacing->new_objs.end(), std::back_inserter(children_to_insert), [p=_prop](object* parent) { return p->get(parent); });
			change_e::invoker(_em).invoke(replacing_args{ replacing->index, children_to_insert });
		}
		else if (auto* replaced = std::get_if<replaced_args>(&args))
		{
			std::vector<object*> children_removed;
			std::transform(replaced->old_objs.begin(), replaced->old_objs.end(), std::back_inserter(children_removed),  [p=_prop](object* parent) { return p->get(parent); });
			change_e::invoker(_em).invoke(replaced_args{ replaced->index, children_removed });
			PerformLayout();
		}
		else
			_ASSERT(false);
	}
	*/
	void on_object_picked (ITypeInfo* type)
	{
		_ASSERT(false);
		/*
		auto& objs = parent()->parent()->objects();
		bool different_type_selected = objs.any([prop=_prop,type](object* o) { return (prop->get(o) ? prop->get(o)->type() : nullptr) != type; });
		if (different_type_selected)
		{
			this->grid()->change_property (objs, _prop, type);
			expand_all();
		}

		_popup.reset();
		*/
	}

	#pragma region IExpandableItem
	virtual IItem* as_item() override final { return this; }

	virtual uint32_t child_count() const override final { return _child_manager ? _child_manager->ChildCount() : 0; }

	virtual IItem* child_at (uint32_t index) const override final
	{
		return _child_manager->ChildAt(index);
	}

	virtual bool expanded() const override final { return _child_manager; }
	
	virtual void expand() override final
	{
		_ASSERT(!_child_manager);
		auto hr = MakeObjectItemChildManager(this, this, &_child_manager); LOG_IF_FAILED(hr);
		::InvalidateRect(root()->grid()->HWnd(), 0, 0);
	}

	virtual void collapse() override final
	{
		if (_child_manager)
		{
			_child_manager.reset();
			::InvalidateRect(root()->grid()->HWnd(), 0, 0);
		}
	}
	#pragma endregion

	#pragma region IObjectItem
	virtual IGroupItem* ChildGroupItemAt(uint32_t index) const override
	{
		FAIL_FAST_IF(!_child_manager);
		return _child_manager->ChildAt(index);
	}
	virtual IObjectList* objects() override final
	{
		return this;
	}
	#pragma endregion
};

std::unique_ptr<IPGPropertyItem> make_object_property_item (IGroupItem* parent, DISPID prop)
{
	return std::make_unique<object_property_item>(parent, prop);
}
