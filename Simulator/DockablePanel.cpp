
#include "pch.h"
#include "Simulator.h"

using namespace std;

static ATOM WndClassAtom;
static constexpr wchar_t WndClassName[] = L"LogArea-{0428F330-C81A-4AC7-AEDA-D4E2BBEFFB03}";
static const float CloseButtonSizeDips = 16;
static const float CloseButtonMarginDips = 2;
static const float TitleBarHeightDips = CloseButtonMarginDips + CloseButtonSizeDips + CloseButtonMarginDips;
static const int SplitterWidthDips = 4;

class DockablePanel : public IDockablePanel
{
	Side _side;
	HWND _hwnd;
	SIZE _clientSize;
	HFONT_unique_ptr _titleBarFont;
	int _dpiX, _dpiY;
	bool _closeButtonDown = false;
	bool _draggingSplitter = false;
	POINT _draggingSplitterLastMouseScreenLocation;
	EventManager _em;
	wstring _title;

public:
	DockablePanel (HWND hWndParent, const RECT& rect, Side side, const wchar_t* title)
		: _side(side), _title(title)
	{
		HINSTANCE hInstance;
		BOOL bRes = GetModuleHandleEx (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) WndClassName, &hInstance);
		if (!bRes)
			throw win32_exception(GetLastError());

		if (WndClassAtom == 0)
		{
			WNDCLASSEX wcex;
			wcex.cbSize = sizeof (wcex);
			wcex.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
			wcex.lpfnWndProc = &WindowProcStatic;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hInstance;
			wcex.hIcon = nullptr;
			wcex.hCursor = LoadCursor (nullptr, IDC_ARROW);
			wcex.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
			wcex.lpszMenuName = nullptr;
			wcex.lpszClassName = WndClassName;
			wcex.hIconSm = 0;
			WndClassAtom = RegisterClassEx (&wcex);
			if (WndClassAtom == 0)
				throw win32_exception(GetLastError());
		}

		auto hwnd = ::CreateWindow (WndClassName, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
									rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
									hWndParent, nullptr, hInstance, this);
		if (hwnd == nullptr)
			throw win32_exception(GetLastError());
		assert (hwnd == _hwnd);

		HDC screen = GetDC(0);
		_dpiX = GetDeviceCaps (screen, LOGPIXELSX);
		_dpiY = GetDeviceCaps (screen, LOGPIXELSY);
		ReleaseDC (0, screen);

