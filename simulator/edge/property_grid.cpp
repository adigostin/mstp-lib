
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "property_grid.h"
#include "utility_functions.h"

using namespace edge;

namespace edge
{
	std::unique_ptr<root_item_i> make_root_item (property_grid_i* grid, const char* heading, std::span<object* const> objects);

	#pragma warning (push)
	#pragma warning (disable: 4250)

	class property_grid : public event_manager, public property_grid_i
	{
		d2d_window_i* const _window;
		com_ptr<IDWriteTextFormat> _text_format;
		com_ptr<IDWriteTextFormat> _bold_text_format;
		com_ptr<IDWriteTextFormat> _wingdings;
		std::unique_ptr<text_editor_i> _text_editor;
		D2D1_RECT_F _rectd;
		float _name_column_factor = 0.6f;
		std::vector<std::unique_ptr<root_item_i>> _root_items;
		pgitem_i* _selected_item = nullptr;
		HWND _tooltip = nullptr;
		std::optional<D2D1_POINT_2F> _last_tt_location;
		float _border_width_not_aligned = 0;

		static constexpr float font_size = 12;

	public:
		property_grid (d2d_window_i* window, const D2D1_RECT_F& bounds)
			: _window(window)
			, _rectd(bounds)
		{
			auto hinstance = (HINSTANCE)::GetWindowLongPtr (window->hwnd(), GWLP_HINSTANCE);

			_tooltip = CreateWindowEx (WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr,
				WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				window->hwnd(), nullptr, hinstance, nullptr);

			TOOLINFO ti = { sizeof(TOOLINFO) };
			ti.uFlags   = TTF_SUBCLASS;
			ti.hwnd     = _window->hwnd();
			ti.lpszText = nullptr;
			ti.rect     = tooltip_rect();
			SendMessage(_tooltip, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);

			SendMessage (_tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 1500);
			SendMessage (_tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, (LPARAM)(LONG)MAXSHORT);
			SendMessage (_tooltip, TTM_SETMAXTIPWIDTH, 0, ti.rect.right - ti.rect.left);

			auto hr = window->dwrite_factory()->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
													   DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_text_format); assert(SUCCEEDED(hr));

			hr = window->dwrite_factory()->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
												  DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_bold_text_format); assert(SUCCEEDED(hr));

