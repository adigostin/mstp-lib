
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pg_internal.h"

using namespace edge;
using namespace pg;

enum IDS
{
	ID_INPLACE_EDIT = 103,
};

extern HRESULT MakeRootItem (IPGInternal* grid, bool showEmptySel, IObjectList* objects,
							 edge::string_convert_context_i* scc, IRootItem** ppRootItem);

class PropertyGridImpl : public IPGInternal, IThemeChangedEvents
{
	ULONG _refCount = 0;
	HWND _hWnd;
	IThemeColorProvider* _tcp;
	wil::unique_hwnd _text_editor;
	WNDPROC _oldInplaceProc;
	RECT _bounds;
	float _name_column_factor = 0.6f;
	vector_nothrow<com_ptr<IRootItem>> _root_items;
	HWND _tooltip = nullptr;
	std::optional<POINT> _last_tt_location;
	float _borderWidthDIPs = 0;
	bool _read_only = false;
	bool _scroll_bar_visible = false;
	LONG _top_y = 0;
	WeakRefToThis _weakRefToThis;
	AdviseSinkToken _themeChangedToken;
	IItem* _selectedItem = nullptr;
	IItem* _hotItem = nullptr;
	PaintResources _paintres;

	static constexpr float line_width_not_aligned = 0.6f;

public:
	HRESULT InitInstance (HWND hWnd, const RECT& bounds, IThemeColorProvider* tcp)
	{
		HRESULT hr;

		hr = _weakRefToThis.InitInstance(AsUnknown()); RETURN_IF_FAILED(hr);

		_hWnd = hWnd;
		_bounds = bounds;
		_tcp = tcp;

		if (_hWnd)
		{
			_tooltip = CreateWindowEx (WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr,
				WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				_hWnd, nullptr, (HINSTANCE)&__ImageBase, nullptr);

			TOOLINFO ti = { sizeof(TOOLINFO) };
			ti.uFlags   = TTF_SUBCLASS;
			ti.hwnd     = _hWnd;
			ti.lpszText = nullptr;
			ti.rect     = _bounds;
			SendMessage(_tooltip, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);

			SendMessage (_tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 1500);
			SendMessage (_tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, (LPARAM)(LONG)MAXSHORT);
			SendMessage (_tooltip, TTM_SETMAXTIPWIDTH, 0, ti.rect.right - ti.rect.left);

			BOOL bres = SetWindowSubclass (_hWnd, SubClassProc, 0, reinterpret_cast<DWORD_PTR>(this)); _ASSERT(bres);
		}

		if (tcp)
		{
			hr = AdviseSink<IThemeChangedEvents>(tcp, _weakRefToThis, &_themeChangedToken); RETURN_IF_FAILED(hr);
		}

		hr = MakePaintResources(wil::GetDC(_hWnd).get(), _paintres); RETURN_IF_FAILED(hr);

		::InvalidateRect(_hWnd, &_bounds, FALSE);

		return S_OK;
	}

	~PropertyGridImpl()
	{
		if (_hWnd)
		{
			BOOL bres = RemoveWindowSubclass (_hWnd, SubClassProc, 0);

			::InvalidateRect(_hWnd, &_bounds, FALSE);
			::DestroyWindow(_tooltip);
		}
	}

	IUnknown* AsUnknown() { return static_cast<IPropertyGrid*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IPropertyGrid>(this, riid, ppvObject)
			|| TryQI<IPGInternal>(this, riid, ppvObject)
			|| TryQI<IThemeChangedEvents>(this, riid, ppvObject)
		)
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region IThemeChangedEvents
	virtual HRESULT STDMETHODCALLTYPE OnThemeChanged() override
	{
		::InvalidateRect(_hWnd, 0, 0);
		return S_OK;
	}
	#pragma endregion

	virtual void InvalidateItem (IItem* i) noexcept override
	{
		LONG dpi = edge::dpi(_hWnd);
		LONG bwp = BorderWidth(dpi);

		enum_items ([this,bwp,i](IItem* item, LONG item_y, bool& cancel) {
			if (i == item)
			{
				LONG render_y = _bounds.top + bwp + item_y - _top_y;
				RECT itemrc = { _bounds.left, render_y, _bounds.right, render_y + item->Height() };
				::InvalidateRect(_hWnd, &itemrc, 0);
				cancel = true;
			}
		});
	}

	static LRESULT CALLBACK SubClassProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
	{
		auto pg = reinterpret_cast<PropertyGridImpl*>(dwRefData);

		if (uMsg == WM_DPICHANGED_AFTERPARENT)
		{
			pg->process_dpi_changed();
			return DefSubclassProc (hWnd, uMsg, wParam, lParam);
		}

		if (uMsg == WM_SETCURSOR)
		{
			if (pg->process_wm_setcursor (hWnd, wParam, lParam))
				return TRUE;
			return DefSubclassProc (hWnd, uMsg, wParam, lParam);
		}

		if ((uMsg == WM_LBUTTONDOWN) || (uMsg == WM_RBUTTONDOWN))
		{
			auto pp = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			uint32_t dpi = edge::dpi(hWnd);
			if (PtInRect(&pg->_bounds, pp))
			{
				auto button = (uMsg == WM_LBUTTONDOWN) ? mouse_button::left : mouse_button::right;
				auto mks = (UINT)(UINT)wParam | ((::GetKeyState(VK_MENU) < 0) ? MK_ALT : 0);
				auto olr = pg->OnMouseButtonDown ({ button, mks, pp });
				if (olr)
					return *olr;
			}

			return DefSubclassProc (hWnd, uMsg, wParam, lParam);
		}

		if ((uMsg == WM_LBUTTONUP) || (uMsg == WM_RBUTTONUP))
		{
			auto pp = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			uint32_t dpi = edge::dpi(hWnd);
			if (PtInRect(&pg->_bounds, pp))
			{
				auto button = (uMsg == WM_LBUTTONUP) ? mouse_button::left : mouse_button::right;
				auto mks = (UINT)(UINT)wParam | ((::GetKeyState(VK_MENU) < 0) ? MK_ALT : 0);
				auto olr = pg->OnMouseButtonUp ({ button, mks, pp });
				if (olr)
					return *olr;
			}

			return DefSubclassProc (hWnd, uMsg, wParam, lParam);
		}

		if (uMsg == WM_MOUSEMOVE)
		{
			TRACKMOUSEEVENT tme = { .cbSize = sizeof(tme), .dwFlags = TME_LEAVE, .hwndTrack = hWnd };
			BOOL bres = TrackMouseEvent (&tme);

			auto pp = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			uint32_t dpi = edge::dpi(hWnd);
			if (PtInRect(&pg->_bounds, pp))
			{
				auto mks = (UINT)(UINT)wParam | ((::GetKeyState(VK_MENU) < 0) ? MK_ALT : 0);
				pg->OnMouseMove (hWnd, pp, mks);
			}

			return DefSubclassProc (hWnd, uMsg, wParam, lParam);
		}
/*
		if (uMsg == WM_MOUSEWHEEL)
		{
			auto pp = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			::ScreenToClient(hWnd, &pp);
			uint32_t dpi = edge::dpi(hWnd);
			auto pd = pointp_to_pointd(pp, dpi);
			if (point_in_rect(_rectd, pd))
			{
				auto mks = (UINT)GET_KEYSTATE_WPARAM(wParam) | ((::GetKeyState(VK_MENU) < 0) ? MK_ALT : 0);
				short delta = GET_WHEEL_DELTA_WPARAM(wParam);
				return process_wm_mousewheel (hWnd, pd, mks, delta);
			}

			return std::nullopt;
		}
*/
		if (uMsg == WM_MOUSELEAVE)
		{
			if (pg->_hotItem)
			{
				pg->_hotItem = nullptr;
				::InvalidateRect(pg->_hWnd, 0, 0);
			}

			return DefSubclassProc (hWnd, uMsg, wParam, lParam);
		}
		
		if ((uMsg == WM_KEYDOWN) || (uMsg == WM_SYSKEYDOWN))
		{
			if (pg->process_key_down ((UINT) wParam, get_modifier_keys()))
				return 0;
			return DefSubclassProc (hWnd, uMsg, wParam, lParam);
		}

		if ((uMsg == WM_KEYUP) || (uMsg == WM_SYSKEYUP))
		{
			if (pg->process_key_up ((UINT) wParam, get_modifier_keys()))
				return 0;
			return DefSubclassProc (hWnd, uMsg, wParam, lParam);
		}

		if (uMsg == WM_CHAR)
		{
			if (pg->process_char_key((uint32_t)wParam))
				return 0;
			return DefSubclassProc (hWnd, uMsg, wParam, lParam);
		}
		/*
		if (uMsg == WM_GETDLGCODE)
		{
			if (_text_editor)
				return DLGC_WANTALLKEYS;

			return resultBaseClass;
		}
		*/
		if ((uMsg == WM_SETFOCUS) || (uMsg == WM_KILLFOCUS))
		{
			::InvalidateRect (hWnd, nullptr, 0);
			return 0;
		}
		
		if (uMsg == WM_ERASEBKGND)
			return 0; // 0 means the window remains marked for erasing, so the fErase member of the PAINTSTRUCT structure will be TRUE.

		if (uMsg == WM_PAINT)
		{
			//DefSubclassProc (hWnd, uMsg, wParam, lParam);
			//::InvalidateRect (hWnd, nullptr, 0);
			return pg->ProcessWmPaint(hWnd);
		}

		return DefSubclassProc (hWnd, uMsg, wParam, lParam);
	}

