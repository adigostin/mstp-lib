
#include "pch.h"
#include "SimulatorDefs.h"
#include "D2DWindow.h"

using namespace std;
using namespace D2D1;

class LogTextArea : public D2DWindow
{
	friend class LogArea;

	typedef D2DWindow base;
	ComPtr<IDWriteFactory> _dWriteFactory;
	ComPtr<IDWriteTextFormat> _textFormat;
	ComPtr<ID2D1SolidColorBrush> _windowBrush;
	ComPtr<ID2D1SolidColorBrush> _windowTextBrush;
	ComPtr<Bridge> _bridge;
	int _selectedPort = -1;
	int _selectedTree = -1;
	vector<string> _lines;
	UINT_PTR _timerId = 0;
	int _animationCurrentLineCount = 0;
	int _animationEndLineCount = 0;
	UINT _animationScrollFramesRemaining = 0;
	int _topLineIndex = 0;
	int _numberOfLinesFitting = 10;
	static constexpr UINT AnimationDurationMilliseconds = 75;
	static constexpr UINT AnimationScrollFramesMax = 10;

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

	~LogTextArea()
	{
		if (_bridge != nullptr)
			_bridge->GetBridgeLogLineGeneratedEvent().RemoveHandler (OnLogLineGeneratedStatic, this);
	}

	virtual void Render(ID2D1DeviceContext* dc) const override final
	{
		dc->Clear(GetD2DSystemColor(COLOR_WINDOW));

		auto clientSize = GetClientSizeDips();

		if ((_bridge == nullptr) || _lines.empty())
		{
			static constexpr wchar_t TextNoBridge[] = L"The STP activity log is shown here.\r\nSelect a bridge to see its log.";
			static constexpr wchar_t TextNoEntries[] = L"No log text generated yet.\r\nYou may want to enable STP on the selected bridge.";

			auto oldta = _textFormat->GetTextAlignment();
			_textFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_CENTER);
			ComPtr<IDWriteTextLayout> tl;
			auto text = (_bridge == nullptr) ? TextNoBridge : TextNoEntries;
			auto hr = _dWriteFactory->CreateTextLayout (text, wcslen(text), _textFormat, clientSize.width, 10000, &tl); ThrowIfFailed(hr);
			_textFormat->SetTextAlignment(oldta);
			DWRITE_TEXT_METRICS metrics;
			tl->GetMetrics (&metrics);
			dc->DrawTextLayout ({ clientSize.width / 2 - metrics.width / 2 - metrics.left, clientSize.height / 2 }, tl, _windowTextBrush);
		}
		else
		{
			wstring_convert<codecvt_utf8<wchar_t>> converter;
			wstring line;

			float y = 0;
			float lineHeight = 0;
			for (int lineIndex = _topLineIndex; (lineIndex < _animationCurrentLineCount) && (y < clientSize.height); lineIndex++)
			{
				line = converter.from_bytes(_lines[lineIndex]);
				
				if ((line.length() >= 2) && (line[line.length() - 2] == '\r') && (line[line.length() - 1] == '\n'))
					line.resize (line.length() - 2);

				ComPtr<IDWriteTextLayout> tl;
				auto hr = _dWriteFactory->CreateTextLayout (line.c_str(), (UINT32) line.length(), _textFormat, 10000, 10000, &tl); ThrowIfFailed(hr);

				if (lineHeight == 0)
				{
					DWRITE_TEXT_METRICS metrics;
					tl->GetMetrics (&metrics);
					lineHeight = metrics.height;
				}

				dc->DrawTextLayout ({ 0, y }, tl, _windowTextBrush, D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
				y += lineHeight;

				if (y >= clientSize.height)
					break;
			}
		}
	}

	static void OnLogLineGeneratedStatic (void* callbackArg, Bridge* b, const BridgeLogLine& ll)
	{
		static_cast<LogTextArea*>(callbackArg)->OnLogLineGenerated(ll);
	}

	void OnLogLineGenerated (const BridgeLogLine& ll)
	{
		if (((_selectedPort == -1) || (_selectedPort == ll.portIndex))
			&& ((_selectedTree == -1) || (_selectedTree == ll.treeIndex)))
		{
			_lines.push_back(ll.text);

			bool lastLineVisible = (_topLineIndex + _numberOfLinesFitting >= _animationCurrentLineCount);

			if (!lastLineVisible)
			{
				// The user has scrolled away from the last time. We append the text without doing any scrolling.

				// If the user scrolled away, the animation is supposed to have been stopped.
				assert (_animationCurrentLineCount == _animationEndLineCount);
				assert (_animationScrollFramesRemaining == 0);

				SCROLLINFO si = { sizeof (si) };
				si.fMask = SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
				si.nMin = 0;
				si.nMax = (int) _lines.size() - 1;
				si.nPage = _numberOfLinesFitting;
				SetScrollInfo (GetHWnd(), SB_VERT, &si, TRUE);

				_animationCurrentLineCount = (int) _lines.size();
				_animationEndLineCount     = (int) _lines.size();

				InvalidateRect(GetHWnd(), nullptr, FALSE);
			}
			else
			{
				// The last line is visible, meaning that the user didn't scroll away from it.
				// An animation might be pending or not. In any case, we restart it with the new parameters.
				_animationEndLineCount = (int) _lines.size();
				_animationScrollFramesRemaining = AnimationScrollFramesMax;

				if (_timerId != 0)
				{
					KillTimer (GetHWnd(), _timerId);
					_timerId = 0;
				}

				UINT animationFrameLengthMilliseconds = AnimationDurationMilliseconds / AnimationScrollFramesMax;
				_timerId = SetTimer (GetHWnd(), 1, animationFrameLengthMilliseconds, NULL);
			}
		}
	}

