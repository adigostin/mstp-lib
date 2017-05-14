
#include "pch.h"
#include "Window.h"

using namespace std;

static ATOM WndClassAtom;
static constexpr wchar_t WndClassName[] = L"DockablePanel-{669FD954-5F64-4073-ADDC-33AFA47190D8}";
static const float CloseButtonSizeDips = 16;
static const float CloseButtonMarginDips = 2;
static const float TitleBarHeightDips = CloseButtonMarginDips + CloseButtonSizeDips + CloseButtonMarginDips;
static const int SplitterWidthDips = 4;

class DockablePanel : public Window, public IDockablePanel
{
	using base = Window;

	string const _panelUniqueName;
	Side _side;
	HFONT_unique_ptr _titleBarFont;
	int _dpiX, _dpiY;
	bool _closeButtonDown = false;
	bool _draggingSplitter = false;
	POINT _draggingSplitterLastMouseScreenLocation;
	wstring _title;

public:
	DockablePanel (HINSTANCE hInstance, const char* panelUniqueName, HWND hWndParent, const RECT& rect, Side side, const wchar_t* title)
		: base (hInstance, WndClassName, 0, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, rect, hWndParent, nullptr)
		, _panelUniqueName(panelUniqueName)
		, _side(side)
		, _title(title)
	{
		HDC screen = GetDC(GetHWnd());
		_dpiX = GetDeviceCaps (screen, LOGPIXELSX);
		_dpiY = GetDeviceCaps (screen, LOGPIXELSY);
		ReleaseDC (GetHWnd(), screen);

		NONCLIENTMETRICS ncMetrics = { sizeof(NONCLIENTMETRICS) };
		SystemParametersInfo (SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncMetrics, 0);
		auto f = CreateFontIndirect (&ncMetrics.lfCaptionFont);
		if (f == nullptr)
			throw win32_exception(GetLastError());
		_titleBarFont = HFONT_unique_ptr(f);
	}

	virtual const std::string& GetUniqueName() const override final { return _panelUniqueName; }

	virtual Side GetSide() const override final { return _side; }

	LONG SplitterSizePixels()   const { return (LONG) (SplitterWidthDips  * _dpiX / 96.0); }

	LONG TitleBarHeightPixels() const { return (LONG) (TitleBarHeightDips * _dpiY / 96.0); }

	virtual POINT GetContentLocation() const override final
	{
		switch (_side)
		{
			case Side::Left:
				return POINT { 0, TitleBarHeightPixels() };

			case Side::Right:
				return POINT { SplitterSizePixels(), TitleBarHeightPixels() };

			case Side::Top:
				return POINT { 0, TitleBarHeightPixels() };

			default:
				throw not_implemented_exception();
		}
	}

	virtual SIZE GetContentSize() const override final
	{
		if ((_side == Side::Left) || (_side == Side::Right))
			return SIZE { GetClientWidthPixels() - SplitterSizePixels(), GetClientHeightPixels() - TitleBarHeightPixels() };
		else
			return SIZE { GetClientWidthPixels(), GetClientHeightPixels() - SplitterSizePixels() - TitleBarHeightPixels() };
	}

	RECT GetSplitterRect()
	{
		switch (_side)
		{
			case Side::Left:
				return RECT { GetClientWidthPixels() - SplitterSizePixels(), 0, GetClientWidthPixels(), GetClientHeightPixels() };

			case Side::Right:
				return RECT { 0, 0, SplitterSizePixels(), GetClientHeightPixels() };

			case Side::Top:
				return RECT { 0, GetClientHeightPixels() - SplitterSizePixels(), GetClientWidthPixels(), GetClientHeightPixels() };

			default:
				throw not_implemented_exception();
		}
	}

	RECT GetTitleBarRect()
	{
		switch (_side)
		{
			case Side::Left:
				return RECT { 0, 0, GetClientWidthPixels() - SplitterSizePixels(), TitleBarHeightPixels() };

			case Side::Right:
				return RECT { SplitterSizePixels(), 0, GetClientWidthPixels(), TitleBarHeightPixels() };

			case Side::Top:
				return RECT { 0, 0, GetClientWidthPixels(), TitleBarHeightPixels() };

			default:
				throw not_implemented_exception();
		}
	}

	RECT GetCloseButtonRect()
	{
		LONG closeButtonMarginPixels = (LONG) (CloseButtonMarginDips * _dpiY / 96.0);

		auto rect = GetTitleBarRect();
		rect.right -= closeButtonMarginPixels;
		rect.top += closeButtonMarginPixels;
		rect.bottom -= closeButtonMarginPixels;
		rect.left = rect.right - (rect.bottom - rect.top);
		return rect;
	}

