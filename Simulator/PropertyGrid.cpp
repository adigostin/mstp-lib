
#include "pch.h"
#include "PropertyGrid.h"
#include "Win32/UtilityFunctions.h"

using namespace std;

static constexpr UINT WM_CLOSE_POPUP = WM_APP + 1;

static constexpr float CellLRPadding = 3.0f;
static constexpr D2D1_RECT_F HeadingTextPadding = { 5, 2, 5, 2 };

#pragma region Item-derived classes
class PropertyGrid::HeadingItem : public Item
{
	TextLayout _tl;

public:
	std::wstring const _heading;

	HeadingItem (PropertyGrid* pg, const wchar_t* heading) : Item(pg), _heading(heading) { }

	virtual float GetHeight() const override final { return HeadingTextPadding.top + _tl.metrics.height + HeadingTextPadding.bottom; }

	virtual void RecreateTextLayouts (float nameColumnWidth) override final
	{
		_tl = TextLayout::Create (_pg->GetDWriteFactory(), _pg->_headingTextFormat, _heading.c_str(), _pg->GetClientWidthDips() - HeadingTextPadding.left - HeadingTextPadding.right);
	}

	virtual void Render (ID2D1RenderTarget* rt, float lineY, float textY, float nameColumnWidth, float lineWidth) const override final
	{
		ID2D1SolidColorBrushPtr brush;
		rt->CreateSolidColorBrush (GetD2DSystemColor(COLOR_ACTIVECAPTION), &brush);
		rt->FillRectangle (D2D1::RectF(0, textY, _pg->GetClientWidthDips(), lineY), brush);
		brush->SetColor (GetD2DSystemColor(COLOR_CAPTIONTEXT));
		rt->DrawTextLayout ({ HeadingTextPadding.left, textY + HeadingTextPadding.top }, _tl.layout, brush);
	}
};

class PropertyGrid::GroupItem : public Item
{
	TextLayout _tl;

public:
	std::vector<Object*> const _selectedObjects;
	const PropertyGroup* const _pd;

	GroupItem (PropertyGrid* pg, const std::vector<Object*>& selectedObjects, const PropertyGroup* pd)
		: Item(pg), _selectedObjects(selectedObjects), _pd(pd)
	{ }

	virtual float GetHeight() const override final { return _tl.metrics.height; }

	virtual void RecreateTextLayouts (float nameColumnWidth) override final
	{
		std::wstring label;
		if (_pd->_labelGetter != nullptr)
			label = _pd->_labelGetter (_selectedObjects);
		else
			label = _pd->_name;

		_tl = TextLayout::Create (_pg->GetDWriteFactory(), _pg->_headingTextFormat, label.c_str(), _pg->GetClientWidthDips());
	}

	virtual void Render (ID2D1RenderTarget* rt, float lineY, float textY, float nameColumnWidth, float lineWidth) const override final
	{
		rt->DrawTextLayout ({ CellLRPadding, textY }, _tl.layout, _pg->_windowTextBrush);
	}
};

class PropertyGrid::PropertyItem : public Item
{
	TextLayout _labelTL;
	TextLayout _valueTL;

public:
	std::vector<Object*> const _selectedObjects;
	const Property* const _pd;

	PropertyItem (PropertyGrid* pg, const std::vector<Object*>& selectedObjects, const Property* pd)
		: Item(pg), _selectedObjects(selectedObjects), _pd(pd)
	{ }

	virtual float GetHeight() const override final { return std::max (_labelTL.metrics.height, _valueTL.metrics.height); }

	virtual void RecreateTextLayouts (float nameColumnWidth) override final
	{
		float maxWidth = _pg->GetNameColumnWidth() - 2 * CellLRPadding;

		if (_pd->_labelGetter != nullptr)
		{
			assert(false);
			//item->labelTL = TextLayout::Create (GetDWriteFactory(), _pg->_textFormat, item->pd->_labelGetter(_selectedObjects).c_str(), maxWidth);
		}
		else
			_labelTL = TextLayout::Create (_pg->GetDWriteFactory(), _pg->_textFormat, _pd->_name, maxWidth);

		float pixelWidth = _pg->GetDipSizeFromPixelSize ({ 1, 0 }).width;
		float lineWidth = roundf(1.0f / pixelWidth) * pixelWidth;
		maxWidth = std::max (100.0f, _pg->GetClientWidthDips()) - _pg->GetNameColumnWidth() - lineWidth - 2 * CellLRPadding;

		_valueTL = TextLayout::Create (_pg->GetDWriteFactory(), _pg->_textFormat, GetValueText().c_str(), maxWidth);
	}

