
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "resource.h"
#include "win32/d2d_window.h"
#include "win32/utility_functions.h"
#include "win32/text_layout.h"

using namespace D2D1;
using namespace edge;

#pragma warning (disable: 4250)

class log_window : public d2d_window, public log_window_i
{
	using base = d2d_window;

	selection_i* const _selection;
	std::shared_ptr<project_i> const _project;
	com_ptr<IDWriteTextFormat> _textFormat;
	bridge* _bridge = nullptr;
	int _selectedPort = -1;
	int _selectedTree = -1;
	std::vector<const BridgeLogLine*> _lines;
	UINT_PTR _timerId = 0;
	int _animationCurrentLineCount = 0;
	int _animationEndLineCount = 0;
	UINT _animationScrollFramesRemaining = 0;
	int _topLineIndex = 0;
	int _numberOfLinesFitting = 0;
	static constexpr UINT AnimationDurationMilliseconds = 75;
	static constexpr UINT AnimationScrollFramesMax = 10;

public:
	log_window (HINSTANCE hInstance, HWND hWndParent, const RECT& rect, ID3D11DeviceContext1* d3d_dc, IDWriteFactory* dWriteFactory, selection_i* selection, const std::shared_ptr<project_i>& project)
		: base (hInstance, WS_EX_CLIENTEDGE, WS_VISIBLE | WS_CHILD | WS_HSCROLL | WS_VSCROLL, rect, hWndParent, 0, d3d_dc, dWriteFactory)
		, _selection(selection)
		, _project(project)
	{
		auto hr = dWriteFactory->CreateTextFormat (L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 11, L"en-US", &_textFormat); assert(SUCCEEDED(hr));

		_numberOfLinesFitting = CalcNumberOfLinesFitting (_textFormat, client_height(), dWriteFactory);

		_selection->changed().add_handler (&OnSelectionChanged, this);
	}

	~log_window()
	{
		_selection->changed().remove_handler(&OnSelectionChanged, this);
		if (_bridge != nullptr)
			_bridge->log_line_generated().remove_handler(OnLogLineGeneratedStatic, this);
	}

	virtual HWND hwnd() const override { return base::hwnd(); }

	using base::invalidate;

	static void OnSelectionChanged (void* callbackArg, selection_i* selection)
	{
		auto logArea = static_cast<log_window*>(callbackArg);

		if (selection->objects().size() != 1)
			logArea->SelectBridge(nullptr);
		else
		{
			auto b = dynamic_cast<bridge*>(selection->objects().front());
			if (b == nullptr)
			{
				auto port = dynamic_cast<class port*>(selection->objects().front());
				if (port != nullptr)
					b = port->bridge();
			}

			logArea->SelectBridge(b);
		}
	}

	void render (ID2D1DeviceContext* dc) const final
	{
		dc->Clear(GetD2DSystemColor(COLOR_WINDOW));

		dc->SetTransform(dpi_transform());

		com_ptr<ID2D1SolidColorBrush> text_brush;
		d2d_dc()->CreateSolidColorBrush (GetD2DSystemColor(COLOR_WINDOWTEXT), &text_brush);

		if ((_bridge == nullptr) || _lines.empty())
		{
			static constexpr char TextNoBridge[] = "The STP activity log is shown here.\r\nSelect a bridge to see its log.";
			static constexpr char TextNoEntries[] = "No log text generated yet.\r\nYou may want to enable STP on the selected bridge.";

			auto oldta = _textFormat->GetTextAlignment();
			_textFormat->SetTextAlignment (DWRITE_TEXT_ALIGNMENT_CENTER);
			auto text = (_bridge == nullptr) ? TextNoBridge : TextNoEntries;
			auto tl = edge::text_layout_with_metrics (dwrite_factory(), _textFormat, text, client_width());
			_textFormat->SetTextAlignment(oldta);
			D2D1_POINT_2F origin = { client_width() / 2 - tl.width() / 2 - tl.left(), client_height() / 2 };
			dc->DrawTextLayout (origin, tl, text_brush);
		}
		else
		{
			float y = 0;
			float lineHeight = text_layout_with_metrics(dwrite_factory(), _textFormat, L"A").height();
			for (int lineIndex = _topLineIndex; (lineIndex < _animationCurrentLineCount) && (y < client_height()); lineIndex++)
			{
				std::wstring line (_lines[lineIndex]->text.begin(), _lines[lineIndex]->text.end());

				if ((line.length() >= 2) && (line[line.length() - 2] == '\r') && (line[line.length() - 1] == '\n'))
					line.resize (line.length() - 2);

				auto tl = text_layout (dwrite_factory(), _textFormat, line);
				dc->DrawTextLayout ({ 0, y }, tl, text_brush, D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
				y += lineHeight;

				if (y >= client_height())
					break;
			}
		}
	}

	static void OnLogLineGeneratedStatic (void* callbackArg, bridge* b, const BridgeLogLine* ll)
	{
		static_cast<log_window*>(callbackArg)->OnLogLineGenerated(ll);
	}

	void OnLogLineGenerated (const BridgeLogLine* ll)
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
				assert (_animationCurrentLineCount == _animationEndLineCount);
				assert (_animationScrollFramesRemaining == 0);

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
	}