	HRESULT MakePaintResources (HDC hdc, PaintResources& ctx)
	{
		ctx.dpi = edge::dpi(_hWnd);

		NONCLIENTMETRICS ncMetrics = { .cbSize = sizeof(NONCLIENTMETRICS) };
		BOOL bRes = SystemParametersInfo (SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncMetrics, 0); RETURN_IF_WIN32_BOOL_FALSE(bRes);

		ctx.normalFont = wil::unique_hfont (CreateFontIndirect(&ncMetrics.lfMessageFont)); RETURN_LAST_ERROR_IF_NULL(ctx.normalFont);
		auto undo1 = wil::SelectObject(hdc, ctx.normalFont.get());
		GetTextMetricsW(hdc, &ctx.tmNormalFont);

		ncMetrics.lfMessageFont.lfWeight = FW_BOLD;
		ctx.boldFont = wil::unique_hfont (CreateFontIndirect(&ncMetrics.lfMessageFont)); RETURN_LAST_ERROR_IF_NULL(ctx.boldFont);
		auto undo2 = wil::SelectObject(hdc, ctx.boldFont.get());
		GetTextMetricsW(hdc, &ctx.tmBoldFont);

		ncMetrics.lfCaptionFont.lfWeight = FW_BOLD;
		ctx.captionFont = wil::unique_hfont (CreateFontIndirect (&ncMetrics.lfCaptionFont)); RETURN_LAST_ERROR_IF_NULL(ctx.captionFont);
		auto undo3 = wil::SelectObject(hdc, ctx.captionFont.get());
		GetTextMetricsW(hdc, &ctx.tmCaptionFont);

		if (_tcp)
		{
			ctx.backBrush.reset (::CreateSolidBrush(_tcp->color_win32(theme_color::background)));
			ctx.disabledForeBrush.reset (::CreateSolidBrush(_tcp->color_win32(theme_color::disabled_fore)));
		}

		return S_OK;
	}

	LRESULT ProcessWmPaint (HWND hWnd)
	{
		PAINTSTRUCT ps;
		HDC hdcOuter = ::BeginPaint(hWnd, &ps);
		auto endpaint = wil::scope_exit([hWnd,&ps] { ::EndPaint(hWnd, &ps); });
		
		HDC hdc;
		auto hpb = ::BeginBufferedPaint(hdcOuter, &ps.rcPaint, BPBF_COMPATIBLEBITMAP, nullptr, &hdc); RETURN_LAST_ERROR_IF_NULL(hpb);
		auto endbp = wil::scope_exit([hpb] { ::EndBufferedPaint(hpb, TRUE); });

		// TODO: once the grid gets focus, only an item should have focus.
		bool focused = GetFocus() == hWnd;

		LONG dpi = edge::dpi(hWnd);
		LONG bwp = BorderWidth(dpi);

		SetBkMode (hdc,TRANSPARENT);

		auto[_, bottom_y] = enum_items ([&](IItem* item, LONG item_y, bool& cancel) {
			if (item_y + item->Height() <= _top_y)
			{
				// item is scrolled above the visible area
			}
			else
			{
				LONG render_y = _bounds.top + bwp + item_y - _top_y;

				if (render_y >= _bounds.bottom)
				{
					cancel = true;
					return;
				}

				RECT itemrc = { _bounds.left, render_y, _bounds.right, render_y + item->Height() };
				if (RectVisible(hdc, &itemrc))
				{
					PaintItemFlags flags = (PaintItemFlags)0;
					if (item == _selectedItem)
						flags |= PaintItemFlags::Selected;
					if (item == _hotItem)
						flags |= PaintItemFlags::Hot;
					item->Paint (hdc, _paintres, flags, render_y, _tcp);
				}
			}
		});

		RECT rc = { _bounds.left, _bounds.top + bwp + bottom_y - _top_y, _bounds.right, _bounds.bottom };
		FillRect (hdc, &rc, _paintres.backBrush.get());

		return 0;
	}

	BOOL process_wm_setcursor (HWND hwnd, WPARAM wparam, LPARAM lparam)
	{
		if (((HWND)wparam == hwnd) && (LOWORD(lparam) == HTCLIENT))
		{
			POINT pt;
			if (::GetCursorPos(&pt) && ::ScreenToClient (hwnd, &pt))
			{
				if (PtInRect(&_bounds, pt))
				{
					HCURSOR cursor = nullptr;
					//if ((pd.x >= value_column_x()) && (pd.x < _rectd.right))
					{
						if (auto htr = hit_test(pt); htr.item)
						{
							if (htr.code == htcode::value)
								cursor = htr.item->cursor_at(pt, htr.render_y);
						}
					}

					if (!cursor)
						cursor = ::LoadCursor(nullptr, IDC_ARROW);

					::SetCursor(cursor);
					return TRUE;
				}
			}
		}

		return FALSE;
	}

	// Returns the canceled item, or nullptr if no item was canceled.
	template<typename callback_t> requires std::is_invocable_v<callback_t, IItem*, LONG, bool&>
	IItem* enum_items_from (IItem* item, LONG& y, const callback_t& callback) const
	{
		bool cancel = false;
		callback(item, y, cancel);
		if (cancel)
			return item;

		y += item->Height();

		if (auto ei = item->AsExpandable())
		{
			uint32_t cc = ei->child_count();
			for (uint32_t i = 0; i < cc; i++)
			{
				auto child = ei->child_at(i);
				auto canceled_item = enum_items_from(child, y, callback);
				if (canceled_item)
					return canceled_item;
			}
		}

		return nullptr;
	}

	// Returns the canceled item and its y (not the same as "render_y" due to vertical scrolling).
	// If the callback doesn't cancel any item, returns nullptr and the total height.
	template<typename callback_t> requires std::is_invocable_v<callback_t, IItem*, LONG, bool&>
	std::pair<IItem*, LONG> enum_items (const callback_t& callback) const
	{
		LONG y = 0;
		for (auto& root_item : _root_items)
		{
			auto canceled_item = enum_items_from(root_item.get(), y, callback);
			if (canceled_item)
				return { canceled_item, y };
		}

		return { nullptr, y };
	}