	void SelectBridge (Bridge* b)
	{
		if (_bridge.Get() != b)
		{
			if (_bridge != nullptr)
			{
				if (_animationScrollFramesRemaining > 0)
					EndAnimation();

				_lines.clear();
				_bridge->GetBridgeLogLineGeneratedEvent().RemoveHandler (OnLogLineGeneratedStatic, this);
				_bridge = nullptr;
			}

			_bridge = b;

			if (b != nullptr)
			{
				for (auto& ll : _bridge->GetLogLines())
				{
					if (((_selectedPort == -1) || (_selectedPort == ll.portIndex))
						&& ((_selectedTree == -1) || (_selectedTree == ll.treeIndex)))
					{
						_lines.push_back(ll.text);
					}
				}

				_bridge->GetBridgeLogLineGeneratedEvent().AddHandler (OnLogLineGeneratedStatic, this);
			}

			_topLineIndex = max (0, (int) _lines.size() - _numberOfLinesFitting);
			_animationCurrentLineCount = (int) _lines.size();
			_animationEndLineCount     = (int) _lines.size();

			SCROLLINFO si = { sizeof (si) };
			si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
			si.nMin = 0;
			si.nMax = (int) _lines.size() - 1;
			si.nPage = _numberOfLinesFitting;
			si.nPos = _topLineIndex;
			SetScrollInfo (GetHWnd(), SB_VERT, &si, TRUE);

			InvalidateRect (GetHWnd(), nullptr, FALSE);
		}
	}

	virtual optional<LRESULT> WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override final
	{
		if (msg == WM_SIZE)
		{
			base::WindowProc (hwnd, msg, wParam, lParam); // Pass it to the base class first, which stores the client size.
			ProcessWmSize (wParam, lParam);
			return 0;
		}

		if (msg == WM_VSCROLL)
		{
			ProcessWmVScroll (wParam, lParam);
			return 0;
		}

		if ((msg == WM_TIMER) && (_timerId != 0) && (wParam == _timerId))
		{
			ProcessAnimationTimer();
			return 0;
		}

		if (msg == WM_MOUSEWHEEL)
		{
			ProcessWmMouseWheel (wParam, lParam);
			return 0;
		}

		return base::WindowProc (hwnd, msg, wParam, lParam);
	}

	void ProcessAnimationTimer()
	{
		assert (_animationEndLineCount != _animationCurrentLineCount);
		assert (_animationScrollFramesRemaining != 0);

		size_t linesToAddInThisAnimationFrame = (_animationEndLineCount - _animationCurrentLineCount) / _animationScrollFramesRemaining;
		_animationCurrentLineCount += linesToAddInThisAnimationFrame;

		if (_animationCurrentLineCount <= _numberOfLinesFitting)
			_topLineIndex = 0;
		else
			_topLineIndex = _animationCurrentLineCount - _numberOfLinesFitting;

		InvalidateRect (GetHWnd(), nullptr, FALSE);

		// Need to set SIF_DISABLENOSCROLL due to what seems like a Windows bug:
		// GetScrollInfo returns garbage if called right after SetScrollInfo, if SetScrollInfo made the scroll bar change from invisible to visible, 
		SCROLLINFO si = { sizeof (si) };
		si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
		si.nMin = 0;
		si.nMax = _animationCurrentLineCount - 1;
		si.nPage = _numberOfLinesFitting;
		si.nPos = _topLineIndex;
		SetScrollInfo (GetHWnd(), SB_VERT, &si, TRUE);

		if (_timerId != 0)
		{
			KillTimer (GetHWnd(), _timerId);
			_timerId = 0;
		}

		_animationScrollFramesRemaining--;
		if (_animationScrollFramesRemaining > 0)
		{
			UINT animationFrameLengthMilliseconds = AnimationDurationMilliseconds / AnimationScrollFramesMax;
			_timerId = SetTimer (GetHWnd(), (UINT_PTR) 1, animationFrameLengthMilliseconds, NULL);
			if (_timerId == 0)
				throw win32_exception(GetLastError());
		}
	}

