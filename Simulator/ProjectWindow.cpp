#include "pch.h"
#include "Simulator.h"
#include "Win32Defs.h"
#include "Resource.h"
#include "Bridge.h"
#include "Port.h"

using namespace std;

static ATOM wndClassAtom;
static constexpr wchar_t ProjectWindowWndClassName[] = L"ProjectWindow-{24B42526-2970-4B3C-A753-2DABD22C4BB0}";
static constexpr wchar_t RegValueNameShowCmd[] = L"WindowShowCmd";
static constexpr wchar_t RegValueNameWindowLeft[] = L"WindowLeft";
static constexpr wchar_t RegValueNameWindowTop[] = L"WindowTop";
static constexpr wchar_t RegValueNameWindowRight[] = L"WindowRight";
static constexpr wchar_t RegValueNameWindowBottom[] = L"WindowBottom";

class ProjectWindow : public IProjectWindow
{
	ISimulatorApp*          const _app;
	shared_ptr<IProject>    const _project;
	shared_ptr<ISelection>  const _selection;
	shared_ptr<IActionList> const _actionList;
	unique_ptr<IEditArea> _editArea;
	unique_ptr<IDockContainer> _dockContainer;
	IDockablePanel* _logPanel;
	unique_ptr<ILogArea> _logArea;
	IDockablePanel* _propsPanel;
	unique_ptr<IPropertiesWindow> _propsWindow;
	IDockablePanel* _vlanPanel;
	unique_ptr<IVlanWindow> _vlanWindow;
	HWND _hwnd;
	SIZE _clientSize;
	EventManager _em;
	RECT _restoreBounds;
	unsigned int _selectedVlanNumber = 1;

public:
	ProjectWindow (ISimulatorApp* app,
				   const shared_ptr<IProject>& project,
				   const shared_ptr<ISelection>& selection,
				   const shared_ptr<IActionList>& actionList,
				   EditAreaFactory editAreaFactory,
				   int nCmdShow,
				   unsigned int selectedVlan)
		: _app(app), _project(project), _actionList(actionList), _selection(selection), _selectedVlanNumber(selectedVlan)
	{
		HINSTANCE hInstance;
		BOOL bRes = GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&wndClassAtom, &hInstance);
		if (!bRes)
			throw win32_exception(GetLastError());