	virtual void Render (ID2D1RenderTarget* rt, float lineY, float textY, float nameColumnWidth, float lineWidth) const override final
	{
		auto& brush = ((_pd->_customEditor != nullptr) || _pd->HasSetter()) ? _pg->_windowTextBrush : _pg->_grayTextBrush;
		rt->DrawTextLayout ({ CellLRPadding, textY }, _labelTL.layout, brush);
		float x = nameColumnWidth + lineWidth / 2;
		rt->DrawLine ({ x, textY }, { x, lineY - lineWidth / 2 }, _pg->_grayTextBrush, lineWidth);
		rt->DrawTextLayout ({ nameColumnWidth + lineWidth + CellLRPadding, textY }, _valueTL.layout, brush);
	}

	std::wstring GetValueText() const
	{
		auto str = _pd->to_wstring (_selectedObjects[0]);
		for (size_t i = 1; i < _selectedObjects.size(); i++)
		{
			if (_pd->to_wstring(_selectedObjects[i]) != str)
				return L"(multiple selection)";
		}

		return str;
	}
};
#pragma endregion

PropertyGrid::PropertyGrid (HINSTANCE hInstance, const RECT& rect, HWND hWndParent, IDWriteFactory* dWriteFactory, IWindowWithWorkQueue* iwwwq)
	: base (hInstance, 0, WS_CHILD | WS_VISIBLE, rect, hWndParent, nullptr, dWriteFactory)
	, _iwwwq(iwwwq)
{
	auto hr = dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
											   DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &_textFormat); assert(SUCCEEDED(hr));

	hr = dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
										  DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &_boldTextFormat); assert(SUCCEEDED(hr));

	hr = dWriteFactory->CreateTextFormat (L"Wingdings", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
										  DWRITE_FONT_STRETCH_NORMAL, 14, L"en-US", &_wingdings); assert(SUCCEEDED(hr));

	hr = dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
										  DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &_headingTextFormat); assert(SUCCEEDED(hr));

	GetRenderTarget()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOW), &_windowBrush);
	GetRenderTarget()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOWTEXT), &_windowTextBrush);
	GetRenderTarget()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_GRAYTEXT), &_grayTextBrush);
}

PropertyGrid::~PropertyGrid()
{
	//for (Object* so : _selectedObjects)
	//	so->GetPropertyChangedEvent().RemoveHandler (&OnSelectedObjectPropertyChanged, this);
}

std::optional<LRESULT> PropertyGrid::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	auto resultBaseClass = base::WindowProc (hwnd, msg, wParam, lParam);

	if (msg == WM_SIZE)
	{
		for (auto& item : _items)
			item->RecreateTextLayouts(GetNameColumnWidth());
		return 0;
	}

	if (msg == WM_LBUTTONUP)
	{
		ProcessLButtonUp ((DWORD) wParam, POINT { LOWORD(lParam), HIWORD(lParam) });
		return 0;
	}

	if (msg == WM_SETCURSOR)
	{
		if (((HWND) wParam == GetHWnd()) && (LOWORD (lParam) == HTCLIENT))
		{
			// Let's check the result because GetCursorPos fails when the input desktop is not the current desktop
			// (happens for example when the monitor goes to sleep and then the lock screen is displayed).
			POINT pt;
			if (::GetCursorPos (&pt))
			{
				if (ScreenToClient (GetHWnd(), &pt))
				{
					this->ProcessWmSetCursor(pt);
					return TRUE;
				}
			}
		}

		return nullopt;
	}

	return resultBaseClass;
}

void PropertyGrid::ProcessWmSetCursor (POINT pt)
{
	auto dipLocation = GetDipLocationFromPixelLocation(pt);
	auto item = GetItemAt(dipLocation);
	if ((item == nullptr) || (dynamic_cast<PropertyItem*>(item) == nullptr))
	{
		::SetCursor (::LoadCursor (nullptr, IDC_ARROW));
		return;
	}

	auto pd = static_cast<PropertyItem*>(item)->_pd;
	bool canEdit = ((pd->_customEditor != nullptr) || pd->HasSetter()) && (dipLocation.x >= GetNameColumnWidth());
	::SetCursor (::LoadCursor (nullptr, canEdit ? IDC_HAND : IDC_ARROW));
}

