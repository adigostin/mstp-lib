
#include "pch.h"
#include "property_grid.h"
#include "property_grid_items.h"
#include "utility_functions.h"
#include "d2d_window.h"
#include "text_editor.h"

using namespace edge;

static constexpr UINT WM_CLOSE_POPUP = WM_APP + 1;
static constexpr UINT WM_WORK        = WM_APP + 2;

static constexpr D2D1_RECT_F title_text_padding = { 4, 1, 4, 1 };
static constexpr float indent_width = 15;
static constexpr float font_size = 12;

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
	com_ptr<IDWriteTextFormat> _titleTextFormat;
	std::unique_ptr<text_editor_i> _text_editor;
	float _nameColumnSize = 0.5f;
	float _pixel_width;
	float _line_thickness;
	std::unique_ptr<root_item> _root_item;
	pgitem* _selected_item = nullptr;
	text_layout _title_layout;

	std::queue<std::function<void()>> _workQueue;

	// ========================================================================

public:
	property_grid (HINSTANCE hInstance, const RECT& rect, HWND hWndParent, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory)
		: base (hInstance, 0, WS_CHILD | WS_VISIBLE, rect, hWndParent, 0, deviceContext, dWriteFactory)
	{
		auto hr = dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
												   DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_textFormat); assert(SUCCEEDED(hr));

		hr = dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
											  DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_boldTextFormat); assert(SUCCEEDED(hr));

		hr = dWriteFactory->CreateTextFormat (L"Wingdings", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
											  DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_wingdings); assert(SUCCEEDED(hr));

		hr = dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
											  DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-US", &_titleTextFormat); assert(SUCCEEDED(hr));

		recalc_pixel_width_and_line_thickness();
	}

	void recalc_pixel_width_and_line_thickness()
	{
		_pixel_width = GetDipSizeFromPixelSize ({ 1, 0 }).width;
		_line_thickness = roundf(1.0f / _pixel_width) * _pixel_width;
	}


	virtual IDWriteFactory* dwrite_factory() const override final { return base::dwrite_factory(); }

	virtual IDWriteTextFormat* text_format() const override final { return _textFormat; }

	virtual float value_text_width() const override final
	{
		float ncw = name_column_width();
		float left = ncw + _line_thickness / 2;
		float right = client_width();
		float w = right - left - 2 * text_lr_padding;
		return w > 0 ? w : 0;
	}

	virtual void perform_layout() override final
	{
		assert(false); // not implemented
		this->invalidate();
	}

	virtual void invalidate() override final
	{
		base::invalidate();
	}

	virtual std::optional<LRESULT> window_proc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		auto resultBaseClass = base::window_proc (hwnd, msg, wParam, lParam);

		if (msg == 0x02E3) // WM_DPICHANGED_AFTERPARENT
		{
			recalc_pixel_width_and_line_thickness();
			return 0;
		}

		if (msg == WM_GETDLGCODE)
		{
			if (_text_editor)
				return DLGC_WANTALLKEYS;

			return resultBaseClass;
		}

		if (msg == WM_SIZE)
		{
			if (_root_item)
				create_text_layouts();

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

		auto item = item_at(dip);
		if (item.first != nullptr)
		{
			auto offset = dip - item.second.location;
			if (offset.width >= name_column_width())
				cursor = item.first->cursor (offset);
		}

		if (cursor == nullptr)
			cursor = ::LoadCursor (nullptr, IDC_ARROW);

		::SetCursor(cursor);
	}

	void enum_items (const std::function<void(pgitem*, const item_layout&, bool& cancel)>& callback) const
	{
		std::function<void(const std::vector<std::unique_ptr<pgitem>>& items, float& y, size_t indent, bool& cancel)> enum_items_inner;

		enum_items_inner = [this, &enum_items_inner, &callback](const std::vector<std::unique_ptr<pgitem>>& items, float y, size_t indent, bool& cancel)
		{
			float ncw = name_column_width();
			for (auto& item : items)
			{
				auto item_height = item->text_height();
				item_layout ii;
			
				auto horz_line_y = y + item_height;
				horz_line_y = ceilf (horz_line_y / _pixel_width) * _pixel_width;

				ii.location = { 0, y };
				ii.name_rect = { 0, y, ncw - _line_thickness / 2, horz_line_y };
				ii.value_rect = { ncw + _line_thickness / 2, y, client_width(), horz_line_y };

				callback(item.get(), ii, cancel);
				if (cancel)
					break;
		
				y = horz_line_y + _line_thickness;

				if (auto ei = dynamic_cast<expandable_item*>(item.get()); (ei != nullptr) && ei->expanded())
					enum_items_inner (ei->children(), y, indent + 1, cancel);
			}
		};

		float y = title_height();
		bool cancel = false;
		enum_items_inner (_root_item->children(), y, 0, cancel);
	}

	void create_text_layouts() const
	{
		auto ncw = name_column_width();
		auto value_width = std::max (0.0f, value_column_width() - 2 * text_lr_padding);

		std::function<void(const std::vector<std::unique_ptr<pgitem>>& items, size_t indent)> create_inner;

		create_inner = [&create_inner, this, ncw, value_width] (const std::vector<std::unique_ptr<pgitem>>& items, size_t indent)
		{
			auto name_width = std::max(0.0f, ncw - indent * indent_width - 2 * text_lr_padding);

			for (auto& item : items)
			{
				item->create_text_layouts (dwrite_factory(), _textFormat, name_width, value_width);

				if (auto ei = dynamic_cast<expandable_item*>(item.get()); (ei != nullptr) && ei->expanded())
					create_inner (ei->children(), indent + 1);
			}
		};

		create_inner (_root_item->children(), 0);
	}

	float title_height() const
	{
		float h = 0;
		if (_title_layout.layout != nullptr)
		{
			h = title_text_padding.top + _title_layout.metrics.height + title_text_padding.bottom;
			h = ceilf (h / _pixel_width) * _pixel_width;
		}

		return h;
	}

	virtual void render (ID2D1DeviceContext* dc) const override
	{
		dc->Clear(GetD2DSystemColor(COLOR_WINDOW));

		auto tr = dpi_transform();
		dc->SetTransform ({ (float)tr._11, (float)tr._12, (float)tr._21, (float)tr._22, (float)tr._31, (float)tr._32 });

		if (!_root_item)
		{
			auto tl = text_layout::create (dwrite_factory(), _textFormat, "(no selection)", -1);
			D2D1_POINT_2F p = { client_width() / 2 - tl.metrics.width / 2, client_height() / 2 - tl.metrics.height / 2};
			com_ptr<ID2D1SolidColorBrush> brush;
			dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOWTEXT), &brush);
			dc->DrawTextLayout (p, tl.layout, brush);
			return;
		}

		bool focused = GetFocus() == hwnd();

		if (_title_layout.layout != nullptr)
		{
			com_ptr<ID2D1SolidColorBrush> brush;
			dc->CreateSolidColorBrush (GetD2DSystemColor(COLOR_ACTIVECAPTION), &brush);
			dc->FillRectangle ({ 0, 0, client_width(), title_height() }, brush);
			brush->SetColor (GetD2DSystemColor(COLOR_CAPTIONTEXT));
			dc->DrawTextLayout ({ title_text_padding.left, title_text_padding.top }, _title_layout.layout, brush);
		}

		render_context rc;
		rc.dc = dc;
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOW), &rc.back_brush);
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOWTEXT), &rc.fore_brush);
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_MENUHILIGHT), &rc.selected_back_brush_focused);
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_MENU), &rc.selected_back_brush_not_focused);
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOWTEXT), &rc.selected_fore_brush);
		dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_GRAYTEXT), &rc.disabled_fore_brush);

		float ncw = name_column_width();
		float bottom = 0;
		enum_items ([&](pgitem* item, const item_layout& layout, bool& cancel)
		{
			bool selected = (item == _selected_item);
			item->render_name (rc, layout, selected, focused);

			if (selected && _text_editor)
				_text_editor->render(dc);
			else
				item->render_value (rc, layout, selected, focused);

			bottom = layout.name_rect.bottom + _line_thickness;

			if (layout.name_rect.bottom >= client_height())
				cancel = true;

			D2D1_POINT_2F p0 = { 0, layout.name_rect.bottom + _line_thickness / 2 };
			D2D1_POINT_2F p1 = { client_width(), layout.name_rect.bottom + _line_thickness / 2 };
			dc->DrawLine (p0, p1, rc.disabled_fore_brush, _line_thickness);
		});

		if (bottom > title_height())
			dc->DrawLine ({ ncw, title_height() }, { ncw, bottom }, rc.disabled_fore_brush, _line_thickness);
	}

	float name_column_width() const
	{
		float w = std::max (100.0f, client_width()) * _nameColumnSize;
		w = roundf (w / _pixel_width) * _pixel_width;
		return w;
	}

	float value_column_width() const
	{
		return this->client_width() - name_column_width();
	}

	void discard_editor()
	{
		if (_text_editor)
		{
			base::invalidate (_text_editor->rect());
			_text_editor = nullptr;
		}
	}

	virtual preferred_height_changed_e::subscriber preferred_height_changed() override { return preferred_height_changed_e::subscriber(this); }

	virtual LONG preferred_height_pixels() const override
	{
		float bottom = 0;
		enum_items([&bottom, this](pgitem* item, const item_layout& layout, bool& cancel) { bottom = layout.value_rect.bottom + _line_thickness; } );
		auto size_pixels = pointd_to_pointp ({ 0, bottom }, +1);
		return size_pixels.y;
	}

	void set_title (std::string_view title) override
	{
		if (title.empty())
			_title_layout.layout = nullptr;
		else
		{
			auto max_width = client_width() - title_text_padding.left - title_text_padding.right;
			_title_layout = text_layout::create(base::dwrite_factory(), _titleTextFormat, title, max_width);
		}
		invalidate();
	}

	void select_objects (object* const* objects, size_t size) final
	{
		discard_editor();
		_selected_item = nullptr;
		_root_item.reset (new root_item(this, objects, size));
		create_text_layouts();
		::InvalidateRect (hwnd(), NULL, FALSE);
	}

	property_changed_e::subscriber property_changed() override
	{
		return property_changed_e::subscriber(this);
	}

	int ShowEditor (POINT ptScreen, const NVP* nameValuePairs)
	{
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
		int dpiX = GetDeviceCaps (hdc, LOGPIXELSX);
		int dpiY = GetDeviceCaps (hdc, LOGPIXELSY);
		::ReleaseDC (hwnd, hdc);

		int lrpadding = 7 * dpiX / 96;
		int udpadding = ((count <= 5) ? 5 : 0) * dpiY / 96;
		LONG buttonWidth = std::max (100l * dpiX / 96, maxTextWidth + 2 * lrpadding) + 2 * GetSystemMetrics(SM_CXEDGE);
		LONG buttonHeight = maxTextHeight + 2 * udpadding + 2 * GetSystemMetrics(SM_CYEDGE);

		int margin = 4 * dpiX / 96;
		int spacing = 2 * dpiX / 96;
		int y = margin;
		for (auto nvp = nameValuePairs; nvp->first != nullptr;)
		{
			auto button = CreateWindowExA (0, "Button", nvp->first, WS_CHILD | WS_VISIBLE | BS_NOTIFY | BS_FLAT, margin, y, buttonWidth, buttonHeight, hwnd, (HMENU) (INT_PTR) nvp->second, hInstance, nullptr);
			::SendMessage (button, WM_SETFONT, (WPARAM) font.get(), FALSE);

			nvp++;
			y += buttonHeight + ((nvp->first != nullptr) ? spacing : margin);
		}
		RECT wr = { 0, 0, margin + buttonWidth + margin, y };
		::AdjustWindowRectEx (&wr, (DWORD) GetWindowLongPtr(hwnd, GWL_STYLE), FALSE, (DWORD) GetWindowLongPtr(hwnd, GWL_EXSTYLE));
		::SetWindowPos (hwnd, nullptr, ptScreen.x, ptScreen.y, wr.right - wr.left, wr.bottom - wr.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);

		int result = -1;
		MSG msg;
		while (GetMessage(&msg, 0, 0, 0))
		{
			if ((msg.hwnd == hwnd) && (msg.message == WM_CLOSE_POPUP))
			{
				result = (int) msg.wParam;
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
		return result;
	}

	std::pair<pgitem*, item_layout> item_at (D2D1_POINT_2F dip) const
	{
		std::pair<pgitem*, item_layout> result = { };

		enum_items ([&](pgitem* item, const item_layout& layout, bool& cancel)
		{
			if (dip.y < layout.name_rect.bottom)
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

		if (_text_editor && point_in_rect(_text_editor->rect(), dip))
			return _text_editor->process_mouse_button_down(button, modifier_keys, pixel, dip);

		auto clicked_item = item_at(dip);

		if (_selected_item != clicked_item.first)
		{
			discard_editor();
			_selected_item = clicked_item.first;
			invalidate();
		}

		if (auto clicked_value_item = dynamic_cast<value_pgitem*>(clicked_item.first);
			(clicked_value_item != nullptr) && (dip.x >= name_column_width()) && clicked_value_item->_prop->has_setter())
		{
			auto editor_rect = clicked_item.second.value_rect;
			editor_rect.left += text_lr_padding;
			editor_rect.right -= text_lr_padding;
			auto str = clicked_value_item->convert_to_string();
			_text_editor = text_editor_factory(this, dwrite_factory(), _textFormat, 0xFFC0C0C0, 0xFF000000, editor_rect, str.c_str());
			_text_editor->process_mouse_button_down (button, modifier_keys, pixel, dip);
			return;
		}

		if (clicked_item.first != nullptr)
			clicked_item.first->process_mouse_button_down (button, modifier_keys, dip - clicked_item.second.location);
	}

	void process_mouse_button_up (mouse_button button, UINT modifier_keys, POINT pixel, D2D1_POINT_2F dip)
	{
		if (_text_editor && point_in_rect(_text_editor->rect(), dip))
			return _text_editor->process_mouse_button_up (button, modifier_keys, pixel, dip);

		auto clicked_item = item_at(dip);
		if (clicked_item.first != nullptr)
			clicked_item.first->process_mouse_button_down (button, modifier_keys, dip - clicked_item.second.location);
		/*
		if (dip.x < name_column_width())
			return;

			if (pi->_pd->_customEditor != nullptr)
			{
				_customEditor = pi->_pd->_customEditor (pi->_selected_objects);
				_customEditor->ShowModal(hwnd());
				_customEditor = nullptr;
			}
			else if (pi->_pd->has_setter())
			{
				::ClientToScreen (hwnd(), &pt);

				bool changed = false;

				// TODO: move this code to virtual functions
				if (auto boolPD = dynamic_cast<const typed_property<bool>*>(pi->_pd))
				{
					static constexpr NVP nvps[] = { { "False", 0 }, { "True", 1 }, { 0, 0 } };
					int newValueInt = ShowEditor (pt, nvps);
					if (newValueInt != -1)
					{
						bool newValue = (bool) newValueInt;

						for (auto so : pi->_selected_objects)
						{
							if ((so->*(boolPD->_getter))() != newValue)
							{
								(so->*(boolPD->_setter)) (newValue);
								changed = true;
							}
						}
					}
				}
				else if (auto enumPD = dynamic_cast<const EnumProperty*>(pi->_pd); enumPD != nullptr)
				{
					int newValue = ShowEditor (pt, enumPD->_nameValuePairs);
					if (newValue != -1)
					{
						for (auto so : pi->_selected_objects)
						{
							if ((so->*(enumPD->_getter))() != newValue)
							{
								(so->*(enumPD->_setter)) (newValue);
								changed = true;
							}
						}
					}
				}
				else if (auto stringPD = dynamic_cast<const typed_property<string>*>(pi->_pd))
				{
					auto value = pi->GetValueText();
					ShowStringEditor (pi->_pd, item, pt, value.c_str(), [pi, stringPD, &changed](const string& newStr)
					{
						for (object* so : pi->_selected_objects)
						{
							if ((so->*(stringPD->_getter))() != newStr)
							{
								(so->*(stringPD->_setter)) (newStr);
								changed = true;
							}
						}
					});
				}
				else if (auto u16pd = dynamic_cast<const typed_property<uint16_t>*>(pi->_pd))
				{
					auto value = pi->GetValueText();
					ShowStringEditor (pi->_pd, item, pt, value.c_str(), [pi, u16pd, &changed](const string& newStr)
					{
						uint16_t newVal = (uint16_t) std::stoul(newStr);

						for (object* so : pi->_selected_objects)
						{
							if ((so->*(u16pd->_getter))() != newVal)
							{
								(so->*(u16pd->_setter)) (newVal);
								changed = true;
							}
						}
					});
				}
				else
				if (auto u32pd = dynamic_cast<const uint32_property*>(pi->_pd))
				{
					auto value = pi->GetValueText();
					ShowStringEditor (pi->_pd, item, pt, value.c_str(), [pi, u32pd, &changed](const string& newStr)
					{
						uint32_t newVal = (uint32_t) std::stoul(newStr);

						for (object* so : pi->_selected_objects)
						{
							if ((so->*(u32pd->_getter))() != newVal)
							{
								(so->*(u32pd->_setter)) (newVal);
								changed = true;
							}
						}
					});
				}
				else
					MessageBoxA (hwnd(), pi->_pd->_name, "aaaa", 0);

				if (changed)
					this->event_invoker<PropertyChangedByUserEvent>()(pi->_pd);
			}
		}
		else
			assert(false); // not implemented
			*/
	}

	void process_mouse_move (UINT modifier_keys, POINT pixel, D2D1_POINT_2F dip)
	{
		if (_text_editor && point_in_rect(_text_editor->rect(), dip))
			return _text_editor->process_mouse_move (modifier_keys, pixel, dip);
	}

	void try_commit_editor()
	{
		if (_text_editor == nullptr)
			return;

		auto prop_item = dynamic_cast<value_pgitem*>(_selected_item); assert(prop_item);
		auto& objects = prop_item->parent()->objects();
		std::vector<std::string> old_values;
		old_values.reserve(objects.size());
		for (auto o : objects)
			old_values.push_back (prop_item->_prop->get_to_string(o));

		auto u8str = _text_editor->u8str();
		auto wstr = _text_editor->wstr();

		for (size_t i = 0; i < objects.size(); i++)
		{
			bool set_ok = prop_item->_prop->try_set_from_string(objects[i], u8str);
			if (!set_ok)
			{
				for (size_t j = 0; j < i; j++)
					prop_item->_prop->try_set_from_string(objects[j], old_values[j]);

				std::wstringstream ss;
				ss << wstr << " is not a valid value for the " << prop_item->_prop->_name << " property.";
				::MessageBox (hwnd(), ss.str().c_str(), L"aaa", 0);
				::SetFocus (hwnd());
				_text_editor->select_all();
				return;
			}
		}

		property_changed_args args = { objects, std::move(old_values), std::move(u8str) };
		this->event_invoker<property_changed_e>()(std::move(args));

		base::invalidate (_text_editor->rect());
		_text_editor = nullptr;
	}

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