	HRESULT perform_layouts()
	{
		HRESULT hr;

		_scroll_bar_visible = false;

		auto hdc = wil::GetDC(_hWnd);
		PaintResources ctx;
		hr = MakePaintResources(hdc.get(), ctx); RETURN_IF_FAILED(hr);
		LONG visible_height = _bounds.bottom - _bounds.top - 2 * BorderWidth(ctx.dpi);
		auto callback = [this,&ctx,visible_height](IItem* i, LONG item_y, bool& cancel)
			{
				i->PerformLayout(ctx);
				if (item_y + i->Height() > visible_height)
					// TODO: add new function "clear_layout" to IItem and call it for the remaining items.
					cancel = true;
			};

		auto[canceled_item, y] = enum_items (callback);

		if (canceled_item)
		{
			_scroll_bar_visible = true;
			enum_items (callback);
		}

		::InvalidateRect(_hWnd, 0, 0);

		return S_OK;
	}

	//LONG height_pixels (const IItem* item) const
	//{
	//	LONG height = item->Height();
	//	uint32_t dpi = edge::dpi(_hWnd);
	//	LONG h = (LONG)std::ceil(height / 96.0f * dpi);
	//	return h;
	//}
	/*
	render_context make_render_context (ID2D1DeviceContext* dc) const
	{
		render_context rc;
		rc.dc = dc;
		D2D1_COLOR_F back_color = _tcp->color_d2d(theme_color::background);
		D2D1_COLOR_F fore_color = _tcp->color_d2d(theme_color::foreground);
		dc->CreateSolidColorBrush (back_color, &rc.back);
		dc->CreateSolidColorBrush (fore_color, &rc.fore);
		dc->CreateSolidColorBrush (interpolate(back_color, fore_color, 50), &rc.border);
		dc->CreateSolidColorBrush (_tcp->color_d2d(theme_color::disabled_fore), &rc.disabled_fore);
		dc->CreateSolidColorBrush (_tcp->color_d2d(theme_color::selected_back_focused), &rc.selected_back_focused);
		dc->CreateSolidColorBrush (_tcp->color_d2d(theme_color::selected_back_not_focused), &rc.selected_back_not_focused);
		dc->CreateSolidColorBrush (_tcp->color_d2d(theme_color::selected_fore), &rc.selected_fore);
		dc->CreateSolidColorBrush (_tcp->color_d2d(theme_color::active_caption_back), &rc.root_item_back);
		dc->CreateSolidColorBrush (_tcp->color_d2d(theme_color::active_caption_fore), &rc.root_item_fore);
		dc->CreateSolidColorBrush (_tcp->color_d2d(theme_color::tooltip_back), &rc.tooltip_back);
		dc->CreateSolidColorBrush (_tcp->color_d2d(theme_color::tooltip_fore), &rc.tooltip_fore);

		float back_luminance = back_color.r * 0.299f + back_color.g * 0.587f + back_color.b * 0.114f;

		static constexpr D2D1_GRADIENT_STOP stops_light[3] =
		{
			{ 0,    { 0.97f, 0.97f, 0.97f, 1 } },
			{ 0.4f, { 1,     1,     1,     1 } },
			{ 1,    { 0.93f, 0.93f, 0.93f, 1 } },
		};

		static constexpr D2D1_GRADIENT_STOP stops_light_hot[3] =
		{
			{ 0,    { 0.77f, 0.77f, 0.77f, 1 } },
			{ 0.4f, { 0.90f, 0.90f, 0.90f, 1 } },
			{ 1,    { 0.73f, 0.73f, 0.73f, 1 } },
		};

		static constexpr D2D1_GRADIENT_STOP stops_dark[3] =
		{
			{ 0,    { 0.10f, 0.10f, 0.10f, 1 } },
			{ 0.4f, { 0.21f, 0.21f, 0.21f, 1 } },
			{ 1,    { 0,     0,     0,     1 } },
		};

		static constexpr D2D1_GRADIENT_STOP stops_dark_hot[3] =
		{
			{ 0,    { 0.20f, 0.20f, 0.20f, 1 } },
			{ 0.4f, { 0.41f, 0.41f, 0.41f, 1 } },
			{ 1,    { 0.25f, 0.25f, 0.25f, 1 } },
		};

		com_ptr<ID2D1GradientStopCollection> stop_collection;
		auto hr = dc->CreateGradientStopCollection ((back_luminance > 0.6f) ? stops_light : stops_dark, 3, &stop_collection); _ASSERT(SUCCEEDED(hr));
		static constexpr D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES lgbp = { { 0, 0 }, { 1, 0 } };
		hr = dc->CreateLinearGradientBrush (&lgbp, nullptr, stop_collection, &rc.item_gradient_brush); _ASSERT(SUCCEEDED(hr));

		hr = dc->CreateGradientStopCollection ((back_luminance > 0.6f) ? stops_light_hot : stops_dark_hot, 3, &stop_collection); _ASSERT(SUCCEEDED(hr));
		hr = dc->CreateLinearGradientBrush (&lgbp, nullptr, stop_collection, &rc.item_gradient_brush_hot); _ASSERT(SUCCEEDED(hr));

		return rc;
	}
	*/
	virtual HWND HWnd() const noexcept override { return _hWnd; }

	virtual RECT Bounds() const noexcept override { return _bounds; }

	virtual void SetBounds (const RECT& bounds) noexcept override
	{
		if (_bounds != bounds)
		{
			::InvalidateRect(_hWnd, 0, 0);
			_text_editor = nullptr;

			LONG old_width = _bounds.right - _bounds.left;
			LONG new_width = bounds.right - bounds.left;

			_bounds = bounds;

			if (old_width != new_width)
				perform_layouts();

			TOOLINFO ti = { sizeof(TOOLINFO) };
			ti.uFlags   = TTF_SUBCLASS;
			ti.hwnd     = _hWnd;
			ti.lpszText = nullptr;
			ti.rect     = _bounds;
			SendMessage(_tooltip, TTM_SETTOOLINFO, 0, (LPARAM) (LPTOOLINFO) &ti);

			::InvalidateRect(_hWnd, 0, 0);
		}
	}

	virtual void SetBorderWidth (float widthDIPs) noexcept override
	{
		if (_borderWidthDIPs != widthDIPs)
		{
			_borderWidthDIPs = widthDIPs;

			_text_editor = nullptr;
			perform_layouts();
		}
	}

	HRESULT process_dpi_changed()
	{
		HRESULT hr;
		_text_editor = nullptr;
		hr = MakePaintResources(wil::GetDC(_hWnd).get(), _paintres); RETURN_IF_FAILED(hr);
		perform_layouts();
		::InvalidateRect (_hWnd, nullptr, FALSE);
		return S_OK;
	}

	virtual void clear_sections() override
	{
		_root_items.clear();
		::InvalidateRect(_hWnd, 0, 0);
	}

	virtual HRESULT STDMETHODCALLTYPE AddSection (edge::IObjectList* objects, bool showEmptySel, string_convert_context_i* scc) override
	{
		com_ptr<IRootItem> ri;
		auto hr = MakeRootItem (this, showEmptySel, objects, scc, &ri); RETURN_IF_FAILED(hr);
		bool pushed = _root_items.try_push_back(std::move(ri)); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);

		LONG y = 0;
		bool cancel = false;
		enum_items_from(_root_items.back(), y, [this](IItem* i, LONG y, bool& cancel) {
			return i->PerformLayout(_paintres);
		});
		::InvalidateRect(_hWnd, 0, 0);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE RemoveSection (edge::IObjectList* objList) override
	{
		auto it = std::find_if(_root_items.begin(), _root_items.end(), [objList](IRootItem* ri) { return ri->objects() == objList; });
		if (it == _root_items.end())
			RETURN_HR(E_INVALIDARG);
		_root_items.erase(it);
		::InvalidateRect(_hWnd, 0, 0);
		return S_OK;
	}

	//virtual std::span<const std::unique_ptr<IRootItem>> sections() const override { return _root_items; }

	virtual void set_read_only (bool read_only) override final
	{
		_read_only = read_only;
		::InvalidateRect(_hWnd, 0, 0);
	}