PropertyGrid::Item* PropertyGrid::GetItemAt (D2D1_POINT_2F location)
{
	auto item = EnumItems ([location](float textY, float lineY, float lineWidth, Item* item, bool& stopEnum)
	{
		stopEnum = lineY + lineWidth > location.y;
	});

	return item;
}

PropertyGrid::Item* PropertyGrid::EnumItems (std::function<void(float textY, float lineY, float lineWidth, Item* item, bool& stopEnum)> func) const
{
	float pixelWidth = GetDipSizeFromPixelSize ({ 1, 0 }).width;
	float lineWidth = roundf(1.0f / pixelWidth) * pixelWidth;
	float y = 0;
	bool cancelEnum = false;
	for (auto& item : _items)
	{
		float lineY = y + item->GetHeight();
		lineY = roundf (lineY / pixelWidth) * pixelWidth + lineWidth / 2;
		func (y, lineY, lineWidth, item.get(), cancelEnum);
		if (cancelEnum)
			return item.get();
		y = lineY + lineWidth / 2;
	}

	return nullptr;
}

void PropertyGrid::Render (ID2D1RenderTarget* rt) const
{
	rt->Clear(GetD2DSystemColor(COLOR_WINDOW));

	if (_items.empty())
	{
		auto tl = TextLayout::Create (GetDWriteFactory(), _textFormat, L"(no selection)");
		D2D1_POINT_2F p = { GetClientWidthDips() / 2 - tl.metrics.width / 2, GetClientHeightDips() / 2 - tl.metrics.height / 2};
		rt->DrawTextLayout (p, tl.layout, _windowTextBrush);
		return;
	}

	auto nameColumnWidth = GetNameColumnWidth();
	EnumItems ([this, rt, nameColumnWidth](float textY, float lineY, float lineWidth, Item* item, bool& stopEnum)
	{
		item->Render (rt, lineY, textY, nameColumnWidth, lineWidth);
		rt->DrawLine ({ 0, lineY }, { GetClientWidthDips(), lineY }, _grayTextBrush, lineWidth);
	});
}

float PropertyGrid::GetNameColumnWidth() const
{
	float w = std::max (100.0f, GetClientWidthDips()) * _nameColumnSize;
	float pixelWidth = GetDipSizeFromPixelSize ({ 1, 0 }).width;
	w = roundf (w / pixelWidth) * pixelWidth;
	return w;
}

void PropertyGrid::DiscardEditor()
{
	if (_editorInfo != nullptr)
		::SendMessage (_editorInfo->_popupHWnd, WM_CLOSE_POPUP, 0, 0);
}
/*
void PropertyGrid::SelectObjects (Object* const* objects, size_t count)
{
	bool sameSelection = false;
	if (_selectedObjects.size() == count)
	{
		sameSelection = true;
		for (size_t i = 0; i < count; i++)
		{
			if (_selectedObjects[i] != objects[i])
			{
				sameSelection = false;
				break;
			}
		}
	}

	if (sameSelection)
		return;

	DiscardEditor();

	for (Object* so : _selectedObjects)
		so->GetPropertyChangedEvent().RemoveHandler (&OnSelectedObjectPropertyChanged, this);

	_selectedObjects.assign (objects, objects + count);

	for (Object* so : _selectedObjects)
		so->GetPropertyChangedEvent().AddHandler (&OnSelectedObjectPropertyChanged, this);
}
*/

void PropertyGrid::ClearProperties()
{
	DiscardEditor();
	_items.clear();
	::InvalidateRect (GetHWnd(), NULL, FALSE);
}

