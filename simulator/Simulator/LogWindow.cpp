
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "Simulator_.h"
#include "resource.h"

using namespace D2D1;
using namespace edge;

class LogWindowImpl : public ILogWindow, ID2DRenderEventsSink, IBridgeEvents, IObjectCollectionChangeEvents
{
	ULONG _refCount = 0;
	ULONG _sig = 0xAA550003;
	ISimulatorApp* _app;
	com_ptr<ISelection> _selection;
	com_ptr<IStpProject> _project;
	wil::unique_hwnd _hWnd;
	com_ptr<ID2DRenderer> _renderer;
	com_ptr<IDWriteTextFormat> _textFormat;
	com_ptr<IBridge> _bridge;
	int _selectedPort = -1;
	int _selectedTree = -1;
	std::vector<const BridgeLogLine*> _lines;
	UINT_PTR _timerId = 0;
	int _animationCurrentLineCount = 0;
	int _animationEndLineCount = 0;
	UINT _animationScrollFramesRemaining = 0;
	int _topLineIndex = 0;
	int _numberOfLinesFitting = 0;
	WeakRefToThis _weakRefToThis;
	AdviseSinkToken _renderEventsToken;
	AdviseSinkToken _logEventsToken;
	AdviseSinkToken _selectionChangeToken;
	static constexpr UINT AnimationDurationMilliseconds = 75;
	static constexpr UINT AnimationScrollFramesMax = 10;