	virtual bool read_only() const override { return _read_only; }

	//virtual property_edited_e::subscriber property_changed() override final { return property_edited_e::subscriber(_em); }
	//
	//virtual item_set_cursor_e::subscriber item_set_cursor() override { return item_set_cursor_e::subscriber(_em); }
	//
	//virtual item_clicked_e::subscriber item_clicked() override final { return item_clicked_e::subscriber(_em); }

	virtual HRESULT ShowTextEditorOnSelectedItem (bool bold, const wchar_t* str) override final
	{
		_ASSERT (_selectedItem);

		if (!_text_editor || try_commit_editor())
		{
			auto enumres = enum_items([si=_selectedItem](IItem* item, LONG y, bool& cancel) { cancel = (item == si); });
			LONG dpi = edge::dpi(_hWnd);
			LONG itemy = enumres.second;
			LONG item_render_y = _bounds.top + BorderWidth(dpi) + itemy - _top_y;
			LONG vcx = ValueColumnLeft(dpi);
			LONG item_height = enumres.first->Height();

			LONG x = vcx + LineWidth(dpi);
			_text_editor.reset (CreateWindowExW (0, L"EDIT", str, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
											 x, item_render_y, ValueColumnRight(dpi) - x, item_height,
											 _hWnd, (HMENU)ID_INPLACE_EDIT, (HINSTANCE)&__ImageBase, 0)); RETURN_LAST_ERROR_IF_NULL(_text_editor);
			SetWindowLongPtr (_text_editor.get(), GWLP_USERDATA, (LONG_PTR) (void*) this);
			_oldInplaceProc = (WNDPROC) SetWindowLongPtr (_text_editor.get(), GWLP_WNDPROC, (LONG_PTR) (void*) &InplaceEditProc);

			SetWindowFont(_text_editor.get(), _paintres.normalFont.get(), TRUE);
			Edit_SetSel(_text_editor.get(), 0, -1);
			SetFocus(_text_editor.get());
			return S_OK;
		}

		return S_FALSE;
	}

	static LRESULT CALLBACK InplaceEditProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		auto pg = (PropertyGridImpl*)(void*)GetWindowLongPtr (hWnd, GWLP_USERDATA);
		_ASSERT (pg->_text_editor.get() == hWnd);

		if (msg == WM_GETDLGCODE)
		{
			return DLGC_WANTALLKEYS | CallWindowProc (pg->_oldInplaceProc, hWnd, msg, wParam, lParam);
		}
		else if (msg == WM_CHAR)
		{
			// Process this message to avoid message beeps.
			if ((wParam == VK_RETURN) || (wParam == VK_TAB) || (wParam == VK_ESCAPE))
				return 0;

			return CallWindowProc (pg->_oldInplaceProc, hWnd, msg, wParam, lParam);
		}
		else if (msg == WM_KEYDOWN)
		{
			if ((wParam == VK_RETURN) || (wParam == VK_DOWN) || (wParam == VK_UP))
			{
				SendMessage (hWnd, EM_SETSEL, 0, -1); // select all text

				if (pg->try_commit_editor())
				{
					LONG itemIndex = -1;
					auto parent = pg->_selectedItem->parent();
					for (LONG i = 0; i < (LONG)parent->child_count(); i++)
					{
						if (parent->child_at(i) == pg->_selectedItem)
						{
							itemIndex = i;
							break;
						}
					}

					RETURN_HR_IF(E_UNEXPECTED, itemIndex == -1);
					size_t nextItemIndex = itemIndex;
					if (wParam == VK_UP)
						nextItemIndex = itemIndex - ((itemIndex > 0) ? 1 : 0);
					else if (wParam == VK_DOWN)
						nextItemIndex = itemIndex + ((itemIndex < (LONG)parent->child_count() - 1) ? 1 : 0);

					if (nextItemIndex != itemIndex)
					{
						RETURN_HR(E_NOTIMPL);
						/*
						// first discard the inplace editor (accesses _selectedItem), then move _selectedItem one item down, then begin editing it.
						pg->DiscardInplaceEdit();

						pg->_selectedItem = siblings[nextItemIndex];
						InvalidateRect (pg->_hwndArea, nullptr, FALSE);
						InvalidateRect (pg->_hwnd, nullptr, FALSE);

						int y = -pg->_topY;
						bool found = pg->FindItemY (pg->_rootItem, y, pg->_selectedItem); rassert(found);
						pg->TryCreateInplace({ 0, y, pg->_gridAreaClientRect.right, y + pg->_selectedItem->GetHeight() });
						*/
					}
				}

				return 0;
			}
			else if (wParam == VK_ESCAPE)
			{
				_ASSERT(false);
				//pg->LoadInplace();
				return 0;
			}
		}