void PropertyGrid::AddProperties (Object* const* objects, size_t count, const wchar_t* heading)
{
	DiscardEditor();

	if (count > 0)
	{
		std::vector<Object*> selectedObjects;
		selectedObjects.assign (objects, objects + count);

		assert (all_of(selectedObjects.begin(), selectedObjects.end(), [objects](Object* o) { return o->GetProperties() == objects[0]->GetProperties(); }));

		_items.push_back (unique_ptr<Item>(new HeadingItem(this, heading)));

		for (auto ppd = objects[0]->GetProperties(); *ppd != nullptr; ppd++)
		{
			if (auto property = dynamic_cast<const Property*>(*ppd); property != nullptr)
			{
				_items.push_back (unique_ptr<Item>(new PropertyItem (this, selectedObjects, property)));
			}
			else if (auto group = dynamic_cast<const PropertyGroup*>(*ppd); group != nullptr)
			{
				_items.push_back (unique_ptr<Item>(new GroupItem (this, selectedObjects, group)));
			}
			else
				assert(false);
		}

		for (auto& item : _items)
			item->RecreateTextLayouts(GetNameColumnWidth());
	}

	::InvalidateRect (GetHWnd(), NULL, FALSE);
}
/*
//static
void PropertyGrid::OnSelectedObjectPropertyChanged (void* callbackArg, Object* o, const Property* property)
{
	auto pg = static_cast<PropertyGrid*>(callbackArg);

	if ((pg->_editorInfo != nullptr) && (pg->_editorInfo->_property == property))
		pg->DiscardEditor();

	auto it = std::find_if (pg->_items.begin(), pg->_items.end(), [property](const unique_ptr<Item>& i) { return i->pd == property; });
	if (it != pg->_items.end())
	{
		it->get()->CreateValueTextLayout();
		::InvalidateRect (pg->GetHWnd(), NULL, FALSE);
	}
}
*/
static wstring GetWindowText (HWND hwnd)
{
	wstring str (::GetWindowTextLength(hwnd) + 1, 0);
	::GetWindowText (hwnd, str.data(), (int) str.length());
	str.resize (str.length() - 1);
	return str;
};

// static
LRESULT CALLBACK PropertyGrid::EditSubclassProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	auto pg = reinterpret_cast<PropertyGrid*>(dwRefData);

	if ((msg == WM_CHAR) && ((wParam == VK_RETURN) || (wParam == VK_ESCAPE)))
	{
		// disable the beep on these keys
		return 0;
	}

	if ((msg == WM_KEYDOWN) && (wParam == VK_RETURN))
	{
		try
		{
			auto newStr = GetWindowText(hWnd);
			if (newStr != pg->_editorInfo->_initialString)
			{
				pg->_editorInfo->_validating = true;
				pg->_editorInfo->_validateAndSetFunction(newStr);
				pg->_editorInfo->_validating = false;
			}

			auto popup = pg->_editorInfo->_popupHWnd;
			pg->_editorInfo.reset();
			::DestroyWindow (popup);
		}
		catch (const exception& ex)
		{
			string message (ex.what());
			message += "\r\n\r\n(You can press Escape to cancel your edits.)";
			::MessageBoxA (pg->_editorInfo->_popupHWnd, message.c_str(), 0, 0);
			::SetFocus (pg->_editorInfo->_editHWnd);
			::SendMessage (pg->_editorInfo->_editHWnd, EM_SETSEL, 0, -1);
			pg->_editorInfo->_validating = false;
		}

		return 0;
	}

	if (msg == WM_KILLFOCUS)
	{
		if ((pg->_editorInfo == nullptr) || pg->_editorInfo->_validating)
			return DefSubclassProc (hWnd, msg, wParam, lParam);

		auto newStr = GetWindowText (hWnd);
		if (newStr != pg->_editorInfo->_initialString)
		{
			try
			{
				pg->_editorInfo->_validating = true;
				pg->_editorInfo->_validateAndSetFunction(newStr);
				pg->_editorInfo->_validating = false;
			}
			catch (const exception& ex)
			{
				pg->_iwwwq->PostWork ([hwnd=pg->GetHWnd(), message=string(ex.what())] ()
				{
					::MessageBoxA (hwnd, message.c_str(), 0, 0);
				});
			}
		}

		auto popup = pg->_editorInfo->_popupHWnd;
		pg->_editorInfo.reset();
		::DestroyWindow (popup);
		return 0;
	}
	else if ((msg == WM_KEYDOWN) && (wParam == VK_ESCAPE))
	{
		auto popup = pg->_editorInfo->_popupHWnd;
		pg->_editorInfo.reset();
		::DestroyWindow (popup);
		return 0;
	}

	return DefSubclassProc (hWnd, msg, wParam, lParam);
}

