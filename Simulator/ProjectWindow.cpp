#include "pch.h"
#include "Simulator.h"
#include "Win32Defs.h"
#include "Resource.h"
#include "RibbonCommandHandlers/RCHBase.h"
#include "Bridge.h"
#include "Port.h"

using namespace std;

static ATOM wndClassAtom;
static constexpr wchar_t ProjectWindowWndClassName[] = L"ProjectWindow-{24B42526-2970-4B3C-A753-2DABD22C4BB0}";
static constexpr wchar_t RegValueNameShowCmd[] = L"WindowShowCmd";
static constexpr wchar_t RegValueNameWindowX[] = L"WindowX";
static constexpr wchar_t RegValueNameWindowY[] = L"WindowY";
static constexpr wchar_t RegValueNameWindowWidth[] = L"WindowWidth";
static constexpr wchar_t RegValueNameWindowHeight[] = L"WindowHeight";

class ProjectWindow : public IProjectWindow, IUIApplication
{
	ULONG _refCount = 1;
	ComPtr<IProject> const _project;
	ComPtr<ISelection> const _selection;
	ComPtr<IEditArea> _editArea;
	ComPtr<IDockPanel> _dockPanel;
	ComPtr<ILogArea> _logArea;
	ComPtr<IUIFramework> _rf;
	ComPtr<IBridgePropsArea> _bridgePropsArea;
	HWND _hwnd;
	SIZE _clientSize;
	EventManager _em;
	RECT _restoreBounds;
	unordered_map<UINT32, ComPtr<RCHBase>> _commandHandlers;
	unordered_map<Side, LONG> _panelSizes = { { Side::Left, 400 }, { Side::Top, 400 }, { Side::Right, 400 }, { Side::Bottom, 400 } };

public:
	ProjectWindow (IProject* project, HINSTANCE rfResourceHInstance, const wchar_t* rfResourceName,
				   ISelection* selection, EditAreaFactory editAreaFactory, int nCmdShow)
		: _project(project), _selection(selection)
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
				&ProjectWindow::WindowProcStatic, // lpfnWndProc
				0, // cbClsExtra
				0, // cbWndExtra
				hInstance, // hInstance
				LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DESIGNER)), // hIcon
				LoadCursor(nullptr, IDC_ARROW), // hCursor
				nullptr,//(HBRUSH)(COLOR_WINDOW + 1), // hbrBackground
				nullptr, // lpszMenuName
				ProjectWindowWndClassName, // lpszClassName
				LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DESIGNER))
			};

			wndClassAtom = RegisterClassEx(&wndClassEx);
			if (wndClassAtom == 0)
				throw win32_exception(GetLastError());
		}

		int x = CW_USEDEFAULT, y = CW_USEDEFAULT, w = CW_USEDEFAULT, h = CW_USEDEFAULT;
		TryGetSavedWindowLocation (&x, &y, &w, &h, &nCmdShow);
		auto hwnd = ::CreateWindow(ProjectWindowWndClassName, L"STP Simulator", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, w, h, nullptr, 0, hInstance, this);
		if (hwnd == nullptr)
			throw win32_exception(GetLastError());
		assert(hwnd == _hwnd);
		::ShowWindow (hwnd, nCmdShow);
		
		if ((rfResourceHInstance != nullptr) && (rfResourceName != nullptr))
		{
			auto hr = CoCreateInstance(CLSID_UIRibbonFramework, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_rf));
			ThrowIfFailed(hr);
		}

		UINT32 ribbonHeight = 0;
		if ((rfResourceHInstance != nullptr) && (rfResourceName != nullptr))
		{
			for (auto& info : GetRCHInfos())
			{
				auto handler = info->_factory();
				for (UINT32 command : info->_commands)
					_commandHandlers.insert ({ command, handler });
			}

			auto hr = _rf->Initialize(hwnd, this); ThrowIfFailed(hr);
			hr = _rf->LoadUI(rfResourceHInstance, rfResourceName); ThrowIfFailed(hr);

			ComPtr<IUIRibbon> ribbon;
			hr = _rf->GetView(0, IID_PPV_ARGS(&ribbon)); ThrowIfFailed(hr);
			hr = ribbon->GetHeight(&ribbonHeight); ThrowIfFailed(hr);
		}

		RECT dockPanelRect = { 0, (LONG) ribbonHeight, _clientSize.cx, _clientSize.cy };
		_dockPanel = dockPanelFactory (_hwnd, 0xFFFF, dockPanelRect);

		auto logSidePanel = _dockPanel->GetOrCreateSidePanel(Side::Right);
		_logArea = logAreaFactory (logSidePanel->GetHWnd(), 0xFFFF, logSidePanel->GetContentRect());

		_editArea = editAreaFactory (project, _dockPanel->GetHWnd(), 0xFFFF, selection, _rf, _dockPanel->GetContentRect());

		//_bridgePropsArea = bridgePropsAreaFactory (_hwnd, 0xFFFF, { 100, 100, 300, 300 });

		const RCHDeps rchDeps = { this, _rf, _project, _editArea, _selection };
		for (auto p : _commandHandlers)
			p.second->InjectDependencies(rchDeps);

		_selection->GetSelectionChangedEvent().AddHandler (&OnSelectionChanged, this);
	}

	~ProjectWindow()
	{
		_selection->GetSelectionChangedEvent().RemoveHandler (&OnSelectionChanged, this);

		if (_hwnd != nullptr)
			::DestroyWindow(_hwnd);
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
			// GDI now holds a pointer to this object, so let's call AddRef.
			window->AddRef();
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
			window->Release(); // Release the reference we added on WM_NCCREATE.
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
			_dockPanel = nullptr; // destroy it early to avoid doing layout-related processing
			if (_rf != nullptr)
				_rf->Destroy();
			return 0;
		}

		if (msg == WM_SIZE)
		{
			if (wParam == SIZE_RESTORED)
				::GetWindowRect(_hwnd, &_restoreBounds);

			_clientSize = { LOWORD(lParam), HIWORD(lParam) };
			if (_dockPanel != nullptr)
				ResizeDockPanel();

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

		return DefWindowProc(_hwnd, msg, wParam, lParam);
	}

	void ResizeDockPanel()
	{
		int x = 0, y = 0, w = _clientSize.cx, h = _clientSize.cy;

		if (_rf != nullptr)
		{
			ComPtr<IUIRibbon> ribbon;
			auto hr = _rf->GetView(0, IID_PPV_ARGS(&ribbon)); ThrowIfFailed(hr);

			UINT32 ribbonHeight;
			hr = ribbon->GetHeight(&ribbonHeight); ThrowIfFailed(hr);

			y += ribbonHeight;
			h -= ribbonHeight;
		}

		::MoveWindow (_dockPanel->GetHWnd(), x, y, w, h, TRUE);
	}

	static bool TryGetSavedWindowLocation (int* xOut, int* yOut, int* wOut, int* hOut, int* nCmdShowOut)
	{
		auto ReadDword = [](const wchar_t* valueName, DWORD* valueOut) -> bool
		{
			DWORD dataSize = 4;
			auto lresult = RegGetValue(HKEY_CURRENT_USER, App->GetRegKeyPath(), valueName, RRF_RT_REG_DWORD, nullptr, valueOut, &dataSize);
			return lresult == ERROR_SUCCESS;
		};

		int x, y, w, h, showCmd;
		if (ReadDword(RegValueNameShowCmd, (DWORD*)&showCmd)
			&& ReadDword(RegValueNameWindowX, (DWORD*)&x)
			&& ReadDword(RegValueNameWindowY, (DWORD*)&y)
			&& ReadDword(RegValueNameWindowWidth, (DWORD*)&w)
			&& ReadDword(RegValueNameWindowHeight, (DWORD*)&h))
		{
			*xOut = x;
			*yOut = y;
			*wOut = w;
			*hOut = h;
			*nCmdShowOut = showCmd;
			return true;
		}
		else
			return false;
	}

	void SaveWindowLocation() const
	{
		WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
		BOOL bRes = GetWindowPlacement(GetHWnd(), &wp);
		if (bRes && ((wp.showCmd == SW_NORMAL) || (wp.showCmd == SW_MAXIMIZE)))
		{
			HKEY key;
			auto lstatus = RegCreateKeyEx(HKEY_CURRENT_USER, App->GetRegKeyPath(), 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
			if (lstatus == ERROR_SUCCESS)
			{
				int w = _restoreBounds.right - _restoreBounds.left;
				int h = _restoreBounds.bottom - _restoreBounds.top;
				RegSetValueEx(key, RegValueNameWindowX, 0, REG_DWORD, (BYTE*)&_restoreBounds.left, 4);
				RegSetValueEx(key, RegValueNameWindowY, 0, REG_DWORD, (BYTE*)&_restoreBounds.top, 4);
				RegSetValueEx(key, RegValueNameWindowWidth, 0, REG_DWORD, (BYTE*)&w, 4);
				RegSetValueEx(key, RegValueNameWindowHeight, 0, REG_DWORD, (BYTE*)&h, 4);
				RegSetValueEx(key, RegValueNameShowCmd, 0, REG_DWORD, (BYTE*)&wp.showCmd, 4);
				RegCloseKey(key);
			}
		}
	}

	virtual unsigned int GetSelectedTreeIndex() const override final { return 0; }

	virtual SelectedTreeIndexChangedEvent::Subscriber GetSelectedTreeIndexChangedEvent() override final { return SelectedTreeIndexChangedEvent::Subscriber(_em); }
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
	#pragma region IUIApplication
	virtual HRESULT STDMETHODCALLTYPE OnViewChanged(UINT32 viewId, UI_VIEWTYPE typeID, IUnknown *view, UI_VIEWVERB verb, INT32 uReasonCode) override final
	{
		if (_dockPanel != nullptr)
			ResizeDockPanel();
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnCreateUICommand(UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler **commandHandler) override final
	{
		auto it = _commandHandlers.find(commandId);
		if (it == _commandHandlers.end())
			return E_NOTIMPL;

		*commandHandler = it->second;
		it->second->AddRef();
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnDestroyUICommand(UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler *commandHandler) override final
	{
		return E_NOTIMPL;
	}
	#pragma endregion

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		if (!ppvObject)
			return E_INVALIDARG;

		*ppvObject = NULL;
		if (riid == __uuidof(IUnknown))
		{
			*ppvObject = static_cast<IUnknown*>((IProjectWindow*) this);
			AddRef();
			return S_OK;
		}
		else if (riid == __uuidof(IUIApplication))
		{
			*ppvObject = static_cast<IUIApplication*>(this);
			AddRef();
			return S_OK;
		}

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override
	{
		ULONG newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
	#pragma endregion
};

extern const ProjectWindowFactory projectWindowFactory = [](auto... params) { return ComPtr<IProjectWindow>(new ProjectWindow(params...), false); };