		return CallWindowProc (pg->_oldInplaceProc, hWnd, msg, wParam, lParam);
	}

	virtual HRESULT STDMETHODCALLTYPE ShowEnumEditor (POINT pt, std::span<std::pair<const wchar_t*, int> const> nameValuePairs, int* pdwSelectedIndex) noexcept override
	{
		_text_editor = nullptr;
		HWND window_hwnd = _hWnd;
		uint32_t dpi = edge::dpi(window_hwnd);
		POINT ptScreen = pt;
		::ClientToScreen (window_hwnd, &ptScreen);

		HINSTANCE hInstance = (HINSTANCE) GetWindowLongPtr (window_hwnd, GWLP_HINSTANCE);

		static constexpr wchar_t ClassName[] = L"GIGI-{655C4EA9-2A80-46D7-A7FB-D510A32DC6C6}";
		static constexpr UINT WM_CLOSE_POPUP = WM_APP;

		static ATOM atom = 0;
		if (atom == 0)
		{
			static const auto WndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT
			{
				if ((msg == WM_COMMAND) && (HIWORD(wParam) == BN_CLICKED))
				{
					::PostMessage (hwnd, WM_CLOSE_POPUP, LOWORD(wParam), 0);
					return 0;
				}

				return DefWindowProc (hwnd, msg, wParam, lParam);
			};

			WNDCLASS EditorWndClass =
			{
				0, // style
				WndProc,
				0, // cbClsExtra
				0, // cbWndExtra
				hInstance,
				nullptr, // hIcon
				::LoadCursor(nullptr, IDC_ARROW), // hCursor
				(HBRUSH) (COLOR_3DFACE + 1), // hbrBackground
				nullptr, // lpszMenuName
				ClassName, // lpszClassName
			};

			atom = ::RegisterClassW (&EditorWndClass); _ASSERT (atom != 0);
		}

		auto hwnd = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, ClassName, L"aaa", WS_POPUP | WS_BORDER, 0, 0, 0, 0, window_hwnd, nullptr, hInstance, nullptr); _ASSERT (hwnd != nullptr);

		LONG maxTextWidth = 0;
		LONG maxTextHeight = 0;

		NONCLIENTMETRICS ncMetrics = { sizeof(NONCLIENTMETRICS) };
		if (auto proc_addr = GetProcAddress(GetModuleHandleA("user32.dll"), "SystemParametersInfoForDpi"))
		{
			auto proc = reinterpret_cast<BOOL(WINAPI*)(UINT, UINT, PVOID, UINT, UINT)>(proc_addr);
			proc(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncMetrics, 0, dpi);
		}
		else
			SystemParametersInfo (SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncMetrics, 0);
		HFONT_unique_ptr font (CreateFontIndirect (&ncMetrics.lfMenuFont));

		auto hdc = ::GetDC(hwnd);
		auto oldFont = ::SelectObject (hdc, font.get());
		for (uint32_t i = 0; i < nameValuePairs.size(); i++)
		{
			RECT rc = { };
			DrawTextW (hdc, nameValuePairs[i].first, -1, &rc, DT_CALCRECT);
			maxTextWidth = std::max (maxTextWidth, rc.right);
			maxTextHeight = std::max (maxTextHeight, rc.bottom);
		}
		::SelectObject (hdc, oldFont);
		::ReleaseDC (hwnd, hdc);

		int lrpadding = 7 * dpi / 96;
		int udpadding = ((nameValuePairs.size() <= 5) ? 5 : 0) * dpi / 96;
		LONG buttonWidth = std::max (100l * (LONG)dpi / 96, maxTextWidth + 2 * lrpadding) + 2 * GetSystemMetrics(SM_CXEDGE);
		LONG buttonHeight = maxTextHeight + 2 * udpadding + 2 * GetSystemMetrics(SM_CYEDGE);

		int margin = 4 * dpi / 96;
		int spacing = 2 * dpi / 96;
		int y = margin;
		for (uint32_t nvp_index = 0; nvp_index < nameValuePairs.size(); )
		{
			constexpr DWORD dwStyle = WS_CHILD | WS_VISIBLE | BS_NOTIFY | BS_FLAT;
			auto button = CreateWindowEx (0, L"Button", nameValuePairs[nvp_index].first, dwStyle, margin, y, buttonWidth, buttonHeight, hwnd, (HMENU)(size_t)nvp_index, hInstance, nullptr);
			::SendMessage (button, WM_SETFONT, (WPARAM) font.get(), FALSE);
			nvp_index++;
			y += buttonHeight + (nvp_index < nameValuePairs.size() ? spacing : margin);
		}
		RECT wr = { 0, 0, margin + buttonWidth + margin, y };
		::AdjustWindowRectEx (&wr, (DWORD) GetWindowLongPtr(hwnd, GWL_STYLE), FALSE, (DWORD) GetWindowLongPtr(hwnd, GWL_EXSTYLE));
		::SetWindowPos (hwnd, nullptr, ptScreen.x, ptScreen.y, wr.right - wr.left, wr.bottom - wr.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);

		int selected_nvp_index = -1;
		MSG msg;
		while (GetMessage(&msg, 0, 0, 0))
		{
			if ((msg.hwnd == hwnd) && (msg.message == WM_CLOSE_POPUP))
			{
				selected_nvp_index = (int) msg.wParam;
				break;
			}

			if ((msg.message == WM_KEYDOWN) && (msg.wParam == VK_ESCAPE))
				break;

			bool exitLoop = false;
			if ((msg.hwnd != hwnd) && (::GetParent(msg.hwnd) != hwnd))
			{
				if ((msg.message == WM_LBUTTONDOWN) || (msg.message == WM_RBUTTONDOWN) || (msg.message == WM_MBUTTONDOWN)
					|| (msg.message == WM_NCLBUTTONDOWN) || (msg.message == WM_NCRBUTTONDOWN) || (msg.message == WM_NCMBUTTONDOWN))
				{
					ShowWindow (hwnd, SW_HIDE);
					exitLoop = true;
				}
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (exitLoop)
				break;
		}

		::DestroyWindow (hwnd);
		*pdwSelectedIndex = selected_nvp_index;
		return (selected_nvp_index >= 0) ? S_OK : S_FALSE;
	}

	STDMETHOD(NotifyLayoutChanged)(IItem* item) override
	{
		PaintResources ctx;
		auto hr = MakePaintResources (GetDC(_hWnd), ctx); RETURN_IF_FAILED(hr);

		LONG unused = 0;
		enum_items_from (item, unused, [&ctx](IItem* i, LONG render_y, bool& cancel) {
			auto hr = i->PerformLayout(ctx);
			if (FAILED(hr))
				cancel = true;
		});
		
		return S_OK;
	}

	virtual void NotifyItemRemoving (IItem* item) override
	{
		if (item == _selectedItem)
		{
			_selectedItem = nullptr;
			_text_editor = nullptr;
			::InvalidateRect(_hWnd, 0, 0);
		}
		
		if (item == _hotItem)
		{
			_hotItem = nullptr;
			::InvalidateRect(_hWnd, 0, 0);
		}

		if (auto ei = item->AsExpandable())
		{
			for (uint32_t i = 0; i < ei->child_count(); i++)
				NotifyItemRemoving(ei->child_at(i));
		}
	}

	virtual const PaintResources& GetPaintResources() const override { return _paintres; }

	enum class htcode { none, expand, name, value, output };

	struct htresult
	{
		IItem* item;
		LONG     render_y;
		htcode   code;

		operator bool() const { return item != nullptr; }
	};

	htresult hit_test (POINT pp) const
	{
		htresult result = { nullptr };

		uint32_t dpi = edge::dpi(_hWnd);
		enum_items ([this, pp, &result, dpi, bw=BorderWidth(dpi)](IItem* item, LONG item_y, bool& cancel)
		{
			if (item_y + item->Height() <= _top_y)
			{
				// item is scrolled above the visible area
			}
			else
			{
				LONG render_y = _bounds.top + bw + item_y - _top_y;
				if (pp.y < render_y + item->Height())
				{
					result = htresult{ };
					result.item = item;
					result.render_y = render_y;

					if (pp.x < NameColumnLeft(item->indent(), dpi))
						result.code = htcode::expand;
					else if (pp.x < ValueColumnLeft(dpi))
						result.code = htcode::name;
					else
						result.code = htcode::value;

					cancel = true;
				}
			}
		});

		return result;
	}

	STDMETHOD(GetValueText)(edge::IObjectList* section, IDispatch* object, DISPID prop, read_state* pState, BSTR* pbstrValueText) override
	{
		auto it = std::find_if(_root_items.begin(), _root_items.end(), [section](IRootItem* ri) { return ri->objects() == section; });
		if (it == _root_items.end())
			RETURN_HR(E_INVALIDARG);
		IRootItem* root = *it;

		auto findObjectItem = [object](auto& self, IItem* item) -> IObjectItem*
		{
			if (auto oi = wil::try_com_query_nothrow<IObjectItem>(item))
			{
				if (oi->objects()->contains(object))
					return oi;
			}

			if (auto ei = item->AsExpandable())
			{
				for (uint32_t i = 0; i < ei->child_count(); i++)
				{
					if (auto child = self(self, ei->child_at(i)))
						return child;
				}
			}

			return nullptr;
		};

		auto oi = findObjectItem(findObjectItem, root);
		if (!oi)
			RETURN_HR(E_INVALIDARG);

		for (uint32_t i = 0; i < oi->child_count(); i++)
		{
			IGroupItem* gi = oi->ChildGroupItemAt(i);
			for (IPGPropertyItem* pi : gi->children())
			{
				if (pi->property() == prop)
					return pi->GetValue(pState, pbstrValueText);
			}
		}

		RETURN_HR(E_INVALIDARG);
	}

	std::optional<LRESULT> OnMouseButtonDown (const mouse_ud_args& args)
	{
		SetFocus(_hWnd);

		auto clicked_item = hit_test(args.pt);

		auto new_selected_item = (clicked_item.item && clicked_item.item->selectable()) ? clicked_item.item : nullptr;
		if (_selectedItem != new_selected_item)
		{
			_text_editor = nullptr;
			_selectedItem = new_selected_item;
			::InvalidateRect(_hWnd, 0, 0);
		}

		if (clicked_item.item)
		{
			clicked_item.item->ProcessMouseDown (args, clicked_item.render_y);
			return 0;
		}

		return std::nullopt;
	}

	std::optional<LRESULT> OnMouseButtonUp (const mouse_ud_args& args)
	{
		auto clicked_item = hit_test(args.pt);
		if (clicked_item.item)
		{
			// TODO: pass first to the item's on_mouse_down, and if that one returns std::nullopt,
			// then pass to a new "item_mouse_up" event, and if that one returns std::nullopt,
			// then generate the "clicked" event and repeat (first to item, then to pg event handler).
			//auto res = item_clicked_e::invoker(_em).invoke(clicked_item);
			//if (res.has_value())
			//	return res;

			clicked_item.item->ProcessMouseUp (args, clicked_item.render_y);
			return 0;
		}

		return std::nullopt;
	}

	void OnMouseMove (HWND hwnd, POINT pt, UINT mks)
	{
		auto htres = hit_test(pt);
		auto newHotItem = (htres.item && htres.item->selectable()) ? htres.item : nullptr;
		if (_hotItem != newHotItem)
		{
			if (_hotItem)
				InvalidateItem(_hotItem);
			_hotItem = newHotItem;
			if (_hotItem)
				InvalidateItem(_hotItem);
		}
				
		if (!_last_tt_location || (_last_tt_location != pt))
		{
			_last_tt_location = pt;

			RECT rc;
			if (_text_editor && GetWindowRect(_text_editor.get(), &rc) && AdjustWindowRect(&rc, WS_CHILD, 0) && PtInRect(&rc, pt))
			{
				TOOLINFO ti = { sizeof(TOOLINFO), 0, hwnd };
				SendMessage(_tooltip, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
			}
			else
			{
				::SendMessage (_tooltip, TTM_POP, 0, 0);

				wil::unique_process_heap_string text;
				wil::unique_process_heap_string title;

				if (htres.item)
				{
					title = htres.item->description_title();
					text  = htres.item->description_text();

					if (title && !text)
						text = wil::make_process_heap_string_nothrow(L"--");
				}

				TOOLINFO ti = { sizeof(TOOLINFO) };
				ti.hwnd     = hwnd;
				ti.lpszText = text.get();
				SendMessage(_tooltip, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);

				SendMessage(_tooltip, TTM_SETTITLE, TTI_INFO, (LPARAM)title.get());
			}
		}
	}

	std::optional<LRESULT> process_wm_mousewheel (HWND hwnd, POINT pt, UINT mks, short delta)
	{
		uint32_t dpi = edge::dpi(hwnd);
		LONG visible_height = _bounds.bottom - _bounds.top;
		LONG total_height = enum_items([](auto...) { }).second;
		if (total_height > visible_height)
		{
			UINT scroll_lines;
			::SystemParametersInfo (SPI_GETWHEELSCROLLLINES, 0, &scroll_lines, 0);

			//float scroll_line_height = text_layout_with_metrics(_renderer->dwrite_factory(), _text_format, "A").height();
			//float pw = edge::pixel_width(dpi);
			//scroll_line_height = std::ceil(scroll_line_height / pw) * pw + line_width(dpi);
			LONG scroll_line_height = 20;

			LONG scroll_distance = (LONG)scroll_lines * scroll_line_height * delta / 120;
			if (scroll_distance < 0)
			{
				// wheel down; scroll items up
				if (total_height > (_top_y + _bounds.bottom - _bounds.top))
				{
					// there are items below the bottom of the grid that can be scroll up
					LONG scrollable = total_height - (_top_y + _bounds.bottom - _bounds.top);
					scroll_distance = -scroll_distance;
					if (scroll_distance > scrollable)
						scroll_distance = scrollable;
					_top_y += scroll_distance;
					_text_editor = nullptr;
					::InvalidateRect(_hWnd, 0, 0);
				}
			}
			else if (scroll_distance > 0)
			{
				// wheel up; shift items down
				if (_top_y > 0)
				{
					// there are items above the top of the grid that can be scrolled down.
					if (_top_y > scroll_distance)
						_top_y -= scroll_distance;
					else
						_top_y = 0;
					_text_editor = nullptr;
					::InvalidateRect(_hWnd, 0, 0);
				}
			}
		}

		return 0;
	}

	bool try_commit_editor()
	{
		HRESULT hr;

		_ASSERT (_text_editor);
		_ASSERT (_selectedItem);

		wil::unique_process_heap_string errorMessage;

		if (auto vpi = wil::try_com_query_nothrow<IPGValuePropertyItem>(_selectedItem))
		{
			int len = GetWindowTextLength(_text_editor.get());
			auto text = wil::make_process_heap_string_nothrow(0, len);
			if (!text)
				return false;
			GetWindowText(_text_editor.get(), text.get(), len + 1);
			wil::unique_variant value;
			hr = InitVariantFromString(text.get(), &value); LOG_IF_FAILED(hr);
			if (vpi->VarType() != VT_BSTR) {
				wil::unique_variant temp;
				hr = VariantChangeTypeEx (&temp, &value, InvariantLCID, 0, vpi->VarType());
				if (FAILED(hr))
				{
					wil::str_printf_nothrow (errorMessage, L"Cannot change \"%s\" to the property type.", text.get());
					goto showErrorMessage;
				}
				
				value = std::move(temp);
			}
				
			auto root = vpi->root();
			auto objs = vpi->parent()->parent()->objects();
			hr = this->change_property (*objs, vpi->TypeInfo(), vpi->property(), &value);
			if (FAILED(hr))
			{
				com_ptr<IErrorInfo> ei;
				wil::unique_bstr temp;
				if (GetErrorInfo(0, &ei) == S_OK && SUCCEEDED(ei->GetDescription(&temp))) {
					errorMessage = wil::make_process_heap_string_nothrow(temp.get());
				} else {
					wil::str_printf_nothrow(errorMessage, L"Error 0x%08x", hr);
				}
				goto showErrorMessage;
			}
		}
		else if (auto vcci = wil::try_com_query_nothrow<value_collection_child_item_i>(_selectedItem))
		{
			_ASSERT(false);
			//size_t value_index = vcci->parent()->index_of(vcci);
			//auto* objs = vcci->parent()->parent()->parent()->objects();
			//auto root = vcci->root();
			//this->change_property (objs, vcci->parent()->property(), value_index, utf16_to_utf8(_text_editor->wstr()), root->app_context());
		}
		else
			_ASSERT(false);

		::InvalidateRect(_hWnd, nullptr, TRUE);
		_text_editor = nullptr;
		return true;

	showErrorMessage:
		MessageBox (_hWnd, errorMessage.get(), L"Cannot set property", 0);
		::SetFocus (_hWnd);
		Edit_SetSel(_text_editor.get(), 0, -1);
		return false;
	}
	
	virtual HRESULT STDMETHODCALLTYPE change_property (const IObjectList& objects, ITypeInfo* ti, MEMBERID memid, VARIANT* newValue) override
	{
		//auto pe_invoker = property_edited_e::invoker(_em);
		//bool pe_has_handlers = _em->has_handlers<property_edited_e>();

		//DISPPARAMS params = { };
		//EXCEPINFO exception;
		//UINT uArgErr;
		//wil::unique_variant value;

		std::vector<wil::unique_variant> old_values;
		//if (pe_has_handlers)
		//{
		//	old_values.reserve(objects.size());
		//	for (auto o : objects)
		//	{
		//		auto hr = ti->Invoke(o, memid, DISPATCH_PROPERTYGET, &params, &value, &exception, &uArgErr); RETURN_IF_FAILED(hr);
		//		old_values.push_back(std::move(value));
		//	}
		//}

		for (uint32_t i = 0; i < objects.size(); i++)
		{
			DISPID named = DISPID_PROPERTYPUT;
			DISPPARAMS params = { .rgvarg = newValue, .rgdispidNamedArgs=&named, .cArgs = 1, .cNamedArgs = 1 };
			wil::unique_variant result; // TODO: get rid of this
			EXCEPINFO exception;
			UINT uArgErr;
			auto hr = ti->Invoke(objects[i], memid, DISPATCH_PROPERTYPUT, &params, &result, &exception, &uArgErr);
			if (FAILED(hr))
			{
				wil::unique_bstr message;
				if (hr == DISP_E_EXCEPTION)
				{
					hr = exception.scode;
					message.reset(exception.bstrDescription);
				}

				for (uint32_t j = 0; j < i; j++)
				{
					params.rgvarg = old_values[j].addressof();
					ti->Invoke(objects[j], memid, DISPATCH_PROPERTYPUT, &params, &result, &exception, &uArgErr);
				}

				if (message)
				{
					com_ptr<ICreateErrorInfo> cei;
					if (SUCCEEDED(::CreateErrorInfo(&cei)))
					{
						cei->SetDescription(message.get());
						::SetErrorInfo(0, wil::try_com_query_nothrow<IErrorInfo>(cei));
					}

					return hr;
				}

				return hr;
			}
		}

		//if (pe_has_handlers)
		//{
		//	wil::unique_variant newVal;
		//	auto hr = VariantCopy (&newVal, newValue); LOG_IF_FAILED(hr);
		//	auto args = value_property_edited_args{ memid, objects, std::move(old_values), std::move(newVal), NULL };//scc };
		//	pe_invoker.invoke(std::move(args));
		//}

		return S_OK;
	}
	/*
	virtual void change_property (const IObjectList& objects, const object_property* prop, const concrete_type* type) override final
	{
		auto args = object_property_edited_args{ prop, objects };
		for (object* obj : objects)
		{
			std::unique_ptr<object> new_value;
			if (auto event_res = creating_object_e::invoker(_em).invoke(obj, prop, type))
				new_value = std::move(event_res);
			else
				new_value = type->create();

			auto old_value = prop->set(obj, std::move(new_value));
			args.old_values.push_back(std::move(old_value));
		}
		property_edited_e::invoker(_em).invoke(std::move(args));
	}

	virtual void change_property (const IObjectList& objects, const value_collection_property* prop, size_t value_index, std::string new_value_str, edge::string_convert_context_i* scc) override final
	{
		std::vector<std::string> old_values;
		for (auto o : objects)
			old_values.push_back (prop->get_to_string(o, value_index, scc));

		for (size_t i = 0; i < objects.size(); i++)
		{
			try
			{
				prop->set_from_string (new_value_str, objects[i], value_index, scc);
			}
			catch (const std::exception&)
			{
				for (size_t j = 0; j < i; j++)
					prop->set_from_string(old_values[j], objects[j], value_index, scc);
				throw;
			}
		}

		auto args = value_collection_property_edited_args{ prop, objects, value_index, std::move(old_values), std::move(new_value_str), scc };
		property_edited_e::invoker(_em).invoke(std::move(args));
	}
	*/
	LONG BorderWidth (LONG dpi) const
	{
		return (LONG)std::roundf(_borderWidthDIPs / 96 * dpi);
	}

	LONG ScrollBarWidth (LONG dpi) const
	{
		return (LONG)std::roundf(16.0f * dpi / 96);
	}

	virtual LONG LineWidth (LONG dpi) const noexcept override
	{
		return (LONG)roundf(line_width_not_aligned * dpi / 96.0f);
	}

	virtual LONG ExpandColumnLeft (LONG dpi) const noexcept override
	{
		return _bounds.left + BorderWidth(dpi);
	}

	virtual LONG NameColumnLeft (uint32_t indent, LONG dpi) const noexcept override
	{
		return _bounds.left + BorderWidth(dpi) + indent * IndentWidth(dpi);
	}

	virtual LONG ValueColumnLeft (LONG dpi) const noexcept override
	{
		LONG bw = BorderWidth(dpi);
		LONG w = (LONG)std::roundf ((_bounds.right - _bounds.left - 2 * bw) * _name_column_factor);
		LONG minWidth = 75 * dpi / 96;
		if (w < minWidth)
			w = minWidth;
		return _bounds.left + bw + w;
	}

	virtual LONG ValueColumnRight (LONG dpi) const noexcept override
	{
		LONG r = _bounds.right - BorderWidth(dpi);
		if (_scroll_bar_visible)
			r -= ScrollBarWidth(dpi);
		return r;
	}

	virtual LONG IndentWidth (LONG dpi) const noexcept override
	{
		return 8 * dpi / 96;
	}

	virtual const IThemeColorProvider* tcp() const override final { return _tcp; }

	std::optional<LRESULT> process_key_down (uint32_t key, UINT mks)
	{
		if ((key == VK_RETURN) || (key == VK_UP) || (key == VK_DOWN))
		{
			if (!_text_editor || try_commit_editor())
			{
				if (key == VK_UP)
				{
					// select previous item
				}
				else if (key == VK_DOWN)
				{
					// select next item
				}
			}

			return 0;
		}

		if ((key == VK_ESCAPE) && _text_editor)
		{
			_text_editor = nullptr;
			return 0;
		}

		return std::nullopt;
	}

	std::optional<LRESULT> process_key_up (uint32_t key, UINT mks)
	{
		return std::nullopt;
	}

	std::optional<LRESULT> process_char_key (uint32_t key)
	{
		return std::nullopt;
	}

	virtual bool editing_text() const override final
	{
		return _text_editor != nullptr;
	}

	virtual RECT calc_popup_window_pos (IItem* item, LONG item_y, SIZE client_size_requested, DWORD style, DWORD ex_style) const override final
	{
		_ASSERT(false); return { };
		/*
		HWND hwnd = _hWnd;
		uint32_t dpi = edge::dpi(hwnd);
		D2D1_RECT_F item_rect = _rectd;
		item_rect.top = item_y;
		item_rect.bottom = item_y + height_aligned(item);
		RECT item_rect_pixels = edge::rectd_to_rectp(item_rect, dpi, 1);
		::ClientToScreen(hwnd, (LPPOINT)&item_rect_pixels.left);
		::ClientToScreen(hwnd, (LPPOINT)&item_rect_pixels.right);

		POINT anchor_point = { (item_rect_pixels.left + item_rect_pixels.right) / 2, item_rect_pixels.top };

		RECT window_rect = { 0, 0, edge::lengthd_to_lengthp(client_size_requested.width, dpi, 1), edge::lengthd_to_lengthp(client_size_requested.height, dpi, 1) };

		if (auto proc_addr = GetProcAddress(GetModuleHandleA("User32.dll"), "AdjustWindowRectExForDpi"))
		{
			auto proc = reinterpret_cast<BOOL(WINAPI*)(LPRECT,DWORD,BOOL,DWORD,UINT)>(proc_addr);
			proc (&window_rect, style, FALSE, ex_style, dpi);
		}
		else
			AdjustWindowRectEx(&window_rect, style, FALSE, ex_style);

		SIZE size = { window_rect.right - window_rect.left, window_rect.bottom - window_rect.top };

		RECT popup_window_pos;
		BOOL bres = ::CalculatePopupWindowPosition (&anchor_point, &size, 0, &item_rect_pixels, &popup_window_pos); _ASSERT(bres);
		return popup_window_pos;
		*/
	}

	virtual void expand_all() override final
	{
		for (auto& ri : _root_items)
		{
			for (uint32_t i = 0; i < ri->child_count(); i++)
			{
				auto child = ri->child_at(i);
				_ASSERT(false);
				//auto gi = checked_static_cast<IGroupItem*>(child);
				//gi->expand_all();
			}
		}
	}
	
	//virtual creating_object_e::subscriber creating_object() override final
	//{
	//	return creating_object_e::subscriber(*_em);
	//}

	virtual IItem* selected_item() const override final { return _selectedItem; }
};