	virtual std::optional<LRESULT> WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		auto resultBaseClass = base::WindowProc (hwnd, msg, wParam, lParam);

		if (msg == WM_SIZE)
		{
			auto contentHWnd = ::GetWindow (hwnd, GW_CHILD);
			if (contentHWnd != nullptr)
			{
				auto cr = GetContentRect();
				::MoveWindow (contentHWnd, cr.left, cr.top, cr.right - cr.left, cr.bottom - cr.top, TRUE);
			}

			return 0;
		}

		if (msg == WM_ERASEBKGND)
			return 1;

		if (msg == WM_PAINT)
		{
			ProcessWmPaint(hwnd);
			return 0;
		}

		if (msg == WM_LBUTTONDOWN)
		{
			ProcessLButtonDown ((DWORD) wParam, POINT { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
			return 0;
		}

		if (msg == WM_LBUTTONUP)
		{
			ProcessLButtonUp ((DWORD) wParam, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
			return 0;
		}

		if (msg == WM_MOUSEMOVE)
		{
			ProcessMouseMove ((DWORD) wParam, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
			return 0;
		}

		if (msg == WM_SETCURSOR)
		{
			if (((HWND) wParam == hwnd) && (LOWORD (lParam) == HTCLIENT))
			{
				POINT pt;
				BOOL bRes = GetCursorPos (&pt);
				// Let's check the result because GetCursorPos fails when the input desktop is not the current desktop
				// (happens for example when the monitor goes to sleep and then the lock screen is displayed).
				if (bRes)
				{
					bRes = ScreenToClient (hwnd, &pt);
					if (bRes)
						ProcessWmSetCursor (pt);
					return 0;
				}
			}

			return nullopt;
		}

		if (msg == WM_STYLECHANGED)
		{
			const STYLESTRUCT* ss = (const STYLESTRUCT*) lParam;
			if (wParam == GWL_STYLE)
			{
				auto changed = ss->styleNew ^ ss->styleOld;
				if (changed & WS_VISIBLE)
					VisibleChangedEvent::InvokeHandlers (*this, this, (ss->styleNew & WS_VISIBLE) != 0);
			}

			return 0;
		}

		return resultBaseClass;
	}

	void ProcessWmSetCursor (POINT pt)
	{
		auto splitterRect = GetSplitterRect();
		if (PtInRect (&splitterRect, pt))
		{
			if ((_side == Side::Left) || (_side == Side::Right))
				SetCursor (LoadCursor (nullptr, IDC_SIZEWE));
			else
				SetCursor (LoadCursor (nullptr, IDC_SIZENS));
			return;
		}

		auto closeButtonRect = GetCloseButtonRect();
		if (PtInRect (&closeButtonRect, pt))
		{
			SetCursor (LoadCursor (nullptr, IDC_HAND));
			return;
		}

		SetCursor (LoadCursor (nullptr, IDC_ARROW));
	}

	void ProcessWmPaint (HWND hwnd)
	{
		PAINTSTRUCT ps;
		auto hdc = BeginPaint(hwnd, &ps);

		RECT titleBarRect = GetTitleBarRect();
		FillRect (ps.hdc, &titleBarRect, GetSysColorBrush (COLOR_ACTIVECAPTION));

		RECT closeButtonRect = GetCloseButtonRect();
		UINT buttonStateValue = DFCS_CAPTIONCLOSE | DFCS_FLAT | (_closeButtonDown ? DFCS_PUSHED : 0);
		DrawFrameControl (ps.hdc, &closeButtonRect, DFC_CAPTION, buttonStateValue);

		if (_title.length() > 0)
		{
			SetBkMode (ps.hdc, TRANSPARENT);
			SetTextColor (ps.hdc, GetSysColor (COLOR_CAPTIONTEXT));

			RECT titleBarTextRect = { titleBarRect.left + 6, titleBarRect.top, titleBarRect.right, titleBarRect.bottom };
			auto oldSelectedFont = SelectObject (ps.hdc, _titleBarFont.get());
			DrawTextW (ps.hdc, _title.c_str(), (int) _title.length(), &titleBarTextRect, DT_SINGLELINE | DT_VCENTER);
			SelectObject (ps.hdc, oldSelectedFont);
		}

		auto splitterRect = GetSplitterRect();
		HBRUSH splitterBrush = GetSysColorBrush (COLOR_3DFACE);
		FillRect (ps.hdc, &splitterRect, splitterBrush);

		EndPaint (hwnd, &ps);
	}

	void ProcessLButtonDown (DWORD modifierKeys, POINT pt)
	{
		SetCapture (GetHWnd());

		auto closeButtonRect = GetCloseButtonRect();
		if (PtInRect (&closeButtonRect, pt))
		{
			_closeButtonDown = true;
			InvalidateRect (GetHWnd(), &closeButtonRect, FALSE);
		}

		auto splitterRect = GetSplitterRect();
		if (PtInRect (&splitterRect, pt))
		{
			_draggingSplitter = true;
			_draggingSplitterLastMouseScreenLocation = pt;
			ClientToScreen (GetHWnd(), &_draggingSplitterLastMouseScreenLocation);
		}
	}

	void ProcessLButtonUp (DWORD modifierKeys, POINT pixelLocation)
	{
		ReleaseCapture();

		if (_closeButtonDown)
		{
			_closeButtonDown = false;
			auto style = GetWindowLongPtr(GetHWnd(), GWL_STYLE);
			style ^= WS_VISIBLE;
			::SetWindowLongPtr (GetHWnd(), GWL_STYLE, style);
		}
		else if (_draggingSplitter)
		{
			/*
			RECT rect;
			GetWindowRect (_hwnd, &rect);
			DWORD width = rect.right - rect.left;
			HKEY key;
			auto lstatus = RegCreateKeyEx (HKEY_CURRENT_USER, _app->GetRegKeyPath(), 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
			if (lstatus == ERROR_SUCCESS)
			{
			RegSetValueEx (key, WidthRegValueName, 0, REG_DWORD, (BYTE*) &width, 4);
			RegCloseKey (key);
			}
			*/
			_draggingSplitter = false;
		}
	}

	void ProcessMouseMove (DWORD mouseButtonsAndModifierKeysDown, POINT pt)
	{
		if (_closeButtonDown)
		{
			auto closeButtonRect = GetCloseButtonRect();
			if (!PtInRect (&closeButtonRect, pt))
			{
				_closeButtonDown = false;
				InvalidateRect (GetHWnd(), &closeButtonRect, FALSE);
			}
		}
		else if (_draggingSplitter)
		{
			POINT ptScreen = pt;
			::ClientToScreen(GetHWnd(), &ptScreen);

			SIZE offset = { ptScreen.x - _draggingSplitterLastMouseScreenLocation.x, ptScreen.y - _draggingSplitterLastMouseScreenLocation.y };

			SIZE proposedSize;
			if (_side == Side::Left)
				proposedSize = { GetClientWidthPixels() + offset.cx, GetClientHeightPixels() };
			else if (_side == Side::Right)
				proposedSize = { GetClientWidthPixels() - offset.cx, GetClientHeightPixels() };
			else if (_side == Side::Top)
				proposedSize = { GetClientWidthPixels(), GetClientHeightPixels() + offset.cy };
			else
				throw not_implemented_exception();

			SplitterDragging::InvokeHandlers(*this, this, proposedSize);

			_draggingSplitterLastMouseScreenLocation = ptScreen;
		}
	}

	virtual VisibleChangedEvent::Subscriber GetVisibleChangedEvent() override final { return VisibleChangedEvent::Subscriber(this); }

	virtual SplitterDragging::Subscriber GetSplitterDraggingEvent() override final { return SplitterDragging::Subscriber(this); }

	virtual SIZE GetPanelSizeFromContentSize (SIZE contentSize) const override final
	{
		if (_side == Side::Left)
			return { contentSize.cx + SplitterSizePixels(), TitleBarHeightPixels() + contentSize.cy };

		if (_side == Side::Top)
			return SIZE { contentSize.cx, TitleBarHeightPixels() + contentSize.cy + SplitterSizePixels() };

		throw not_implemented_exception();
	}

	virtual HWND GetHWnd() const { return base::GetHWnd(); }
	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return base::QueryInterface(riid, ppvObject); }
	virtual ULONG STDMETHODCALLTYPE AddRef() override { return base::AddRef(); }
	virtual ULONG STDMETHODCALLTYPE Release() override { return base::Release(); }
};

template<typename... Args>
static IDockablePanelPtr Create (Args... args)
{
	return IDockablePanelPtr (new DockablePanel (std::forward<Args>(args)...), false);
}

const DockablePanelFactory dockablePanelFactory = &Create;