void PropertyGrid::ShowStringEditor (const Property* property, Item* item, POINT ptScreen, const wchar_t* str, VSF validateAndSetFunction)
{
	HINSTANCE hInstance = (HINSTANCE) GetWindowLongPtr (GetHWnd(), GWLP_HINSTANCE);

	static constexpr wchar_t ClassName[] = L"GIGI-{F52B45B0-7543-4B85-A1A5-7BB88D678F02}";
	static ATOM atom = 0;
	if (atom == 0)
	{
		WNDCLASS EditorWndClass =
		{
			0, // style
			DefWindowProc,
			0, // cbClsExtra
			0, // cbWndExtra
			hInstance,
			nullptr, // hIcon
			::LoadCursor(nullptr, IDC_ARROW), // hCursor
			(HBRUSH) (COLOR_3DFACE + 1), // hbrBackground
			nullptr,  // lpszMenuName
			ClassName // lpszClassName
		};

		atom = ::RegisterClassW (&EditorWndClass); assert (atom != 0);
	}

	NONCLIENTMETRICS ncMetrics = { sizeof(NONCLIENTMETRICS) };
	SystemParametersInfo (SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncMetrics, 0);

	_editorInfo.reset (new EditorInfo(property, item));
	_editorInfo->_initialString = str;
	_editorInfo->_validateAndSetFunction = validateAndSetFunction;
	_editorInfo->_popupHWnd = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, ClassName, L"aaa", WS_POPUP | WS_BORDER, 0, 0, 0, 0, GetHWnd(), nullptr, hInstance, nullptr);
	assert (_editorInfo->_popupHWnd != nullptr);
	_editorInfo->_font.reset (CreateFontIndirect (&ncMetrics.lfMenuFont));

	auto hdc = ::GetDC(_editorInfo->_popupHWnd);
	int dpiX = GetDeviceCaps (hdc, LOGPIXELSX);
	int dpiY = GetDeviceCaps (hdc, LOGPIXELSY);
	RECT rc = { };
	auto oldFont = ::SelectObject (hdc, _editorInfo->_font.get());
	::DrawTextW (hdc, L"Representative String", -1, &rc, DT_EDITCONTROL | DT_CALCRECT);
	::SelectObject (hdc, oldFont);
	::ReleaseDC (_editorInfo->_popupHWnd, hdc);

	int margin = 4 * dpiX / 96;
	auto editWidth = rc.right + 2 * GetSystemMetrics(SM_CXEDGE);
	auto editHeight = rc.bottom + 2 * GetSystemMetrics(SM_CYEDGE);
	_editorInfo->_editHWnd = CreateWindowEx (0, L"EDIT", str, WS_CHILD | WS_VISIBLE | WS_BORDER, margin, margin, editWidth, editHeight, _editorInfo->_popupHWnd, nullptr, hInstance, nullptr);
	::SendMessageW (_editorInfo->_editHWnd, WM_SETFONT, (WPARAM) _editorInfo->_font.get(), FALSE);
	::SendMessageW (_editorInfo->_editHWnd, EM_SETSEL, 0, -1);
	::SetFocus (_editorInfo->_editHWnd);
	::SetWindowSubclass (_editorInfo->_editHWnd, EditSubclassProc, 1, (DWORD_PTR) this);

	RECT wr = { 0, 0, margin + editWidth + margin, margin + editHeight + margin };
	::AdjustWindowRectEx (&wr, (DWORD) GetWindowLongPtr(_editorInfo->_popupHWnd, GWL_STYLE), FALSE, (DWORD) GetWindowLongPtr(_editorInfo->_popupHWnd, GWL_EXSTYLE));
	::SetWindowPos (_editorInfo->_popupHWnd, nullptr, ptScreen.x, ptScreen.y, wr.right - wr.left, wr.bottom - wr.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

int PropertyGrid::ShowEditor (POINT ptScreen, const NVP* nameValuePairs)
{
	HINSTANCE hInstance = (HINSTANCE) GetWindowLongPtr (GetHWnd(), GWLP_HINSTANCE);

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

	auto hwnd = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, ClassName, L"aaa", WS_POPUP | WS_BORDER, 0, 0, 0, 0, GetHWnd(), nullptr, hInstance, nullptr); assert (hwnd != nullptr);

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
		DrawTextW (hdc, nameValuePairs[count].first, -1, &rc, DT_CALCRECT);
		maxTextWidth = max (maxTextWidth, rc.right);
		maxTextHeight = max (maxTextHeight, rc.bottom);
	}
	::SelectObject (hdc, oldFont);
	int dpiX = GetDeviceCaps (hdc, LOGPIXELSX);
	int dpiY = GetDeviceCaps (hdc, LOGPIXELSY);
	::ReleaseDC (hwnd, hdc);

	int lrpadding = 7 * dpiX / 96;
	int udpadding = ((count <= 5) ? 5 : 0) * dpiY / 96;
	LONG buttonWidth = max (100l * dpiX / 96, maxTextWidth + 2 * lrpadding) + 2 * GetSystemMetrics(SM_CXEDGE);
	LONG buttonHeight = maxTextHeight + 2 * udpadding + 2 * GetSystemMetrics(SM_CYEDGE);

	int margin = 4 * dpiX / 96;
	int spacing = 2 * dpiX / 96;
	int y = margin;
	for (auto nvp = nameValuePairs; nvp->first != nullptr;)
	{
		auto button = CreateWindowEx (0, L"Button", nvp->first, WS_CHILD | WS_VISIBLE | BS_NOTIFY | BS_FLAT, margin, y, buttonWidth, buttonHeight, hwnd, (HMENU) (INT_PTR) nvp->second, hInstance, nullptr);
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

void PropertyGrid::ProcessLButtonUp (DWORD modifierKeys, POINT pt)
{
	auto dLocation = GetDipLocationFromPixelLocation(pt);
	if (dLocation.x < GetNameColumnWidth())
		return;

	auto item = GetItemAt (GetDipLocationFromPixelLocation(pt));
	if (item == nullptr)
		return;

	if (dynamic_cast<HeadingItem*>(item) != nullptr)
	{
	}
	else if (auto pi = dynamic_cast<PropertyItem*>(item); pi != nullptr)
	{
		if (pi->_pd->_customEditor != nullptr)
		{
			_customEditor = pi->_pd->_customEditor (pi->_selectedObjects);
			_customEditor->ShowModal(GetHWnd());
			_customEditor = nullptr;
		}
		else if (pi->_pd->HasSetter())
		{
			::ClientToScreen (GetHWnd(), &pt);

			bool changed = false;

			// TODO: move this code to virtual functions
			if (auto boolPD = dynamic_cast<const TypedProperty<bool>*>(pi->_pd); boolPD != nullptr)
			{
				static constexpr NVP nvps[] = { { L"False", 0 }, { L"True", 1 }, { 0, 0 } };
				int newValueInt = ShowEditor (pt, nvps);
				if (newValueInt != -1)
				{
					bool newValue = (bool) newValueInt;

					for (auto so : pi->_selectedObjects)
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
					for (auto so : pi->_selectedObjects)
					{
						if ((so->*(enumPD->_getter))() != newValue)
						{
							(so->*(enumPD->_setter)) (newValue);
							changed = true;
						}
					}
				}
			}
			else if (auto stringPD = dynamic_cast<const TypedProperty<wstring>*>(pi->_pd); stringPD != nullptr)
			{
				auto value = pi->GetValueText();
				ShowStringEditor (pi->_pd, item, pt, value.c_str(), [pi, stringPD, &changed](const wstring& newStr)
				{
					for (Object* so : pi->_selectedObjects)
					{
						if ((so->*(stringPD->_getter))() != newStr)
						{
							(so->*(stringPD->_setter)) (newStr);
							changed = true;
						}
					}
				});
			}
			else if (auto u16pd = dynamic_cast<const TypedProperty<uint16_t>*>(pi->_pd); u16pd != nullptr)
			{
				auto value = pi->GetValueText();
				ShowStringEditor (pi->_pd, item, pt, value.c_str(), [pi, u16pd, &changed](const wstring& newStr)
				{
					uint16_t newVal = (uint16_t) std::stoul(newStr);

					for (Object* so : pi->_selectedObjects)
					{
						if ((so->*(u16pd->_getter))() != newVal)
						{
							(so->*(u16pd->_setter)) (newVal);
							changed = true;
						}
					}
				});
			}
			else if (auto u32pd = dynamic_cast<const TypedProperty<uint32_t>*>(pi->_pd); u32pd != nullptr)
			{
				auto value = pi->GetValueText();
				ShowStringEditor (pi->_pd, item, pt, value.c_str(), [pi, u32pd, &changed](const wstring& newStr)
				{
					uint32_t newVal = (uint32_t) std::stoul(newStr);

					for (Object* so : pi->_selectedObjects)
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
				MessageBox (GetHWnd(), pi->_pd->_name, L"aaaa", 0);

			if (changed)
				PropertyChangedByUserEvent::InvokeHandlers (this, pi->_pd);
		}
	}
	else
		assert(false); // not implemented
}