HRESULT pg::MakePropertyGrid (HWND hWnd, const RECT& bounds, edge::IThemeColorProvider* tp, IPropertyGrid** ppGrid)
{
	auto p = com_ptr(new (std::nothrow) PropertyGridImpl()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(hWnd, bounds, tp); RETURN_IF_FAILED(hr);
	*ppGrid = p.detach();
	return S_OK;
}

IRootItem* IItem::root()
{
	auto current = this;
	while (true)
	{
		if (auto r = current->as_root())
			return r;
		_ASSERT(current->parent());
		current = current->parent()->as_item();
	}
}

#pragma region struct IItem
ULONG IItem::indent() const
{
	return this->as_root() ? 0 : (parent()->as_item()->indent() + 1);
}

#pragma region struct IExpandableItem
void IExpandableItem::render_expand_button (const PaintResources& ctx, LONG item_y) const
{
	_ASSERT(false);
	/*
	auto grid = this->as_item()->root()->grid();
	LONG dpi = edge::dpi(grid->HWnd());
	float pw = edge::pixel_width(dpi);
	LONG height = this->as_item()->Height();
	_ASSERT(height > 0);
	LONG name_line_x = grid->NameColumnLeft(as_item()->indent(), dpi);

	com_ptr<ID2D1Factory> f;
	rc.dc->GetFactory(&f);
	com_ptr<ID2D1StrokeStyle> ss;
	f->CreateStrokeStyle (D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND), nullptr, 0, &ss);

	float lw = 2.5f;
	float size = std::min(height, grid->IndentWidth(dpi)) - lw;
	if (!expanded())
	{
		POINT tip = { name_line_x - grid->IndentWidth(dpi) / 2 + size / 4, item_y + height / 2 };
		rc.dc->DrawLine (tip, { tip.x - size / 2, tip.y - size / 2 }, rc.fore, lw, ss);
		rc.dc->DrawLine (tip, { tip.x - size / 2, tip.y + size / 2 }, rc.fore, lw, ss);
	}
	else
	{
		POINT tip = { name_line_x - grid->IndentWidth(dpi) / 2, item_y + height / 2 + size / 4 };
		rc.dc->DrawLine (tip, { tip.x - size / 2, tip.y - size / 2 }, rc.fore, lw, ss);
		rc.dc->DrawLine (tip, { tip.x + size / 2, tip.y - size / 2 }, rc.fore, lw, ss);
	}
	*/
}

void IExpandableItem::expand_all()
{
	if (!expanded())
		expand();

	for (uint32_t i = 0; i < this->child_count(); i++)
	{
		auto c = this->child_at(i);
		if (auto ei = c->AsExpandable())
			ei->expand_all();
	}
}
#pragma endregion

HRESULT pg::GetNameText (const IPGPropertyItem* item, wil::unique_bstr& nameText)
{
	nameText.reset();

	ITypeInfo* ti = item->TypeInfo();
	FUNCDESC* fd;
	auto hr = ti->GetFuncDesc(item->GetterFuncIndex(), &fd); RETURN_IF_FAILED(hr);
	auto releaseFD = wil::scope_exit([ti, fd] { ti->ReleaseFuncDesc(fd); });

	wil::unique_bstr name;
	UINT cNames;
	hr = ti->GetNames(item->property(), &name, 1, &cNames); RETURN_IF_FAILED(hr);

	nameText = std::move(name);
	return S_OK;
}

extern HRESULT MakeValuePropertyItem (IGroupItem* parent, ITypeInfo* typeInfo, DISPID prop, VARENUM vartype,
									  vector_nothrow<std::pair<wil::unique_bstr, LONG>> nvps,
									  WORD getterFuncIndex, WORD setterFuncIndex, IPGPropertyItem** ppItem);
extern std::unique_ptr<IPGPropertyItem> make_object_property_item (IGroupItem* parent, DISPID prop);
extern std::unique_ptr<IPGPropertyItem> make_value_collection_item (IGroupItem* parent, DISPID prop);
extern HRESULT MakeObjectCollectionItem (IGroupItem* parent, DISPID prop, IPGPropertyItem** ppItem);

HRESULT MakePropertyItem (IGroupItem* parent, DISPID prop, IPGPropertyItem** ppItem)
{
	HRESULT hr;

	wil::com_ptr_nothrow<ITypeInfo> typeInfo;
	hr = parent->parent()->objects()->front()->GetTypeInfo(0, InvariantLCID, &typeInfo); RETURN_IF_FAILED(hr);
	TYPEATTR* typeAttr;
	hr = typeInfo->GetTypeAttr(&typeAttr); RETURN_IF_FAILED(hr);
	auto releaseTypeAttr = wil::scope_exit([ti=typeInfo.get(), typeAttr] { ti->ReleaseTypeAttr(typeAttr); });

	VARENUM vt;
	int getterFuncIndex = -1;
	int setterFuncIndex = -1;
	HREFTYPE hreftype;
	for (WORD i = 0; i < typeAttr->cFuncs; i++)
	{
		FUNCDESC* fd;
		hr = typeInfo->GetFuncDesc(i, &fd); RETURN_IF_FAILED(hr);
		auto releaseFundDesc = wil::scope_exit([ti=typeInfo.get(), fd] { ti->ReleaseFuncDesc(fd); });
		if (fd->memid == prop)
		{
			if (fd->invkind == INVOKE_PROPERTYGET)
			{
				getterFuncIndex = i;
				vt = (VARENUM)fd->elemdescFunc.tdesc.vt;
				hreftype = fd->elemdescFunc.tdesc.hreftype;
			}
			else if (fd->invkind == INVOKE_PROPERTYPUT)
				setterFuncIndex = i;

			if (getterFuncIndex != -1 && setterFuncIndex != -1)
				break;
		}
	}

	RETURN_HR_IF(E_UNEXPECTED, getterFuncIndex == -1);

	switch (vt)
	{
		case VT_UI1:
		case VT_UI2:
		case VT_UI4:
		case VT_I1:
		case VT_I2:
		case VT_I4:
		case VT_BSTR:
		case VT_BOOL:
			return MakeValuePropertyItem (parent, typeInfo, prop, vt, { }, (WORD)getterFuncIndex, (WORD)setterFuncIndex, ppItem);

		case VT_USERDEFINED:
		{
			com_ptr<ITypeInfo> refTypeInfo;
			hr = typeInfo->GetRefTypeInfo(hreftype, &refTypeInfo); RETURN_IF_FAILED(hr);
			TYPEATTR* refTypeAttr;
			hr = refTypeInfo->GetTypeAttr(&refTypeAttr); RETURN_IF_FAILED(hr);
			auto releaseRefTypeAttr = wil::scope_exit([ti=refTypeInfo.get(), refTypeAttr] { ti->ReleaseTypeAttr(refTypeAttr); });
			if (refTypeAttr->typekind == TKIND_ENUM)
			{
				vector_nothrow<std::pair<wil::unique_bstr, LONG>> nvps;
				bool reserved = nvps.try_reserve(refTypeAttr->cVars); RETURN_HR_IF(E_OUTOFMEMORY, !reserved);
				for (WORD i = 0; i < refTypeAttr->cVars; i++)
				{
					VARDESC* varDesc;
					auto hr = refTypeInfo->GetVarDesc(i, &varDesc); RETURN_IF_FAILED(hr);
					auto releaseVarDesc = wil::scope_exit([&refTypeInfo, varDesc] { refTypeInfo->ReleaseVarDesc(varDesc); });
					RETURN_HR_IF(E_INVALIDARG, varDesc->lpvarValue->vt != VT_I4);
					wil::unique_bstr name;
					wil::unique_bstr docString;
					hr = refTypeInfo->GetDocumentation(varDesc->memid, &name, &docString, nullptr, nullptr); RETURN_IF_FAILED(hr);
					if (docString)
						nvps.try_push_back({ std::move(docString), varDesc->lpvarValue->lVal });
					else if (name)
						nvps.try_push_back({ std::move(name), varDesc->lpvarValue->lVal });
					else
						RETURN_HR(DISP_E_UNKNOWNNAME);
				}

				return MakeValuePropertyItem (parent, typeInfo, prop, vt, std::move(nvps), (WORD)getterFuncIndex, (WORD)setterFuncIndex, ppItem);
			}

			RETURN_HR(E_NOTIMPL);
		}

		default:
			RETURN_HR(E_NOTIMPL);
	}

	/*
	if (auto obj_coll_prop = dynamic_cast<const object_collection_property*>(prop))
	return make_object_collection_item(parent, obj_coll_prop);

	if (auto obj_prop = dynamic_cast<const object_property*>(prop))
	return make_object_property_item(parent, obj_prop);

	if (auto value_coll_prop = dynamic_cast<const value_collection_property*>(prop))
	return make_value_collection_item(parent, value_coll_prop);
	*/
	// TODO: placeholder pg item for unknown types of properties
	RETURN_HR(E_NOTIMPL);
}
