
#include "pch.h"
#include "SimulatorDefs.h"
#include "D2DWindow.h"

using namespace std;
using namespace D2D1;

class LogTextArea : public D2DWindow
{
	typedef D2DWindow base;
	ComPtr<IDWriteFactory> _dWriteFactory;
	ComPtr<IDWriteTextFormat> _textFormat;
	ComPtr<ID2D1SolidColorBrush> _windowBrush;
	ComPtr<ID2D1SolidColorBrush> _windowTextBrush;

public:
	LogTextArea (HWND hWndParent, DWORD controlId, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory)
		: base (WS_EX_CLIENTEDGE, WS_VISIBLE | WS_CHILD | WS_HSCROLL | WS_VSCROLL, rect, hWndParent, controlId, deviceContext, dWriteFactory, wicFactory)
		, _dWriteFactory(dWriteFactory)
	{
		auto hr = dWriteFactory->CreateTextFormat (L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 14, L"en-US", &_textFormat); ThrowIfFailed(hr);

		GetDeviceContext()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOW), &_windowBrush);
		GetDeviceContext()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOWTEXT), &_windowTextBrush);
	}
		
	virtual void Render(ID2D1DeviceContext* dc) const override final
	{
		dc->Clear(GetD2DSystemColor(COLOR_WINDOW));
		
		static constexpr wchar_t Text[] = L"The STP activity log is shown here. Select a bridge to see its log.";
		
		auto clientSize = GetClientSizeDips();

		auto oldta = _textFormat->GetTextAlignment();
		_textFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_CENTER);
		ComPtr<IDWriteTextLayout> tl;
		auto hr = _dWriteFactory->CreateTextLayout (Text, wcslen(Text), _textFormat, clientSize.width, 10000, &tl); ThrowIfFailed(hr);
		_textFormat->SetTextAlignment(oldta);

		DWRITE_TEXT_METRICS metrics;
		tl->GetMetrics (&metrics);
		dc->DrawTextLayout ({ clientSize.width / 2 - metrics.width / 2 - metrics.left, clientSize.height / 2 }, tl, _windowTextBrush);
	}
};

static ATOM WndClassAtom;
static constexpr wchar_t WndClassName[] = L"LogArea-{0428F330-C81A-4AC7-AEDA-D4E2BBEFFB03}";
static const float CloseButtonSizeDips = 16;
static const float CloseButtonMarginDips = 2;
static const float TitleBarHeightDips = CloseButtonMarginDips + CloseButtonSizeDips + CloseButtonMarginDips;
static const int SplitterWidthDips = 4;

class LogArea : public ILogArea
{
	ULONG _refCount = 1;
	HWND _hwnd;
	unique_ptr<LogTextArea> _textArea;
	SIZE _clientSize;
	HFONT_unique_ptr _titleBarFont;
	int _dpiX, _dpiY;
	bool _closeButtonDown = false;
	bool _draggingSplitter = false;
	POINT _draggingSplitterLastMouseLocation;
	EventManager _em;

public:
	LogArea (HWND hWndParent, DWORD controlId, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory)
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
		
		auto hwnd = ::CreateWindowEx (0, WndClassName, L"STP Log", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPCHILDREN,
			rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, hWndParent, (HMENU) controlId, hInstance, this);
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