	static void on_log_cleared(void* arg, bridge* b)
	{
		auto lw = static_cast<log_window*>(arg);
		if (lw->_animationScrollFramesRemaining > 0)
			lw->EndAnimation();
		lw->_lines.clear();
		lw->_animationCurrentLineCount = lw->_animationEndLineCount = 0;
		lw->_topLineIndex = 0;

		SCROLLINFO si = { sizeof (si) };
		si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
		si.nMin = 0;
		si.nMax = 0;
		si.nPos = 0;
		si.nPage = lw->_numberOfLinesFitting;
		SetScrollInfo (lw->hwnd(), SB_VERT, &si, TRUE);

		::InvalidateRect (lw->hwnd(), nullptr, FALSE);
	}

	void SelectBridge (bridge* b)
	{
		if (_bridge != b)
		{
			if (_bridge != nullptr)
			{
				if (_animationScrollFramesRemaining > 0)
					EndAnimation();

				_lines.clear();
				_bridge->log_cleared().remove_handler (on_log_cleared, this);
				_bridge->log_line_generated().remove_handler (OnLogLineGeneratedStatic, this);
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

				_bridge->log_line_generated().add_handler (OnLogLineGeneratedStatic, this);
				_bridge->log_cleared().add_handler (on_log_cleared, this);
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

	std::optional<LRESULT> window_proc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) final
	{
		if (msg == WM_SIZE)
		{
			base::window_proc (hwnd, msg, wParam, lParam); // Pass it to the base class first, which stores the client size.
			process_wm_size (wParam, lParam);
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

		if (msg == WM_CONTEXTMENU)
		{
			ProcessWmContextMenu (hwnd, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
			return 0;
		}

		if (msg == WM_COMMAND)
		{
			if (wParam == ID_CLEAR_ALL_LOGS)
			{
				for (auto& b : _project->bridges())
					b->clear_log();
			}

			return 0;
		}


		return base::window_proc (hwnd, msg, wParam, lParam);
	}

	void ProcessWmContextMenu (HWND hwnd, POINT pt)
	{
		auto hMenu = LoadMenu (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_CONTEXT_MENU_LOG_WINDOW));
		TrackPopupMenuEx (GetSubMenu(hMenu, 0), 0, pt.x, pt.y, hwnd, nullptr);
	}

	void ProcessAnimationTimer()
	{
		assert (_animationEndLineCount != _animationCurrentLineCount);
		assert (_animationScrollFramesRemaining != 0);

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
			_timerId = SetTimer (hwnd(), (UINT_PTR) 1, animationFrameLengthMilliseconds, NULL); assert (_timerId != 0);
		}
	}

	static int CalcNumberOfLinesFitting (IDWriteTextFormat* textFormat, float clientHeightDips, IDWriteFactory* dWriteFactory)
	{
		auto tl = edge::text_layout_with_metrics (dWriteFactory, textFormat, "A");
		return (int) floor(clientHeightDips / tl.height());
	}

	void process_wm_size (WPARAM wParam, LPARAM lParam)
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

		int newNumberOfLinesFitting = CalcNumberOfLinesFitting (_textFormat, client_height(), dwrite_factory());
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
		// GetScrollInfo returns garbage if called right after SetScrollInfo, if SetScrollInfo made the scroll bar change from invisible to visible,

		// TODO: fix this
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
		assert (_animationScrollFramesRemaining > 0);

		// Scroll animation is in progress. Finalize it.
		assert (_animationEndLineCount > _animationCurrentLineCount);
		assert (_timerId != 0);
		BOOL bRes = KillTimer (hwnd(), _timerId); assert(bRes);
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

template<typename... Args>
static std::unique_ptr<log_window_i> Create (Args... args)
{
	return std::make_unique<log_window>(std::forward<Args>(args)...);
}

extern const log_window_factory_t log_window_factory = &Create;
