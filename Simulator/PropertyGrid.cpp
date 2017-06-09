
#include "pch.h"
#include "PropertyGrid.h"
#include "UtilityFunctions.h"

using namespace std;

static constexpr float CellLRPadding = 3.0f;
static constexpr UINT WM_CLOSE_POPUP = WM_APP + 1;

PropertyGrid::PropertyGrid (ISimulatorApp* app, IProjectWindow* projectWindow, IProject* project, const RECT& rect, HWND hWndParent, IDWriteFactory* dWriteFactory)
	: base (app->GetHInstance(), 0, WS_CHILD | WS_VISIBLE, rect, hWndParent, nullptr, dWriteFactory)
	, _app(app)
	, _projectWindow(projectWindow)
	, _project(project)
{
	auto hr = dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
											   DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &_textFormat); ThrowIfFailed(hr);

	hr = dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
										  DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &_boldTextFormat); ThrowIfFailed(hr);

	hr = dWriteFactory->CreateTextFormat (L"Wingdings", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
										  DWRITE_FONT_STRETCH_NORMAL, 14, L"en-US", &_wingdings); ThrowIfFailed(hr);

	GetRenderTarget()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOW), &_windowBrush);
	GetRenderTarget()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOWTEXT), &_windowTextBrush);
	GetRenderTarget()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_GRAYTEXT), &_grayTextBrush);
}

