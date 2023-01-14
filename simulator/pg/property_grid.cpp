
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "include/pg/property_grid.h"
#include "edge/utility_functions.h"
#include "edge/d2d_renderer.h"

using namespace edge;
using namespace pg;

extern std::unique_ptr<root_item_i> make_root_item (property_grid_i* grid, std::string_view heading, object_list_i& objects, edge::string_convert_context_i* scc);

class property_grid : public property_grid_i
{
	std::shared_ptr<event_manager> const _em = std::make_shared<event_manager>();
	d2d_renderer_i* const _renderer;
	theme_color_provider_i* const _tcp;
	com_ptr<IDWriteTextFormat> _text_format;
	com_ptr<IDWriteTextFormat> _bold_text_format;
	com_ptr<IDWriteTextFormat> _wingdings;
	std::unique_ptr<text_editor_i> _text_editor;
	D2D1_RECT_F _rectd;
	float _name_column_factor = 0.6f;
	std::vector<std::unique_ptr<root_item_i>> _root_items;
	HWND _tooltip = nullptr;
	std::optional<D2D1_POINT_2F> _last_tt_location;
	float _border_width_not_aligned = 0;
	bool _read_only = false;
	bool _scroll_bar_visible = false;
	float _top_y = 0;

	static constexpr float font_size = 13;
	static constexpr float line_width_not_aligned = 0.6f;

public:
	property_grid (d2d_renderer_i* renderer, const D2D1_RECT_F& bounds, theme_color_provider_i* tcp)
		: _renderer(renderer)
		, _rectd(bounds)
		, _tcp(tcp)
		, _selected_item(this, std::bind(&property_grid::on_selected_item_changing, std::placeholders::_1))
		, _hot_item(this, std::bind(&property_grid::on_hot_item_changing, std::placeholders::_1))
	{
		auto hinstance = (HINSTANCE)::GetWindowLongPtr (renderer->window().hwnd(), GWLP_HINSTANCE);

		_tooltip = CreateWindowEx (WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr,
			WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			renderer->window().hwnd(), nullptr, hinstance, nullptr);

		TOOLINFO ti = { sizeof(TOOLINFO) };
		ti.uFlags   = TTF_SUBCLASS;
		ti.hwnd     = _renderer->window().hwnd();
		ti.lpszText = nullptr;
		ti.rect     = tooltip_rect(ti.hwnd);
		SendMessage(_tooltip, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti);

		SendMessage (_tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 1500);
		SendMessage (_tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, (LPARAM)(LONG)MAXSHORT);
		SendMessage (_tooltip, TTM_SETMAXTIPWIDTH, 0, ti.rect.right - ti.rect.left);

		auto hr = _renderer->dwrite_factory()->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
													DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_text_format); rassert(SUCCEEDED(hr));