	void ProcessWmSize (WPARAM wParam, LPARAM lParam)
	{
		bool isLastLineVisible = (_topLineIndex + _numberOfLinesFitting >= _animationCurrentLineCount);

		if (_animationScrollFramesRemaining > 0)
		{
			// Scroll animation is in progress. Complete the animation, for now without taking into account the new size of the client area.
			EndAnimation();

			// ???
			if (_animationCurrentLineCount > _numberOfLinesFitting)
				_topLineIndex = _animationCurrentLineCount - _numberOfLinesFitting;
			else
				_topLineIndex = 0;
		}

		ComPtr<IDWriteTextLayout> tl;
		auto hr = _dWriteFactory->CreateTextLayout (L"A", 1, _textFormat, 1000, 1000, &tl); ThrowIfFailed(hr);
		DWRITE_TEXT_METRICS metrics;
		hr = tl->GetMetrics(&metrics);
		size_t newNumberOfLinesFitting = (size_t) floor(GetClientSizeDips().height / metrics.height);
		
		if (_numberOfLinesFitting != newNumberOfLinesFitting)
		{
			_numberOfLinesFitting = newNumberOfLinesFitting;

			if (isLastLineVisible)
			{
				// We must keep the last line at the bottom of the client area.

				if (_animationCurrentLineCount > _numberOfLinesFitting)
					_topLineIndex = _animationCurrentLineCount - _numberOfLinesFitting;
				else
					_topLineIndex = 0;

				InvalidateRect (GetHWnd(), nullptr, FALSE);
			}
		}

		// Need to set SIF_DISABLENOSCROLL due to what seems like a Windows bug:
		// GetScrollInfo returns garbage if called right after SetScrollInfo, if SetScrollInfo made the scroll bar change from invisible to visible, 

		// TODO: fix this
		SCROLLINFO si = { sizeof (si) };
		si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
		si.nMin = 0;
		si.nMax = _animationCurrentLineCount - 1;
		si.nPos = _topLineIndex;
		si.nPage = _numberOfLinesFitting;
		SetScrollInfo (GetHWnd(), SB_VERT, &si, TRUE);
	}

	void EndAnimation()
	{
		assert (_animationScrollFramesRemaining > 0);
		
		// Scroll animation is in progress. Finalize it.
		assert (_animationEndLineCount > _animationCurrentLineCount);
		assert (_timerId != 0);
		BOOL bRes = KillTimer (GetHWnd(), _timerId); assert(bRes);
		_timerId = 0;

		_animationCurrentLineCount = _animationEndLineCount;
		_animationScrollFramesRemaining = 0;
		InvalidateRect (GetHWnd(), nullptr, FALSE);
	}

	void ProcessUserScroll (int newTopLineIndex)
	{
		if (_topLineIndex != newTopLineIndex)
		{
			_topLineIndex = newTopLineIndex;
			InvalidateRect (GetHWnd(), nullptr, FALSE);
			SetScrollPos (GetHWnd(), SB_VERT, _topLineIndex, TRUE);
		}
	}

	void ProcessWmVScroll (WPARAM wParam, LPARAM lParam)
	{
		if (_animationScrollFramesRemaining > 0)
			EndAnimation();

		int newTopLineIndex = _topLineIndex;
		switch (LOWORD(wParam))
		{
			case SB_LINEUP:
				newTopLineIndex = max (_topLineIndex - 1, 0);
				break;

			case SB_PAGEUP:
				newTopLineIndex = max (_topLineIndex - _numberOfLinesFitting, 0);
				break;

			case SB_LINEDOWN:
				newTopLineIndex = _topLineIndex + min(_animationEndLineCount - (_topLineIndex + _numberOfLinesFitting), 1);
				break;

			case SB_PAGEDOWN:
				newTopLineIndex = _topLineIndex + min(_animationEndLineCount - (_topLineIndex + _numberOfLinesFitting), _numberOfLinesFitting);
				break;

			case SB_THUMBTRACK:
				newTopLineIndex = (int) HIWORD(wParam);
				break;
		}

		ProcessUserScroll (newTopLineIndex);
	}

	void ProcessWmMouseWheel (WPARAM wParam, LPARAM lParam)
	{
		WORD fwKeys = GET_KEYSTATE_WPARAM(wParam);
		short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		int xPos = GET_X_LPARAM(lParam); 
		int yPos = GET_Y_LPARAM(lParam); 

		if (_animationScrollFramesRemaining > 0)
			EndAnimation();

		UINT scrollLines;
		SystemParametersInfo (SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0);
		int linesToScroll = -(int) zDelta * (int) scrollLines / WHEEL_DELTA;

		int newTopLineIndex;
		if (linesToScroll < 0)
			newTopLineIndex = max (_topLineIndex + linesToScroll, 0);
		else
			newTopLineIndex = _topLineIndex + min(_animationEndLineCount - (_topLineIndex + _numberOfLinesFitting), linesToScroll);

		ProcessUserScroll (newTopLineIndex);
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
	HWND _editHwnd;
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
		auto areaRect = RECT{ splitterRect.right, titleBarHeightPixels, _clientSize.cx - splitterRect.right, _clientSize.cy };
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

	virtual void SelectBridge (Bridge* b) override final { _textArea->SelectBridge(b); }

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
