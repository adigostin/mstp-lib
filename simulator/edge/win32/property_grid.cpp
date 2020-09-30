
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "property_grid.h"
#include "utility_functions.h"

using namespace edge;

namespace edge
{
	class property_grid;
}

#pragma warning (disable: 4250)

class edge::property_grid : public edge::event_manager, public property_grid_i
{
	d2d_window_i* const _window;
	com_ptr<IDWriteTextFormat> _text_format;
	com_ptr<IDWriteTextFormat> _bold_text_format;
	com_ptr<IDWriteTextFormat> _wingdings;
	std::unique_ptr<text_editor_i> _text_editor;
	RECT _rectp;
	D2D1_RECT_F _rectd;
	float _name_column_factor = 0.6f;
	std::vector<std::unique_ptr<root_item>> _root_items;
	pgitem* _selected_item = nullptr;
	HWND _tooltip = nullptr;
	POINT _last_tt_location = { -1, -1 };
	float _border_width_not_aligned = 0;

public:
	property_grid (d2d_window_i* window, const RECT& rectp)
		: _window(window)
		, _rectp(rectp)
		, _rectd(window->rectp_to_rectd(rectp))
	{
		auto hinstance = (HINSTANCE)::GetWindowLongPtr (_window->hwnd(), GWLP_HINSTANCE);

		_tooltip = CreateWindowEx (WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr,
			WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			_window->hwnd(), nullptr, hinstance, nullptr);

		TOOLINFO ti = { sizeof(TOOLINFO) };
		ti.uFlags   = TTF_SUBCLASS;
		ti.hwnd     = _window->hwnd();
		ti.lpszText = nullptr;
		ti.rect     = rectp;
		SendMessage(_tooltip, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);

		SendMessage (_tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 1500);
		SendMessage (_tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, (LPARAM)(LONG)MAXSHORT);
		SendMessage (_tooltip, TTM_SETMAXTIPWIDTH, 0, rectp.right - rectp.left);

		auto hr = _window->dwrite_factory()->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
												   DWRITE_FONT_STRETCH_NORMAL, pgitem::font_size, L"en-US", &_text_format); assert(SUCCEEDED(hr));

