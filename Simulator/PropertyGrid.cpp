
#include "pch.h"
#include "PropertyGrid.h"
#include "UtilityFunctions.h"

using namespace std;

static constexpr float CellLRPadding = 3.0f;

PropertyGrid::PropertyGrid (ISimulatorApp* app, IProjectWindow* projectWindow, IProject* project, const RECT& rect, HWND hWndParent, IDWriteFactory* dWriteFactory)
	: base (app->GetHInstance(), 0, WS_CHILD | WS_VISIBLE, rect, hWndParent, nullptr, dWriteFactory)
	, _app(app)
	, _projectWindow(projectWindow)
	, _project(project)
{
	auto hr = dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
											   DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &_textFormat); ThrowIfFailed(hr);

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
	if ((item != nullptr) && !item->pd->IsReadOnly())
	{
		if (dipLocation.x >= GetNameColumnWidth())
			::SetCursor (::LoadCursor (nullptr, IDC_HAND));
		else
			::SetCursor (::LoadCursor (nullptr, IDC_ARROW));
	}
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
		y = lineY + lineWidth;
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

	float lastLineY = 0;
	EnumItems ([this, rt, &lastLineY](float textY, float lineY, float lineWidth, const Item& item, bool& stopEnum)
	{
		auto& brush = item.pd->IsReadOnly() ? _grayTextBrush : _windowTextBrush;
		rt->DrawTextLayout ({ CellLRPadding, textY }, item.labelTL.layout, brush);
		rt->DrawTextLayout ({ GetNameColumnWidth() + lineWidth + CellLRPadding, textY }, item.valueTL.layout, brush);
		rt->DrawLine ({ 0, lineY }, { GetClientWidthDips(), lineY }, _grayTextBrush, lineWidth);
		lastLineY = lineY;
	});

	float pixelWidth = GetDipSizeFromPixelSize ({ 1, 0 }).width;
	float lineWidth = roundf(1.0f / pixelWidth) * pixelWidth;
	float x = GetNameColumnWidth() + lineWidth / 2;
	rt->DrawLine ({ x, 0 }, { x, lastLineY }, _grayTextBrush, lineWidth);
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
		if (item.pd->_labelGetter != nullptr)
			item.labelTL = TextLayout::Create (GetDWriteFactory(), _textFormat, item.pd->_labelGetter(_selectedObjects, _projectWindow->GetSelectedVlanNumber()).c_str(), maxWidth);
		else
			item.labelTL = TextLayout::Create (GetDWriteFactory(), _textFormat, item.pd->_name, maxWidth);
	}
}

void PropertyGrid::CreateValueTextLayouts()
{
	float pixelWidth = GetDipSizeFromPixelSize ({ 1, 0 }).width;
	float lineWidth = roundf(1.0f / pixelWidth) * pixelWidth;
	float maxWidth = std::max (100.0f, GetClientWidthDips()) - GetNameColumnWidth() - lineWidth - 2 * CellLRPadding;

	for (auto& item : _items)
	{
		auto str = item.pd->to_wstring (_selectedObjects[0], _projectWindow->GetSelectedVlanNumber());
		item.valueTL.layout = nullptr;
		for (size_t i = 1; i < _selectedObjects.size(); i++)
		{
			if (item.pd->to_wstring(_selectedObjects[i], _projectWindow->GetSelectedVlanNumber()) != str)
			{
				item.valueTL = TextLayout::Create (GetDWriteFactory(), _textFormat, L"(multiple selection)", maxWidth);
				break;
			}
		}

		if (item.valueTL.layout == nullptr)
			item.valueTL = TextLayout::Create (GetDWriteFactory(), _textFormat, str.c_str(), maxWidth);
	}
}

void PropertyGrid::ReloadPropertyValues()
{
	DiscardEditor();

	//throw not_implemented_exception()
	CreateValueTextLayouts();
	::InvalidateRect (GetHWnd(), NULL, FALSE);
}

static const UINT WM_CLOSE_EDITOR = WM_APP + 1;