		hr = _renderer->dwrite_factory()->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
												DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_bold_text_format); rassert(SUCCEEDED(hr));

		hr = _renderer->dwrite_factory()->CreateTextFormat (L"Wingdings", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
												DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_wingdings); rassert(SUCCEEDED(hr));
		_renderer->window().window_proc().add_handler<&property_grid::on_window_proc>(this);
		_renderer->render().add_handler<&property_grid::on_render>(this);
		_tcp->theme_colors_changed().add_handler(&on_theme_colors_changed, this);
		this->invalidate();
	}

	virtual ~property_grid()
	{
		_tcp->theme_colors_changed().remove_handler(&on_theme_colors_changed, this);
		_renderer->render().remove_handler<&property_grid::on_render>(this);
		_renderer->window().window_proc().remove_handler<&property_grid::on_window_proc>(this);
		this->invalidate();
		::DestroyWindow(_tooltip);
	}

	static void on_theme_colors_changed (void* arg)
	{
		auto pg = static_cast<property_grid*>(arg);
		pg->invalidate();
	}

	virtual IDWriteTextFormat* text_format() const override final { return _text_format; }

	virtual IDWriteTextFormat* bold_text_format() const override final { return _bold_text_format; }

	RECT tooltip_rect (HWND hwnd) const
	{
		uint32_t dpi = edge::dpi(hwnd);
		auto tl = edge::pointd_to_pointp({ _rectd.left,  _rectd.top    }, dpi, -1);
		auto br = edge::pointd_to_pointp({ _rectd.right, _rectd.bottom }, dpi, 1);
		return { tl.x, tl.y, br.x, br.y };
	}

	virtual void invalidate() override final
	{
		edge::invalidate(_rectd, _renderer->window().hwnd());
	}

	virtual void invalidate_item (item_i* i) override final
	{
		// TODO: invalidate only area covered by the item.
		invalidate();
	}

	std::optional<LRESULT> on_window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (msg == WM_DPICHANGED_AFTERPARENT)
		{
			process_dpi_changed();
			::InvalidateRect (hwnd, nullptr, FALSE);
			return std::nullopt;
		}

		if (msg == WM_SETCURSOR)
			return process_wm_setcursor (hwnd, wparam, lparam);

		if ((msg == WM_LBUTTONDOWN) || (msg == WM_RBUTTONDOWN))
		{
			auto pp = POINT{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			uint32_t dpi = edge::dpi(hwnd);
			auto pd = edge::pointp_to_pointd(pp, dpi);
			if (point_in_rect(_rectd, pd))
			{
				auto button = (msg == WM_LBUTTONDOWN) ? mouse_button::left : mouse_button::right;
				auto mks = (modifier_key)(UINT)wparam | ((::GetKeyState(VK_MENU) < 0) ? modifier_key::alt : modifier_key::none);
				return process_mouse_button_down ({ button, mks, pp, pd });
			}

			return std::nullopt;
		}

		if ((msg == WM_LBUTTONUP) || (msg == WM_RBUTTONUP))
		{
			auto pp = POINT{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			uint32_t dpi = edge::dpi(hwnd);
			auto pd = pointp_to_pointd(pp, dpi);
			if (point_in_rect(_rectd, pd))
			{
				auto button = (msg == WM_LBUTTONUP) ? mouse_button::left : mouse_button::right;
				auto mks = (modifier_key)(UINT)wparam | ((::GetKeyState(VK_MENU) < 0) ? modifier_key::alt : modifier_key::none);
				return process_mouse_button_up ({ button, mks, pp, pd });
			}

			return std::nullopt;
		}

		if (msg == WM_MOUSEMOVE)
		{
			TRACKMOUSEEVENT tme = { .cbSize = sizeof(tme), .dwFlags = TME_LEAVE, .hwndTrack = hwnd };
			BOOL bres = TrackMouseEvent (&tme);

			auto pp = POINT{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			uint32_t dpi = edge::dpi(hwnd);
			auto pd = pointp_to_pointd(pp, dpi);
			if (point_in_rect(_rectd, pd))
			{
				auto mks = (modifier_key)(UINT)wparam | ((::GetKeyState(VK_MENU) < 0) ? modifier_key::alt : modifier_key::none);
				process_mouse_move (hwnd, { mks, pd });
			}

			return std::nullopt;
		}

		if (msg == WM_MOUSEWHEEL)
		{
			auto pp = POINT{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			::ScreenToClient(hwnd, &pp);
			uint32_t dpi = edge::dpi(hwnd);
			auto pd = pointp_to_pointd(pp, dpi);
			if (point_in_rect(_rectd, pd))
			{
				auto mks = (modifier_key)GET_KEYSTATE_WPARAM(wparam) | ((::GetKeyState(VK_MENU) < 0) ? modifier_key::alt : modifier_key::none);
				short delta = GET_WHEEL_DELTA_WPARAM(wparam);
				return process_wm_mousewheel (hwnd, pd, mks, delta);
			}

			return std::nullopt;
		}

		if (msg == WM_MOUSELEAVE)
		{
			_hot_item = nullptr;
			return std::nullopt;
		}

		if ((msg == WM_KEYDOWN) || (msg == WM_SYSKEYDOWN))
			return process_key_down ((UINT) wparam, get_modifier_keys());

		if ((msg == WM_KEYUP) || (msg == WM_SYSKEYUP))
			return process_key_up ((UINT) wparam, get_modifier_keys());

		if (msg == WM_CHAR)
			return process_char_key((uint32_t)wparam);
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

	std::optional<LRESULT> process_wm_setcursor (HWND hwnd, WPARAM wparam, LPARAM lparam)
	{
		if (((HWND)wparam == hwnd) && (LOWORD(lparam) == HTCLIENT))
		{
			POINT pt;
			if (::GetCursorPos(&pt) && ::ScreenToClient (hwnd, &pt))
			{
				uint32_t dpi = edge::dpi(hwnd);
				auto pd = pointp_to_pointd(pt, dpi);

				if (point_in_rect(_rectd, pd))
				{
					HCURSOR cursor = nullptr;
					//if ((pd.x >= value_column_x()) && (pd.x < _rectd.right))
					{
						if (auto htr = hit_test(pd); htr.item)
						{
							cursor = item_set_cursor_e::invoker(_em).invoke(htr);
							if (!cursor)
							{
								if (htr.code == htcode::value)
									cursor = htr.item->cursor_at(pd, htr.render_y);
							}
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

	// Returns the canceled item, or nullptr if no item was canceled.
	template<typename callback_t> requires std::is_invocable_v<callback_t, item_i*, float, bool&>
	item_i* enum_items (item_i* item, float& y, const callback_t& callback) const
	{
		bool cancel = false;
		callback(item, y, cancel);
		if (cancel)
			return item;

		y += height_aligned(item);

		if (auto ei = dynamic_cast<expandable_item_i*>(item))
		{
			size_t cc = ei->child_count();
			for (size_t i = 0; i < cc; i++)
			{
				auto child = ei->child_at(i);
				auto canceled_item = enum_items(child, y, callback);
				if (canceled_item)
					return canceled_item;
			}
		}

		return nullptr;
	}

	// Returns the canceled item and its y (not the same as "render_y" due to vertical scrolling).
	// If the callback doesn't cancel any item, returns nullptr and the total height.
	template<typename callback_t> requires std::is_invocable_v<callback_t, item_i*, float, bool&>
	std::pair<item_i*, float> enum_items (const callback_t& callback) const
	{
		float y = 0;
		for (auto& root_item : _root_items)
		{
			auto canceled_item = enum_items(root_item.get(), y, callback);
			if (canceled_item)
				return { canceled_item, y };
		}

		return { nullptr, y };
	}

	void perform_layouts()
	{
		_scroll_bar_visible = false;

		uint32_t dpi = edge::dpi(_renderer->window().hwnd());
		float visible_height = _rectd.bottom - _rectd.top - 2 * border_width(dpi);
		auto callback = [this, visible_height](item_i* i, float item_y, bool& cancel)
			{
				i->perform_layout();
				if (item_y + height_aligned(i) > visible_height)
					// TODO: add new function "clear_layout" to item_i and call it for the remaining items.
					cancel = true;
			};

		auto[canceled_item, y] = enum_items (callback);

		if (canceled_item)
		{
			_scroll_bar_visible = true;
			enum_items (callback);
		}

		invalidate();
	}

	float height_aligned (const item_i* item) const
	{
		float height = item->content_height();
		uint32_t dpi = edge::dpi(_renderer->window().hwnd());
		float pw = pixel_width(dpi);
		height = std::ceil(height / pw) * pw;
		return height;
	}

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
		auto hr = dc->CreateGradientStopCollection ((back_luminance > 0.6f) ? stops_light : stops_dark, 3, &stop_collection); rassert(SUCCEEDED(hr));
		static constexpr D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES lgbp = { { 0, 0 }, { 1, 0 } };
		hr = dc->CreateLinearGradientBrush (&lgbp, nullptr, stop_collection, &rc.item_gradient_brush); rassert(SUCCEEDED(hr));

		hr = dc->CreateGradientStopCollection ((back_luminance > 0.6f) ? stops_light_hot : stops_dark_hot, 3, &stop_collection); rassert(SUCCEEDED(hr));
		hr = dc->CreateLinearGradientBrush (&lgbp, nullptr, stop_collection, &rc.item_gradient_brush_hot); rassert(SUCCEEDED(hr));

		return rc;
	}

	void on_render (HWND hwnd, ID2D1DeviceContext* dc) const
	{
		auto rc = make_render_context(dc);
		uint32_t dpi = edge::dpi(hwnd);
		dc->SetTransform (edge::dpi_transform(dpi));

		dc->FillRectangle(_rectd, rc.back);

		float bw = border_width(dpi);
		if (bw > 0)
			dc->DrawRectangle(inflate(_rectd, -bw / 2), rc.border, bw);

		if (_root_items.empty())
		{
			auto tl = text_layout_with_metrics (_renderer->dwrite_factory(), _text_format, "(no selection)");
			D2D1_POINT_2F p = { (_rectd.left + _rectd.right) / 2 - tl.width() / 2, (_rectd.top + _rectd.bottom) / 2 - tl.height() / 2};
			dc->DrawTextLayout (p, tl, rc.fore);
			return;
		}

		bool focused = GetFocus() == hwnd;

		dc->PushAxisAlignedClip (&_rectd, D2D1_ANTIALIAS_MODE_ALIASED);
		enum_items ([dc, &rc, focused, this, bw](item_i* item, float item_y, bool& cancel)
		{
			if (item_y + height_aligned(item) <= _top_y)
			{
				// item is scrolled above the visible area
			}
			else
			{
				float render_y = _rectd.top + bw + item_y - _top_y;
				if (render_y >= _rectd.bottom)
				{
					cancel = true;
					return;
				}

				bool selected = (item == _selected_item.get());
				bool hot = (item == _hot_item.get());
				item->render (rc, render_y, selected, hot, focused);
			}
		});
		dc->PopAxisAlignedClip();

		if (_scroll_bar_visible)
		{
			D2D1_RECT_F r = {
				_rectd.right - bw - scroll_bar_width(dpi),
				_rectd.top + bw,
				_rectd.right - bw,
				_rectd.bottom - bw
			};

			dc->FillRectangle(r, rc.disabled_fore);
		}
	}

	virtual d2d_renderer_i* renderer() const override { return _renderer; }

	virtual win32_window_i& window() const override final { return _renderer->window(); }

	virtual D2D1_RECT_F bounds() const override { return _rectd; }

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
				perform_layouts();

			TOOLINFO ti = { sizeof(TOOLINFO) };
			ti.uFlags   = TTF_SUBCLASS;
			ti.hwnd     = _renderer->window().hwnd();
			ti.lpszText = nullptr;
			ti.rect     = tooltip_rect(ti.hwnd);
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
			perform_layouts();
		}
	}

	void process_dpi_changed()
	{
		_text_editor = nullptr;
		perform_layouts();
	}

	virtual void clear_sections() override
	{
		_root_items.clear();
		invalidate();
	}

	virtual void add_section (std::string_view heading, object_list_i& objects, string_convert_context_i* scc) override
	{
		_root_items.push_back (make_root_item(this, heading, objects, scc));
	}

	virtual std::span<const std::unique_ptr<root_item_i>> sections() const override { return _root_items; }

	virtual void set_read_only (bool read_only) override final
	{
		_read_only = read_only;
		invalidate();
	}

	virtual bool read_only() const override { return _read_only; }

	virtual property_edited_e::subscriber property_changed() override final { return property_edited_e::subscriber(_em); }

	virtual item_set_cursor_e::subscriber item_set_cursor() override { return item_set_cursor_e::subscriber(_em); }

	virtual item_clicked_e::subscriber item_clicked() override final { return item_clicked_e::subscriber(_em); }

	virtual bool try_show_text_editor_on_selected_item (bool bold, std::string_view str) override final
	{
		rassert (_selected_item);

		if (!_text_editor || try_commit_editor())
		{
			auto p = enum_items([si=_selected_item.get()](item_i* item, float y, bool& cancel) { cancel = (item == si); });
			uint32_t dpi = edge::dpi(_renderer->window().hwnd());
			float item_render_y = _rectd.top + border_width(dpi) + p.second - _top_y;
			float vcx = value_column_left(dpi);
			float item_height = height_aligned(p.first);
			D2D1_RECT_F rect = { vcx + line_width(dpi), item_render_y, value_column_right(dpi), item_render_y + item_height };
			_text_editor = text_editor_factory (_renderer, bold ? _bold_text_format : _text_format, _tcp, rect, text_lr_padding, str);
			return true;
		}

		return false;
	}

	virtual int show_enum_editor (D2D1_POINT_2F dip, const nvp* nameValuePairs) override final
	{
		_text_editor = nullptr;
		HWND window_hwnd = _renderer->window().hwnd();
		uint32_t dpi = edge::dpi(window_hwnd);
		POINT ptScreen = edge::pointd_to_pointp(dip, dpi, 0);
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

			atom = ::RegisterClassW (&EditorWndClass); rassert (atom != 0);
		}

		auto hwnd = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, ClassName, L"aaa", WS_POPUP | WS_BORDER, 0, 0, 0, 0, window_hwnd, nullptr, hInstance, nullptr); rassert (hwnd != nullptr);

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

		uint32_t dpi = edge::dpi(_renderer->window().hwnd());
		enum_items ([this, pd, &result, dpi, bw=border_width(dpi)](item_i* item, float item_y, bool& cancel)
		{
			if (item_y + height_aligned(item) <= _top_y)
			{
				// item is scrolled above the visible area
			}
			else
			{
				float render_y = _rectd.top + bw + item_y - _top_y;
				if (pd.y < render_y + height_aligned(item))
				{
					result = htresult{ };
					result.item = item;
					result.render_y = render_y;

					if (pd.x < name_column_left(item->indent()))
						result.code = htcode::expand;
					else if (pd.x < value_column_left(dpi))
						result.code = htcode::name;
					else
						result.code = htcode::value;

					cancel = true;
				}
			}
		});

		return result;
	}

	virtual D2D1_POINT_2F output_of (value_property_item_i* vi) const override
	{
		std::optional<D2D1_POINT_2F> res;

		uint32_t dpi = edge::dpi(_renderer->window().hwnd());
		enum_items ([vi, this, &res, bw=border_width(dpi)](item_i* item, float item_y, bool& cancel)
		{
			if (item == vi)
			{
				res = D2D1_POINT_2F{ _rectd.right, _rectd.top + bw + item_y - _top_y + height_aligned(item) / 2 };
				cancel = true;
			}
		});

		rassert(res);
		return res.value();
	}

	virtual value_property_item_i* find_item (const value_property* prop) const override
	{
		value_property_item_i* res = nullptr;

		enum_items([&res, prop](item_i* item, float y, bool& cancel)
		{
			if (auto vi = dynamic_cast<value_property_item_i*>(item); vi && (vi->property() == prop))
			{
				res = vi;
				cancel = true;
			}
		});

		return res;
	}

	std::optional<LRESULT> process_mouse_button_down (const mouse_ud_args& args)
	{
		if (_text_editor && (_text_editor->mouse_captured() || point_in_rect(_text_editor->rect(), args.pd)))
			return _text_editor->on_mouse_down(args) ? std::optional<LRESULT>(0) : std::nullopt;

		auto clicked_item = hit_test(args.pd);

		auto new_selected_item = (clicked_item.item && clicked_item.item->selectable()) ? clicked_item.item : nullptr;
		if (_selected_item.get() != new_selected_item)
		{
			_text_editor = nullptr;
			_selected_item = new_selected_item;
		}

		if (clicked_item.item)
		{
			clicked_item.item->on_mouse_down (args, clicked_item.render_y);
			return 0;
		}

		return std::nullopt;
	}

	std::optional<LRESULT> process_mouse_button_up (const mouse_ud_args& args)
	{
		if (_text_editor && _text_editor->mouse_captured())
			return _text_editor->on_mouse_up(args) ? std::optional<LRESULT>(0) : std::nullopt;

		auto clicked_item = hit_test(args.pd);
		if (clicked_item.item)
		{
			// TODO: pass first to the item's on_mouse_down, and if that one returns std::nullopt,
			// then pass to a new "item_mouse_up" event, and if that one returns std::nullopt,
			// then generate the "clicked" event and repeat (first to item, then to pg event handler).
			auto res = item_clicked_e::invoker(_em).invoke(clicked_item);
			if (res.has_value())
				return res;

			clicked_item.item->on_mouse_up (args, clicked_item.render_y);
			return 0;
		}

		return std::nullopt;
	}

	void process_mouse_move (HWND hwnd, const mouse_move_args& args)
	{
		if (_text_editor && _text_editor->mouse_captured())
			return _text_editor->on_mouse_move (args);

		auto htres = hit_test(args.pd);
		_hot_item = (htres.item && htres.item->selectable()) ? htres.item : nullptr;
				
		if (!_last_tt_location || (_last_tt_location != args.pd))
		{
			_last_tt_location = args.pd;

			if (_text_editor && point_in_rect(_text_editor->rect(), args.pd))
			{
				TOOLINFO ti = { sizeof(TOOLINFO), 0, hwnd };
				SendMessage(_tooltip, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
			}
			else
			{
				::SendMessage (_tooltip, TTM_POP, 0, 0);

				std::wstring text;
				std::wstring title;

				if (htres.item)
				{
					title = utf8_to_utf16(htres.item->description_title());
					text  = utf8_to_utf16(htres.item->description_text());

					if (!title.empty() && text.empty())
						text = L"--";
				}

				TOOLINFO ti = { sizeof(TOOLINFO) };
				ti.hwnd     = hwnd;
				ti.lpszText = text.data();
				SendMessage(_tooltip, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);

				SendMessage(_tooltip, TTM_SETTITLE, TTI_INFO, (LPARAM)title.data());
			}
		}
	}

	std::optional<LRESULT> process_wm_mousewheel (HWND hwnd, D2D1_POINT_2F pd, modifier_key mks, short delta)
	{
		float visible_height = _rectd.bottom - _rectd.top;
		float total_height = enum_items([](auto...) { }).second;
		if (total_height > visible_height)
		{
			UINT scroll_lines;
			::SystemParametersInfo (SPI_GETWHEELSCROLLLINES, 0, &scroll_lines, 0);

			float scroll_line_height = text_layout_with_metrics(_renderer->dwrite_factory(), _text_format, "A").height();
			uint32_t dpi = edge::dpi(hwnd);
			float pw = edge::pixel_width(dpi);
			scroll_line_height = std::ceil(scroll_line_height / pw) * pw + line_width(dpi);

			float scroll_distance = (float)scroll_lines * scroll_line_height * delta / 120;
			if (scroll_distance < 0)
			{
				// wheel down; scroll items up
				if (total_height > (_top_y + _rectd.bottom - _rectd.top))
				{
					// there are items below the bottom of the grid that can be scroll up
					float scrollable = total_height - (_top_y + _rectd.bottom - _rectd.top);
					scroll_distance = -scroll_distance;
					if (scroll_distance > scrollable)
						scroll_distance = scrollable;
					_top_y += scroll_distance;
					_text_editor = nullptr;
					invalidate();
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
					invalidate();
				}
			}
		}

		return 0;
	}

	bool try_commit_editor()
	{
		rassert (_text_editor);
		rassert (_selected_item);

		HWND hwnd = _renderer->window().hwnd();

		try
		{
			if (auto vpi = dynamic_cast<const value_property_item_i*>(_selected_item.get()))
			{
				auto root = vpi->root();
				auto& objs = vpi->parent()->parent()->objects();
				this->change_property (objs, vpi->property(), utf16_to_utf8(_text_editor->wstr()), root->app_context());
			}
			else if (auto vcci = dynamic_cast<const value_collection_child_item_i*>(_selected_item.get()))
			{
				size_t value_index = vcci->parent()->index_of(vcci);
				auto& objs = vcci->parent()->parent()->parent()->objects();
				auto root = vcci->root();
				this->change_property (objs, vcci->parent()->property(), value_index, utf16_to_utf8(_text_editor->wstr()), root->app_context());
			}
			else
				rassert(false);
		}
		catch (const std::exception& ex)
		{
			auto message = utf8_to_utf16(ex.what());
			::MessageBox (hwnd, message.c_str(), L"Error setting property", 0);
			::SetFocus (hwnd);
			_text_editor->select_all();
			return false;
		}

		::InvalidateRect(hwnd, nullptr, TRUE);
		_text_editor = nullptr;
		return true;
	}

	virtual void change_property (const object_list_i& objects, const value_property* prop, std::string new_value_str, string_convert_context_i* scc) override final
	{
		auto pe_invoker = property_edited_e::invoker(_em);
		bool pe_has_handlers = _em->has_handlers<property_edited_e>();

		std::vector<std::string> old_values;
		if (pe_has_handlers)
		{
			old_values.reserve(objects.size());
			for (auto o : objects)
				old_values.push_back (prop->get_to_string(o, scc));
		}

		for (size_t i = 0; i < objects.size(); i++)
		{
			try
			{
				prop->set_from_string (new_value_str, objects[i], scc);
			}
			catch (const std::exception&)
			{
				for (size_t j = 0; j < i; j++)
					prop->set_from_string(old_values[j], objects[j], scc);

				throw;
			}
		}

		if (pe_has_handlers)
		{
			auto args = value_property_edited_args{ prop, objects, std::move(old_values), std::move(new_value_str), scc };
			pe_invoker.invoke(std::move(args));
		}
	}

	virtual void change_property (const object_list_i& objects, const object_property* prop, const concrete_type* type) override final
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

	virtual void change_property (const object_list_i& objects, const value_collection_property* prop, size_t value_index, std::string new_value_str, edge::string_convert_context_i* scc) override final
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

	float border_width (uint32_t dpi) const
	{
		float pw = edge::pixel_width(dpi);
		return roundf(_border_width_not_aligned / pw) * pw;
	}

	float scroll_bar_width (uint32_t dpi) const
	{
		float pw = edge::pixel_width(dpi);
		return std::round(16 / pw) * pw;
	}

	virtual float line_width (uint32_t dpi) const override
	{
		auto lt = roundf(line_width_not_aligned / edge::pixel_width(dpi)) * edge::pixel_width(dpi);
		return lt;
	}

	virtual float expand_column_left (uint32_t dpi) const override final
	{
		return _rectd.left + border_width(dpi);
	}

	virtual float name_column_left (size_t indent) const override
	{
		uint32_t dpi = edge::dpi(_renderer->window().hwnd());
		float x = _rectd.left + border_width(dpi) + indent * indent_width();
		auto pw = edge::pixel_width(dpi);
		x = roundf(x / pw) * pw;
		return x;
	}

	virtual float value_column_left (uint32_t dpi) const override final
	{
		float bw = border_width(dpi);
		float w = (_rectd.right - _rectd.left - 2 * bw) * _name_column_factor;
		if (w < 75)
			w = 75;
		float x = _rectd.left + bw + w;
		auto pw = pixel_width(dpi);
		x = roundf(x / pw) * pw;
		return x;
	}

	virtual float value_column_right (uint32_t dpi) const override final
	{
		float r = _rectd.right - border_width(dpi);
		if (_scroll_bar_visible)
			r -= scroll_bar_width(dpi);
		return r;
	}

	virtual float indent_width() const override final
	{
		float pw = edge::pixel_width(_renderer->window().hwnd());
		return std::round(8 / pw) * pw;
	}

	virtual const theme_color_provider_i* tcp() const override final { return _tcp; }

	std::optional<LRESULT> process_key_down (uint32_t key, modifier_key mks)
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

		if (_text_editor)
			return _text_editor->on_key_down (key, mks) ? std::optional<LRESULT>(0) : std::nullopt;

		return std::nullopt;
	}

	std::optional<LRESULT> process_key_up (uint32_t key, modifier_key mks)
	{
		if (_text_editor)
			return _text_editor->on_key_up (key, mks) ? std::optional<LRESULT>(0) : std::nullopt;

		return std::nullopt;
	}

	std::optional<LRESULT> process_char_key (uint32_t key)
	{
		if (_text_editor)
			return _text_editor->on_char_key (key) ? std::optional<LRESULT>(0) : std::nullopt;

		return std::nullopt;
	}

	virtual bool editing_text() const override final
	{
		return _text_editor != nullptr;
	}

	virtual RECT calc_popup_window_pos (item_i* item, float item_y, D2D1_SIZE_F client_size_requested, DWORD style, DWORD ex_style) const override final
	{
		HWND hwnd = _renderer->window().hwnd();
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
		BOOL bres = ::CalculatePopupWindowPosition (&anchor_point, &size, 0, &item_rect_pixels, &popup_window_pos); rassert(bres);
		return popup_window_pos;
	}

	virtual void expand_all() override final
	{
		for (auto& ri : _root_items)
		{
			for (size_t i = 0; i < ri->child_count(); i++)
			{
				auto child = ri->child_at(i);
				auto gi = checked_static_cast<group_item_i*>(child);
				gi->expand_all();
			}
		}
	}

	virtual creating_object_e::subscriber creating_object() override final
	{
		return creating_object_e::subscriber(*_em);
	}

	virtual item_i* selected_item() const override final { return _selected_item.get(); }

private:
	class self_clear_item_ptr
	{
		property_grid* const _pg;
		std::function<void(property_grid*)> const _on_clearing;
		item_i* _item = nullptr;

	public:
		self_clear_item_ptr(property_grid* pg, std::function<void(property_grid*)> on_clearing)
			: _pg(pg), _on_clearing(on_clearing)
		{ }

		self_clear_item_ptr(const self_clear_item_ptr&) = delete;
		self_clear_item_ptr& operator=(const self_clear_item_ptr&) = delete;

		~self_clear_item_ptr()
		{
			if (_item)
				clear();
		}

		void operator= (item_i* i)
		{
			if (_item != i)
			{
				if (_item)
					clear();

				_item = i;

				if (_item)
				{
					_item->item_removing().add_handler(&on_item_removing, this);
					_pg->invalidate();
				}
			}
		}

		item_i* get() const { return _item; }

		operator bool() const { return _item; }

		item_i* operator->() const { return _item; }

	private:
		void clear()
		{
			_on_clearing(_pg);
			_item->item_removing().remove_handler(&on_item_removing, this);
			_item = nullptr;
		}

		static void on_item_removing (void* arg, item_i*)
		{
			static_cast<self_clear_item_ptr*>(arg)->clear();
		}
	};

	void on_selected_item_changing()
	{
		_text_editor = nullptr;
		invalidate();
	}

	void on_hot_item_changing()
	{
		invalidate();
	}

	self_clear_item_ptr _selected_item;
	self_clear_item_ptr _hot_item;
};

property_grid_factory_t* const pg::property_grid_factory =
	[](auto... params) -> std::unique_ptr<property_grid_i>
	{ return std::make_unique<property_grid>(std::forward<decltype(params)>(params)...); };

root_item_i* item_i::root()
{
	auto current = this;
	while (true)
	{
		if (auto r = current->as_root())
			return r;
		rassert(current->parent());
		current = current->parent()->as_item();
	}
}

property_grid_i* item_i::grid() const
{
	return root()->grid();
}

#pragma region struct item_i
size_t item_i::indent() const
{
	return dynamic_cast<const root_item_i*>(this) ? 0 : (parent()->as_item()->indent() + 1);
}

// TODO: make this a member of property_grid_i
void item_i::render_default_background (const render_context& rc, float y, bool selected, bool hot, bool focused) const
{
	auto grid = root()->grid();
	uint32_t dpi = edge::dpi(grid->window().hwnd());
	auto lw = grid->line_width(dpi);
	float pw = edge::pixel_width(dpi);
	float height = std::ceil(this->content_height() / pw) * pw;

	D2D1_RECT_F fill_rect = { grid->expand_column_left(dpi), y, grid->value_column_right(dpi), y + height };
	if (selected)
	{
		rc.dc->FillRectangle (&fill_rect, focused ? rc.selected_back_focused.get() : rc.selected_back_not_focused.get());
	}
	else
	{
		auto brush = hot ? rc.item_gradient_brush_hot.get() : rc.item_gradient_brush.get();
		brush->SetStartPoint ({ fill_rect.left, fill_rect.top });
		brush->SetEndPoint ({ fill_rect.left, fill_rect.bottom });
		rc.dc->FillRectangle (&fill_rect, brush);
	}

	float name_line_x = grid->expand_column_left(dpi) + indent() * grid->indent_width();
	rc.dc->DrawLine ({ name_line_x + lw / 2, y }, { name_line_x + lw / 2, y + height }, rc.disabled_fore, lw);
	float linex = grid->value_column_left(dpi) + lw / 2;
	rc.dc->DrawLine ({ linex, y }, { linex, y + height }, rc.disabled_fore, lw);
}
#pragma endregion

#pragma region struct expandable_item_i
void expandable_item_i::render_expand_button (const render_context& rc, float item_y) const
{
	auto grid = this->as_item()->grid();
	uint32_t dpi = edge::dpi(grid->window().hwnd());
	float pw = edge::pixel_width(dpi);
	float height = std::ceil(this->as_item()->content_height() / pw) * pw;
	rassert(height > 0);
	float name_line_x = grid->name_column_left(as_item()->indent());

	edge::com_ptr<ID2D1Factory> f;
	rc.dc->GetFactory(&f);
	edge::com_ptr<ID2D1StrokeStyle> ss;
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

void expandable_item_i::expand_all()
{
	if (!expanded())
		expand();

	for (size_t i = 0; i < this->child_count(); i++)
	{
		auto c = this->child_at(i);
		if (auto ei = dynamic_cast<expandable_item_i*>(c))
			ei->expand_all();
	}
}
#pragma endregion

#pragma region property_item_i
edge::text_layout_with_metrics property_item_i::make_name_layout() const
{
	auto grid = root()->grid();
	uint32_t dpi = edge::dpi(grid->window().hwnd());
	float name_layout_width = grid->value_column_left(dpi) - grid->name_column_left(indent()) - grid->line_width(dpi) - 2 * text_lr_padding;
	if (name_layout_width <= 0)
		return { };

	return text_layout_with_metrics (grid->renderer()->dwrite_factory(), grid->text_format(), property()->name(), name_layout_width);
}
#pragma endregion

#pragma region value_property_item_i
value_layout_t value_property_item_i::make_value_layout() const
{
	auto root = this->root();
	auto grid = root->grid();
	uint32_t dpi = edge::dpi(grid->window().hwnd());

	float width = grid->value_column_right(dpi) - grid->value_column_left(dpi) - grid->line_width(dpi) - 2 * text_lr_padding;
	if (width <= 0)
		return { };

	auto factory = grid->renderer()->dwrite_factory();
	auto& objs = parent()->parent()->objects();
	bool changed_from_default = objs.any ([p=property()](object* o) { return p->changed_from_default(o); });
	auto format = changed_from_default ? grid->bold_text_format() : grid->text_format();

	text_layout_with_metrics tl;
	value_layout_t::read_state state;
	try
	{
		bool multiple_values = objs.any(1, objs.size(), [p=property(), first=objs[0]](object* o) { return !p->equal(first, o); });
		if (multiple_values)
		{
			tl = text_layout_with_metrics (factory, format, "(multiple values)", width);
			state = value_layout_t::read_state::multiple_values;
		}
		else
		{
			auto str = property()->get_to_string(objs.front(), root->app_context());
			tl = text_layout_with_metrics (factory, format, str, width);
			state = value_layout_t::read_state::ok;
		}
	}
	catch (const std::exception& ex)
	{
		tl = text_layout_with_metrics (factory, format, ex.what(), width);
		state = value_layout_t::read_state::read_exception;
	}

	return { std::move(tl), state };
}
#pragma endregion


using namespace pg;

extern std::unique_ptr<value_property_item_i> make_value_property_item (group_item_i* parent, const edge::value_property* prop);
extern std::unique_ptr<property_item_i> make_object_property_item (group_item_i* parent, const edge::object_property* prop);
extern std::unique_ptr<property_item_i> make_value_collection_item (group_item_i* parent, const edge::value_collection_property* prop);
extern std::unique_ptr<property_item_i> make_object_collection_item (group_item_i* parent, const edge::object_collection_property* prop);

std::unique_ptr<property_item_i> make_property_item (group_item_i* parent, const edge::property* prop)
{
	if (auto custom_item_prop = dynamic_cast<const custom_item_property_i*>(prop))
		return custom_item_prop->create_item(parent, prop);

	if (auto value_prop = dynamic_cast<const value_property*>(prop))
		return make_value_property_item(parent, value_prop);

	if (auto obj_coll_prop = dynamic_cast<const object_collection_property*>(prop))
		return make_object_collection_item(parent, obj_coll_prop);

	if (auto obj_prop = dynamic_cast<const object_property*>(prop))
		return make_object_property_item(parent, obj_prop);

	if (auto value_coll_prop = dynamic_cast<const value_collection_property*>(prop))
		return make_value_collection_item(parent, value_coll_prop);

	// TODO: placeholder pg item for unknown types of properties
	rassert(false); return nullptr;
}