	static const inline WNDCLASSEX wnd_class = {
		.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
		.hCursor = ::LoadCursor(nullptr, IDC_ARROW),
		.lpszClassName = L"log_window",
	};

public:
	HRESULT InitInstance (ISimulatorApp* app, HWND hWndParent, const RECT& rect, ISelection* selection, IStpProject* project)
	{
		HRESULT hr;

		_weakRefToThis.InitInstance(AsUnknown());

		_app = app;
		_selection = selection;
		_project = project;
			
		static const WNDCLASS wnd_class = {
			.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
			.lpfnWndProc = WndProcStatic,
			.hInstance = (HINSTANCE)&__ImageBase,
			.hCursor = ::LoadCursor(nullptr, IDC_ARROW),
			.lpszClassName = L"log_window",
		};

		auto atom = RegisterClass(&wnd_class);

		int x = rect.left;
		int y = rect.top;
		int w = rect.right - rect.left;
		int h = rect.bottom - rect.top;
		_hWnd.reset (CreateWindowEx (WS_EX_CLIENTEDGE, wnd_class.lpszClassName, L"", WS_VISIBLE | WS_CHILD | WS_HSCROLL | WS_VSCROLL,
									 x, y, w, h, hWndParent, nullptr, (HINSTANCE)&__ImageBase, nullptr));
		RETURN_LAST_ERROR_IF_NULL(_hWnd);
		SetWindowLongPtr (_hWnd.get(), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

		hr = MakeD2DRenderer (_hWnd.get(), _app->GetD3DDC(), _app->GetDWriteFactory(), _app->GetD2DFactory(), &_renderer); RETURN_IF_FAILED(hr);
		
		hr = _app->GetDWriteFactory()->CreateTextFormat (L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 11, L"en-US", &_textFormat); WI_ASSERT(SUCCEEDED(hr));

		_numberOfLinesFitting = CalcNumberOfLinesFitting(_hWnd.get());

		hr = AdviseSink<IObjectCollectionChangeEvents>(_selection, _weakRefToThis, &_selectionChangeToken); RETURN_IF_FAILED(hr);
		
		hr = AdviseSink<ID2DRenderEventsSink>(_renderer, _weakRefToThis, &_renderEventsToken); RETURN_IF_FAILED(hr);

		return S_OK;
	}

	~LogWindowImpl()
	{
		select_bridge(nullptr);
	}

	IUnknown* AsUnknown() { return static_cast<ILogWindow*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<ILogWindow>(this, riid, ppvObject)
			|| TryQI<ID2DRenderEventsSink>(this, riid, ppvObject)
			|| TryQI<IBridgeEvents>(this, riid, ppvObject)
			|| TryQI<IObjectCollectionChangeEvents>(this, riid, ppvObject)
		)
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	virtual HWND hwnd() const override { return _hWnd.get(); }

	#pragma region IObjectCollectionChangeEvents
	virtual HRESULT STDMETHODCALLTYPE OnCollectionChanging (IUnknown* sender, const struct ObjectCollectionChangeArgs* args) override
	{
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnCollectionChanged (IUnknown* sender, const struct ObjectCollectionChangeArgs* args) override
	{
		if (_selection->size() != 1)
			select_bridge(nullptr);
		else
		{
			auto o = _selection->front();
			if (auto b = wil::try_com_query_nothrow<IBridge>(o))
				select_bridge(b);
			else if (auto p = wil::try_com_query_nothrow<IPort>(o))
				select_bridge(p->bridge());
		}

		return S_OK;
	}
	#pragma endregion

	#pragma region ID2DRenderEventsSink
	virtual HRESULT STDMETHODCALLTYPE OnD2DRender (HWND hWnd, ID2D1DeviceContext* dc) override
	{
		HRESULT hr;

		com_ptr<ID2DThemeColorProvider> tcp;
		hr = _app->GetThemeColorProvider()->QueryInterface(IID_PPV_ARGS(&tcp)); RETURN_IF_FAILED(hr);
		dc->Clear(tcp->color_d2d(edge::theme_color::background));

		dc->SetTransform(dpi_transform(hWnd));

		com_ptr<ID2D1SolidColorBrush> text_brush = tcp->make_brush(dc, edge::theme_color::foreground);

		auto dpi = edge::dpi(hWnd);
		RECT clientRectPixels;
		::GetClientRect(hWnd, &clientRectPixels);
		float clientWidth = clientRectPixels.right * 96.0f / dpi;
		float clientHeight = clientRectPixels.bottom * 96.0f / dpi;

		if ((_bridge == nullptr) || _lines.empty())
		{
			static constexpr wchar_t TextNoBridge[] = L"The STP activity log is shown here.\r\nSelect a bridge to see its log.";
			static constexpr wchar_t TextNoEntries[] = L"No log text generated yet.\r\nYou may want to enable STP on the selected bridge.";

			auto oldta = _textFormat->GetTextAlignment();
			_textFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_CENTER);
			auto text = (_bridge == nullptr) ? TextNoBridge : TextNoEntries;
			TextLayoutWithMetrics tl;
			hr = CreateTextLayoutWithMetrics(_app->GetDWriteFactory(), _textFormat, text, -1, clientWidth, tl); RETURN_IF_FAILED(hr);
			_textFormat->SetTextAlignment(oldta);
			D2D1_POINT_2F origin = { clientWidth / 2 - tl.metrics.width / 2 - tl.metrics.left, clientHeight / 2 };
			dc->DrawTextLayout (origin, tl, text_brush);
		}
		else
		{
			float y = 0;
			TextLayoutWithMetrics tl;
			hr = CreateTextLayoutWithMetrics(_app->GetDWriteFactory(), _textFormat, L"A", -1, 0, tl); RETURN_IF_FAILED(hr);
			float lineHeight = tl.metrics.height;
			for (int lineIndex = _topLineIndex; (lineIndex < _animationCurrentLineCount) && (y < clientHeight); lineIndex++)
			{
				std::wstring line (_lines[lineIndex]->text.begin(), _lines[lineIndex]->text.end());

				if ((line.length() >= 2) && (line[line.length() - 2] == '\r') && (line[line.length() - 1] == '\n'))
					line.resize (line.length() - 2);

				com_ptr<IDWriteTextLayout> tl;
				hr = _app->GetDWriteFactory()->CreateTextLayout(line.c_str(), (UINT32)line.length(),
					_textFormat, 10'000, 10'000, &tl); RETURN_IF_FAILED(hr);
				dc->DrawTextLayout ({ 0, y }, tl, text_brush, D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
				y += lineHeight;

				if (y >= clientHeight)
					break;
			}
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnBeforeD2DRender (HWND hWnd, ID2D1DeviceContext* dc) override { return S_OK; }

	virtual HRESULT STDMETHODCALLTYPE OnAfterD2DRender (HWND hWnd, ID2D1DeviceContext* dc) override { return S_OK; }

	virtual HRESULT STDMETHODCALLTYPE OnD2DDCReleasing (ID2D1DeviceContext* dc) override { return S_OK; }

	virtual HRESULT STDMETHODCALLTYPE OnD2DDCRecreated (ID2D1DeviceContext* dc) override { return S_OK; }
	#pragma endregion

	#pragma region IBridgeEvents
	virtual HRESULT STDMETHODCALLTYPE OnLogLineGenerated (IBridge*, const BridgeLogLine* ll) override
	{
		if (((_selectedPort == -1) || (_selectedPort == ll->portIndex))
			&& ((_selectedTree == -1) || (_selectedTree == ll->treeIndex)))
		{
			_lines.push_back(ll);

			bool lastLineVisible = (_topLineIndex + _numberOfLinesFitting >= _animationCurrentLineCount);

			if (!lastLineVisible)
			{
				// The user has scrolled away from the last time. We append the text without doing any scrolling.

				// If the user scrolled away, the animation is supposed to have been stopped.
				_ASSERT (_animationCurrentLineCount == _animationEndLineCount);
				_ASSERT (_animationScrollFramesRemaining == 0);

				SCROLLINFO si = { sizeof (si) };
				si.fMask = SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
				si.nMin = 0;
				si.nMax = (int) _lines.size() - 1;
				si.nPage = _numberOfLinesFitting;
				SetScrollInfo (hwnd(), SB_VERT, &si, TRUE);

				_animationCurrentLineCount = (int) _lines.size();
				_animationEndLineCount     = (int) _lines.size();

				InvalidateRect(hwnd(), nullptr, FALSE);
			}
			else
			{
				// The last line is visible, meaning that the user didn't scroll away from it.
				// An animation might be pending or not. In any case, we restart it with the new parameters.
				_animationEndLineCount = (int) _lines.size();
				_animationScrollFramesRemaining = AnimationScrollFramesMax;

				if (_timerId != 0)
				{
					KillTimer (hwnd(), _timerId);
					_timerId = 0;
				}

				UINT animationFrameLengthMilliseconds = AnimationDurationMilliseconds / AnimationScrollFramesMax;
				_timerId = SetTimer (hwnd(), 1, animationFrameLengthMilliseconds, NULL);
			}
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnLogCleared (IBridge*) override
	{
		if (_animationScrollFramesRemaining > 0)
			EndAnimation();
		_lines.clear();
		_animationCurrentLineCount = _animationEndLineCount = 0;
		_topLineIndex = 0;

		SCROLLINFO si = { sizeof (si) };
		si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
		si.nMin = 0;
		si.nMax = 0;
		si.nPos = 0;
		si.nPage = _numberOfLinesFitting;
		SetScrollInfo (hwnd(), SB_VERT, &si, TRUE);

		::InvalidateRect (hwnd(), nullptr, FALSE);

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnPacketTransmit (IBridge* sender, ULONG txPortIndex, packet_t&& packet) override
	{
		return S_OK;
	}
	#pragma endregion

	void select_bridge (IBridge* b)
	{
		if (_bridge != b)
		{
			if (_bridge != nullptr)
			{
				if (_animationScrollFramesRemaining > 0)
					EndAnimation();

				_lines.clear();
				_logEventsToken.reset();
				_bridge = nullptr;
			}

			_bridge = b;

			if (b != nullptr)
			{
				for (auto& ll : _bridge->GetLogLines())
				{
					if (((_selectedPort == -1) || (_selectedPort == ll->portIndex))
						&& ((_selectedTree == -1) || (_selectedTree == ll->treeIndex)))
					{
						_lines.push_back(ll.get());
					}
				}

				auto hr = AdviseSink<IBridgeEvents>(_bridge, _weakRefToThis, &_logEventsToken); LOG_IF_FAILED(hr);
			}

			_topLineIndex = std::max (0, (int) _lines.size() - _numberOfLinesFitting);
			_animationCurrentLineCount = (int) _lines.size();
			_animationEndLineCount     = (int) _lines.size();

			SCROLLINFO si = { sizeof (si) };
			si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
			si.nMin = 0;
			si.nMax = (int) _lines.size() - 1;
			si.nPage = _numberOfLinesFitting;
			si.nPos = _topLineIndex;
			SetScrollInfo (hwnd(), SB_VERT, &si, TRUE);

			InvalidateRect (hwnd(), nullptr, FALSE);
		}
	}

	static LRESULT CALLBACK WndProcStatic (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (auto w = reinterpret_cast<LogWindowImpl*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)))
			return w->WndProc (hWnd, uMsg, wParam, lParam);
		return DefWindowProc (hWnd, uMsg, wParam, lParam);
	}

	LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (uMsg == WM_SIZE)
		{
			process_wm_size (hWnd, wParam, lParam);
			return DefWindowProc (hWnd, uMsg, wParam, lParam);
		}

		if (uMsg == WM_VSCROLL)
		{
			ProcessWmVScroll (wParam, lParam);
			return DefWindowProc (hWnd, uMsg, wParam, lParam);
		}

		if ((uMsg == WM_TIMER) && (_timerId != 0) && (wParam == _timerId))
		{
			ProcessAnimationTimer();
			return DefWindowProc (hWnd, uMsg, wParam, lParam);
		}

		if (uMsg == WM_MOUSEWHEEL)
		{
			ProcessWmMouseWheel (wParam, lParam);
			return 0; // consume it
		}

		if (uMsg == WM_CONTEXTMENU)
		{
			ProcessWmContextMenu (hWnd, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
			return 0; // consume it
		}

		if (uMsg == WM_COMMAND)
		{
			if (wParam == ID_CLEAR_ALL_LOGS)
			{
				for (ULONG i = 0; i < _project->BridgeCount(); i++)
					_project->BridgeAt(i)->clear_log();
				return 0; // consume it
			}

			return DefWindowProc (hWnd, uMsg, wParam, lParam);
		}

		return DefWindowProc (hWnd, uMsg, wParam, lParam);
	}

	void ProcessWmContextMenu (HWND hwnd, POINT pt)
	{
		auto hMenu = LoadMenu (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_CONTEXT_MENU_LOG_WINDOW));
		TrackPopupMenuEx (GetSubMenu(hMenu, 0), 0, pt.x, pt.y, hwnd, nullptr);
	}

	void ProcessAnimationTimer()
	{
		_ASSERT (_animationEndLineCount != _animationCurrentLineCount);
		_ASSERT (_animationScrollFramesRemaining != 0);

		int linesToAddInThisAnimationFrame = (_animationEndLineCount - _animationCurrentLineCount) / _animationScrollFramesRemaining;
		_animationCurrentLineCount += linesToAddInThisAnimationFrame;

		if (_animationCurrentLineCount <= _numberOfLinesFitting)
			_topLineIndex = 0;
		else
			_topLineIndex = _animationCurrentLineCount - _numberOfLinesFitting;

		InvalidateRect (hwnd(), nullptr, FALSE);

		// Need to set SIF_DISABLENOSCROLL due to what seems like a Windows bug:
		// GetScrollInfo returns garbage if called right after SetScrollInfo, if SetScrollInfo made the scroll bar change from invisible to visible,
		SCROLLINFO si = { sizeof (si) };
		si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
		si.nMin = 0;
		si.nMax = _animationCurrentLineCount - 1;
		si.nPage = _numberOfLinesFitting;
		si.nPos = _topLineIndex;
		SetScrollInfo (hwnd(), SB_VERT, &si, TRUE);

		if (_timerId != 0)
		{
			KillTimer (hwnd(), _timerId);
			_timerId = 0;
		}

		_animationScrollFramesRemaining--;
		if (_animationScrollFramesRemaining > 0)
		{
			UINT animationFrameLengthMilliseconds = AnimationDurationMilliseconds / AnimationScrollFramesMax;
			_timerId = SetTimer (hwnd(), (UINT_PTR) 1, animationFrameLengthMilliseconds, NULL); _ASSERT (_timerId != 0);
		}
	}

	int CalcNumberOfLinesFitting (HWND hWnd) const
	{
		auto dpi = edge::dpi(hWnd);
		RECT clientRectPixels;
		::GetClientRect(hWnd, &clientRectPixels);
		float clientHeight = clientRectPixels.bottom * 96.0f / dpi;

		TextLayoutWithMetrics tl;
		auto hr = CreateTextLayoutWithMetrics(_app->GetDWriteFactory(), _textFormat, L"A", -1, 0, tl); _ASSERT(SUCCEEDED(hr));
		return (int) floor(clientHeight / tl.metrics.height);
	}

	void process_wm_size (HWND hWnd, WPARAM wParam, LPARAM lParam)
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

		int newNumberOfLinesFitting = CalcNumberOfLinesFitting(hWnd);
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

				InvalidateRect (hwnd(), nullptr, FALSE);
			}
		}

		// Need to set SIF_DISABLENOSCROLL due to what seems like a Windows bug:
		// GetScrollInfo returns garbage if called right after SetScrollInfo,
		// if SetScrollInfo made the scroll bar change from invisible to visible.
		SCROLLINFO si = { sizeof (si) };
		si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
		si.nMin = 0;
		si.nMax = _animationCurrentLineCount - 1;
		si.nPos = _topLineIndex;
		si.nPage = _numberOfLinesFitting;
		SetScrollInfo (hwnd(), SB_VERT, &si, TRUE);
	}

	void EndAnimation()
	{
		_ASSERT (_animationScrollFramesRemaining > 0);

		// Scroll animation is in progress. Finalize it.
		_ASSERT (_animationEndLineCount > _animationCurrentLineCount);
		_ASSERT (_timerId != 0);
		BOOL bRes = KillTimer (hwnd(), _timerId); _ASSERT(bRes);
		_timerId = 0;

		_animationCurrentLineCount = _animationEndLineCount;
		_animationScrollFramesRemaining = 0;
		InvalidateRect (hwnd(), nullptr, FALSE);
	}

	void ProcessUserScroll (int newTopLineIndex)
	{
		if (_topLineIndex != newTopLineIndex)
		{
			_topLineIndex = newTopLineIndex;
			InvalidateRect (hwnd(), nullptr, FALSE);
			SetScrollPos (hwnd(), SB_VERT, _topLineIndex, TRUE);
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
				newTopLineIndex = std::max (_topLineIndex - 1, 0);
				break;

			case SB_PAGEUP:
				newTopLineIndex = std::max (_topLineIndex - _numberOfLinesFitting, 0);
				break;

			case SB_LINEDOWN:
				newTopLineIndex = _topLineIndex + std::min(_animationEndLineCount - (_topLineIndex + _numberOfLinesFitting), 1);
				break;

			case SB_PAGEDOWN:
				newTopLineIndex = _topLineIndex + std::min(_animationEndLineCount - (_topLineIndex + _numberOfLinesFitting), _numberOfLinesFitting);
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
			newTopLineIndex = std::max (_topLineIndex + linesToScroll, 0);
		else
			newTopLineIndex = _topLineIndex + std::min(_animationEndLineCount - (_topLineIndex + _numberOfLinesFitting), linesToScroll);

		ProcessUserScroll (newTopLineIndex);
	}
};

HRESULT MakeLogWindow (ISimulatorApp* app, HWND hWndParent, const RECT& rect,
					   ISelection* selection, IStpProject* project, ILogWindow** ppLogWindow)
{
	auto p = com_ptr(new (std::nothrow) LogWindowImpl()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance (app, hWndParent, rect, selection, project); RETURN_IF_FAILED(hr);
	*ppLogWindow = p.detach();
	return S_OK;
}
