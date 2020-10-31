
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "property_grid.h"
#include "utility_functions.h"
#include "collections.h"
#include "window.h"

namespace edge
{
	static std::unique_ptr<root_item_i> make_root_item (property_grid_i* grid, const char* heading, std::span<object* const> objects, pg_app_context_i* app_context);
	static std::vector<std::unique_ptr<group_item_i>> make_group_items (object_item_i* parent, const concrete_type* type);

	#pragma warning (push)
	#pragma warning (disable: 4250)

	class property_grid : public event_manager, public property_grid_i
	{
		d2d_window_i* const _window;
		const theme_color_provider_i* const _tcp;
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

		static constexpr float font_size = 13;

	public:
		property_grid (d2d_window_i* window, const D2D1_RECT_F& bounds, const theme_color_provider_i* tcp)
			: _window(window)
			, _rectd(bounds)
			, _tcp(tcp)
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

			auto hr = window->renderer().dwrite_factory()->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
													   DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_text_format); rassert(SUCCEEDED(hr));

			hr = window->renderer().dwrite_factory()->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
												  DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_bold_text_format); rassert(SUCCEEDED(hr));

			hr = window->renderer().dwrite_factory()->CreateTextFormat (L"Wingdings", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
												  DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_wingdings); rassert(SUCCEEDED(hr));
			_window->window_proc().add_handler<&property_grid::on_window_proc>(this);
			_window->renderer().render().add_handler<&property_grid::on_render>(this);
			this->invalidate();
		}

		virtual ~property_grid()
		{
			_window->renderer().render().remove_handler<&property_grid::on_render>(this);
			_window->window_proc().remove_handler<&property_grid::on_window_proc>(this);
			this->invalidate();
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
			RECT r = _window->rectd_to_rectp(_rectd, 1);
			::InvalidateRect (_window->hwnd(), &r, FALSE);
		}

		std::optional<LRESULT> on_window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
		{
			if (msg == WM_SETCURSOR)
				return process_wm_setcursor (wparam, lparam);

			/*
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
			*/
			return std::nullopt;
		}

		std::optional<LRESULT> process_wm_setcursor (WPARAM wparam, LPARAM lparam)
		{
			if (((HWND)wparam == _window->hwnd()) && (LOWORD(lparam) == HTCLIENT))
			{
				POINT pt;
				if (::GetCursorPos(&pt) && ::ScreenToClient (_window->hwnd(), &pt))
				{
					auto pd = _window->pointp_to_pointd(pt);

					if (point_in_rect(_rectd, pd))
					{
						HCURSOR cursor = nullptr;
						//if ((pd.x >= value_column_x()) && (pd.x < _rectd.right))
						{
							auto htr = hit_test(pd);
							if (htr.item)
							{
								if (htr.code == htcode::input)
									cursor = ::LoadCursor(nullptr, IDC_CROSS);
								else if (htr.code == htcode::value)
									cursor = htr.item->cursor_at(pd, htr.y);
							}
						}

						if (!cursor)
							cursor = ::LoadCursor(nullptr, IDC_ARROW);

						::SetCursor(cursor);
						return TRUE;
					}
				}
			}

			return std::nullopt;
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

		virtual pg_render_context make_render_context (ID2D1DeviceContext* dc) const override final
		{
			pg_render_context rc;
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
			dc->CreateSolidColorBrush ({ 0, (back_luminance > 0.6f) ? 0.5f : 0.8f, 0, 1 }, &rc.data_bind_fore);

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
			auto hr = dc->CreateGradientStopCollection ((back_luminance > 0.6f) ? stops_light : stops_dark, 3, &stop_collection); rassert(SUCCEEDED(hr));
			static constexpr D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES lgbp = { { 0, 0 }, { 1, 0 } };
			hr = dc->CreateLinearGradientBrush (&lgbp, nullptr, stop_collection, &rc.item_gradient_brush); rassert(SUCCEEDED(hr));

			com_ptr<ID2D1Factory> factory;
			dc->GetFactory(&factory);
			factory->CreatePathGeometry(&rc.triangle_geo);
			com_ptr<ID2D1GeometrySink> sink;
			rc.triangle_geo->Open(&sink);
			text_layout_with_line_metrics dummy (_window->renderer().dwrite_factory(), _text_format, L"A");
			float triangle_height = dummy.height();
			const float cs = triangle_height * 4 / 5;
			sink->BeginFigure({ 0, triangle_height / 2 - cs / 2 }, D2D1_FIGURE_BEGIN_FILLED);
			sink->AddLine ({ cs / 2, triangle_height / 2 });
			sink->AddLine ({ 0, triangle_height / 2 + cs / 2 });
			sink->EndFigure (D2D1_FIGURE_END_CLOSED);
			sink->Close();

			return rc;
		}

		void on_render (ID2D1DeviceContext* dc) const
		{
			auto rc = make_render_context(dc);

			dc->SetTransform (_window->dpi_transform());

			dc->FillRectangle(_rectd, rc.back);

			float bw = border_width();
			if (bw > 0)
				dc->DrawRectangle(inflate(_rectd, -bw / 2), rc.border, bw);

			if (_root_items.empty())
			{
				auto tl = text_layout_with_metrics (_window->renderer().dwrite_factory(), _text_format, "(no selection)");
				D2D1_POINT_2F p = { (_rectd.left + _rectd.right) / 2 - tl.width() / 2, (_rectd.top + _rectd.bottom) / 2 - tl.height() / 2};
				dc->DrawTextLayout (p, tl, rc.fore);
				return;
			}

			bool focused = GetFocus() == _window->hwnd();

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
			});
			dc->PopAxisAlignedClip();
		}

		virtual d2d_window_i* window() const override { return _window; }

		virtual const D2D1_RECT_F& bounds() const override { return _rectd; }

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

		virtual void add_section (const char* heading, std::span<object* const> objects, pg_app_context_i* app_context) override
		{
			_root_items.push_back (make_root_item(this, heading, objects, app_context));
			invalidate();
		}

		virtual std::span<const std::unique_ptr<root_item_i>> sections() const override { return _root_items; }

		virtual bool read_only() const override { return false; }

		virtual property_edited_e::subscriber property_changed() override { return property_edited_e::subscriber(this); }

		virtual text_editor_i* show_text_editor (const D2D1_RECT_F& rect, bool bold, float lr_padding, std::string_view str) override final
		{
			uint32_t fill_argb = 0xFF00'0000u | GetSysColor(COLOR_WINDOW);
			uint32_t text_argb = 0xFF00'0000u | GetSysColor(COLOR_WINDOWTEXT);
			_text_editor = text_editor_factory (_window, _window->renderer().dwrite_factory(), bold ? _bold_text_format : _text_format, fill_argb, text_argb, rect, lr_padding, str);
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

				atom = ::RegisterClassW (&EditorWndClass); rassert (atom != 0);
			}

			auto hwnd = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, ClassName, L"aaa", WS_POPUP | WS_BORDER, 0, 0, 0, 0, _window->hwnd(), nullptr, hInstance, nullptr); rassert (hwnd != nullptr);

			auto dpi = _window->dpi();

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

			rassert(res);
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

			rassert(res);
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

			auto prop_item = dynamic_cast<value_property_item_i*>(_selected_item); rassert(prop_item);
			auto text_utf16 = _text_editor->wstr();
			auto text_utf8 = utf16_to_utf8(text_utf16);
			try
			{
				change_property (prop_item->parent()->parent()->objects(), prop_item->property(), text_utf8, prop_item->root()->app_context());
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

		virtual void change_property (const std::vector<object*>& objects, const value_property* prop, std::string_view new_value_str, pg_app_context_i* app_context) override final
		{
			auto pe_invoker = this->event_invoker<property_edited_e>();
			bool pe_has_handlers = pe_invoker.has_handlers();

			std::vector<std::string> old_values;
			if (pe_has_handlers)
			{
				old_values.reserve(objects.size());
				for (auto o : objects)
					old_values.push_back (prop->get_to_string(o, app_context));
			}

			for (size_t i = 0; i < objects.size(); i++)
			{
				try
				{
					prop->set_from_string (new_value_str, objects[i], app_context);
				}
				catch (const std::exception&)
				{
					for (size_t j = 0; j < i; j++)
						prop->set_from_string(old_values[j], objects[j], app_context);

					throw;
				}
			}

			if (pe_has_handlers)
			{
				value_property_edited_args args = { prop, objects, std::move(old_values) };
				pe_invoker(std::move(args));
			}
		}

		virtual void change_property (const std::vector<object*>& objects, const object_property* prop, const concrete_type* type) override final
		{
			property_grid_i::object_property_edited_args args;
			args.prop = prop;
			args.objects = objects;
			for (object* obj : objects)
			{
				auto old_value = prop->set(obj, type->create());
				args.old_values.push_back(std::move(old_value));
			}
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

		virtual float indent_width() const override final
		{
			float pw = _window->pixel_width();
			return std::round(10 / pw) * pw;
		}

		virtual const theme_color_provider_i* tcp() const override final { return _tcp; }

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

		virtual RECT calc_popup_window_pos (pgitem_i* item, float item_y, D2D1_SIZE_F client_size_requested, DWORD style, DWORD ex_style) const override final
		{
			D2D1_RECT_F item_rect = _rectd;
			item_rect.top = item_y;
			item_rect.bottom = item_y + item->content_height_aligned();
			RECT item_rect_pixels = _window->rectd_to_rectp(item_rect, 1);
			::ClientToScreen(_window->hwnd(), (LPPOINT)&item_rect_pixels.left);
			::ClientToScreen(_window->hwnd(), (LPPOINT)&item_rect_pixels.right);

			POINT anchor_point = { (item_rect_pixels.left + item_rect_pixels.right) / 2, (item_rect_pixels.top + item_rect_pixels.bottom) / 2 };

			RECT window_rect = { 0, 0, _window->lengthd_to_lengthp(client_size_requested.width, 0), _window->lengthd_to_lengthp(client_size_requested.height, 0) };

			if (auto proc_addr = GetProcAddress(GetModuleHandleA("User32.dll"), "AdjustWindowRectExForDpi"))
			{
				auto proc = reinterpret_cast<BOOL(WINAPI*)(LPRECT,DWORD,BOOL,DWORD,UINT)>(proc_addr);
				proc (&window_rect, style, FALSE, ex_style, _window->dpi());
			}
			else
				AdjustWindowRectEx(&window_rect, style, FALSE, ex_style);

			SIZE size = { window_rect.right - window_rect.left, window_rect.bottom - window_rect.top };

			RECT popup_window_pos;
			BOOL bres = ::CalculatePopupWindowPosition (&anchor_point, &size, 0, &item_rect_pixels, &popup_window_pos); rassert(bres);
			return popup_window_pos;
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

	root_item_i* pgitem_i::root()
	{
		if (auto r = this->as_root())
			return r;
		return parent()->root();
	}
	#pragma endregion

	#pragma region expandable_item_i
	void expandable_item_i::render_expand_button (const pg_render_context& rc, float item_y) const
	{
		auto grid = root()->grid();
		float height = content_height_aligned();
		float name_line_x = grid->bounds().left + grid->border_width() + indent() * grid->indent_width();

		com_ptr<ID2D1Factory> f;
		rc.dc->GetFactory(&f);
		com_ptr<ID2D1StrokeStyle> ss;
		f->CreateStrokeStyle (D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND), nullptr, 0, &ss);

		float lw = 2.5f;
		float size = std::min(height, grid->indent_width()) - lw;
		if (!expanded())
		{
			D2D1_POINT_2F tip = { name_line_x - grid->indent_width() / 2 + size / 4, item_y + height / 2 };
			rc.dc->DrawLine (tip, { tip.x - size / 2, tip.y - size / 2 }, rc.fore, lw, ss);
			rc.dc->DrawLine (tip, { tip.x - size / 2, tip.y + size / 2 }, rc.fore, lw, ss);
		}
		else
		{
			D2D1_POINT_2F tip = { name_line_x - grid->indent_width() / 2, item_y + height / 2 + size / 4 };
			rc.dc->DrawLine (tip, { tip.x - size / 2, tip.y - size / 2 }, rc.fore, lw, ss);
			rc.dc->DrawLine (tip, { tip.x + size / 2, tip.y - size / 2 }, rc.fore, lw, ss);
		}
	}
	#pragma endregion
	/*
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
	*/
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

		return text_layout_with_metrics (grid->window()->renderer().dwrite_factory(), grid->text_format(), property()->name, name_layout_width);
	}

	value_item_i::value_layout value_property_item_i::make_value_layout() const
	{
		auto grid = root()->grid();

		float width = grid->bounds().right - grid->border_width() - grid->value_column_x() - grid->line_thickness() - 2 * text_lr_padding;
		if (width <= 0)
			return { };

		auto factory = grid->window()->renderer().dwrite_factory();
		auto format = changed_from_default() ? grid->bold_text_format() : grid->text_format();

		text_layout_with_metrics tl;
		bool readable;
		try
		{
			if (multiple_values())
				tl = text_layout_with_metrics (factory, format, "(multiple values)", width);
			else
			{
				auto str = property()->get_to_string(parent()->parent()->objects().front(), root()->app_context());
				tl = text_layout_with_metrics (factory, format, str, width);
			}
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
		auto rectd = grid->bounds();
		float height = content_height_aligned();

		D2D1_RECT_F fill_rect = { rectd.left + bw, pd.y, rectd.right - bw, pd.y + height };

		if (selected)
		{
			rc.dc->FillRectangle (&fill_rect, focused ? rc.selected_back_focused.get() : rc.selected_back_not_focused.get());
		}
		else
		{
			rc.item_gradient_brush->SetStartPoint ({ fill_rect.left, fill_rect.top });
			rc.item_gradient_brush->SetEndPoint ({ fill_rect.left, fill_rect.bottom });
			rc.dc->FillRectangle (&fill_rect, rc.item_gradient_brush);
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
			rc.dc->GetTransform(&oldtr);
			float padding = line_width;
			rc.dc->SetTransform (D2D1::Matrix3x2F::Translation(rectd.left + bw + padding + line_width / 2, pd.y) * oldtr);

			if (bound)
				rc.dc->FillGeometry(rc.triangle_geo, rc.data_bind_fore);

			rc.dc->DrawGeometry(rc.triangle_geo, rc.data_bind_fore, line_width);

			rc.dc->SetTransform(&oldtr);
		}

		float name_line_x = rectd.left + bw + indent() * grid->indent_width();
		rc.dc->DrawLine ({ name_line_x + lt / 2, pd.y }, { name_line_x + lt / 2, pd.y + height }, rc.disabled_fore, lt);
		auto fore = bound ? rc.data_bind_fore.get() : rc.fore.get();
		rc.dc->DrawTextLayout ({ rectd.left + bw + indent() * grid->indent_width() + lt + text_lr_padding, pd.y }, name(), fore);

		float linex = grid->value_column_x() + lt / 2;
		rc.dc->DrawLine ({ linex, pd.y }, { linex, pd.y + height }, rc.disabled_fore, lt);

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
		return prop->description ? std::string(prop->description) : std::string();
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
						grid->change_property (objects, property(), new_value_str, root()->app_context());
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
			D2D1_RECT_F editor_rect = { vcx + grid->line_thickness(), item_y, grid->bounds().right - grid->border_width(), item_y + content_height_aligned() };
			bool bold = changed_from_default();
			std::string str = multiple_values() ? std::string() : property()->get_to_string(parent()->parent()->objects().front(), root()->app_context());
			auto editor = grid->show_text_editor (editor_rect, bold, text_lr_padding, str);
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
		std::vector<std::unique_ptr<property_item_i>> _children;

	public:
		group_item (object_item_i* parent, const property_group* group)
			: _parent(parent), _group(group)
		{
			perform_layout();
			expand();
		}

		virtual object_item_i* parent() const override final { return _parent; }

		virtual const std::vector<std::unique_ptr<property_item_i>>& children() const override final { return _children; }

		virtual bool selectable() const override final { return false; }

		virtual void on_mouse_down (const mouse_ud_args& ma, float item_y) { }

		virtual void on_mouse_up   (const mouse_ud_args& ma, float item_y) { }

		virtual std::string description_title() const override final { return { }; }

		virtual std::string description_text() const override final { return { }; }

		virtual size_t child_count() const override final { return _children.size(); }

		virtual pgitem_i* child_at(size_t index) const override final { return _children[index]->as_item(); }

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

		std::unique_ptr<property_item_i> make_child_item (const property* prop);

		void create_children()
		{
			rassert (_children.empty());
			auto type = _parent->objects().front()->type();

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
				_layout = text_layout_with_metrics (grid->window()->renderer().dwrite_factory(), grid->bold_text_format(), _group->name, layout_width);
			else
				_layout.clear();
		}

		virtual void render (const pg_render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const override
		{
			if (_layout)
			{
				auto grid = root()->grid();
				float bw = grid->border_width();
				auto rectd = grid->bounds();
				rc.dc->FillRectangle ({ rectd.left + bw, pd.y, rectd.right - bw, pd.y + content_height_aligned() }, rc.back);
				rc.dc->DrawTextLayout ({ rectd.left + bw + indent() * grid->indent_width() + text_lr_padding, pd.y }, _layout, rc.fore);
			}
		}

		virtual float content_height() const override
		{
			return _layout ? _layout.height() : 0;
		}

		HCURSOR cursor_at(D2D1_POINT_2F pd, float item_y) const final { return ::LoadCursor(nullptr, IDC_ARROW); }
	};

	class root_item : public root_item_i
	{
		property_grid_i* const _grid;
		std::string const _heading;
		pg_app_context_i* const _app_context;
		std::vector<object*> const _objects;
		std::vector<std::unique_ptr<group_item_i>> _children;
		bool _expanded = false;
		text_layout_with_metrics _text_layout;

	public:
		root_item (property_grid_i* grid, const char* heading, std::span<object* const> objects, pg_app_context_i* app_context)
			: _objects(objects.begin(), objects.end()), _grid(grid), _heading(heading ? heading : ""), _app_context(app_context)
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

		virtual pg_app_context_i* app_context() const override final { return _app_context; }

		virtual root_item* as_root() override final { return this; }

		virtual expandable_item_i* parent() const override { rassert(false); return nullptr; }

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
			_text_layout.clear();
			if (!_heading.empty())
			{
				float layout_width = _grid->width() - 2 * _grid->border_width() - 2 * title_lr_padding;
				if (layout_width > 0)
					_text_layout = text_layout_with_metrics (_grid->window()->renderer().dwrite_factory(), _grid->bold_text_format(), _heading, layout_width);
			}
		}

		virtual void render (const pg_render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const override final
		{
			if (_text_layout)
			{
				D2D1_RECT_F rect = {
					_grid->bounds().left + _grid->border_width(),
					pd.y,
					_grid->bounds().right - _grid->border_width(),
					pd.y + content_height_aligned()
				};
				rc.dc->FillRectangle (&rect, rc.root_item_back);
				rc.dc->DrawTextLayout ({ rect.left + title_lr_padding, rect.top + title_ud_padding }, _text_layout, rc.root_item_fore);
			}
		}

		virtual float content_height() const override final
		{
			if (_text_layout)
				return _text_layout.height() + 2 * title_ud_padding;
			else
				return 0;
		}

		HCURSOR cursor_at(D2D1_POINT_2F pd, float item_y) const final { return ::LoadCursor(nullptr, IDC_ARROW); }

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
			for (auto& gi : _children)
			{
				for (auto& pi : gi->children())
				{
					if (pi->property() == args.property)
					{
						pi->on_property_changing (obj, args);
						return;
					}
				}
			}
		}

		void on_property_changed (object* obj, const property_change_args& args)
		{
			for (auto& gi : _children)
			{
				for (auto& pi : gi->children())
				{
					if (pi->property() == args.property)
					{
						pi->on_property_changed (obj, args);
						return;
					}
				}
			}
		}

		void create_children()
		{
			rassert(!_objects.empty());

			auto type = _objects[0]->type();
			if (!std::all_of (_objects.begin(), _objects.end(), [type](object* o) { return o->type() == type; }))
				// TODO: some "(multiple types selected)" pg item
				return;

			_children = make_group_items(this, type);
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

		void perform_layout() final
		{
			_name = make_name_layout();
			_value = make_value_layout();
		}

		//virtual void perform_name_layout() override final { _name = make_name_layout(); }

		//virtual void perform_value_layout() override final { _value = make_value_layout(); }

		float content_height() const final
		{
			return std::max (name() ? name().height() : 0, value().tl ? value().tl.height() : 0);
		}

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
					brush = rc.data_bind_fore;
				else if (root()->grid()->read_only() || !can_edit())
					brush = rc.disabled_fore;
				else
					brush = rc.fore;

				rc.dc->DrawTextLayout ({ pd.x + root()->grid()->line_thickness() + text_lr_padding, pd.y }, value().tl, brush);
			}
		}

		void on_property_changing (object* obj, const property_change_args& args) final { }

		void on_property_changed (object* obj, const property_change_args& args) final
		{
			_value = make_value_layout();
			root()->grid()->invalidate();
		}
	};

	class object_collection_item : public object_collection_item_i
	{
		group_item_i* const _parent;
		const object_collection_property* const _prop;
		bool const _multiple_selection;
		text_layout_with_metrics _name_layout;
		text_layout_with_metrics _value_layout;
		bool _expanded = false;

	public:
		object_collection_item (group_item_i* parent, const object_collection_property* prop)
			: _parent(parent), _prop(prop)
			, _multiple_selection(_parent->parent()->objects().size() > 1)
		{
			perform_layout();
			//expand();
		}

		bool read_only() const { return _multiple_selection || root()->grid()->read_only(); }

		virtual group_item_i* parent() const override final { return _parent; }

		virtual void perform_layout() override final
		{
			auto grid = root()->grid();
			auto& objs = parent()->parent()->objects();
			bool modified = std::any_of (objs.begin(), objs.end(), [p=_prop](object* o) { return p->collection_cast(o)->child_count(); });
			IDWriteTextFormat* tf = modified ? grid->bold_text_format() : grid->text_format();

			_name_layout = text_layout_with_metrics(grid->window()->renderer().dwrite_factory(), tf, _prop->name);

			if (_multiple_selection)
				_value_layout = text_layout_with_metrics(grid->window()->renderer().dwrite_factory(), tf, "(multiple selection)");
			else
			{
				size_t entry_count = _prop->collection_cast(objs.front())->child_count();
				std::string str;
				if (entry_count == 0)
					str = "(empty)";
				else if (entry_count == 1)
					str = "one entry";
				else
					str = std::to_string(entry_count) + " entries";
				_value_layout = text_layout_with_metrics(grid->window()->renderer().dwrite_factory(), tf, str);
			}
		}

		virtual void render (const pg_render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const override final
		{
			auto grid = root()->grid();
			auto lw = grid->line_thickness();
			float bw = grid->border_width();
			auto rectd = grid->bounds();
			float height = content_height_aligned();

			D2D1_RECT_F fill_rect = { rectd.left + bw, pd.y, rectd.right - bw, pd.y + height };

			if (selected)
			{
				rc.dc->FillRectangle (&fill_rect, focused ? rc.selected_back_focused.get() : rc.selected_back_not_focused.get());
			}
			else
			{
				rc.item_gradient_brush->SetStartPoint ({ fill_rect.left, fill_rect.top });
				rc.item_gradient_brush->SetEndPoint ({ fill_rect.left, fill_rect.bottom });
				rc.dc->FillRectangle (&fill_rect, rc.item_gradient_brush);
			}

			float name_line_x = rectd.left + bw + indent() * grid->indent_width();
			rc.dc->DrawLine ({ name_line_x + lw / 2, pd.y }, { name_line_x + lw / 2, pd.y + height }, rc.disabled_fore, lw);
			rc.dc->DrawTextLayout ({ name_line_x + lw + text_lr_padding, pd.y }, _name_layout, rc.fore);

			float linex = grid->value_column_x() + lw / 2;
			rc.dc->DrawLine ({ linex, pd.y }, { linex, pd.y + height }, rc.disabled_fore, lw);

			ID2D1Brush* brush = read_only() ? rc.disabled_fore : rc.fore;
			rc.dc->DrawTextLayout ({ grid->value_column_x() + lw + text_lr_padding, pd.y }, _value_layout, brush);
		}

		virtual float content_height() const override final { return 20; }

		virtual HCURSOR cursor_at(D2D1_POINT_2F pd, float item_y) const override final
		{
			return ::LoadCursor(nullptr, read_only() ? IDC_ARROW : IDC_HAND);
		}

		virtual void on_mouse_down (const mouse_ud_args& ma, float item_y) override final { }

		virtual void on_mouse_up (const mouse_ud_args& ma, float item_y) override final { }

		virtual std::string description_title() const override final { return _prop->name; }

		virtual std::string description_text() const override final
		{
			return _prop->description ? std::string(_prop->description) : std::string();
		}

		// expandable_item_i
		size_t child_count() const final { return 0; }
		pgitem_i* child_at(size_t index) const final { return nullptr; }
		void expand() final { throw not_implemented_exception(); }
		void collapse() final { throw not_implemented_exception(); }
		bool expanded() const final { return _expanded; }

		// property_item_i
		pgitem_i* as_item() final { return this; }
		void on_property_changing (object* obj, const property_change_args& args) final { rassert(false); }
		void on_property_changed (object* obj, const property_change_args& args) final { rassert(false); }

		// object_collection_item_i
		const object_collection_property* property() const final { return _prop; }
	};

	class object_picker_popup : public win32_window_i
	{
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
		std::function<void(const concrete_type*)> const _callback;
		property_grid_i*         const _grid;
		float                    const _client_width = 200;
		float                    const _client_height = 400;
		window                   _window;
		d2d_renderer             _renderer;
		float                    const _lrpadding;
		//text_layout_with_metrics const _bottom_hint_text;
		HHOOK _mouse_hook = nullptr;
		std::vector<const concrete_type*> _types;

	public:
		object_picker_popup (object_property_item_i* item, float item_y, std::function<void(const concrete_type*)> callback)
			: _item(item)
			, _callback(callback)
			, _grid(item->root()->grid())
			, _window(wnd_class, ex_style, style, _grid->window()->hwnd(), _grid->calc_popup_window_pos(item, item_y, { _client_width, _client_height }, style, ex_style))
			, _renderer(this, _grid->window()->renderer().d3d_dc(), _grid->window()->renderer().dwrite_factory())
			, _lrpadding(std::round(5 / pixel_width()) * pixel_width())
		{
			_window.window_proc().add_handler<&object_picker_popup::on_window_proc>(this);
			_renderer.render().add_handler<&object_picker_popup::on_render>(this);
			::ShowWindow (hwnd(), SW_SHOWNOACTIVATE);

			for (auto t : concrete_type::known_types())
			{
				if (t->is_same_or_derived_from(item->property()->child_type()))
					_types.push_back(t);
			}
		}

		~object_picker_popup()
		{
			::ShowWindow (hwnd(), SW_HIDE);
			_renderer.render().remove_handler<&object_picker_popup::on_render>(this);
			_window.window_proc().remove_handler<&object_picker_popup::on_window_proc>(this);
		}

	private:

		HWND hwnd() const final { return _window.hwnd(); }

		window_proc_e::subscriber window_proc() final { return _window.window_proc(); }

		std::optional<LRESULT> on_window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
		{
			if (msg == WM_LBUTTONDOWN)
			{
				_callback(_types.front());
				return 0;
			}

			return std::nullopt;
		}

		void on_render (ID2D1DeviceContext* dc) const
		{
			auto rc = _grid->make_render_context(dc);
			dc->SetTransform(dpi_transform());
			dc->Clear(rc.back->GetColor());
			float layout_width = _client_width - 2 * _lrpadding;
			float y = 0;
			for (const concrete_type* t : _types)
			{
				text_layout_with_metrics name (_renderer.dwrite_factory(), _grid->bold_text_format(), t->name, layout_width);
				dc->DrawTextLayout ({ _lrpadding, y }, name, rc.fore);
				y += (name.height() * 1.2f);
			}
		}

		static LRESULT CALLBACK mouse_hook_proc(int Code, WPARAM wParam, LPARAM lParam);
	};

	class object_property_item : public object_property_item_i
	{
		group_item_i* const _parent;
		const object_property* const _prop;
		bool _expanded = false;
		text_layout_with_metrics _name;
		std::vector<object*> _objects;
		std::vector<std::unique_ptr<group_item_i>> _children;
		size_t _property_changing_count = 0;

		enum class value_state { all_null, multiple_selection, all_same_type };
		std::pair<value_state, text_layout_with_metrics> _value;

		static inline std::optional<object_picker_popup> _popup;

	public:
		object_property_item (group_item_i* parent, const object_property* prop)
			: _parent(parent)
			, _prop(prop)
			, _name (make_name_layout())
			, _value (make_value_layout())
		{
			_objects.reserve(parent->parent()->objects().size());
			for (auto obj : parent->parent()->objects())
				_objects.push_back(prop->get(obj));
		}

		~object_property_item()
		{
			_popup.reset();
		}

		// pgitem_i
		group_item_i* parent() const final { return _parent; }

		text_layout_with_metrics make_name_layout() const
		{
			auto grid = root()->grid();
			auto dwf = grid->window()->renderer().dwrite_factory();
			float ncx = grid->name_column_x(indent());
			float name_width = grid->value_column_x() - ncx;
			return text_layout_with_metrics(dwf, grid->text_format(), _prop->name, name_width);
		}

		std::pair<value_state, text_layout_with_metrics> make_value_layout() const
		{
			auto grid = root()->grid();
			auto dwf = grid->window()->renderer().dwrite_factory();
			float ncx = grid->name_column_x(indent());

			float value_width = grid->bounds().right - ncx;
			auto& objs = _parent->parent()->objects();
			if (std::all_of(objs.begin(), objs.end(), [prop=_prop](object* o) { return !prop->get(o); }))
				return { value_state::all_null, text_layout_with_metrics(dwf, grid->text_format(), "(not set)", value_width) };

			auto type = _prop->get(objs.front()) ? _prop->get(objs.front())->type() : nullptr;
			bool all_same_type = std::all_of(objs.begin(), objs.end(), [prop=_prop,type](object* o) { return (prop->get(o) ? prop->get(o)->type() : nullptr) == type; });
			if (!all_same_type)
				return { value_state::multiple_selection, text_layout_with_metrics(dwf, grid->bold_text_format(), "(multiple selection)", value_width) };

			return { value_state::all_same_type, text_layout_with_metrics(dwf, grid->bold_text_format(), type->name, value_width) };
		}

		void perform_layout() final
		{
			_name = make_name_layout();
			_value = make_value_layout();
		}

		D2D1_RECT_F expand_button_click_rect (float item_y) const
		{
			rassert (_value.first == value_state::all_same_type);
			auto grid = root()->grid();
			auto rectd = grid->bounds();
			float name_line_x = rectd.left + grid->border_width() + indent() * grid->indent_width();
			return { name_line_x - grid->indent_width(), item_y, name_line_x, item_y + this->content_height_aligned() };
		}

		void render (const pg_render_context& rc, D2D1_POINT_2F pd, bool selected, bool focused) const final
		{
			auto grid = root()->grid();
			auto lt = grid->line_thickness();
			float bw = grid->border_width();
			auto rectd = grid->bounds();
			float height = content_height_aligned();

			D2D1_RECT_F fill_rect = { rectd.left + bw, pd.y, rectd.right - bw, pd.y + height };

			if (selected)
			{
				rc.dc->FillRectangle (&fill_rect, focused ? rc.selected_back_focused.get() : rc.selected_back_not_focused.get());
			}
			else
			{
				rc.item_gradient_brush->SetStartPoint ({ fill_rect.left, fill_rect.top });
				rc.item_gradient_brush->SetEndPoint ({ fill_rect.left, fill_rect.bottom });
				rc.dc->FillRectangle (&fill_rect, rc.item_gradient_brush);
			}

			float name_line_x = rectd.left + bw + indent() * grid->indent_width();

			if (_value.first == value_state::all_same_type)
				render_expand_button(rc, pd.y);

			rc.dc->DrawLine ({ name_line_x + lt/2, pd.y }, { name_line_x + lt/2, pd.y + height }, rc.disabled_fore, lt);
			rc.dc->DrawTextLayout ({ name_line_x + lt + text_lr_padding, pd.y }, _name, rc.fore);

			float value_linex = grid->value_column_x();
			rc.dc->DrawLine ({ value_linex + lt/2, pd.y }, { value_linex + lt/2, pd.y + height }, rc.disabled_fore, lt);
			rc.dc->DrawTextLayout ({ grid->value_column_x() + lt + text_lr_padding, pd.y }, _value.second, rc.fore);

//			rc.dc->DrawTextLayout(pd, _name, rc.fore);
		}

		float content_height() const final { return std::max(_name.height(), _value.second.height()); }

		HCURSOR cursor_at (D2D1_POINT_2F pd, float item_y) const final
		{
			auto grid = root()->grid();
			if (grid->read_only())
				return ::LoadCursor(nullptr, IDC_ARROW);

			return ::LoadCursor(nullptr, IDC_HAND);
		}

		void on_mouse_down (const mouse_ud_args& ma, float item_y) final
		{
			if (_value.first == value_state::all_same_type)
			{
				if (point_in_rect(expand_button_click_rect(item_y), ma.pd))
				{
					if (_expanded)
						collapse();
					else
						expand();
				}
			}
		}

		void on_mouse_up (const mouse_ud_args& ma, float item_y) final
		{
			auto grid = root()->grid();
			if (grid->read_only())
				return;
			auto vcx = grid->value_column_x();
			if (ma.pd.x < vcx)
				return;

			_popup.emplace(this, item_y, std::bind(&object_property_item::on_object_picked, this, std::placeholders::_1));
		}

		std::string description_title() const final { return _prop->name; }

		std::string description_text() const final { return _prop->description ? std::string(_prop->description) : std::string(); }

		// expandable_item_i
		size_t child_count() const final { return _children.size(); }

		pgitem_i* child_at(size_t index) const final { return _children[index].get(); }

		void expand() final
		{
			rassert (!_expanded);
			if (_value.first != value_state::all_same_type)
				return;

			auto type = _prop->get(_parent->parent()->objects().front())->type();
			_children = make_group_items(this, type);
			_expanded = true;
			root()->grid()->invalidate();
		}

		void collapse() final
		{
			rassert (_expanded);
			_children.clear();
			_expanded = false;
			root()->grid()->invalidate();
		}

		bool expanded() const final { return _expanded; }

		// object_item_i
		const std::vector<object*>& objects() const final { return _objects; }

		// property_item_i
		pgitem_i* as_item() final { return this; }

		void on_property_changing (object* obj, const property_change_args& args) final
		{
			if (_property_changing_count == 0)
			{
				_children.clear();
				_expanded = false;
			}

			rassert (_property_changing_count < _objects.size());
			_property_changing_count++;
		}

		void on_property_changed (object* obj, const property_change_args& args) final
		{
			rassert(_property_changing_count > 0);
			_property_changing_count--;
			if (_property_changing_count == 0)
			{
				_objects.clear();
				for (auto obj : _parent->parent()->objects())
					_objects.push_back(_prop->get(obj));
				_value = make_value_layout(); // this one first cause expand() uses it (TODO: refactor this)
				expand();
				root()->grid()->invalidate();
			}
		}

		// object_property_item_i
		const object_property* property() const final { return _prop; }

		void on_object_picked (const concrete_type* type)
		{
			auto& objs = parent()->parent()->objects();
			if (std::any_of(objs.begin(), objs.end(),
				[prop=_prop,type](object* o) { return (prop->get(o) ? prop->get(o)->type() : nullptr) != type; }))
			{
				root()->grid()->change_property (objs, _prop, type);
			}

			_popup.reset();
		}
	};

	std::unique_ptr<property_item_i> group_item::make_child_item (const property* prop)
	{
		if (auto f = dynamic_cast<const pg_custom_item_i*>(prop))
			return f->create_item(this, prop);

		if (auto value_prop = dynamic_cast<const value_property*>(prop))
			return std::make_unique<default_value_pgitem>(this, value_prop);

		if (auto obj_coll_prop = dynamic_cast<const object_collection_property*>(prop))
			return std::make_unique<object_collection_item>(this, obj_coll_prop);

		if (auto obj_prop = dynamic_cast<const object_property*>(prop))
			return std::make_unique<object_property_item>(this, obj_prop);

		// TODO: placeholder pg item for unknown types of properties
		throw not_implemented_exception();
	}

	static std::unique_ptr<root_item_i> make_root_item (property_grid_i* grid, const char* heading, std::span<object* const> objects, pg_app_context_i* app_context)
	{
		return std::make_unique<root_item>(grid, heading, objects, app_context);
	}

	static std::vector<std::unique_ptr<group_item_i>> make_group_items (object_item_i* parent, const concrete_type* type)
	{
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

		std::vector<std::unique_ptr<group_item_i>> group_items;
		for (const property_group* g : groups)
			group_items.push_back (std::make_unique<group_item>(parent, g));
		return group_items;
	}
}