		hr = _window->dwrite_factory()->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
											  DWRITE_FONT_STRETCH_NORMAL, pgitem::font_size, L"en-US", &_bold_text_format); assert(SUCCEEDED(hr));

		hr = _window->dwrite_factory()->CreateTextFormat (L"Wingdings", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
											  DWRITE_FONT_STRETCH_NORMAL, pgitem::font_size, L"en-US", &_wingdings); assert(SUCCEEDED(hr));
	}

	virtual ~property_grid()
	{
		::DestroyWindow(_tooltip);
	}

	virtual IDWriteFactory* dwrite_factory() const override final { return _window->dwrite_factory(); }

	virtual IDWriteTextFormat* text_format() const override final { return _text_format; }

	virtual IDWriteTextFormat* bold_text_format() const override final { return _bold_text_format; }

	virtual void invalidate() override final { ::InvalidateRect (_window->hwnd(), nullptr, FALSE); } // TODO: optimize
	/*
	virtual std::optional<LRESULT> window_proc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		auto resultBaseClass = base::window_proc (hwnd, msg, wParam, lParam);

		if (msg == WM_GETDLGCODE)
		{
			if (_text_editor)
				return DLGC_WANTALLKEYS;

			return resultBaseClass;
		}

		if ((msg == WM_SETFOCUS) || (msg == WM_KILLFOCUS))
		{
			::InvalidateRect (hwnd, nullptr, 0);
			return 0;
		}

		return resultBaseClass;
	}
	*/
	virtual HCURSOR cursor_at (POINT pp, D2D1_POINT_2F pd) const override
	{
		if ((pd.x >= value_column_x()) && (pd.x < _rectd.right))
		{
			auto htr = hit_test(pd);
			if (htr.item)
				return htr.item->cursor();
		}

		return nullptr;
	}

	void enum_items (const std::function<void(pgitem*, float y, bool& cancel)>& callback) const
	{
		std::function<void(pgitem* item, float& y, size_t indent, bool& cancel)> enum_items_inner;

		enum_items_inner = [this, &enum_items_inner, &callback, vcx=value_column_x(), bw=border_width()](pgitem* item, float& y, size_t indent, bool& cancel)
		{
			auto item_height = item->content_height_aligned();
			if (item_height > 0)
			{
				callback(item, y, cancel);
				if (cancel)
					return;

				y += item_height;
			}

			if (auto ei = dynamic_cast<expandable_item*>(item); ei && ei->expanded())
			{
				for (auto& child : ei->children())
				{
					enum_items_inner (child.get(), y, indent + 1, cancel);
					if (cancel)
						break;
				}
			}
		};

		float y = _rectd.top + border_width();
		bool cancel = false;
		for (auto& root_item : _root_items)
		{
			if (y >= _rectd.bottom)
				break;
			enum_items_inner (root_item.get(), y, 0, cancel);
			if (cancel)
				break;
		}
	}

	void perform_layout (pgitem* item)
	{
		item->perform_layout();

		if (auto ei = dynamic_cast<expandable_item*>(item); ei && ei->expanded())
		{
			for (auto& child : ei->children())
				perform_layout (child.get());
		};

		invalidate();
	}

	virtual void render (ID2D1DeviceContext* dc) const override
	{
		dc->SetTransform (_window->dpi_transform());

		com_ptr<ID2D1SolidColorBrush> back_brush;
		dc->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOW), &back_brush);
		dc->FillRectangle(_rectd, back_brush);

		com_ptr<ID2D1SolidColorBrush> fore_brush;
		dc->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOWTEXT), &fore_brush);

		float bw = border_width();
		if (bw > 0)
		{
			com_ptr<ID2D1SolidColorBrush> border_brush;
			dc->CreateSolidColorBrush ({ 0.5f, 0.5f, 0.5f, 1 }, &border_brush);
			dc->DrawRectangle(inflate(_rectd, -bw / 2), border_brush, bw);
		}

		if (_root_items.empty())
		{
			auto tl = text_layout_with_metrics (dwrite_factory(), _text_format, "(no selection)");
			D2D1_POINT_2F p = { (_rectd.left + _rectd.right) / 2 - tl.width() / 2, (_rectd.top + _rectd.bottom) / 2 - tl.height() / 2};
			dc->DrawTextLayout (p, tl, fore_brush);
			return;
		}

		bool focused = GetFocus() == _window->hwnd();

		render_context rc;
		rc.dc = dc;
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOW), &rc.back_brush);
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOWTEXT), &rc.fore_brush);
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_MENUHILIGHT), &rc.selected_back_brush_focused);
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_MENU), &rc.selected_back_brush_not_focused);
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOWTEXT), &rc.selected_fore_brush);
		dc->CreateSolidColorBrush ({ 0.7f, 0.7f, 0.7f, 1 }, &rc.disabled_fore_brush);
		dc->CreateSolidColorBrush ({ 0, 0.5f, 0, 1 }, &rc.data_bind_fore_brush);

		static constexpr D2D1_GRADIENT_STOP stops[3] =
		{
			{ 0,    { 0.97f, 0.97f, 0.97f, 1 } },
			{ 0.3f, { 1,     1,     1,     1 } },
			{ 1,    { 0.93f, 0.93f, 0.93f, 1 } },
		};
		com_ptr<ID2D1GradientStopCollection> stop_collection;
		auto hr = dc->CreateGradientStopCollection (stops, _countof(stops), &stop_collection); assert(SUCCEEDED(hr));
		static constexpr D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES lgbp = { { 0, 0 }, { 1, 0 } };
		hr = dc->CreateLinearGradientBrush (&lgbp, nullptr, stop_collection, &rc.item_gradient_brush); assert(SUCCEEDED(hr));

		com_ptr<ID2D1Factory> factory;
		dc->GetFactory(&factory);
		factory->CreatePathGeometry(&rc.triangle_geo);
		com_ptr<ID2D1GeometrySink> sink;
		rc.triangle_geo->Open(&sink);
		text_layout_with_line_metrics dummy (_window->dwrite_factory(), _text_format, L"A");
		float triangle_height = dummy.height();
		const float cs = triangle_height * 4 / 5;
		sink->BeginFigure({ 0, triangle_height / 2 - cs / 2 }, D2D1_FIGURE_BEGIN_FILLED);
		sink->AddLine ({ cs / 2, triangle_height / 2 });
		sink->AddLine ({ 0, triangle_height / 2 + cs / 2 });
		sink->EndFigure (D2D1_FIGURE_END_CLOSED);
		sink->Close();

		dc->PushAxisAlignedClip (&_rectd, D2D1_ANTIALIAS_MODE_ALIASED);
		enum_items ([dc, &rc, focused, this, bw=border_width()](pgitem* item, float y, bool& cancel)
		{
			if (y >= _rectd.bottom)
			{
				cancel = true;
				return;
			}

			bool selected = (item == _selected_item);
			item->render (rc, { _rectd.left, y }, selected, focused);

			if (selected && _text_editor)
				_text_editor->render(dc);
		});
		dc->PopAxisAlignedClip();
	}

	virtual d2d_window_i* window() const override { return _window; }

	virtual RECT rectp() const override { return _rectp; }

	virtual D2D1_RECT_F rectd() const override { return _rectd; }

	virtual void set_rect (const RECT& rectp) override
	{
		auto rectd = _window->rectp_to_rectd(rectp);

		if ((_rectp != rectp) || (_rectd != rectd))
		{
			_window->invalidate(_rectp);
			_text_editor = nullptr;

			// When the grid is moved without resizing, the width still changes slightly
			// due to floating point rounding errors, that's why the check.
			// The limit is far less than a pixel width, so we shouldn't see any artifacts.
			float old_width = _rectd.right - _rectd.left;
			float new_width = rectd.right - rectd.left;
			float limit = 0.01f;
			bool layout_changed = fabsf(old_width - new_width) >= limit;

			_rectp = rectp;
			_rectd = rectd;

			if (layout_changed)
			{
				for (auto& ri : _root_items)
					perform_layout(ri.get());
			}

			TOOLINFO ti = { sizeof(TOOLINFO) };
			ti.uFlags   = TTF_SUBCLASS;
			ti.hwnd     = _window->hwnd();
			ti.lpszText = nullptr;
			ti.rect     = rectp;
			SendMessage(_tooltip, TTM_SETTOOLINFO, 0, (LPARAM) (LPTOOLINFO) &ti);

			_window->invalidate(_rectp);
		}
	}

	virtual void set_border_width (float bw) override
	{
		if (_border_width_not_aligned != bw)
		{
			_border_width_not_aligned = bw;

			_text_editor = nullptr;
			for (auto& ri : _root_items)
				perform_layout(ri.get());
			_window->invalidate(_rectd);
		}
	}

	virtual void on_dpi_changed() override
	{
		_rectd = _window->rectp_to_rectd(_rectp);

		_text_editor = nullptr;
		for (auto& ri : _root_items)
			perform_layout(ri.get());
		_window->invalidate(_rectd);
	}

	virtual void clear() override
	{
		_text_editor = nullptr;
		_selected_item = nullptr;
		_root_items.clear();
		invalidate();
	}

	virtual void add_section (const char* heading, std::span<object* const> objects) override
	{
		_root_items.push_back (std::make_unique<root_item>(this, heading, objects));
		invalidate();
	}

	virtual std::span<const std::unique_ptr<root_item>> sections() const override { return _root_items; }

	virtual bool read_only() const override { return false; }

	virtual property_edited_e::subscriber property_changed() override { return property_edited_e::subscriber(this); }

	virtual text_editor_i* show_text_editor (const D2D1_RECT_F& rect, bool bold, float lr_padding, std::string_view str) override final
	{
		uint32_t fill_argb = 0xFF00'0000u | GetSysColor(COLOR_WINDOW);
		uint32_t text_argb = 0xFF00'0000u | GetSysColor(COLOR_WINDOWTEXT);
		_text_editor = text_editor_factory (_window, dwrite_factory(), bold ? _bold_text_format : _text_format, fill_argb, text_argb, rect, lr_padding, str);
		return _text_editor.get();
	}

	virtual int show_enum_editor (D2D1_POINT_2F dip, const nvp* nameValuePairs) override final
	{
		_text_editor = nullptr;
		POINT ptScreen = _window->pointd_to_pointp(dip, 0);
		::ClientToScreen (_window->hwnd(), &ptScreen);

		HINSTANCE hInstance = (HINSTANCE) GetWindowLongPtr (_window->hwnd(), GWLP_HINSTANCE);

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

			atom = ::RegisterClassW (&EditorWndClass); assert (atom != 0);
		}

		auto hwnd = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, ClassName, L"aaa", WS_POPUP | WS_BORDER, 0, 0, 0, 0, _window->hwnd(), nullptr, hInstance, nullptr); assert (hwnd != nullptr);

		LONG maxTextWidth = 0;
		LONG maxTextHeight = 0;

		NONCLIENTMETRICS ncMetrics = { sizeof(NONCLIENTMETRICS) };
		auto proc_addr = GetProcAddress(GetModuleHandleA("user32.dll"), "SystemParametersInfoForDpi");
		if (proc_addr != nullptr)
		{
			auto proc = reinterpret_cast<BOOL(WINAPI*)(UINT, UINT, PVOID, UINT, UINT)>(proc_addr);
			proc(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncMetrics, 0, _window->dpi());
		}
		else
			SystemParametersInfo (SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncMetrics, 0);
		HFONT_unique_ptr font (CreateFontIndirect (&ncMetrics.lfMenuFont));

		auto hdc = ::GetDC(hwnd);
		auto oldFont = ::SelectObject (hdc, font.get());
		size_t count;
		for (count = 0; nameValuePairs[count].name != nullptr; count++)
		{
			RECT rc = { };
			DrawTextA (hdc, nameValuePairs[count].name, -1, &rc, DT_CALCRECT);
			maxTextWidth = std::max (maxTextWidth, rc.right);
			maxTextHeight = std::max (maxTextHeight, rc.bottom);
		}
		::SelectObject (hdc, oldFont);
		::ReleaseDC (hwnd, hdc);

		int lrpadding = 7 * _window->dpi() / 96;
		int udpadding = ((count <= 5) ? 5 : 0) * _window->dpi() / 96;
		LONG buttonWidth = std::max (100l * (LONG)_window->dpi() / 96, maxTextWidth + 2 * lrpadding) + 2 * GetSystemMetrics(SM_CXEDGE);
		LONG buttonHeight = maxTextHeight + 2 * udpadding + 2 * GetSystemMetrics(SM_CYEDGE);

		int margin = 4 * _window->dpi() / 96;
		int spacing = 2 * _window->dpi() / 96;
		int y = margin;
		for (size_t nvp_index = 0; nameValuePairs[nvp_index].name != nullptr;)
		{
			constexpr DWORD dwStyle = WS_CHILD | WS_VISIBLE | BS_NOTIFY | BS_FLAT;
			auto button = CreateWindowExA (0, "Button", nameValuePairs[nvp_index].name, dwStyle, margin, y, buttonWidth, buttonHeight, hwnd, (HMENU) nvp_index, hInstance, nullptr);
			::SendMessage (button, WM_SETFONT, (WPARAM) font.get(), FALSE);
			nvp_index++;
			y += buttonHeight + (nameValuePairs[nvp_index].name ? spacing : margin);
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
		return selected_nvp_index;
	}

	virtual htresult hit_test (D2D1_POINT_2F pd) const override
	{
		htresult result = { nullptr };

		enum_items ([this, pd, &result](pgitem* item, float y, bool& cancel)
		{
			float h = item->content_height_aligned();
			if (pd.y < y + h)
			{
				result = htresult{ };
				result.item = item;
				result.y = y;

				if (pd.x < name_column_x(item->indent()))
				{
					result.code = htcode::expand;

					if (auto vi = dynamic_cast<value_item*>(item); vi && dynamic_cast<const pg_bindable_property_i*>(vi->property()))
						result.code = htcode::input;
				}
				else if (pd.x < value_column_x())
					result.code = htcode::name;
				else
					result.code = htcode::value;

				cancel = true;
			}
		});

		return result;
	}

	virtual D2D1_POINT_2F input_of (value_item* vi) const override
	{
		std::optional<D2D1_POINT_2F> res;

		enum_items ([vi, this, &res](pgitem* item, float y, bool& cancel)
		{
			if (item == vi)
			{
				res = D2D1_POINT_2F{ _rectd.left, y + item->content_height_aligned() / 2 };
				cancel = true;
			}
		});

		assert(res);
		return res.value();
	}

	virtual D2D1_POINT_2F output_of (value_item* vi) const override
	{
		std::optional<D2D1_POINT_2F> res;

		enum_items ([vi, this, &res](pgitem* item, float y, bool& cancel)
		{
			if (item == vi)
			{
				res = D2D1_POINT_2F{ _rectd.right, y + item->content_height_aligned() / 2 };
				cancel = true;
			}
		});

		assert(res);
		return res.value();
	}

	virtual value_item* find_item (const value_property* prop) const override
	{
		value_item* res = nullptr;

		enum_items([&res, prop](pgitem* item, float y, bool& cancel)
		{
			if (auto vi = dynamic_cast<value_item*>(item); vi && (vi->property() == prop))
			{
				res = vi;
				cancel = true;
			}
		});

		return res;
	}

	virtual handled on_mouse_down (mouse_button button, modifier_key mks, POINT pp, D2D1_POINT_2F pd) override final
	{
		if (_text_editor && (_text_editor->mouse_captured() || point_in_rect(_text_editor->rect(), pd)))
			return _text_editor->on_mouse_down(button, mks, pp, pd);

		auto clicked_item = hit_test(pd);

		auto new_selected_item = (clicked_item.item && clicked_item.item->selectable()) ? clicked_item.item : nullptr;
		if (_selected_item != new_selected_item)
		{
			_text_editor = nullptr;
			_selected_item = new_selected_item;
			invalidate();
		}

		if (clicked_item.item)
		{
			clicked_item.item->on_mouse_down (button, mks, pp, pd, clicked_item.y);
			return handled(true);
		}

		return handled(false);
	}

	virtual handled on_mouse_up (mouse_button button, modifier_key mks, POINT pp, D2D1_POINT_2F pd) override final
	{
		if (_text_editor && _text_editor->mouse_captured())
			return _text_editor->on_mouse_up (button, mks, pp, pd);

		auto clicked_item = hit_test(pd);
		if (clicked_item.item != nullptr)
		{
			clicked_item.item->on_mouse_up (button, mks, pp, pd, clicked_item.y);
			return handled(true);
		}

		return handled(false);
	}

	virtual void on_mouse_move (modifier_key mks, POINT pp, D2D1_POINT_2F pd) override final
	{
		if (_text_editor && _text_editor->mouse_captured())
			return _text_editor->on_mouse_move (mks, pp, pd);

		if (_last_tt_location != pp)
		{
			_last_tt_location = pp;

			if (_text_editor && point_in_rect(_text_editor->rect(), pd))
			{
				TOOLINFO ti = { sizeof(TOOLINFO), 0, _window->hwnd() };
				SendMessage(_tooltip, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
			}
			else
			{
				::SendMessage (_tooltip, TTM_POP, 0, 0);

				std::wstring text;
				std::wstring title;

				auto htres = hit_test(pd);
				if (htres.item)
				{
					title = utf8_to_utf16(htres.item->description_title());
					text  = utf8_to_utf16(htres.item->description_text());

					if (!title.empty() && text.empty())
						text = L"--";
				}

				TOOLINFO ti = { sizeof(TOOLINFO) };
				ti.hwnd     = _window->hwnd();
				ti.lpszText = text.data();
				SendMessage(_tooltip, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);

				SendMessage(_tooltip, TTM_SETTITLE, TTI_INFO, (LPARAM)title.data());
			}
		}
	}

	void try_commit_editor()
	{
		if (_text_editor == nullptr)
			return;

		auto prop_item = dynamic_cast<value_item*>(_selected_item); assert(prop_item);
		auto text_utf16 = _text_editor->wstr();
		auto text_utf8 = utf16_to_utf8(text_utf16);
		try
		{
			change_property (prop_item->parent()->parent()->objects(), prop_item->property(), text_utf8);
		}
		catch (const std::exception& ex)
		{
			auto message = utf8_to_utf16(ex.what());
			::MessageBox (_window->hwnd(), message.c_str(), L"Error setting property", 0);
			::SetFocus (_window->hwnd());
			_text_editor->select_all();
			return;
		}

		_window->invalidate(_text_editor->rect());
		_text_editor = nullptr;
	}

	virtual void change_property (const std::vector<object*>& objects, const value_property* prop, std::string_view new_value_str) override final
	{
		std::vector<std::string> old_values;
		old_values.reserve(objects.size());
		for (auto o : objects)
			old_values.push_back (prop->get_to_string(o));

		for (size_t i = 0; i < objects.size(); i++)
		{
			try
			{
				prop->set_from_string (new_value_str, objects[i]);
			}
			catch (const std::exception&)
			{
				for (size_t j = 0; j < i; j++)
					prop->set_from_string(old_values[j], objects[j]);

				throw;
			}
		}

		property_edited_args args = { objects, std::move(old_values), std::string(new_value_str) };
		this->event_invoker<property_edited_e>()(std::move(args));
	}

	virtual float border_width() const override
	{
		auto pw = _window->pixel_width();
		auto bw = roundf(_border_width_not_aligned / pw) * pw;
		return bw;
	}

	virtual float line_thickness() const override
	{
		static constexpr float line_thickness_not_aligned = 0.6f;
		auto lt = roundf(line_thickness_not_aligned / _window->pixel_width()) * _window->pixel_width();
		return lt;
	}

	virtual float name_column_x (size_t indent) const override
	{
		float x = _rectd.left + border_width() + indent * pgitem::indent_step;
		auto pw = _window->pixel_width();
		x = roundf(x / pw) * pw;
		return x;
	}

	virtual float value_column_x() const override
	{
		float bw = border_width();
		float w = (_rectd.right - _rectd.left - 2 * bw) * _name_column_factor;
		if (w < 75)
			w = 75;
		float x = _rectd.left + bw + w;
		auto pw = _window->pixel_width();
		x = roundf(x / pw) * pw;
		return x;
	}

	virtual handled on_key_down (uint32_t key, modifier_key mks) override
	{
		if ((key == VK_RETURN) || (key == VK_UP) || (key == VK_DOWN))
		{
			try_commit_editor();

			if (key == VK_UP)
			{
				// select previous item
			}
			else if (key == VK_DOWN)
			{
				// select next item
			}

			return handled(true);
		}

		if ((key == VK_ESCAPE) && _text_editor)
		{
			_text_editor = nullptr;
			return handled(true);
		}

		if (_text_editor)
			return _text_editor->on_key_down (key, mks);

		return handled(false);
	}

	virtual handled on_key_up (uint32_t key, modifier_key mks) override
	{
		if (_text_editor)
			return _text_editor->on_key_up (key, mks);

		return handled(false);
	}

	virtual handled on_char_key (uint32_t key) override
	{
		if (_text_editor)
			return _text_editor->on_char_key (key);

		return handled(false);
	}

	virtual bool editing_text() const override final
	{
		return _text_editor != nullptr;
	}
};

extern std::unique_ptr<property_grid_i> edge::property_grid_factory (d2d_window_i* window, const RECT& rectp)
{
	return std::make_unique<property_grid>(window, rectp);
};