			hr = window->dwrite_factory()->CreateTextFormat (L"Wingdings", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
												  DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_wingdings); assert(SUCCEEDED(hr));
		}

		virtual ~property_grid()
		{
			_window->invalidate();
			::DestroyWindow(_tooltip);
		}

		virtual IDWriteTextFormat* text_format() const override final { return _text_format; }

		virtual IDWriteTextFormat* bold_text_format() const override final { return _bold_text_format; }

		RECT tooltip_rect() const
		{
			auto tl = _window->pointd_to_pointp({ _rectd.left,  _rectd.top    }, -1);
			auto br = _window->pointd_to_pointp({ _rectd.right, _rectd.bottom },  1);
			return { tl.x, tl.y, br.x, br.y };
		}

		void invalidate()
		{
			::InvalidateRect (_window->hwnd(), nullptr, FALSE); } // TODO: optimize
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
					return htr.item->cursor_at(pd, htr.y);
			}

			return ::LoadCursor(nullptr, IDC_ARROW);
		}

		void enum_items (const std::function<void(pgitem_i*, float y, bool& cancel)>& callback) const
		{
			std::function<void(pgitem_i* item, float& y, size_t indent, bool& cancel)> enum_items_inner;

			enum_items_inner = [this, &enum_items_inner, &callback, vcx=value_column_x(), bw=border_width()](pgitem_i* item, float& y, size_t indent, bool& cancel)
			{
				auto item_height = item->content_height_aligned();
				if (item_height > 0)
				{
					callback(item, y, cancel);
					if (cancel)
						return;

					y += item_height;
				}

				if (auto ei = dynamic_cast<expandable_item_i*>(item); ei && ei->expanded())
				{
					for (size_t i = 0; i < ei->child_count(); i++)
					{
						pgitem_i* child = ei->child_at(i);
						enum_items_inner (child, y, indent + 1, cancel);
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

		void perform_layout (pgitem_i* item)
		{
			item->perform_layout();

			if (auto ei = dynamic_cast<expandable_item_i*>(item); ei && ei->expanded())
			{
				for (size_t i = 0; i < ei->child_count(); i++)
					perform_layout(ei->child_at(i));
			};

			invalidate();
		}

		virtual void render (const d2d_render_args& ra) const override
		{
			auto dc = ra.dc;

			dc->SetTransform (_window->dpi_transform());

			dc->FillRectangle(_rectd, ra.back_brush);

			float bw = border_width();
			if (bw > 0)
			{
				com_ptr<ID2D1SolidColorBrush> border_brush;
				dc->CreateSolidColorBrush (interpolate(ra.back_brush->GetColor(), ra.fore_brush->GetColor(), 50), &border_brush);
				dc->DrawRectangle(inflate(_rectd, -bw / 2), border_brush, bw);
			}

			if (_root_items.empty())
			{
				auto tl = text_layout_with_metrics (_window->dwrite_factory(), _text_format, "(no selection)");
				D2D1_POINT_2F p = { (_rectd.left + _rectd.right) / 2 - tl.width() / 2, (_rectd.top + _rectd.bottom) / 2 - tl.height() / 2};
				dc->DrawTextLayout (p, tl, ra.fore_brush);
				return;
			}

			bool focused = GetFocus() == _window->hwnd();

			D2D1_COLOR_F back = ra.back_brush->GetColor();
			float luminance = back.r * 0.299f + back.g * 0.587f + back.b * 0.114f;

			pg_render_context rc;
			rc.ra = &ra;
			dc->CreateSolidColorBrush ({ 0.7f, 0.7f, 0.7f, 1 }, &rc.disabled_fore_brush);
			dc->CreateSolidColorBrush ({ 0, (luminance > 0.6f) ? 0.5f : 0.8f, 0, 1 }, &rc.data_bind_fore_brush);

			static constexpr D2D1_GRADIENT_STOP stops_light[3] =
			{
				{ 0,    { 0.97f, 0.97f, 0.97f, 1 } },
				{ 0.4f, { 1,     1,     1,     1 } },
				{ 1,    { 0.93f, 0.93f, 0.93f, 1 } },
			};

			static constexpr D2D1_GRADIENT_STOP stops_dark[3] =
			{
				{ 0,    { 0.10f, 0.10f, 0.10f, 1 } },
				{ 0.4f, { 0.21f, 0.21f, 0.21f, 1 } },
				{ 1,    { 0,     0,     0,     1 } },
			};

			com_ptr<ID2D1GradientStopCollection> stop_collection;
			auto hr = dc->CreateGradientStopCollection ((luminance > 0.6f) ? stops_light : stops_dark, 3, &stop_collection); assert(SUCCEEDED(hr));
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
			enum_items ([dc, &rc, focused, this, bw=border_width()](pgitem_i* item, float y, bool& cancel)
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

		virtual D2D1_RECT_F rectd() const override { return _rectd; }

		virtual void set_bounds (const D2D1_RECT_F& bounds) override
		{
			auto rectd = bounds;

			if (_rectd != rectd)
			{
				invalidate();
				_text_editor = nullptr;

				// When the grid is moved without resizing, the width still changes slightly
				// due to floating point rounding errors, that's why the check.
				// The limit is far less than a pixel width, so we shouldn't see any artifacts.
				float old_width = _rectd.right - _rectd.left;
				float new_width = rectd.right - rectd.left;
				float limit = 0.01f;
				bool layout_changed = fabsf(old_width - new_width) >= limit;

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
				ti.rect     = tooltip_rect();
				SendMessage(_tooltip, TTM_SETTOOLINFO, 0, (LPARAM) (LPTOOLINFO) &ti);

				invalidate();
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
				invalidate();
			}
		}

		virtual void on_dpi_changed() override
		{
			//_rectd = _window->rectp_to_rectd(_rectp);

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
			_root_items.push_back (make_root_item(this, heading, objects));
			invalidate();
		}

		virtual std::span<const std::unique_ptr<root_item_i>> sections() const override { return _root_items; }

		virtual bool read_only() const override { return false; }

		virtual property_edited_e::subscriber property_changed() override { return property_edited_e::subscriber(this); }

		virtual text_editor_i* show_text_editor (const D2D1_RECT_F& rect, bool bold, float lr_padding, std::string_view str) override final
		{
			uint32_t fill_argb = 0xFF00'0000u | GetSysColor(COLOR_WINDOW);
			uint32_t text_argb = 0xFF00'0000u | GetSysColor(COLOR_WINDOWTEXT);
			_text_editor = text_editor_factory (_window, _window->dwrite_factory(), bold ? _bold_text_format : _text_format, fill_argb, text_argb, rect, lr_padding, str);
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

			auto dpi = ::GetDpiForWindow(_window->hwnd());

			LONG maxTextWidth = 0;
			LONG maxTextHeight = 0;

			NONCLIENTMETRICS ncMetrics = { sizeof(NONCLIENTMETRICS) };
			BOOL bres = SystemParametersInfoForDpi (SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncMetrics, 0, dpi);
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

			int lrpadding = 7 * dpi / 96;
			int udpadding = ((count <= 5) ? 5 : 0) * dpi / 96;
			LONG buttonWidth = std::max (100l * (LONG)dpi / 96, maxTextWidth + 2 * lrpadding) + 2 * GetSystemMetrics(SM_CXEDGE);
			LONG buttonHeight = maxTextHeight + 2 * udpadding + 2 * GetSystemMetrics(SM_CYEDGE);

			int margin = 4 * dpi / 96;
			int spacing = 2 * dpi / 96;
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

			enum_items ([this, pd, &result](pgitem_i* item, float y, bool& cancel)
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

						if (auto vi = dynamic_cast<value_property_item_i*>(item); vi && dynamic_cast<const pg_bindable_property_i*>(vi->property()))
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

		virtual D2D1_POINT_2F input_of (value_property_item_i* vi) const override
		{
			std::optional<D2D1_POINT_2F> res;

			enum_items ([vi, this, &res](pgitem_i* item, float y, bool& cancel)
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

		virtual D2D1_POINT_2F output_of (value_property_item_i* vi) const override
		{
			std::optional<D2D1_POINT_2F> res;

			enum_items ([vi, this, &res](pgitem_i* item, float y, bool& cancel)
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

		virtual value_property_item_i* find_item (const value_property* prop) const override
		{
			value_property_item_i* res = nullptr;

			enum_items([&res, prop](pgitem_i* item, float y, bool& cancel)
			{
				if (auto vi = dynamic_cast<value_property_item_i*>(item); vi && (vi->property() == prop))
				{
					res = vi;
					cancel = true;
				}
			});

			return res;
		}

		virtual handled on_mouse_down (const mouse_ud_args& args) override final
		{
			if (_text_editor && (_text_editor->mouse_captured() || point_in_rect(_text_editor->rect(), args.pd)))
				return _text_editor->on_mouse_down(args);

			auto clicked_item = hit_test(args.pd);

			auto new_selected_item = (clicked_item.item && clicked_item.item->selectable()) ? clicked_item.item : nullptr;
			if (_selected_item != new_selected_item)
			{
				_text_editor = nullptr;
				_selected_item = new_selected_item;
				invalidate();
			}

			if (clicked_item.item)
			{
				clicked_item.item->on_mouse_down (args, clicked_item.y);
				return handled(true);
			}

			return handled(false);
		}

		virtual handled on_mouse_up (const mouse_ud_args& args) override final
		{
			if (_text_editor && _text_editor->mouse_captured())
				return _text_editor->on_mouse_up (args);

			auto clicked_item = hit_test(args.pd);
			if (clicked_item.item != nullptr)
			{
				clicked_item.item->on_mouse_up (args, clicked_item.y);
				return handled(true);
			}

			return handled(false);
		}

		virtual void on_mouse_move (const mouse_move_args& args) override final
		{
			if (_text_editor && _text_editor->mouse_captured())
				return _text_editor->on_mouse_move (args);

			if (!_last_tt_location || (_last_tt_location != args.pd))
			{
				_last_tt_location = args.pd;

				if (_text_editor && point_in_rect(_text_editor->rect(), args.pd))
				{
					TOOLINFO ti = { sizeof(TOOLINFO), 0, _window->hwnd() };
					SendMessage(_tooltip, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
				}
				else
				{
					::SendMessage (_tooltip, TTM_POP, 0, 0);

					std::wstring text;
					std::wstring title;

					auto htres = hit_test(args.pd);
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

			auto prop_item = dynamic_cast<value_property_item_i*>(_selected_item); assert(prop_item);
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
			auto pe_invoker = this->event_invoker<property_edited_e>();
			bool pe_has_handlers = pe_invoker.has_handlers();

			std::vector<std::string> old_values;
			if (pe_has_handlers)
			{
				old_values.reserve(objects.size());
				for (auto o : objects)
					old_values.push_back (prop->get_to_string(o));
			}

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

			if (pe_has_handlers)
			{
				property_edited_args args = { objects, std::move(old_values), std::string(new_value_str) };
				pe_invoker(std::move(args));
			}
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
			float x = _rectd.left + border_width() + indent * indent_width();
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

		virtual float indent_width() const override final { return 10; }

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

	#pragma warning (pop)

	property_grid_factory_t* const property_grid_factory =
		[](auto... params) -> std::unique_ptr<property_grid_i>
		{ return std::make_unique<property_grid>(std::forward<decltype(params)>(params)...); };

	// ========================================================================

	#pragma region pgitem_i
	size_t pgitem_i::indent() const
	{
		return this->as_root() ? (size_t)0 : (this->parent()->indent() + 1);
	}

	float pgitem_i::content_height_aligned() const
	{
		auto grid = root()->grid();
		float pixel_width = grid->window()->pixel_width();
		return std::ceilf (content_height() / pixel_width) * pixel_width + grid->line_thickness();
	}

	HCURSOR pgitem_i::cursor_at(D2D1_POINT_2F pd, float item_y) const
	{
		return LoadCursor(nullptr, IDC_ARROW);
	}

	root_item_i* pgitem_i::root()
	{
		if (auto r = this->as_root())
			return r;
		return parent()->root();
	}
	#pragma endregion

	#pragma region value_item_i
	float value_item_i::content_height() const
	{
		auto& name = this->name();
		auto& value = this->value();
		return std::max (name ? name.height() : 0.0f, value.tl ? value.tl.height() : 0.0f);
	}

	void value_item_i::perform_layout()
	{
		this->perform_name_layout();
		this->perform_value_layout();
	}
	#pragma endregion

	#pragma region value_property_item_i
	bool value_property_item_i::multiple_values() const
	{
		auto& objs = parent()->parent()->objects();
		for (size_t i = 1; i < objs.size(); i++)
		{
			if (!property()->equal(objs[0], objs[i]))
				return true;
		}

		return false;
	}

	bool value_property_item_i::can_edit() const
	{
		// TODO: Allow editing and setting a property that couldn't be read.
		if (!value().readable)
			return false;

		if (dynamic_cast<const pg_custom_editor_i*>(property()))
			return true;

		auto& objs = parent()->parent()->objects();
		bool can_set = std::all_of (objs.begin(), objs.end(), [prop=property()](object* o) { return prop->can_set(o); });
		return can_set;
	}

	bool value_property_item_i::changed_from_default() const
	{
		for (auto o : parent()->parent()->objects())
		{
			if (property()->changed_from_default(o))
				return true;
		}

		return false;
	}

	text_layout_with_metrics value_property_item_i::make_name_layout() const
	{
		auto grid = root()->grid();
		float name_layout_width = grid->value_column_x() - grid->name_column_x(indent()) - grid->line_thickness() - 2 * text_lr_padding;
		if (name_layout_width <= 0)
			return { };

		return text_layout_with_metrics (grid->window()->dwrite_factory(), grid->text_format(), property()->name, name_layout_width);
	}

	value_item_i::value_layout value_property_item_i::make_value_layout() const
	{
		auto grid = root()->grid();

		float width = grid->rectd().right - grid->border_width() - grid->value_column_x() - grid->line_thickness() - 2 * text_lr_padding;
		if (width <= 0)
			return { };

		auto factory = grid->window()->dwrite_factory();
		auto format = changed_from_default() ? grid->bold_text_format() : grid->text_format();

		text_layout_with_metrics tl;
		bool readable;
		try
		{
			if (multiple_values())
				tl = text_layout_with_metrics (factory, format, "(multiple values)", width);
			else
				tl = text_layout_with_metrics (factory, format, property()->get_to_string(parent()->parent()->objects().front()), width);
			readable = true;
		}
		catch (const std::exception& ex)
		{
			tl = text_layout_with_metrics (factory, format, ex.what(), width);
			readable = false;
		}

		return { std::move(tl), readable };
	}

	void value_property_item_i::render (const pg_render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const
	{
		auto grid = root()->grid();
		auto lt = grid->line_thickness();
		float bw = grid->border_width();
		auto rectd = grid->rectd();
		float height = content_height_aligned();

		D2D1_RECT_F fill_rect = { rectd.left + bw, pd.y, rectd.right - bw, pd.y + height };

		if (selected)
		{
			rc.ra->dc->FillRectangle (&fill_rect, focused ? rc.ra->selected_back_brush_focused.get() : rc.ra->selected_back_brush_not_focused.get());
		}
		else
		{
			rc.item_gradient_brush->SetStartPoint ({ fill_rect.left, fill_rect.top });
			rc.item_gradient_brush->SetEndPoint ({ fill_rect.left, fill_rect.bottom });
			rc.ra->dc->FillRectangle (&fill_rect, rc.item_gradient_brush);
		}

		bool bindable = false;
		bool bound = false;

		if (auto bp = dynamic_cast<const pg_bindable_property_i*>(property()))
		{
			bindable = true;
			auto& objs = parent()->parent()->objects();
			bound = std::any_of (objs.begin(), objs.end(), [bp](object* o) { return bp->bound(o); });
		}

		if (bindable)
		{
			float line_width_not_aligned = 1.6f;
			LONG line_width_pixels = grid->window()->lengthd_to_lengthp (line_width_not_aligned, 0);
			float line_width = grid->window()->lengthp_to_lengthd(line_width_pixels);

			D2D1::Matrix3x2F oldtr;
			rc.ra->dc->GetTransform(&oldtr);
			float padding = line_width;
			rc.ra->dc->SetTransform (D2D1::Matrix3x2F::Translation(rectd.left + bw + padding + line_width / 2, pd.y) * oldtr);

			if (bound)
				rc.ra->dc->FillGeometry(rc.triangle_geo, rc.data_bind_fore_brush);

			rc.ra->dc->DrawGeometry(rc.triangle_geo, rc.data_bind_fore_brush, line_width);

			rc.ra->dc->SetTransform(&oldtr);
		}

		float name_line_x = rectd.left + bw + indent() * grid->indent_width() + lt / 2;
		rc.ra->dc->DrawLine ({ name_line_x, pd.y }, { name_line_x, pd.y + height }, rc.disabled_fore_brush, lt);
		auto fore = bound ? rc.data_bind_fore_brush.get() : rc.ra->fore_brush.get();
		rc.ra->dc->DrawTextLayout ({ rectd.left + bw + indent() * grid->indent_width() + lt + text_lr_padding, pd.y }, name(), fore);

		float linex = grid->value_column_x() + lt / 2;
		rc.ra->dc->DrawLine ({ linex, pd.y }, { linex, pd.y + height }, rc.disabled_fore_brush, lt);

		this->render_value (rc, { grid->value_column_x(), pd.y }, selected, focused, bound);
	}

	std::string value_property_item_i::description_title() const
	{
		std::stringstream ss;
		ss << property()->name << " (" << property()->type_name() << ")";
		return ss.str();
	}

	std::string value_property_item_i::description_text() const
	{
		auto prop = this->property();
		if (prop->description)
			return prop->description;
		else
			return std::string();
	}

	HCURSOR value_property_item_i::cursor_at(D2D1_POINT_2F pd, float item_y) const
	{
		if (root()->grid()->read_only() || !can_edit())
			return ::LoadCursor(nullptr, IDC_ARROW);

		if (auto bool_p = dynamic_cast<const edge::bool_p*>(property()))
			return ::LoadCursor(nullptr, IDC_HAND);

		if (property()->nvps())
			return ::LoadCursor(nullptr, IDC_HAND);

		if (dynamic_cast<const pg_custom_editor_i*>(property()))
			return ::LoadCursor(nullptr, IDC_HAND);

		return ::LoadCursor (nullptr, IDC_IBEAM);
	}

	void value_property_item_i::on_mouse_down (const mouse_ud_args& ma, float item_y)
	{
		auto grid = root()->grid();
		auto vcx = grid->value_column_x();
		if (ma.pd.x < vcx)
			return;

		if (auto cep = dynamic_cast<const pg_custom_editor_i*>(property()))
		{
			auto editor = cep->create_editor(parent()->parent()->objects());
			editor->show(grid->window());
			return;
		}

		if (grid->read_only() || !can_edit())
			return;

		if (auto nvps = property()->nvps())
		{
			int selected_nvp_index = grid->show_enum_editor(ma.pd, nvps);
			if (selected_nvp_index >= 0)
			{
				auto changed = [new_value=nvps[selected_nvp_index].value, prop=property()]
				(const object* o) { return prop->get_enum_value_as_int(o) != new_value; };
				auto& objects = parent()->parent()->objects();
				if (std::any_of(objects.begin(), objects.end(), changed))
				{
					try
					{
						auto new_value_str = nvps[selected_nvp_index].name;
						grid->change_property (objects, property(), new_value_str);
					}
					catch (const std::exception& ex)
					{
						auto message = utf8_to_utf16(ex.what());
						::MessageBox (grid->window()->hwnd(), message.c_str(), L"Error setting property", 0);
					}
				}

			}
		}
		else
		{
			D2D1_RECT_F editor_rect = { vcx + grid->line_thickness(), item_y, grid->rectd().right - grid->border_width(), item_y + content_height_aligned() };
			bool bold = changed_from_default();
			auto editor = grid->show_text_editor (editor_rect, bold, text_lr_padding, multiple_values() ? "" : property()->get_to_string(parent()->parent()->objects().front()));
			//editor->on_mouse_down (button, mks, pp, pd);
		}
	}

	void value_property_item_i::on_mouse_up (const mouse_ud_args& ma, float item_y)
	{
	}
	#pragma endregion

	class group_item : public group_item_i
	{
		object_item_i* const _parent;
		const property_group* const _group;
		text_layout_with_metrics _layout;
		bool _expanded = false;
		std::vector<std::unique_ptr<pgitem_i>> _children;

	public:
		group_item (object_item_i* parent, const property_group* group)
			: _parent(parent), _group(group)
		{
			perform_layout();
			expand();
		}

		virtual object_item_i* parent() const override final { return _parent; }

		virtual bool selectable() const override final { return false; }

		virtual void on_mouse_down (const mouse_ud_args& ma, float item_y) { }

		virtual void on_mouse_up   (const mouse_ud_args& ma, float item_y) { }

		virtual std::string description_title() const override final { return { }; }

		virtual std::string description_text() const override final { return { }; }

		virtual size_t child_count() const override final { return _children.size(); }

		virtual pgitem_i* child_at(size_t index) const override final { return _children[index].get(); }

		virtual void expand() override final
		{
			rassert (!_expanded);
			create_children();
			_expanded = true;
			// TODO: tell the PG to rearrange items
		}

		virtual void collapse() override final
		{
			rassert(false); // not implemented
		}

		virtual bool expanded() const override final { return _expanded; }

		std::unique_ptr<pgitem_i> make_child_item (const property* prop);

		void create_children()
		{
			rassert (_children.empty());
			auto type = parent()->objects().front()->type();

			for (auto prop : type->make_property_list())
			{
				if (prop->ui_visible && (prop->group == _group))
					_children.push_back(make_child_item(prop));
			}
		}

		virtual void perform_layout() override final
		{
			auto grid = root()->grid();
			float layout_width = grid->width() - 2 * grid->border_width() - 2 * title_lr_padding;
			if (layout_width > 0)
				_layout = text_layout_with_metrics (grid->window()->dwrite_factory(), grid->bold_text_format(), _group->name, layout_width);
			else
				_layout = nullptr;
		}

		virtual void render (const pg_render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const override
		{
			if (_layout)
			{
				auto grid = root()->grid();
				float bw = grid->border_width();
				auto rectd = grid->rectd();
				rc.ra->dc->FillRectangle ({ rectd.left + bw, pd.y, rectd.right - bw, pd.y + content_height_aligned() }, rc.ra->back_brush);
				rc.ra->dc->DrawTextLayout ({ rectd.left + bw + indent() * grid->indent_width() + text_lr_padding, pd.y }, _layout, rc.ra->fore_brush);
			}
		}

		virtual float content_height() const override
		{
			return _layout ? _layout.height() : 0;
		}
	};

	class root_item : public root_item_i
	{
		property_grid_i* const _grid;
		std::string _heading;
		std::vector<object*> const _objects;
		std::vector<std::unique_ptr<group_item>> _children;
		bool _expanded = false;
		text_layout_with_metrics _text_layout;

	public:
		root_item (property_grid_i* grid, const char* heading, std::span<object* const> objects)
			: _objects(objects.begin(), objects.end()), _grid(grid), _heading(heading ? heading : "")
		{
			perform_layout();
			expand();

			for (auto obj : _objects)
			{
				obj->property_changing().add_handler<&root_item::on_property_changing>(this);
				obj->property_changed().add_handler<&root_item::on_property_changed>(this);
			}
		}

		~root_item()
		{
			for (auto obj : _objects)
			{
				obj->property_changed().remove_handler<&root_item::on_property_changed>(this);
				obj->property_changing().remove_handler<&root_item::on_property_changing>(this);
			}
		}

		virtual property_grid_i* grid() const override final { return _grid; }

		virtual root_item* as_root() override final { return this; }

		virtual expandable_item_i* parent() const override { assert(false); return nullptr; }

		virtual bool selectable() const override final { return false; }

		virtual void on_mouse_down (const mouse_ud_args& ma, float item_y) override final { }

		virtual void on_mouse_up   (const mouse_ud_args& ma, float item_y) override final { }

		virtual std::string description_title() const override final { return { }; }

		virtual std::string description_text() const override final { return { }; }

		virtual size_t child_count() const override final { return _children.size(); }

		virtual pgitem_i* child_at(size_t index) const override final { return _children[index].get(); }

		virtual bool expanded() const override final { return _expanded; }

		virtual const std::vector<object*>& objects() const override final { return _objects; }

		virtual void perform_layout() override final
		{
			_text_layout = nullptr;
			if (!_heading.empty())
			{
				float layout_width = _grid->width() - 2 * _grid->border_width() - 2 * title_lr_padding;
				if (layout_width > 0)
					_text_layout = text_layout_with_metrics (_grid->window()->dwrite_factory(), _grid->bold_text_format(), _heading, layout_width);
			}
		}

		virtual void render (const pg_render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const override final
		{
			if (_text_layout)
			{
				com_ptr<ID2D1SolidColorBrush> brush;
				rc.ra->dc->CreateSolidColorBrush (GetD2DSystemColor(COLOR_ACTIVECAPTION), &brush);
				D2D1_RECT_F rect = {
					_grid->rectd().left + _grid->border_width(),
					pd.y,
					_grid->rectd().right - _grid->border_width(),
					pd.y + content_height_aligned()
				};
				rc.ra->dc->FillRectangle (&rect, brush);
				brush->SetColor (GetD2DSystemColor(COLOR_CAPTIONTEXT));
				rc.ra->dc->DrawTextLayout ({ rect.left + title_lr_padding, rect.top + title_ud_padding }, _text_layout, brush);
			}
		}

		virtual float content_height() const override final
		{
			if (_text_layout)
				return _text_layout.height() + 2 * title_ud_padding;
			else
				return 0;
		}

		virtual void expand() override final
		{
			rassert (!_expanded);
			create_children();
			_expanded = true;
			// TODO: tell the PG to rearrange items
		}

		virtual void collapse() override final
		{
			rassert (!_expanded);
			rassert(false); // not implemented
		}

		void on_property_changing (object* obj, const property_change_args& args)
		{
		}

		void on_property_changed (object* obj, const property_change_args& args)
		{
			if (!args.property->ui_visible)
				return;

			auto root_item = this->root();

			if (auto prop = dynamic_cast<const value_property*>(args.property))
			{
				for (auto& gi : _children)
				{
					for (size_t i = 0; i < gi->child_count(); i++)
					{
						auto child_item = gi->child_at(i);
						if (auto vi = dynamic_cast<value_property_item_i*>(child_item); vi->property() == prop)
						{
							vi->perform_value_layout();
							root_item->grid()->invalidate();
							break;
						}
					}
				}
			}
			else if (auto prop = dynamic_cast<const value_collection_property*>(args.property))
			{
				assert(false); // not implemented
			}
			else
			{
				assert(false); // not implemented
			}
		}

		void create_children()
		{
			rassert(!_objects.empty());

			auto type = _objects[0]->type();
			if (!std::all_of (_objects.begin(), _objects.end(), [type](object* o) { return o->type() == type; }))
				// TODO: some "(multiple types selected)" pg item
				return;

			struct group_comparer
			{
				bool operator() (const property_group* g1, const property_group* g2) const { return g1->prio < g2->prio; }
			};

			std::set<const property_group*, group_comparer> groups;

			for (auto prop : type->make_property_list())
			{
				if (groups.find(prop->group) == groups.end())
					groups.insert(prop->group);
			}

			for (const property_group* g : groups)
				_children.push_back (std::make_unique<group_item>(this, g));
		}
	};

	class default_value_pgitem : public value_property_item_i
	{
		group_item_i*         const _parent;
		const value_property* const _property;
		text_layout_with_metrics _name;
		value_layout _value;

	public:
		default_value_pgitem (group_item_i* parent, const edge::value_property* property)
			: _parent(parent), _property(property)
		{
			this->perform_layout();
		}

		virtual void perform_name_layout() override final { _name = make_name_layout(); }

		virtual void perform_value_layout() override final { _value = make_value_layout(); }

		virtual const text_layout_with_metrics& name() const override final { return _name; }

		virtual const value_layout& value() const override final { return _value; }

		virtual group_item_i* parent() const override final { return _parent; }

		virtual const value_property* property() const override { return _property; }

		virtual void render_value (const pg_render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused, bool data_bound) const override final
		{
			if (auto& tl = value().tl)
			{
				ID2D1Brush* brush;
				if (data_bound)
					brush = rc.data_bind_fore_brush;
				else if (root()->grid()->read_only() || !can_edit())
					brush = rc.disabled_fore_brush;
				else
					brush = rc.ra->fore_brush;

				rc.ra->dc->DrawTextLayout ({ pd.x + root()->grid()->line_thickness() + text_lr_padding, pd.y }, value().tl, brush);
			}
		}
	};

	std::unique_ptr<pgitem_i> group_item::make_child_item (const property* prop)
	{
		if (auto f = dynamic_cast<const pg_custom_item_i*>(prop))
			return f->create_item(this, prop);

		if (auto value_prop = dynamic_cast<const value_property*>(prop))
			return std::make_unique<default_value_pgitem>(this, value_prop);

		// TODO: placeholder pg item for unknown types of properties
		throw not_implemented_exception();
	}

	std::unique_ptr<root_item_i> make_root_item (property_grid_i* grid, const char* heading, std::span<object* const> objects)
	{
		return std::make_unique<root_item>(grid, heading, objects);
	}
}