LRESULT CALLBACK EditorWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_ACTIVATE)
		return 0; // DefWindowProc would set the keyboard focus

	if (msg == WM_MOUSEACTIVATE)
		return MA_NOACTIVATE;

	if ((msg == WM_COMMAND) && (HIWORD(wParam) == BN_CLICKED))
	{
		::PostMessage (hwnd, WM_CLOSE_EDITOR, LOWORD(wParam), 0);
		return 0;
	}

	return DefWindowProc (hwnd, msg, wParam, lParam);
}

int PropertyGrid::ShowEditor (POINT ptScreen, const NVP* nameValuePairs)
{
	HINSTANCE hInstance;
	BOOL bRes = ::GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) &EditorWndProc, &hInstance); assert(bRes);

	static ATOM atom = 0;
	if (atom == 0)
	{
		WNDCLASS EditorWndClass =
		{
			0, // style
			EditorWndProc,
			0, // cbClsExtra
			0, // cbWndExtra
			hInstance,
			nullptr, // hIcon
			::LoadCursor(nullptr, IDC_ARROW), // hCursor
			(HBRUSH) (COLOR_WINDOW + 1), // hbrBackground
			nullptr, // lpszMenuName
			L"GIGI", // lpszClassName
		};

		atom = ::RegisterClassW (&EditorWndClass); assert (atom != 0);
	}

	auto hwnd = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"GIGI", L"aaa", WS_POPUP | WS_BORDER, 0, 0, 0, 0, GetHWnd(), nullptr, hInstance, nullptr); assert (hwnd != nullptr);

	auto hdc = ::GetDC(hwnd);
	int dpiX = GetDeviceCaps (hdc, LOGPIXELSX);
	int dpiY = GetDeviceCaps (hdc, LOGPIXELSY);
	int padding = 7 * dpiX / 96;
	LONG buttonWidth = 100 * dpiX / 96;
	LONG buttonHeight = 0;
	for (auto nvp = nameValuePairs; nvp->first != nullptr; nvp++)
	{
		RECT rc = { };
		DrawTextW (hdc, nvp->first, -1, &rc, DT_CALCRECT);
		buttonWidth = max (buttonWidth, rc.right - rc.left + 2 * padding);
		buttonHeight = rc.bottom - rc.top + 2 * padding;
	}

	NONCLIENTMETRICS ncMetrics = { sizeof(NONCLIENTMETRICS) };
	SystemParametersInfo (SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncMetrics, 0);
	HFONT_unique_ptr font (CreateFontIndirect (&ncMetrics.lfMessageFont));

	int margin = 2 * dpiX / 96;
	int y = margin;
	for (auto nvp = nameValuePairs; nvp->first != nullptr; nvp++)
	{
		auto button = CreateWindowEx (0, L"Button", nvp->first, WS_CHILD | WS_VISIBLE | BS_NOTIFY, margin, y, buttonWidth, buttonHeight, hwnd, (HMENU) (INT_PTR) nvp->second, hInstance, nullptr);
		::SendMessage (button, WM_SETFONT, (WPARAM) font.get(), FALSE);
		y += buttonHeight + margin;
	}
	RECT wr = { 0, 0, margin + buttonWidth + margin, y };
	::AdjustWindowRectEx (&wr, (DWORD) GetWindowLongPtr(hwnd, GWL_STYLE), FALSE, (DWORD) GetWindowLongPtr(hwnd, GWL_EXSTYLE));
	::SetWindowPos (hwnd, nullptr, ptScreen.x, ptScreen.y, wr.right - wr.left, wr.bottom - wr.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
	::ReleaseDC (hwnd, hdc);

	int result = -1;
	MSG msg;
	while (GetMessage(&msg, 0, 0, 0))
	{
		if ((msg.hwnd == hwnd) && (msg.message == WM_CLOSE_EDITOR))
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
	if ((item != nullptr) && !item->pd->IsReadOnly())
	{
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
		else
			MessageBox (GetHWnd(), item->pd->_name, L"aaaa", 0);
	}
}