		if (wndClassAtom == 0)
		{
			WNDCLASSEX wndClassEx =
			{
				sizeof(wndClassEx),
				CS_DBLCLKS, // style
				&WindowProcStatic, // lpfnWndProc
				0, // cbClsExtra
				0, // cbWndExtra
				hInstance, // hInstance
				LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DESIGNER)), // hIcon
				LoadCursor(nullptr, IDC_ARROW), // hCursor
				nullptr,//(HBRUSH)(COLOR_WINDOW + 1), // hbrBackground
				MAKEINTRESOURCE(IDR_MAIN_MENU), // lpszMenuName
				ProjectWindowWndClassName,      // lpszClassName
				LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DESIGNER))
			};

			wndClassAtom = RegisterClassEx(&wndClassEx);
			if (wndClassAtom == 0)
				throw win32_exception(GetLastError());
		}

		bool read = TryGetSavedWindowLocation (&_restoreBounds, &nCmdShow);
		int x = CW_USEDEFAULT, y = CW_USEDEFAULT, w = CW_USEDEFAULT, h = CW_USEDEFAULT;
		if (read)
		{
			x = _restoreBounds.left;
			y = _restoreBounds.top;
			w = _restoreBounds.right - _restoreBounds.left;
			h = _restoreBounds.bottom - _restoreBounds.top;
		}
		auto hwnd = ::CreateWindow(ProjectWindowWndClassName, L"STP Simulator", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, w, h, nullptr, 0, hInstance, this);
		if (hwnd == nullptr)
			throw win32_exception(GetLastError());
		assert(hwnd == _hwnd);
		if (!read)
			::GetWindowRect(_hwnd, &_restoreBounds);
		::ShowWindow (_hwnd, nCmdShow);

		_dockContainer = dockPanelFactory (_hwnd, 0xFFFF, GetClientRectPixels());

		_logPanel = _dockContainer->GetOrCreateDockablePanel(Side::Right, L"STP Log");
		_logArea = logAreaFactory (_logPanel->GetHWnd(), _logPanel->GetContentRect(), _app->GetD3DDeviceContext(), _app->GetDWriteFactory());
		_logPanel->GetVisibleChangedEvent().AddHandler (&OnLogPanelVisibleChanged, this);
		SetMainMenuItemCheck (ID_VIEW_STPLOG, true);

		_propsPanel = _dockContainer->GetOrCreateDockablePanel (Side::Left, L"Properties");
		_propsWindow.reset (propertiesWindowFactory (_app, this, project, selection, actionList, _propsPanel->GetHWnd(), _propsPanel->GetContentLocation()));
		_dockContainer->ResizePanel (_propsPanel, _propsPanel->GetPanelSizeFromContentSize(_propsWindow->GetClientSize()));
		_propsPanel->GetVisibleChangedEvent().AddHandler (&OnPropsPanelVisibleChanged, this);
		SetMainMenuItemCheck (ID_VIEW_PROPERTIES, true);

		_vlanPanel = _dockContainer->GetOrCreateDockablePanel (Side::Top, L"VLAN");
		_vlanWindow.reset (vlanWindowFactory (_app, this, _project, selection, actionList, _vlanPanel->GetHWnd(), _vlanPanel->GetContentLocation()));
		_dockContainer->ResizePanel (_vlanPanel, _vlanPanel->GetPanelSizeFromContentSize(_vlanWindow->GetClientSize()));
		_vlanPanel->GetVisibleChangedEvent().AddHandler (&OnVlanPanelVisibleChanged, this);
		SetMainMenuItemCheck (ID_VIEW_VLANS, true);

		_editArea.reset (editAreaFactory (app, this, project, selection, actionList, _dockContainer->GetHWnd(), _dockContainer->GetContentRect(), _app->GetD3DDeviceContext(), _app->GetDWriteFactory()));

		_selection->GetSelectionChangedEvent().AddHandler (&OnSelectionChanged, this);
	}

	~ProjectWindow()
	{
		_selection->GetSelectionChangedEvent().RemoveHandler (&OnSelectionChanged, this);

		if (_hwnd != nullptr)
			::DestroyWindow(_hwnd);
	}

	void SetMainMenuItemCheck (UINT item, bool checked)
	{
		auto menu = ::GetMenu(_hwnd);
		MENUITEMINFO mii = { sizeof(mii) };
		mii.fMask = MIIM_STATE;
		mii.fState = checked ? MFS_CHECKED : MFS_UNCHECKED;
		::SetMenuItemInfo (menu, item, FALSE, &mii);
	}

	static void OnPropsPanelVisibleChanged (void* callbackArg, IDockablePanel* panel, bool visible)
	{
		static_cast<ProjectWindow*>(callbackArg)->SetMainMenuItemCheck (ID_VIEW_PROPERTIES, visible);
	}

	static void OnLogPanelVisibleChanged (void* callbackArg, IDockablePanel* panel, bool visible)
	{
		static_cast<ProjectWindow*>(callbackArg)->SetMainMenuItemCheck (ID_VIEW_STPLOG, visible);
	}

	static void OnVlanPanelVisibleChanged (void* callbackArg, IDockablePanel* panel, bool visible)
	{
		static_cast<ProjectWindow*>(callbackArg)->SetMainMenuItemCheck (ID_VIEW_VLANS, visible);
	}

	static void OnSelectionChanged (void* callbackArg, ISelection* selection)
	{
		auto pw = static_cast<ProjectWindow*>(callbackArg);

		if (selection->GetObjects().size() != 1)
			pw->_logArea->SelectBridge(nullptr);
		else
		{
			auto b = dynamic_cast<Bridge*>(selection->GetObjects()[0].Get());
			if (b == nullptr)
			{
				auto port = dynamic_cast<Port*>(selection->GetObjects()[0].Get());
				if (port != nullptr)
					b = port->GetBridge();
			}

			pw->_logArea->SelectBridge(b);
		}
	}

	virtual HWND GetHWnd() const override { return _hwnd; }

	// From http://blogs.msdn.com/b/oldnewthing/archive/2005/04/22/410773.aspx
	static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		//if (AssertFunctionRunning)
		//{
		//	// Let's try not to run application code while the assertion dialog is shown. We'll probably mess things up even more.
		//	return DefWindowProc(hwnd, uMsg, wParam, lParam);
		//}

		ProjectWindow* window;
		if (uMsg == WM_NCCREATE)
		{
			LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
			window = reinterpret_cast<ProjectWindow*>(lpcs->lpCreateParams);
			window->_hwnd = hwnd;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
		}
		else
			window = reinterpret_cast<ProjectWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

		if (window == nullptr)
		{
			// this must be one of those messages sent before WM_NCCREATE or after WM_NCDESTROY.
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}

		LRESULT result = window->WindowProc(uMsg, wParam, lParam);

		if (uMsg == WM_NCDESTROY)
		{
			window->_hwnd = nullptr;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
		}

		return result;
	}

	LRESULT WindowProc(UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_CREATE)
		{
			auto cs = reinterpret_cast<const CREATESTRUCT*>(lParam);
			_clientSize = { cs->cx, cs->cy };
			return 0;
		}

		if (msg == WM_CLOSE)
		{
			SaveWindowLocation();
			PostQuitMessage(0);
			return DefWindowProc(_hwnd, msg, wParam, lParam); // this calls DestroyWindow
		}

		if (msg == WM_DESTROY)
		{
			_dockContainer = nullptr; // destroy it early to avoid doing layout-related processing
			return 0;
		}

		if (msg == WM_SIZE)
		{
			if (wParam == SIZE_RESTORED)
				::GetWindowRect(_hwnd, &_restoreBounds);

			_clientSize = { LOWORD(lParam), HIWORD(lParam) };
			if (_dockContainer != nullptr)
				::MoveWindow (_dockContainer->GetHWnd(), 0, 0, _clientSize.cx, _clientSize.cy, TRUE);

			return 0;
		}

		if (msg == WM_MOVE)
		{
			WINDOWPLACEMENT wp = { sizeof(wp) };
			::GetWindowPlacement(GetHWnd(), &wp);
			if (wp.showCmd == SW_NORMAL)
				::GetWindowRect(GetHWnd(), &_restoreBounds);
			return 0;
		}

		if (msg == WM_ERASEBKGND)
			return 1;

		if (msg == WM_PAINT)
		{
			PAINTSTRUCT ps;
			BeginPaint(GetHWnd(), &ps);
			EndPaint(GetHWnd(), &ps);
			return 0;
		}

		if (msg == WM_COMMAND)
		{
			if ((wParam == ID_VIEW_PROPERTIES) || (wParam == ID_VIEW_STPLOG) || (wParam == ID_VIEW_VLANS))
			{
				auto panel = (wParam == ID_VIEW_PROPERTIES) ? _propsPanel : ((wParam == ID_VIEW_STPLOG) ? _logPanel : _vlanPanel);
				auto style = (DWORD) GetWindowLongPtr(panel->GetHWnd(), GWL_STYLE);
				style ^= WS_VISIBLE;
				SetWindowLongPtr (panel->GetHWnd(), GWL_STYLE, style);
				return 0;
			}

			if (wParam == ID_FILE_SAVE)
			{
				_project->Save (L"C:\\Project.stp");
				return 0;
			}

			return DefWindowProc(_hwnd, msg, wParam, lParam);
		}

		return DefWindowProc(_hwnd, msg, wParam, lParam);
	}

	bool TryGetSavedWindowLocation (_Out_ RECT* restoreBounds, _Out_ int* nCmdShow)
	{
		auto ReadDword = [this](const wchar_t* valueName, DWORD* valueOut) -> bool
		{
			DWORD dataSize = 4;
			auto lresult = RegGetValue(HKEY_CURRENT_USER, _app->GetRegKeyPath(), valueName, RRF_RT_REG_DWORD, nullptr, valueOut, &dataSize);
			return lresult == ERROR_SUCCESS;
		};

		int cmd;
		RECT rb;
		if (ReadDword(RegValueNameShowCmd, (DWORD*)&cmd)
			&& ReadDword(RegValueNameWindowLeft, (DWORD*)&rb.left)
			&& ReadDword(RegValueNameWindowTop, (DWORD*)&rb.top)
			&& ReadDword(RegValueNameWindowRight, (DWORD*)&rb.right)
			&& ReadDword(RegValueNameWindowBottom, (DWORD*)&rb.bottom))
		{
			*restoreBounds = rb;
			*nCmdShow = cmd;
			return true;
		}

		return false;
	}

	void SaveWindowLocation() const
	{
		WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
		BOOL bRes = GetWindowPlacement(GetHWnd(), &wp);
		if (bRes && ((wp.showCmd == SW_NORMAL) || (wp.showCmd == SW_MAXIMIZE)))
		{
			HKEY key;
			auto lstatus = RegCreateKeyEx(HKEY_CURRENT_USER, _app->GetRegKeyPath(), 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
			if (lstatus == ERROR_SUCCESS)
			{
				RegSetValueEx(key, RegValueNameWindowLeft, 0, REG_DWORD, (BYTE*)&_restoreBounds.left, 4);
				RegSetValueEx(key, RegValueNameWindowTop, 0, REG_DWORD, (BYTE*)&_restoreBounds.top, 4);
				RegSetValueEx(key, RegValueNameWindowRight, 0, REG_DWORD, (BYTE*)&_restoreBounds.right, 4);
				RegSetValueEx(key, RegValueNameWindowBottom, 0, REG_DWORD, (BYTE*)&_restoreBounds.bottom, 4);
				RegSetValueEx(key, RegValueNameShowCmd, 0, REG_DWORD, (BYTE*)&wp.showCmd, 4);
				RegCloseKey(key);
			}
		}
	}

	virtual void SelectVlan (unsigned int vlanNumber) override final
	{
		if ((vlanNumber == 0) || (vlanNumber > 4095))
			throw invalid_argument (u8"Invalid VLAN number.");

		if (_selectedVlanNumber != vlanNumber)
		{
			_selectedVlanNumber = vlanNumber;
			SelectedVlanNumerChangedEvent::InvokeHandlers(_em, this, vlanNumber);
			::InvalidateRect (GetHWnd(), nullptr, FALSE);
		}
	};

	virtual unsigned int GetSelectedVlanNumber() const override final { return _selectedVlanNumber; }

	virtual SelectedVlanNumerChangedEvent::Subscriber GetSelectedVlanNumerChangedEvent() override final { return SelectedVlanNumerChangedEvent::Subscriber(_em); }

	virtual IProject* GetProject() const override final { return _project.get(); }

	/*
	LRESULT ProcessWmClose()
	{
		auto& allWindowsForProject = _project->GetProjectWindows();

		rassert (!allWindowsForProject.empty());

		if (allWindowsForProject.size() > 1)
		{
		_project->RemoveAndDestroyProjectWindow(this);
		return;
		}

		// Closing the last remaining window of this project. Let's check for changes and ask the user whether to save them.
		bool continueClosing = SaveProject (_project->GetFilePath(), true, SaveProjectOption::SaveIfChangedAskUserFirst, L"Save changes?");
		if (!continueClosing)
		return;

		// Copy some pointers from "this" to the stack cause destroying the project window might release the last reference to it and destroy this object.
		IEditorProject* project = _project;
		IEditorApplication* app = _app;

		_project->RemoveAndDestroyProjectWindow (this);

		//if (project->GetProjectWindows().empty())
		//{
		//	app->CloseProject (project);
		//	if (app->GetOpenProjects().empty())
		//		PostQuitMessage (0);
		//}
	}
	*/
};

template<typename... Args>
static IProjectWindow* Create (Args... args)
{
	return new ProjectWindow (std::forward<Args>(args)...);
}

extern const ProjectWindowFactory projectWindowFactory = Create;