		auto splitterRect = GetSplitterRect();
		LONG titleBarHeightPixels = (LONG) (TitleBarHeightDips * _dpiY / 96.0);
		auto areaRect = RECT{ 0, titleBarHeightPixels, splitterRect.left, _clientSize.cy };
		_textArea = unique_ptr<LogTextArea>(new LogTextArea (_hwnd, 1, areaRect, deviceContext, dWriteFactory, wicFactory));
	}

	~LogArea()
	{
		_textArea = nullptr;
		::DestroyWindow(_hwnd);
	}

	RECT GetSplitterRect()
	{
		LONG titleBarHeightPixels = (LONG) (TitleBarHeightDips * _dpiY / 96.0);
		LONG splitterWidthPixels = (LONG) (SplitterWidthDips * _dpiX / 96.0);
		return RECT { 0, titleBarHeightPixels, splitterWidthPixels, _clientSize.cy };
	}

	RECT GetTitleBarRect()
	{
		LONG titleBarHeightPixels = (LONG) (TitleBarHeightDips * _dpiY / 96.0);
		return RECT { 0, 0, _clientSize.cx, titleBarHeightPixels };
	}

	RECT GetCloseButtonRect()
	{
		LONG closeButtonSizePixels = (LONG) (CloseButtonSizeDips * _dpiY / 96.0);
		LONG closeButtonMarginPixels = (LONG) (CloseButtonMarginDips * _dpiY / 96.0);

		RECT rect;
		rect.left = _clientSize.cx - closeButtonMarginPixels - closeButtonSizePixels;
		rect.top = closeButtonMarginPixels;
		rect.right = rect.left + closeButtonSizePixels;
		rect.bottom = rect.top + closeButtonSizePixels;

		// Move it slightly, it looks better.
		rect.left--;
		rect.right--;
		rect.top++;
		rect.bottom++;

		return rect;
	}

	void ResizeChildWindows()
	{
		if (_textArea != nullptr)
		{
			LONG titleBarHeightPixels = (LONG) (TitleBarHeightDips * _dpiY / 96.0);
			LONG splitterWidthPixels  = (LONG) (SplitterWidthDips * _dpiX / 96.0);
			MoveWindow (_textArea->GetHWnd(), splitterWidthPixels, titleBarHeightPixels, _clientSize.cx - splitterWidthPixels, _clientSize.cy - titleBarHeightPixels, TRUE);
		}
	}

	static LRESULT CALLBACK WindowProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		LogArea* window;
		if (uMsg == WM_NCCREATE)
		{
			LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			window = reinterpret_cast<LogArea*>(lpcs->lpCreateParams);
			window->_hwnd = hwnd;
			SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
		}
		else
			window = reinterpret_cast<LogArea*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

		if (window == nullptr)
		{
			// this must be one of those messages sent before WM_NCCREATE or after WM_NCDESTROY.
			return DefWindowProc (hwnd, uMsg, wParam, lParam);
		}

		auto result = window->WindowProc (hwnd, uMsg, wParam, lParam);

		if (uMsg == WM_NCDESTROY)
		{
			window->_hwnd = nullptr;
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

			//RECT rect;
			//GetWindowRect (_hwnd, &rect);
			//MapWindowPoints (HWND_DESKTOP, GetParent(_hwnd), (LPPOINT) &rect, 2);
			//LogAreaRectChangedEvent::InvokeHandlers (_em, this, rect);

			ResizeChildWindows();
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
		RECT clientRect;
		GetClientRect (_hwnd, &clientRect);

		auto splitterRect = GetSplitterRect();
		if (PtInRect (&splitterRect, pt))
		{
			SetCursor (LoadCursor (nullptr, IDC_SIZEWE));
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

		int textLength = GetWindowTextLength (_hwnd);
		if (textLength > 0)
		{
			unique_ptr<wchar_t[]> text (new wchar_t[textLength + 1]);
			GetWindowText (_hwnd, text.get(), textLength + 1);

			SetBkMode (ps.hdc, TRANSPARENT);
			SetTextColor (ps.hdc, GetSysColor (COLOR_CAPTIONTEXT));

			RECT titleBarTextRect = { titleBarRect.left + 10, titleBarRect.top, titleBarRect.right, titleBarRect.bottom };
			auto oldSelectedFont = SelectObject (ps.hdc, _titleBarFont.get());
			DrawTextW (ps.hdc, text.get(), -1, &titleBarTextRect, DT_SINGLELINE | DT_VCENTER);
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
			_draggingSplitterLastMouseLocation = pt;
		}
	}

	void ProcessLButtonUp (DWORD modifierKeys, POINT pixelLocation)
	{
		ReleaseCapture();

		if (_closeButtonDown)
		{
			_closeButtonDown = false;
			::InvalidateRect (_hwnd, nullptr, FALSE);
			LogAreaCloseButtonClicked::InvokeHandlers (_em, this);
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
		RECT clientRect;
		::GetClientRect (_hwnd, &clientRect);
		const RECT closeButtonRect = GetCloseButtonRect();

		if (_closeButtonDown)
		{
			if (!PtInRect (&closeButtonRect, pt))
			{
				_closeButtonDown = false;
				InvalidateRect (_hwnd, &closeButtonRect, FALSE);
			}
		}
		else if (_draggingSplitter)
		{
			LONG offset = pt.x - _draggingSplitterLastMouseLocation.x;
			LogAreaResizingEvent::InvokeHandlers (_em, this, Side::Left, offset);
		}
	}

	virtual HWND GetHWnd() const override final { return _hwnd; }

	virtual LogAreaCloseButtonClicked::Subscriber GetLogAreaCloseButtonClicked() override final { return LogAreaCloseButtonClicked::Subscriber(_em); }

	virtual LogAreaResizingEvent::Subscriber GetLogAreaResizingEvent() override final { return LogAreaResizingEvent::Subscriber(_em); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override final { throw exception("Not implemented."); }

	virtual ULONG STDMETHODCALLTYPE AddRef() override final
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override final
	{
		auto newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
	#pragma endregion};
};

extern const LogAreaFactory logAreaFactory = [](auto... params) { return ComPtr<ILogArea>(new LogArea(params...), false); };
