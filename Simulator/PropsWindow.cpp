
#include "pch.h"
#include "Simulator.h"
#include "Win32/Window.h"
#include "Bridge.h"
#include "Port.h"

using namespace std;

static ATOM wndClassAtom;
static constexpr wchar_t PropertiesWindowWndClassName[] = L"PropertiesWindow-{24B42526-2970-4B3C-A753-2DABD22C4BB0}";

class PropertiesWindow : public Window, public IPropertiesWindow
{
	using base = Window;

	ISimulatorApp* const _app;
	IProjectWindow* const _projectWindow;
	IProjectPtr const _project;
	ISelectionPtr const _selection;
	HFONT _font;
	IBridgePropsWindowPtr _bridgePropsControl;

	static constexpr DWORD Style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

public:
	PropertiesWindow (ISimulatorApp* app,
					  IProjectWindow* projectWindow,
					  IProject* project,
					  ISelection* selection,
					  IActionList* actionList,
					  HWND hWndParent,
					  POINT location)
		: base (app->GetHInstance(), PropertiesWindowWndClassName, 0, Style, RECT { location.x, location.y, 0, 0 }, hWndParent, nullptr)
		, _app(app)
		, _projectWindow(projectWindow)
		, _project(project)
		, _selection(selection)
	{
		NONCLIENTMETRICS ncm = { sizeof(NONCLIENTMETRICS) };
		SystemParametersInfo (SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
		_font = ::CreateFontIndirect (&ncm.lfMessageFont);

		_bridgePropsControl = bridgePropertiesControlFactory (_app, _projectWindow, _project, selection, actionList, GetHWnd(), { 0, 0 });

		SIZE ws = _bridgePropsControl->GetWindowSize();
		::MoveWindow (GetHWnd(), 0, 0, ws.cx, ws.cy, TRUE);

		_selection->GetChangedEvent().AddHandler (&OnSelectionChanged, this);
	}

	~PropertiesWindow()
	{
		_selection->GetChangedEvent().RemoveHandler (&OnSelectionChanged, this);

		::DeleteObject(_font);
	}

	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override final
	{
		auto resultBaseClass = base::WindowProc (hwnd, msg, wParam, lParam);

		if (msg == WM_SIZE)
		{
			if (_bridgePropsControl != nullptr)
			{
				::MoveWindow (_bridgePropsControl->GetHWnd(), 0, 0, GetClientWidthPixels(), GetClientHeightPixels(), FALSE);
				if (_bridgePropsControl->IsVisible())
					::UpdateWindow(_bridgePropsControl->GetHWnd());
			}
			return 0;
		}

		if (msg == WM_PAINT)
		{
			PAINTSTRUCT ps;
			::BeginPaint (GetHWnd(), &ps);
			auto oldFont = ::SelectObject (ps.hdc, _font);
			static constexpr wchar_t Text[] = L"(nothing selected)";
			RECT rc = { 0, 0, GetClientWidthPixels(), GetClientHeightPixels() };
			::DrawTextW (ps.hdc, Text, -1, &rc, DT_VCENTER | DT_CENTER | DT_SINGLELINE);
			::SelectObject (ps.hdc, oldFont);
			::EndPaint (GetHWnd(), &ps);
			return 0;
		}

		return resultBaseClass;
	}

	static void OnSelectionChanged (void* callbackArg, ISelection* selection)
	{
		auto window = static_cast<PropertiesWindow*>(callbackArg);

		bool showBridgeWindow = false;
		bool showPortsWindow = false;
		if (!selection->GetObjects().empty())
		{
			if (all_of (selection->GetObjects().begin(), selection->GetObjects().end(), [](Object* o) { return o->Is<Bridge>(); }))
				showBridgeWindow = true;
			else if (all_of (selection->GetObjects().begin(), selection->GetObjects().end(), [](Object* o) { return o->Is<Port>(); }))
				showPortsWindow = true;
		}

		::ShowWindow(window->_bridgePropsControl->GetHWnd(), showBridgeWindow ? SW_SHOW : SW_HIDE);
		// TODO: same for port props window
	}

	virtual HWND GetHWnd() const override final { return base::GetHWnd(); }
	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return base::QueryInterface(riid, ppvObject); }
	virtual ULONG STDMETHODCALLTYPE AddRef() override { return base::AddRef(); }
	virtual ULONG STDMETHODCALLTYPE Release() override { return base::Release(); }
};

template <typename... Args>
static IPropertiesWindowPtr Create (Args... args)
{
	return IPropertiesWindowPtr(new PropertiesWindow (std::forward<Args>(args)...), false);
};

const PropertiesWindowFactory propertiesWindowFactory = &Create;