std::optional<LRESULT> PropertyGrid::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	auto resultBaseClass = base::WindowProc (hwnd, msg, wParam, lParam);

	if (msg == WM_SIZE)
	{
		CreateLabelTextLayouts();
		CreateValueTextLayouts();
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

void PropertyGrid::ProcessWmSetCursor (POINT pt) const
{
	auto dipLocation = GetDipLocationFromPixelLocation(pt);
	auto item = GetItemAt(dipLocation);
	if ((item == nullptr) || (dynamic_cast<const Property*>(item->pd) == nullptr))
	{
		::SetCursor (::LoadCursor (nullptr, IDC_ARROW));
		return;
	}

	auto pd = static_cast<const Property*>(item->pd);
	if (pd->IsReadOnly())
	{
		::SetCursor (::LoadCursor (nullptr, IDC_ARROW));
		return;
	}

	if (dipLocation.x >= GetNameColumnWidth())
		::SetCursor (::LoadCursor (nullptr, IDC_HAND));
	else
		::SetCursor (::LoadCursor (nullptr, IDC_ARROW));
}

const PropertyGrid::Item* PropertyGrid::GetItemAt (D2D1_POINT_2F location) const
{
	auto item = EnumItems ([location](float textY, float lineY, float lineWidth, const Item& item, bool& stopEnum)
	{
		stopEnum = lineY + lineWidth > location.y;
	});

	return item;
}

const PropertyGrid::Item* PropertyGrid::EnumItems (std::function<void(float textY, float lineY, float lineWidth, const Item& item, bool& stopEnum)> func) const
{
	float pixelWidth = GetDipSizeFromPixelSize ({ 1, 0 }).width;
	float lineWidth = roundf(1.0f / pixelWidth) * pixelWidth;
	float y = 0;
	bool cancelEnum = false;
	for (auto& item : _items)
	{
		float lineY = y + std::max (item.labelTL.metrics.height, item.valueTL.metrics.height);
		lineY = roundf (lineY / pixelWidth) * pixelWidth + lineWidth / 2;
		func (y, lineY, lineWidth, item, cancelEnum);
		if (cancelEnum)
			return &item;
		y = lineY + lineWidth / 2;
	}

	return nullptr;
}

void PropertyGrid::Render (ID2D1RenderTarget* rt) const
{
	rt->Clear(GetD2DSystemColor(COLOR_WINDOW));

	if (_selectedObjects.empty())
	{
		auto tl = TextLayout::Create (GetDWriteFactory(), _textFormat, L"(no selection)");
		D2D1_POINT_2F p = { GetClientWidthDips() / 2 - tl.metrics.width / 2, GetClientHeightDips() / 2 - tl.metrics.height / 2};
		rt->DrawTextLayout (p, tl.layout, _windowTextBrush);
		return;
	}

	auto nameColumnWidth = GetNameColumnWidth();
	EnumItems ([this, rt, nameColumnWidth](float textY, float lineY, float lineWidth, const Item& item, bool& stopEnum)
	{
		if (dynamic_cast<const PropertyGroup*>(item.pd) != nullptr)
		{
			rt->DrawTextLayout ({ CellLRPadding, textY }, item.labelTL.layout, _windowTextBrush);
		}
		else if (dynamic_cast<const Property*>(item.pd) != nullptr)
		{
			auto pd = static_cast<const Property*>(item.pd);
			auto& brush = pd->IsReadOnly() ? _grayTextBrush : _windowTextBrush;
			rt->DrawTextLayout ({ CellLRPadding, textY }, item.labelTL.layout, brush);
			if (item.valueTL.layout != nullptr)
			{
				float x = nameColumnWidth + lineWidth / 2;
				rt->DrawLine ({ x, textY }, { x, lineY - lineWidth / 2 }, _grayTextBrush, lineWidth);
				rt->DrawTextLayout ({ nameColumnWidth + lineWidth + CellLRPadding, textY }, item.valueTL.layout, brush);
			}
		}
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
	if (_editorInfo.has_value())
		::SendMessage (_editorInfo->_popupHWnd, WM_CLOSE_POPUP, 0, 0);
}

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

	_selectedObjects.assign (objects, objects + count);
	_items.clear();

	if (!_selectedObjects.empty())
	{
		auto props = _selectedObjects[0]->GetProperties();
		if (all_of(_selectedObjects.begin(), _selectedObjects.end(), [props](Object* o) { return o->GetProperties() == props; }))
		{
			for (auto ppd = props; *ppd != nullptr; ppd++)
				_items.push_back (Item { *ppd });

			CreateLabelTextLayouts();
			CreateValueTextLayouts();
		}
	}

	::InvalidateRect (GetHWnd(), NULL, FALSE);
}

void PropertyGrid::CreateLabelTextLayouts()
{
	float maxWidth = GetNameColumnWidth() - 2 * CellLRPadding;
	for (auto& item : _items)
	{
		IDWriteTextFormat* textFormat = _textFormat;
		if (dynamic_cast<const PropertyGroup*>(item.pd) != nullptr)
			textFormat = _boldTextFormat;

		if (item.pd->_labelGetter != nullptr)
			item.labelTL = TextLayout::Create (GetDWriteFactory(), textFormat, item.pd->_labelGetter(_selectedObjects, _projectWindow->GetSelectedVlanNumber()).c_str(), maxWidth);
		else
			item.labelTL = TextLayout::Create (GetDWriteFactory(), textFormat, item.pd->_name, maxWidth);
	}
}

void PropertyGrid::CreateValueTextLayouts()
{
	float pixelWidth = GetDipSizeFromPixelSize ({ 1, 0 }).width;
	float lineWidth = roundf(1.0f / pixelWidth) * pixelWidth;
	float maxWidth = std::max (100.0f, GetClientWidthDips()) - GetNameColumnWidth() - lineWidth - 2 * CellLRPadding;

	for (auto& item : _items)
	{
		if (dynamic_cast<const Property*>(item.pd) != nullptr)
		{
			auto pd = static_cast<const Property*>(item.pd);

			auto str = pd->to_wstring (_selectedObjects[0], _projectWindow->GetSelectedVlanNumber());
			item.valueTL.layout = nullptr;
			for (size_t i = 1; i < _selectedObjects.size(); i++)
			{
				if (pd->to_wstring(_selectedObjects[i], _projectWindow->GetSelectedVlanNumber()) != str)
				{
					item.valueTL = TextLayout::Create (GetDWriteFactory(), _textFormat, L"(multiple selection)", maxWidth);
					break;
				}
			}

			if (item.valueTL.layout == nullptr)
				item.valueTL = TextLayout::Create (GetDWriteFactory(), _textFormat, str.c_str(), maxWidth);
		}
	}
}

void PropertyGrid::ReloadPropertyValues()
{
	DiscardEditor();

	//throw not_implemented_exception()
	CreateValueTextLayouts();
	::InvalidateRect (GetHWnd(), NULL, FALSE);
}

// static
LRESULT CALLBACK PropertyGrid::EditSubclassProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	auto pg = reinterpret_cast<PropertyGrid*>(dwRefData);

	if ((msg == WM_CHAR) && ((wParam == VK_RETURN) || (wParam == VK_ESCAPE)))
	{
		// disable the beep on these keys
		return 0;
	}

	if (((msg == WM_KEYDOWN) && (wParam == VK_RETURN))
		 /*|| (msg == WM_KILLFOCUS)*/)
	{
		if (pg->_editorInfo->_validating)
			return DefSubclassProc (hWnd, msg, wParam, lParam);

		wstring newStr (::GetWindowTextLength(hWnd) + 1, 0);
		::GetWindowText (hWnd, newStr.data(), (int) newStr.length());
		newStr.resize (newStr.length() - 1);
		if (newStr == pg->_editorInfo->_initialString)
		{
			::PostMessage (pg->_editorInfo->_popupHWnd, WM_CLOSE_POPUP, 0, 0);
			return 0;
		}

		try
		{
			pg->_editorInfo->_validating = true;
			pg->_editorInfo->_validateAndSetFunction(newStr);
			pg->_editorInfo->_validating = false;
			::PostMessage (pg->_editorInfo->_popupHWnd, WM_CLOSE_POPUP, 0, 0);
		}
		catch (const exception& ex)
		{
			//::SetFocus (pg->_editorInfo->_editHWnd);
			::SetFocus (nullptr);
			pg->_projectWindow->PostWork ([pg, message=string(ex.what())] () mutable
			{
				message += "\r\n\r\n(You can press Escape to cancel your edits.)";
				::MessageBoxA (pg->_editorInfo->_popupHWnd, message.c_str(), 0, 0);
				::SetFocus (pg->_editorInfo->_editHWnd);
				::SendMessage (pg->_editorInfo->_editHWnd, EM_SETSEL, 0, -1);
				pg->_editorInfo->_validating = false;
			});
		}

		return 0;
	}
	else if ((msg == WM_KEYDOWN) && (wParam == VK_ESCAPE))
	{
		::PostMessage (pg->_editorInfo->_popupHWnd, WM_CLOSE_POPUP, 0, 0);
		return 0;
	}

	return DefSubclassProc (hWnd, msg, wParam, lParam);
}

void PropertyGrid::ShowEditor (POINT ptScreen, const wchar_t* str, VSF validateAndSetFunction)
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
	HFONT_unique_ptr font (CreateFontIndirect (&ncMetrics.lfMenuFont));

	_editorInfo = EditorInfo();
	_editorInfo->_initialString = str;
	_editorInfo->_validateAndSetFunction = validateAndSetFunction;
	_editorInfo->_popupHWnd = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, ClassName, L"aaa", WS_POPUP | WS_BORDER, 0, 0, 0, 0, GetHWnd(), nullptr, hInstance, nullptr);
	assert (_editorInfo->_popupHWnd != nullptr);

	auto hdc = ::GetDC(_editorInfo->_popupHWnd);
	int dpiX = GetDeviceCaps (hdc, LOGPIXELSX);
	int dpiY = GetDeviceCaps (hdc, LOGPIXELSY);
	RECT rc = { };
	auto oldFont = ::SelectObject (hdc, font.get());
	::DrawTextW (hdc, L"Representative String", -1, &rc, DT_EDITCONTROL | DT_CALCRECT);
	::SelectObject (hdc, oldFont);
	::ReleaseDC (_editorInfo->_popupHWnd, hdc);

	int margin = 4 * dpiX / 96;
	auto editWidth = rc.right + 2 * GetSystemMetrics(SM_CXEDGE);
	auto editHeight = rc.bottom + 2 * GetSystemMetrics(SM_CYEDGE);
	_editorInfo->_editHWnd = CreateWindowEx (0, L"EDIT", str, WS_CHILD | WS_VISIBLE | WS_BORDER, margin, margin, editWidth, editHeight, _editorInfo->_popupHWnd, nullptr, hInstance, nullptr);
	::SendMessage (_editorInfo->_editHWnd, WM_SETFONT, (WPARAM) font.get(), FALSE);
	::SendMessageW(_editorInfo->_editHWnd, EM_SETSEL, 0, -1);
	::SetFocus(_editorInfo->_editHWnd);
	::SetWindowSubclass (_editorInfo->_editHWnd, EditSubclassProc, 1, (DWORD_PTR) this);

	RECT wr = { 0, 0, margin + editWidth + margin, margin + editHeight + margin };
	::AdjustWindowRectEx (&wr, (DWORD) GetWindowLongPtr(_editorInfo->_popupHWnd, GWL_STYLE), FALSE, (DWORD) GetWindowLongPtr(_editorInfo->_popupHWnd, GWL_EXSTYLE));
	::SetWindowPos (_editorInfo->_popupHWnd, nullptr, ptScreen.x, ptScreen.y, wr.right - wr.left, wr.bottom - wr.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);

	MSG msg;
	while (GetMessage(&msg, 0, 0, 0))
	{
		//if ((msg.message < WM_APP) && (msg.message != 0x118))
		//{
		//	wstringstream ss;
		//	ss << L"HWND=0x" << hex << uppercase << msg.hwnd << L", msg=0x" << hex << uppercase << msg.message << endl;
		//	OutputDebugString (ss.str().c_str());
		//}

		if (msg.message == WM_ACTIVATE)
			__debugbreak();

		if ((msg.hwnd == _editorInfo->_popupHWnd) && (msg.message == WM_CLOSE_POPUP))
			break;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	::DestroyWindow (_editorInfo->_popupHWnd);
	_editorInfo.reset();
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
	auto item = GetItemAt (GetDipLocationFromPixelLocation(pt));
	if (item == nullptr)
		return;

	if (dynamic_cast<const PropertyGroup*>(item->pd) != nullptr)
	{
	}
	else if (dynamic_cast<const Property*>(item->pd) != nullptr)
	{
		auto pd = static_cast<const Property*>(item->pd);
		if (pd->IsReadOnly())
			return;

		::ClientToScreen (GetHWnd(), &pt);

		if (dynamic_cast<const TypedProperty<bool>*>(item->pd) != nullptr)
		{
			auto boolPD = dynamic_cast<const TypedProperty<bool>*>(item->pd);
			static constexpr NVP nvps[] = { { L"False", 0 }, { L"True", 1 }, { 0, 0 } };
			int newValueInt = ShowEditor (pt, nvps);
			if (newValueInt != -1)
			{
				bool newValue = (bool) newValueInt;

				auto timestamp = GetTimestampMilliseconds();
				for (auto so : _selectedObjects)
				{
					if (boolPD->_getter(so, _projectWindow->GetSelectedVlanNumber()) != newValue)
					{
						boolPD->_setter (so, (bool) newValue, _projectWindow->GetSelectedVlanNumber(), timestamp);
						_project->SetModified(true);
					}
				}
			}
		}
		else if (dynamic_cast<const EnumProperty*>(item->pd) != nullptr)
		{
			auto enumPD = dynamic_cast<const EnumProperty*>(item->pd);
			int newValue = ShowEditor (pt, enumPD->_nameValuePairs);
			if (newValue != -1)
			{
				auto timestamp = GetTimestampMilliseconds();
				for (auto so : _selectedObjects)
				{
					if (enumPD->_getter(so, _projectWindow->GetSelectedVlanNumber()) != newValue)
					{
						enumPD->_setter (so, newValue, _projectWindow->GetSelectedVlanNumber(), timestamp);
						_project->SetModified(true);
					}
				}
			}
		}
		else if (dynamic_cast<const TypedProperty<wstring>*>(item->pd) != nullptr)
		{
			auto stringPD = dynamic_cast<const TypedProperty<wstring>*>(item->pd);
			auto vlanNumber = _projectWindow->GetSelectedVlanNumber();
			auto value = stringPD->_getter (_selectedObjects[0], vlanNumber);

			ShowEditor (pt, value.c_str(), [this, stringPD, vlanNumber](const wstring& newStr)
			{
				auto timestamp = GetTimestampMilliseconds();
				for (Object* o : _selectedObjects)
					stringPD->_setter (o, newStr, vlanNumber, timestamp);
			});
		}
		else
			MessageBox (GetHWnd(), item->pd->_name, L"aaaa", 0);
	}
	else
		throw not_implemented_exception();
}
