
#include "pch.h"
#include "property_grid.h"
#include "property_grid_items.h"
#include "utility_functions.h"
#include "d2d_window.h"

using namespace edge;

static constexpr UINT WM_CLOSE_POPUP = WM_APP + 1;
static constexpr UINT WM_WORK        = WM_APP + 2;

static constexpr float indent_width = 15;
static constexpr float line_thickness_not_aligned = 0.6f;
static constexpr float separator_height = 4;
static constexpr float description_min_height = 20;

namespace edge
{
	class property_grid;
}

#pragma warning (disable: 4250)

class edge::property_grid : d2d_window, public virtual property_grid_i
{
	using base = d2d_window;

	com_ptr<IDWriteTextFormat> _textFormat;
	com_ptr<IDWriteTextFormat> _boldTextFormat;
	com_ptr<IDWriteTextFormat> _wingdings;
	std::unique_ptr<text_editor_i> _text_editor;
	float _name_column_factor = 0.5f;
	float _pixel_width;
	float _line_thickness;
	float _description_height = 120;
	std::vector<std::unique_ptr<root_item>> _root_items;
	pgitem* _selected_item = nullptr;
	std::optional<float> _description_resize_offset;

	std::queue<std::function<void()>> _workQueue;

	// ========================================================================

public:
	property_grid (HINSTANCE hInstance, DWORD exStyle, const RECT& rect, HWND hWndParent, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory)
		: base (hInstance, exStyle, WS_CHILD | WS_VISIBLE, rect, hWndParent, 0, deviceContext, dWriteFactory)
	{
		auto hr = dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
												   DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_textFormat); assert(SUCCEEDED(hr));