		NONCLIENTMETRICS ncMetrics = { sizeof(NONCLIENTMETRICS) };
		SystemParametersInfo (SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncMetrics, 0);
		auto f = CreateFontIndirect (&ncMetrics.lfCaptionFont);
		if (f == nullptr)
			throw win32_exception(GetLastError());
		_titleBarFont = HFONT_unique_ptr(f);
	}

	~DockablePanel()
	{
		if (_hwnd != nullptr)
			::DestroyWindow(_hwnd);
	}

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
			return SIZE { _clientSize.cx - SplitterSizePixels(), _clientSize.cy - TitleBarHeightPixels() };
		else
			return SIZE { _clientSize.cx, _clientSize.cy - SplitterSizePixels() - TitleBarHeightPixels() };
	}

	RECT GetSplitterRect()
	{
		switch (_side)
		{
			case Side::Left:
				return RECT { _clientSize.cx - SplitterSizePixels(), 0, _clientSize.cx, _clientSize.cy };

			case Side::Right:
				return RECT { 0, 0, SplitterSizePixels(), _clientSize.cy };

			case Side::Top:
				return RECT { 0, _clientSize.cy - SplitterSizePixels(), _clientSize.cx, _clientSize.cy };

			default:
				throw not_implemented_exception();
		}
	}

	RECT GetTitleBarRect()
	{
		switch (_side)
		{
			case Side::Left:
				return RECT { 0, 0, _clientSize.cx - SplitterSizePixels(), TitleBarHeightPixels() };

			case Side::Right:
				return RECT { SplitterSizePixels(), 0, _clientSize.cx, TitleBarHeightPixels() };

			case Side::Top:
				return RECT { 0, 0, _clientSize.cx, TitleBarHeightPixels() };

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

	static LRESULT CALLBACK WindowProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		DockablePanel* panel;
		if (uMsg == WM_NCCREATE)
		{
			LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			panel = reinterpret_cast<DockablePanel*>(lpcs->lpCreateParams);
			panel->_hwnd = hwnd;
			SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(panel));
		}
		else
			panel = reinterpret_cast<DockablePanel*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

		if (panel == nullptr)
		{
			// this must be one of those messages sent before WM_NCCREATE or after WM_NCDESTROY.
			return DefWindowProc (hwnd, uMsg, wParam, lParam);
		}

		auto result = panel->WindowProc (hwnd, uMsg, wParam, lParam);

		if (uMsg == WM_NCDESTROY)
		{
			panel->_hwnd = nullptr;
			SetWindowLongPtr (hwnd, GWLP_USERDATA, 0);
		}

		if (result)
			return result.value();

		return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	std::optional<LRESULT> WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_CREATE)
		{
			auto cs = reinterpret_cast<const CREATESTRUCT*>(lParam);
			_clientSize = { cs->cx, cs->cy };
			return 0;
		}
		else if (msg == WM_SIZE)
		{
			_clientSize = { LOWORD(lParam), HIWORD(lParam) };

			auto contentHWnd = ::GetWindow (_hwnd, GW_CHILD);
			if (contentHWnd != nullptr)
			{
				auto cr = GetContentRect();
				::MoveWindow (contentHWnd, cr.left, cr.top, cr.right - cr.left, cr.bottom - cr.top, TRUE);
			}

			return 0;
		}
		else if (msg == WM_ERASEBKGND)
		{
			return 1;
		}
		else if (msg == WM_PAINT)
		{
			ProcessWmPaint();
			return 0;
		}
		else if (msg == WM_LBUTTONDOWN)
		{
			ProcessLButtonDown ((DWORD) wParam, POINT { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
			return 0;
		}
		else if (msg == WM_LBUTTONUP)
		{
			ProcessLButtonUp ((DWORD) wParam, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
			return 0;
		}
		else if (msg == WM_MOUSEMOVE)
		{
			ProcessMouseMove ((DWORD) wParam, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
			return 0;
		}
		else if (msg == WM_SETCURSOR)
		{
			if (((HWND) wParam == _hwnd) && (LOWORD (lParam) == HTCLIENT))
			{
				POINT pt;
				BOOL bRes = GetCursorPos (&pt);
				// Let's check the result because GetCursorPos fails when the input desktop is not the current desktop
				// (happens for example when the monitor goes to sleep and then the lock screen is displayed).
				if (bRes)
				{
					bRes = ScreenToClient (_hwnd, &pt);
					if (bRes)
						ProcessWmSetCursor (pt);
					return 0;
				}
			}

			return nullopt;
		}

		return nullopt;
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

	void ProcessWmPaint()
	{
		PAINTSTRUCT ps;
		auto hdc = BeginPaint(_hwnd, &ps);

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
			DrawTextW (ps.hdc, _title.c_str(), _title.length(), &titleBarTextRect, DT_SINGLELINE | DT_VCENTER);
			SelectObject (ps.hdc, oldSelectedFont);
		}

		auto splitterRect = GetSplitterRect();
		HBRUSH splitterBrush = GetSysColorBrush (COLOR_3DFACE);
		FillRect (ps.hdc, &splitterRect, splitterBrush);

		EndPaint (_hwnd, &ps);
	}

	void ProcessLButtonDown (DWORD modifierKeys, POINT pt)
	{
		SetCapture (_hwnd);

		auto closeButtonRect = GetCloseButtonRect();
		if (PtInRect (&closeButtonRect, pt))
		{
			_closeButtonDown = true;
			InvalidateRect (_hwnd, &closeButtonRect, FALSE);
		}

		auto splitterRect = GetSplitterRect();
		if (PtInRect (&splitterRect, pt))
		{
			_draggingSplitter = true;
			_draggingSplitterLastMouseScreenLocation = pt;
			ClientToScreen (_hwnd, &_draggingSplitterLastMouseScreenLocation);
		}
	}

	void ProcessLButtonUp (DWORD modifierKeys, POINT pixelLocation)
	{
		ReleaseCapture();

		if (_closeButtonDown)
		{
			_closeButtonDown = false;
			::InvalidateRect (_hwnd, nullptr, FALSE);
			CloseButtonClicked::InvokeHandlers (_em, this);
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
				InvalidateRect (_hwnd, &closeButtonRect, FALSE);
			}
		}
		else if (_draggingSplitter)
		{
			POINT ptScreen = pt;
			::ClientToScreen(_hwnd, &ptScreen);

			SIZE offset = { ptScreen.x - _draggingSplitterLastMouseScreenLocation.x, ptScreen.y - _draggingSplitterLastMouseScreenLocation.y };

			SIZE proposedSize;
			if (_side == Side::Left)
				proposedSize = { _clientSize.cx + offset.cx, _clientSize.cy };
			else if (_side == Side::Right)
				proposedSize = { _clientSize.cx - offset.cx, _clientSize.cy };
			else if (_side == Side::Top)
				proposedSize = { _clientSize.cx, _clientSize.cy + offset.cy };
			else
				throw not_implemented_exception();

			SplitterDragging::InvokeHandlers(_em, this, proposedSize);

			_draggingSplitterLastMouseScreenLocation = ptScreen;
		}
	}

	virtual HWND GetHWnd() const override final { return _hwnd; }

	virtual CloseButtonClicked::Subscriber GetCloseButtonClickedEvent() override final { return CloseButtonClicked::Subscriber(_em); }

	virtual SplitterDragging::Subscriber GetSplitterDraggingEvent() override final { return SplitterDragging::Subscriber(_em); }

	virtual SIZE GetPanelSizeFromContentSize (SIZE contentSize) const override final
	{
		if (_side == Side::Top)
			return SIZE { contentSize.cx, TitleBarHeightPixels() + contentSize.cy + SplitterSizePixels() };

		throw not_implemented_exception();
	}
};

extern const DockablePanelFactory dockablePanelFactory = [](auto... params) { return unique_ptr<IDockablePanel>(new DockablePanel(params...)); };