		hr = dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
											  DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_boldTextFormat); assert(SUCCEEDED(hr));

		hr = dWriteFactory->CreateTextFormat (L"Wingdings", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
											  DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_wingdings); assert(SUCCEEDED(hr));

		recalc_pixel_width_and_line_thickness();
	}

	void recalc_pixel_width_and_line_thickness()
	{
		_pixel_width = GetDipSizeFromPixelSize ({ 1, 0 }).width;
		_line_thickness = roundf(line_thickness_not_aligned / _pixel_width) * _pixel_width;
	}


	virtual IDWriteFactory* dwrite_factory() const override final { return base::dwrite_factory(); }

	virtual IDWriteTextFormat* text_format() const override final { return _textFormat; }

	virtual void invalidate() override final
	{
		base::invalidate();
	}

	virtual std::optional<LRESULT> window_proc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		auto resultBaseClass = base::window_proc (hwnd, msg, wParam, lParam);

		if (msg == WM_GETDLGCODE)
		{
			if (_text_editor)
				return DLGC_WANTALLKEYS;

			return resultBaseClass;
		}

		if (msg == WM_SIZE)
		{
			if (!_root_items.empty())
			{
				discard_editor();
				for (auto& ri : _root_items)
					create_text_layouts(ri.get());
			}

			return 0;
		}

		if (msg == 0x02E3) // WM_DPICHANGED_AFTERPARENT
		{
			recalc_pixel_width_and_line_thickness();
			if (!_root_items.empty())
			{
				discard_editor();
				for (auto& ri : _root_items)
					create_text_layouts(ri.get());
			}
			return 0;
		}

		if ((msg == WM_SETFOCUS) || (msg == WM_KILLFOCUS))
		{
			::InvalidateRect (hwnd, nullptr, 0);
			return 0;
		}

		if (((msg == WM_LBUTTONDOWN) || (msg == WM_RBUTTONDOWN))
			|| ((msg == WM_LBUTTONUP) || (msg == WM_RBUTTONUP)))
		{
			auto button = ((msg == WM_LBUTTONDOWN) || (msg == WM_LBUTTONUP)) ? mouse_button::left : mouse_button::right;
			auto pt = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			auto dip = pointp_to_pointd(pt) + D2D1_SIZE_F{ _pixel_width / 2, _pixel_width / 2 };
			if ((msg == WM_LBUTTONDOWN) || (msg == WM_RBUTTONDOWN))
				process_mouse_button_down (button, (UINT)wParam, pt, dip);
			else
				process_mouse_button_up (button, (UINT)wParam, pt, dip);
			return 0;
		}

		if (msg == WM_MOUSEMOVE)
		{
			UINT modifiers = (UINT)wParam;
			if (::GetKeyState(VK_MENU) < 0)
				modifiers |= MK_ALT;
			auto pt = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			auto dip = pointp_to_pointd(pt) + D2D1_SIZE_F{ _pixel_width / 2, _pixel_width / 2 };
			process_mouse_move (modifiers, pt, dip);
		}

		if ((msg == WM_KEYDOWN) || (msg == WM_SYSKEYDOWN))
		{
			auto handled = process_virtual_key_down ((UINT) wParam, GetModifierKeys());
			if (handled == handled::yes)
				return 0;

			return std::nullopt;
		}

		if ((msg == WM_KEYUP) || (msg == WM_SYSKEYUP))
		{
			auto handled = process_virtual_key_up ((UINT) wParam, GetModifierKeys());
			if (handled == handled::yes)
				return 0;

			return std::nullopt;
		}

		if (msg == WM_CHAR)
		{
			if (_text_editor)
			{
				auto handled = _text_editor->process_character_key ((uint32_t) wParam);
				if (handled == handled::yes)
					return 0;
				else
					return std::nullopt;
			}

			return std::nullopt;
		}

		if (msg == WM_SETCURSOR)
		{
			if (_description_resize_offset)
			{
				SetCursor (LoadCursor(nullptr, IDC_SIZENS));
				return TRUE;
			}

			if (((HWND) wParam == hwnd) && (LOWORD (lParam) == HTCLIENT))
			{
				// Let's check the result because GetCursorPos fails when the input desktop is not the current desktop
				// (happens for example when the monitor goes to sleep and then the lock screen is displayed).
				POINT pt;
				if (::GetCursorPos (&pt))
				{
					if (ScreenToClient (hwnd, &pt))
					{
						auto dip = pointp_to_pointd(pt.x, pt.y) + D2D1_SIZE_F{ _pixel_width / 2, _pixel_width / 2 };
						process_wm_setcursor({ pt.x, pt.y }, dip);
						return TRUE;
					}
				}
			}

			return std::nullopt;
		}

		if (msg == WM_WORK)
		{
			_workQueue.front()();
			_workQueue.pop();
			return 0;
		}

		return resultBaseClass;
	}

	void process_wm_setcursor (POINT pt, D2D1_POINT_2F dip)
	{
		HCURSOR cursor = nullptr;

		if ((_description_height > separator_height) && point_in_rect(description_separator_rect(), dip))
		{
			SetCursor(LoadCursor(nullptr, IDC_SIZENS));
			return;
		}

		auto item = item_at(dip);
		if (item.first != nullptr)
		{
			if (dip.x >= item.second.x_value)
				cursor = item.first->cursor();
		}

		if (cursor == nullptr)
			cursor = ::LoadCursor (nullptr, IDC_ARROW);

		::SetCursor(cursor);
	}

	void enum_items (const std::function<void(pgitem*, const item_layout&, bool& cancel)>& callback) const
	{
		std::function<void(pgitem* item, float& y, size_t indent, bool& cancel)> enum_items_inner;

		enum_items_inner = [this, &enum_items_inner, &callback, vcx=value_column_x()](pgitem* item, float& y, size_t indent, bool& cancel)
		{
			auto item_height = item->content_height();

			auto horz_line_y = y + item_height;
			horz_line_y = ceilf (horz_line_y / _pixel_width) * _pixel_width;

			item_layout il;
			il.y_top = y;
			il.y_bottom = horz_line_y;
			il.x_left = 0;
			il.x_name = indent * indent_width;
			il.x_value = vcx;
			il.x_right = client_width();

			callback(item, il, cancel);
			if (cancel)
				return;

			y = horz_line_y + _line_thickness;

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

		float y = 0;
		bool cancel = false;
		for (auto& root_item : _root_items)
		{
			enum_items_inner (root_item.get(), y, 0, cancel);
			if (cancel)
				break;
		}
	}

	void create_text_layouts (root_item* ri)
	{
		auto vcx = value_column_x();
		auto vcw = std::max (75.0f, client_width() - vcx);

		std::function<void(pgitem* item, size_t indent)> create_inner;

		create_inner = [&create_inner, this, vcx, vcw] (pgitem* item, size_t indent)
		{
			item_layout_horz il;
			il.x_left = 0;
			il.x_name = indent * indent_width;
			il.x_value = vcx;
			il.x_right = client_width();

			item->create_text_layouts (dwrite_factory(), _textFormat, il, _line_thickness);

			if (auto ei = dynamic_cast<expandable_item*>(item); ei && ei->expanded())
			{
				for (auto& child : ei->children())
					create_inner (child.get(), indent + 1);
			}
		};

		create_inner (ri, 0);

		invalidate();
	}

	virtual void render (ID2D1DeviceContext* dc) const override
	{
		dc->Clear(GetD2DSystemColor(COLOR_WINDOW));

		auto tr = dpi_transform();
		dc->SetTransform ({ (float)tr._11, (float)tr._12, (float)tr._21, (float)tr._22, (float)tr._31, (float)tr._32 });

		if (_root_items.empty())
		{
			auto tl = text_layout::create (dwrite_factory(), _textFormat, "(no selection)");
			D2D1_POINT_2F p = { client_width() / 2 - tl.metrics.width / 2, client_height() / 2 - tl.metrics.height / 2};
			com_ptr<ID2D1SolidColorBrush> brush;
			dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOWTEXT), &brush);
			dc->DrawTextLayout (p, tl.layout, brush);
			return;
		}

		bool focused = GetFocus() == hwnd();

		render_context rc;
		rc.dc = dc;
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOW), &rc.back_brush);
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOWTEXT), &rc.fore_brush);
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_MENUHILIGHT), &rc.selected_back_brush_focused);
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_MENU), &rc.selected_back_brush_not_focused);
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOWTEXT), &rc.selected_fore_brush);
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_GRAYTEXT), &rc.disabled_fore_brush);

		enum_items ([&, this](pgitem* item, const item_layout& layout, bool& cancel)
		{
			bool selected = (item == _selected_item);
			item->render (rc, layout, _line_thickness, selected, focused);

			if (selected && _text_editor)
				_text_editor->render(dc);

			if (layout.y_bottom + _line_thickness >= client_height())
				cancel = true;

			D2D1_POINT_2F p0 = { 0, layout.y_bottom + _line_thickness / 2 };
			D2D1_POINT_2F p1 = { client_width(), layout.y_bottom + _line_thickness / 2 };
			dc->DrawLine (p0, p1, rc.disabled_fore_brush, _line_thickness);
		});

		if (_description_height > separator_height)
		{
			auto separator_color = interpolate(GetD2DSystemColor(COLOR_WINDOW), GetD2DSystemColor (COLOR_WINDOWTEXT), 80);
			com_ptr<ID2D1SolidColorBrush> brush;
			dc->CreateSolidColorBrush (separator_color, &brush);
			dc->FillRectangle(description_separator_rect(), brush);
			if (auto value_item = dynamic_cast<value_pgitem*>(_selected_item))
			{
				auto desc_rect = description_rect();
				float lr_padding = 3;
				std::stringstream ss;
				ss << value_item->_prop->_name << " (" << value_item->_prop->type_name() << ")";
				auto title_layout = text_layout::create(dwrite_factory(), _boldTextFormat, ss.str(), client_width() - 2 * lr_padding);
				dc->DrawTextLayout({ desc_rect.left + lr_padding, desc_rect.top }, title_layout.layout, rc.fore_brush);

				if (value_item->_prop->_description)
				{
					auto desc_layout = text_layout::create(dwrite_factory(), _textFormat, value_item->_prop->_description, client_width() - 2 * lr_padding);
					dc->DrawTextLayout({ desc_rect.left + lr_padding, desc_rect.top + title_layout.metrics.height }, desc_layout.layout, rc.fore_brush);
				}
			}
		}
	}

	D2D1_RECT_F description_rect() const
	{
		assert(_description_height > separator_height);
		D2D1_RECT_F rect = { 0, client_height() - _description_height + separator_height, client_width(), client_height() };
		rect = align_to_pixel (rect, dpi());
		return rect;
	}

	D2D1_RECT_F description_separator_rect() const
	{
		assert(_description_height > separator_height);
		D2D1_RECT_F separator = { 0, client_height() - _description_height, client_width(), client_height() - _description_height + separator_height };
		separator = align_to_pixel (separator, dpi());
		return separator;
	}

	float value_column_x() const
	{
		float w = client_width() * _name_column_factor;
		w = roundf (w / _pixel_width) * _pixel_width;
		return std::max (75.0f, w);
	}

	void discard_editor()
	{
		if (_text_editor)
		{
			base::invalidate (_text_editor->rect());
			_text_editor = nullptr;
		}
	}
	
	virtual void clear() override
	{
		discard_editor();
		_selected_item = nullptr;
		_root_items.clear();
		invalidate();
	}

	virtual void add_section (const char* heading, object* const* objects, size_t size) override
	{
		_root_items.push_back (std::make_unique<root_item>(this, heading, objects, size));
		create_text_layouts(_root_items.back().get());
		invalidate();
	}

	virtual void set_description_height (float height) override
	{
		_description_height = height;
		invalidate();
	}

	virtual property_changed_e::subscriber property_changed() override { return property_changed_e::subscriber(this); }

	virtual description_height_changed_e::subscriber description_height_changed() override { return description_height_changed_e::subscriber(this); }

	virtual text_editor_i* show_text_editor (const D2D1_RECT_F& rect, float lr_padding, std::string_view str) override final
	{
		discard_editor();
		uint32_t fill_argb = 0xFF00'0000u | GetSysColor(COLOR_WINDOW);
		uint32_t text_argb = 0xFF00'0000u | GetSysColor(COLOR_WINDOWTEXT);
		_text_editor = text_editor_factory (this, dwrite_factory(), _textFormat, fill_argb, text_argb, rect, lr_padding, str);
		return _text_editor.get();
	}

	virtual int show_enum_editor (D2D1_POINT_2F dip, const NVP* nameValuePairs) override final
	{
		discard_editor();
		POINT ptScreen = pointd_to_pointp(dip, 0);
		::ClientToScreen (hwnd(), &ptScreen);
		
		HINSTANCE hInstance = (HINSTANCE) GetWindowLongPtr (hwnd(), GWLP_HINSTANCE);

		static constexpr wchar_t ClassName[] = L"GIGI-{655C4EA9-2A80-46D7-A7FB-D510A32DC6C6}";
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

		auto hwnd = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, ClassName, L"aaa", WS_POPUP | WS_BORDER, 0, 0, 0, 0, this->hwnd(), nullptr, hInstance, nullptr); assert (hwnd != nullptr);

		LONG maxTextWidth = 0;
		LONG maxTextHeight = 0;

		NONCLIENTMETRICS ncMetrics = { sizeof(NONCLIENTMETRICS) };
		auto proc_addr = GetProcAddress(GetModuleHandleA("user32.dll"), "SystemParametersInfoForDpi");
		if (proc_addr != nullptr)
		{
			auto proc = reinterpret_cast<BOOL(WINAPI*)(UINT, UINT, PVOID, UINT, UINT)>(proc_addr);
			proc(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncMetrics, 0, dpi());
		}
		else
			SystemParametersInfo (SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncMetrics, 0);
		HFONT_unique_ptr font (CreateFontIndirect (&ncMetrics.lfMenuFont));

		auto hdc = ::GetDC(hwnd);
		auto oldFont = ::SelectObject (hdc, font.get());
		size_t count;
		for (count = 0; nameValuePairs[count].first != nullptr; count++)
		{
			RECT rc = { };
			DrawTextA (hdc, nameValuePairs[count].first, -1, &rc, DT_CALCRECT);
			maxTextWidth = std::max (maxTextWidth, rc.right);
			maxTextHeight = std::max (maxTextHeight, rc.bottom);
		}
		::SelectObject (hdc, oldFont);
		::ReleaseDC (hwnd, hdc);

		int lrpadding = 7 * dpi() / 96;
		int udpadding = ((count <= 5) ? 5 : 0) * dpi() / 96;
		LONG buttonWidth = std::max (100l * (LONG)dpi() / 96, maxTextWidth + 2 * lrpadding) + 2 * GetSystemMetrics(SM_CXEDGE);
		LONG buttonHeight = maxTextHeight + 2 * udpadding + 2 * GetSystemMetrics(SM_CYEDGE);

		int margin = 4 * dpi() / 96;
		int spacing = 2 * dpi() / 96;
		int y = margin;
		for (size_t nvp_index = 0; nameValuePairs[nvp_index].first != nullptr;)
		{
			constexpr DWORD dwStyle = WS_CHILD | WS_VISIBLE | BS_NOTIFY | BS_FLAT;
			auto button = CreateWindowExA (0, "Button", nameValuePairs[nvp_index].first, dwStyle, margin, y, buttonWidth, buttonHeight, hwnd, (HMENU) nvp_index, hInstance, nullptr);
			::SendMessage (button, WM_SETFONT, (WPARAM) font.get(), FALSE);
			nvp_index++;
			y += buttonHeight + (nameValuePairs[nvp_index].first ? spacing : margin);
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

	std::pair<pgitem*, item_layout> item_at (D2D1_POINT_2F dip) const
	{
		std::pair<pgitem*, item_layout> result = { };

		enum_items ([&](pgitem* item, const item_layout& layout, bool& cancel)
		{
			if (dip.y < layout.y_bottom)
			{
				result.first = item;
				result.second = layout;
				cancel = true;
			}
		});

		return result;
	}
	
	void process_mouse_button_down (mouse_button button, UINT modifier_keys, POINT pixel, D2D1_POINT_2F dip)
	{
		::SetFocus (hwnd());

		if (_description_height > separator_height)
		{
			auto ds_rect = description_separator_rect();
			if (point_in_rect(ds_rect, dip))
			{
				discard_editor();
				_description_resize_offset = dip.y - ds_rect.top;
				return;
			}

			if (dip.y >= ds_rect.bottom)
				return;
		}

		if (_text_editor && point_in_rect(_text_editor->rect(), dip))
			return _text_editor->process_mouse_button_down(button, modifier_keys, pixel, dip);

		auto clicked_item = item_at(dip);

		auto new_selected_item = (clicked_item.first && clicked_item.first->selectable()) ? clicked_item.first : nullptr;
		if (_selected_item != new_selected_item)
		{
			discard_editor();
			_selected_item = new_selected_item;
			invalidate();
		}

		if (clicked_item.first != nullptr)
			clicked_item.first->process_mouse_button_down (button, modifier_keys, pixel, dip, clicked_item.second);
	}

	void process_mouse_button_up (mouse_button button, UINT modifier_keys, POINT pixel, D2D1_POINT_2F dip)
	{
		if (_description_resize_offset)
		{
			_description_resize_offset.reset();
			event_invoker<description_height_changed_e>()(_description_height);
			return;
		}

		if (_text_editor && point_in_rect(_text_editor->rect(), dip))
			return _text_editor->process_mouse_button_up (button, modifier_keys, pixel, dip);

		auto clicked_item = item_at(dip);
		if (clicked_item.first != nullptr)
			clicked_item.first->process_mouse_button_up (button, modifier_keys, pixel, dip, clicked_item.second);
	}

	void process_mouse_move (UINT modifier_keys, POINT pixel, D2D1_POINT_2F dip)
	{
		if (_description_resize_offset)
		{
			_description_height = client_height() - dip.y + _description_resize_offset.value();
			_description_height = std::max (description_min_height, _description_height);
			invalidate();
			return;
		}

		if (_text_editor && point_in_rect(_text_editor->rect(), dip))
			return _text_editor->process_mouse_move (modifier_keys, pixel, dip);
	}

	void try_commit_editor()
	{
		if (_text_editor == nullptr)
			return;

		auto prop_item = dynamic_cast<value_pgitem*>(_selected_item); assert(prop_item);
		bool changed = try_change_property (prop_item->parent()->objects(), prop_item->_prop, _text_editor->u8str().c_str());
		if (!changed)
		{
			std::wstringstream ss;
			ss << _text_editor->wstr() << " is not a valid value for the " << prop_item->_prop->_name << " property.";
			::MessageBox (hwnd(), ss.str().c_str(), L"aaa", 0);
			::SetFocus (hwnd());
			_text_editor->select_all();
			return;
		}

		base::invalidate (_text_editor->rect());
		_text_editor = nullptr;
	}

	virtual bool try_change_property (const std::vector<object*>& objects, const value_property* prop, std::string_view new_value_str) override final
	{
		std::vector<std::string> old_values;
		old_values.reserve(objects.size());
		for (auto o : objects)
			old_values.push_back (prop->get_to_string(o));

		for (size_t i = 0; i < objects.size(); i++)
		{
			bool set_ok = prop->try_set_from_string(objects[i], new_value_str);
			if (!set_ok)
			{
				for (size_t j = 0; j < i; j++)
					prop->try_set_from_string(objects[j], old_values[j]);

				return false;
			}
		}

		property_changed_args args = { objects, std::move(old_values), std::string(new_value_str) };
		this->event_invoker<property_changed_e>()(std::move(args));
		return true;
	}

	virtual float line_thickness() const override final { return _line_thickness; }

	handled process_virtual_key_down (UINT key, UINT modifier_keys)
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

			return handled::yes;
		}
		
		if ((key == VK_ESCAPE) && _text_editor)
		{
			discard_editor();
			return handled::yes;
		}

		if (_text_editor)
			return _text_editor->process_virtual_key_down (key, modifier_keys);

		return handled::no;
	}

	handled process_virtual_key_up (UINT key, UINT modifier_keys)
	{
		if (_text_editor)
			return _text_editor->process_virtual_key_up (key, modifier_keys);

		return handled::no;
	}
};

extern edge::property_grid_factory_t* const edge::property_grid_factory =
	[](auto... params) -> std::unique_ptr<property_grid_i>
	{ return std::make_unique<property_grid>(std::forward<decltype(params)>(params)...); };
